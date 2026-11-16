// SPDX-License-Identifier: Apache-2.0
// System executable: resolves the SAME /system/lib64/libMiscTaWrapper.so used
// by sony-modem-switcher. Sony ships it as PRODUCT_COPY_FILES, not a Soong
// library. Explicit dlopen avoids a duplicate installed prebuilt/SONAME.
#include "guard_logic.h"
#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <openssl/sha.h>
#ifndef MODEM_GUARD_HOST
#include <sys/system_properties.h>
#endif

namespace {
constexpr char BASE[] = "/system/etc/customization/modem/";
constexpr char FSG[] = "/dev/block/bootdevice/by-name/fsg";
constexpr char HISTORY[] = "/cache/modem/history";
constexpr char SWITCHER[] = "/system/bin/sony-modem-switcher";
int logfd = -1;
int kmsg = -1;
double now() {
    timespec t{};
    if (clock_gettime(CLOCK_MONOTONIC, &t)) _exit(125);
    return t.tv_sec + t.tv_nsec / 1e9;
}
void pause_ms(unsigned ms) {
    timespec t{static_cast<time_t>(ms / 1000), static_cast<long>(ms % 1000) * 1000000};
    while (nanosleep(&t, &t) && errno == EINTR) {}
}
void log(const char* fmt, ...) {
    char text[768];
    va_list ap; va_start(ap, fmt); vsnprintf(text, sizeof(text), fmt, ap); va_end(ap);
    if (logfd >= 0) dprintf(logfd, "%.3f %s\n", now(), text);
    if (kmsg >= 0) dprintf(kmsg, "<5>modem_guard: %.3f %s\n", now(), text);
}
bool stopped(const char* name) {
#ifdef MODEM_GUARD_HOST
    (void)name; return true;
#else
    char prop[96], value[PROP_VALUE_MAX]{};
    snprintf(prop, sizeof(prop), "init.svc.%s", name);
    __system_property_get(prop, value);
    return !value[0] || !strcmp(value, "stopped");
#endif
}
bool consumers_stopped() {
    return stopped("vendor.rmt_storage") && stopped("vendor.per_mgr") &&
        stopped("vendor.per_proxy") && stopped("vendor.qcrild") && stopped("vendor.qcrild2");
}
// Never return to the transaction with a surviving timed-out child. Init's
// service-cgroup timeout is the final backstop for pathological kernel I/O.
int reap(pid_t pid, double until) {
    int status = 0;
    while (now() < until) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        if (r < 0 && errno != EINTR) return 125;
        pause_ms(20);
    }
    kill(pid, SIGKILL);
    double grace = now() + 0.5;
    while (now() < grace) {
        if (waitpid(pid, &status, WNOHANG) == pid) return 124;
        pause_ms(10);
    }
    log("fatal: child did not reap after SIGKILL; abort transaction");
    _exit(125);
}
template<class F> int child(unsigned milliseconds, F fn) {
    pid_t p = fork();
    if (p < 0) return 125;
    if (!p) {
        pid_t parent = getppid();
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parent) _exit(125);
        _exit(fn());
    }
    return reap(p, now() + milliseconds / 1000.0);
}
struct Unit { uint32_t n; unsigned char bytes[256]; };
struct Snapshot { Unit cust, current, command; int rc42; };
bool equal(const Unit& a, const Unit& b) {
    return a.n == b.n && a.n <= sizeof(a.bytes) && !memcmp(a.bytes, b.bytes, a.n);
}
std::string text(const Unit& u) {
    if (!u.n || u.n > sizeof(u.bytes) || memchr(u.bytes, 0, u.n)) return {};
    return std::string(reinterpret_cast<const char*>(u.bytes), u.n);
}
// ABI from the exported C symbols and AArch64 callers: read arg3 is a
// uint32_t* capacity/result, write arg3 is a uint32_t VALUE (not size_t*).
struct TA {
    void* handle;
    int (*size)(uint32_t, uint32_t*);
    int (*read)(uint32_t, void*, uint32_t*);
    int (*write)(uint32_t, const void*, uint32_t);
    static const char* library() {
#ifdef MODEM_GUARD_HOST
        if (const char* p = getenv("MODEM_GUARD_FAKE_TA")) return p;
#endif
        return "/system/lib64/libMiscTaWrapper.so";
    }
    TA() : handle(dlopen(library(), RTLD_NOW | RTLD_LOCAL)),
        size(nullptr), read(nullptr), write(nullptr) {
        if (handle) {
            size = reinterpret_cast<decltype(size)>(dlsym(handle, "miscta_get_unit_size"));
            read = reinterpret_cast<decltype(read)>(dlsym(handle, "miscta_read_unit"));
            write = reinterpret_cast<decltype(write)>(dlsym(handle, "miscta_write_unit_critical"));
        }
    }
    bool get(uint32_t id, Unit& out) {
        uint32_t n = 0;
        if (!size || !read || size(id, &n) || !n || n > sizeof(out.bytes)) return false;
        out = {};
        out.n = sizeof(out.bytes); // wrapper copies the supplied capacity, then returns actual size
        return !read(id, out.bytes, &out.n) && out.n == n;
    }
    bool snapshot(Snapshot& s) {
        uint32_t n = 0;
        if (!size || !read || !write) return false;
        s.rc42 = size(42, &n);
        return (s.rc42 == 0 || s.rc42 == 5) && get(2212, s.cust) &&
               get(2404, s.current) && get(2405, s.command);
    }
    bool put(uint32_t id, const std::string& bytes) {
        Unit check{};
        return write && !write(id, bytes.data(), bytes.size()) && get(id, check) && text(check) == bytes;
    }
};
int snapshot(Snapshot& out, unsigned budget_ms = 2000) {
    auto* shared = static_cast<Snapshot*>(mmap(nullptr, sizeof(Snapshot), PROT_READ | PROT_WRITE,
                                             MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (shared == MAP_FAILED) return 125;
    memset(shared, 0, sizeof(*shared));
    shared->rc42 = -1;
    int rc = child(budget_ms, [&] { TA ta; return ta.snapshot(*shared) ? 0 : 1; });
    out = *shared; // diagnostics only on failure; callers must check rc
    munmap(shared, sizeof(*shared));
    return rc;
}
// 0=no command, 1=explicit safe image command, -1=unsupported/malformed.
int command(const Unit& u, std::string& target) {
    if (u.n <= 2) return 0;
    if (u.n > 256 || u.bytes[0] != 0xf0 || u.bytes[1] != 0x7a || u.bytes[2] != 1) return -1;
    target.assign(reinterpret_cast<const char*>(u.bytes + 3), u.n - 3);
    return modem_guard::image_name(target) ? 1 : -1;
}
bool hash_fd(int fd, size_t n, unsigned char digest[SHA256_DIGEST_LENGTH]) {
    SHA256_CTX ctx; SHA256_Init(&ctx);
    unsigned char buf[8192];
    while (n) {
        ssize_t r = read(fd, buf, std::min(n, sizeof(buf)));
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        SHA256_Update(&ctx, buf, r); n -= r;
    }
    SHA256_Final(digest, &ctx); return true;
}
// Return -2 missing/invalid image, -1 unreadable, 0 differs, 1 matches.
int compare(const std::string& name, const char* base = BASE, const char* partition = FSG) {
    if (!modem_guard::image_name(name)) return -2;
    std::string path = std::string(base) + name;
    int image = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (image < 0) return errno == ENOENT ? -2 : -1;
    struct stat st{};
    if (fstat(image, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 16 * 1024 * 1024) {
        close(image); return -2;
    }
    int fsg = open(partition, O_RDONLY | O_CLOEXEC);
    unsigned char a[32], b[32];
    bool ok = fsg >= 0 && hash_fd(image, st.st_size, a) && hash_fd(fsg, st.st_size, b);
    close(image); if (fsg >= 0) close(fsg);
    if (!ok) return -1;
    return !memcmp(a, b, sizeof(a));
}
constexpr char STATUS[] = "/cache/modem/modem_switcher_status";
int status(const char* path = STATUS) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return -1;
    char b[3]{}; ssize_t n = read(fd, b, sizeof(b)); close(fd);
    return n >= 2 && b[0] >= '0' && b[0] <= '4' && b[1] == ',' ? b[0] - '0' : -1;
}
// Sony's switcher only (re)creates its status file when it is ABSENT on an early
// no-op, so a "2,default" left by an earlier failed run survives every later good
// boot and CustomizationSelector/Xperia Parts refuse to read or switch the modem
// config (XDA X49). Only called after a clean switcher run whose TA 2404 label and
// physical fsg both verify as `image`: rewrite the existing file in the stock
// format "<status>,<config>" (no newline). No TA or partition writes.
bool refresh_status(const std::string& image, const char* path = STATUS) {
    if (!modem_guard::image_name(image)) return false;
    int fd = open(path, O_WRONLY | O_TRUNC | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    const std::string line = "0," + image;
    bool ok = write(fd, line.data(), line.size()) == static_cast<ssize_t>(line.size()) && !fsync(fd);
    return !close(fd) && ok;
}
void reports(const char* stage) {
    log("report_snapshot=%s cached_status=%d", stage, status());
    for (const char* name : {"modem_switcher_status", "modem_switcher_report"}) {
        std::string path = std::string("/cache/modem/") + name;
        int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) continue;
        dprintf(logfd, "--- %s %s (may be stale; max 32768 bytes) ---\n", stage, name);
        char buf[1024]; size_t total = 0;
        while (total < 32768) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            (void)write(logfd, buf, n); total += n;
        }
        dprintf(logfd, "\n--- end ---\n"); close(fd);
    }
}
int switch_once() {
    if (!consumers_stopped()) { log("refused: modem consumers already active"); return 1; }
    return child(7000, [] {
        execl(SWITCHER, SWITCHER, static_cast<char*>(nullptr));
        return 127;
    });
}
int arm(const Snapshot& expected, const std::string& target) {
    return child(3000, [&] {
        TA ta; Snapshot current{};
        // Compare-and-check against the last complete snapshot. No blind
        // overwrite of a concurrent carrier request; 2212 is never written.
        if (!ta.snapshot(current) || !equal(current.cust, expected.cust) ||
            !equal(current.current, expected.current) || !equal(current.command, expected.command)) return 1;
        std::string pending;
        if (command(current.command, pending) != 0) return 1;
        if (!ta.put(2405, std::string("\xf0\x7a\x01", 3) + target)) return 2;
        if (!ta.put(2404, "temporary_modem")) return 3;
        Unit cust{};
        return ta.get(2212, cust) && equal(cust, expected.cust) ? 0 : 4;
    });
}
void publish_image(const std::string& image) {
#ifndef MODEM_GUARD_HOST
    if (__system_property_set("sys.modem_guard.image", image.c_str())) log("image property publish failed");
#else
    (void)image;
#endif
}
int transaction() {
    reports("entry");
    publish_image("");
    if (!consumers_stopped()) { log("skip: not an exclusive pre-modem boot window"); return 1; }
    struct stat node{};
    if (stat("/dev/block/sda1", &node) || !S_ISBLK(node.st_mode) ||
        node.st_uid != 2997 || node.st_gid != 2997 || (node.st_mode & 07777) != 0770) {
        log("skip: TA DAC still incorrect after init repair (no raw TA access)"); return 1;
    }
    Snapshot initial{};
    double deadline = now() + 15;
    bool ready = false;
    while (now() < deadline) {
        double remaining = deadline - now();
        if (remaining <= 0) break;
        unsigned ms = std::min(2000u, static_cast<unsigned>(remaining * 1000));
        int rc = snapshot(initial, std::max(1u, ms));
        log("TA probe rc=%d unit42=%d", rc, initial.rc42);
        if (!rc) { ready = true; break; }
        if (now() + 0.15 < deadline) pause_ms(150);
    }
    if (!ready) { log("skip: TA readiness deadline; switcher NOT executed"); return 1; }
    std::string requested, original = text(initial.current);
    int cmd = command(initial.command, requested);
    if (cmd < 0) { log("skip: malformed or opcode02 command (no guessed default)"); return 1; }
    std::string target = cmd == 1 ? requested : original;
    // Readable but invalid 2404 must not unlock the stock generic fallback.
    if ((!modem_guard::image_name(original) && !(original == "temporary_modem" && cmd == 1)) || compare(target) < 0) {
        log("skip: missing/unsafe image or invalid current config; no fallback"); return 1;
    }
    log("TA ready; target=%s explicit=%d", target.c_str(), cmd == 1);
    reports("before");
    int rc = switch_once();
    log("first switcher exit=%d", rc);
    reports("after-first");
    if (rc == 124 || rc == 125 || rc == 127 || (rc >= 128 && rc < 192)) {
        log("stop: switcher timeout/signal/exec failure; no retry"); return 1;
    }
    Snapshot after{};
    if (snapshot(after) || !equal(after.cust, initial.cust)) {
        log("stop: post-switch TA unavailable/changed customization"); return 1;
    }
    std::string current = text(after.current), pending;
    if (command(after.command, pending) != 0) {
        log("stop: pending/uncleared command; do not overwrite it"); return 1;
    }
    // Prefer the valid pre-switch 2404 (or explicit command) if the stock
    // binary changed it to generic after a transient failure. Do NOT mine
    // private CS preferences before userdata is ready; TA is the authority.
    int comparison = compare(target);
    auto decision = modem_guard::decide(true, target, comparison != -2, comparison != -1,
                                       comparison == 1, status());
    log("post-check target=%s current=%s prefix=%d decision=%d status=%d",
        target.c_str(), current.c_str(), comparison, static_cast<int>(decision), status());
    if (decision == modem_guard::Decision::Match) {
        int cached = status();
        if (rc == 0 && current == target && cached != 0) {
            bool fixed = refresh_status(target);
            log("stale status %d with verified fsg/2404 match: refreshed=%d now=%d", cached, fixed, status());
        }
        log("finish: fsg matches; label_equal=%d cached_status=%d", current == target, status());
        if (current == target) publish_image(target);
        return current == target ? 0 : 1;
    }
    if (decision != modem_guard::Decision::Repair || !consumers_stopped()) return 1;
    // Only ONE re-arm/re-run. Partial arming is preserved in history, never
    // followed by an unverified switch or an automatic reboot/reset loop.
    log("repair intent: fsg mismatch; arm 2405 then 2404, both with API readback");
    int armed = arm(after, target);
    log("arm exit=%d", armed);
    if (armed) { log("stop: partial/failed arm; inspect TA using Sony API"); return 1; }
    rc = switch_once();
    log("repair switcher exit=%d", rc);
    reports("after-repair");
    Snapshot final{};
    int checked = snapshot(final);
    comparison = compare(target);
    bool ok = !rc && !checked && equal(final.cust, initial.cust) && text(final.current) == target &&
              command(final.command, pending) == 0 && comparison == 1 && status() == 0;
    log("repair verified=%d TA_rc=%d prefix=%d status=%d", ok, checked, comparison, status());
    if (ok) publish_image(target);
    return ok ? 0 : 1;
}
bool history(const char* directory = HISTORY) {
    int dir = open(directory, O_DIRECTORY | O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    // Created here (type_transition -> modem_guard_history_file), not by vendor_init,
    // which may not search cache_file. Parent /cache/modem is the switcher's own dir.
    if (dir < 0 && errno == ENOENT && !mkdir(directory, 0700))
        dir = open(directory, O_DIRECTORY | O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (dir < 0) return false;
    struct stat st{};
    if (fstat(dir, &st) || st.st_uid != geteuid() || (st.st_mode & 0777) != 0700) { close(dir); return false; }
    unlinkat(dir, "boot.9.log", 0);
    for (int i = 8; i >= 0; --i) {
        char a[32], b[32]; snprintf(a, sizeof(a), "boot.%d.log", i); snprintf(b, sizeof(b), "boot.%d.log", i + 1);
        if (renameat(dir, a, dir, b) && errno != ENOENT) { close(dir); return false; }
    }
    logfd = openat(dir, "boot.0.log", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    fsync(dir); close(dir); return logfd >= 0;
}
} // namespace

#ifndef MODEM_GUARD_TEST
int main(int argc, char**) {
    if (geteuid()) return 2;
    umask(077);
    kmsg = open("/dev/kmsg", O_WRONLY | O_CLOEXEC | O_NOCTTY);
    if (argc != 1) return 2;
    // History is diagnostics only: never let it block the modem safety transaction.
    if (!history()) log("history unavailable (errno %d); continuing with kmsg logging only", errno);
    log("begin (15s readiness, 40s supervisor, 45s init service timeout)");
    pid_t p = fork();
    if (p < 0) return 2;
    if (!p) {
        setpgid(0, 0);
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        _exit(transaction());
    }
    setpgid(p, p);
    int result = reap(p, now() + 40);
    // Kill only this invocation's process group, including a stuck probe or
    // transitioned switcher. Never pattern-kill other modem/TA services.
    kill(-p, SIGKILL);
    log("end exit=%d; init may now continue to modem consumers", result);
    fsync(logfd); close(logfd); if (kmsg >= 0) close(kmsg);
    return result;
}
#endif
