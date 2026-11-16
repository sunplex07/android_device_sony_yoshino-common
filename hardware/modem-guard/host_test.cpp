// SPDX-License-Identifier: Apache-2.0
#define MODEM_GUARD_HOST
#define MODEM_GUARD_TEST
#include "modem_guard.cpp"
#include "../attach-primer/repair_decision.h"
#include <cassert>
#include <dirent.h>
#include <fstream>

static void file(const std::string& p, const std::string& bytes) {
    std::ofstream f(p, std::ios::binary); f << bytes; assert(f.good());
}
int main(int argc, char** argv) {
    assert(argc == 2);
    setenv("MODEM_GUARD_FAKE_TA", argv[1], 1);
    using modem_guard::Decision;
    using modem_guard::decide;
    const std::string carrier = "amss_fsg_lilac_tmobile_us_ims_tar.mbn";
    assert(modem_guard::image_name(carrier));
    assert(!modem_guard::image_name("../" + carrier));
    assert(!modem_guard::image_name("amss_fsg_../foo_tar.mbn"));
    assert(!modem_guard::image_name(carrier + std::string("\0", 1)));
    assert(decide(false, carrier, true, true, false, 2) == Decision::Unreachable);
    assert(decide(true, carrier, true, true, false, 2) == Decision::Repair);
    assert(decide(true, carrier, true, true, false, 0) == Decision::Repair);
    assert(decide(true, carrier, true, true, true, 2) == Decision::Match);
    assert(decide(true, "temporary_modem", false, true, false, 2) == Decision::Temporary);
    assert(decide(true, carrier, false, true, false, 2) == Decision::MissingImage);
    assert(decide(true, carrier, true, false, false, 2) == Decision::Unreachable);
    puts("PASS decision: unreachable/status2+generic/match/temporary/missing/read-error");

    Snapshot s{};
    for (const char* m : {"good", "42present"}) {
        setenv("FAKE_TA_MODE", m, 1); assert(snapshot(s, 500) == 0);
        assert(text(s.current) == carrier && text(s.cust) == "408");
    }
    for (const char* m : {"unreachable", "missing", "readfail"}) {
        setenv("FAKE_TA_MODE", m, 1); assert(snapshot(s, 500) != 0);
    }
    setenv("FAKE_TA_MODE", "hang", 1);
    double begin = now(); assert(snapshot(s, 100) == 124); assert(now() - begin < 1.0);
    int ws; errno = 0; assert(waitpid(-1, &ws, WNOHANG) == -1 && errno == ECHILD);
    puts("PASS real subprocess boundary: wrapper ABI, read-path failure, 100ms timeout/reap");

    Unit u{}; std::string target;
    u.n = 1; assert(command(u, target) == 0);
    const std::string request = std::string("\xf0\x7a\x01", 3) + carrier;
    u.n = request.size(); memcpy(u.bytes, request.data(), u.n);
    assert(command(u, target) == 1 && target == carrier);
    u.bytes[2] = 2; assert(command(u, target) == -1);
    u.bytes[2] = 1; u.bytes[3] = '/'; assert(command(u, target) == -1);
    puts("PASS command: empty/explicit/unsupported02/unsafe basename");

    char tmp[] = "/tmp/modem-guard-host-XXXXXX"; assert(mkdtemp(tmp));
    const std::string base = std::string(tmp) + "/", fsg = base + "fsg";
    const std::string audit = base + "ta-audit";
    setenv("FAKE_TA_AUDIT", audit.c_str(), 1);
    setenv("FAKE_TA_MODE", "good", 1); assert(snapshot(s, 500) == 0);
    assert(arm(s, carrier) == 0);
    { std::ifstream f(audit); int first, second; f >> first >> second; assert(first == 2405 && second == 2404); }
    unlink(audit.c_str());
    setenv("FAKE_TA_MODE", "writefail2404", 1); assert(arm(s, carrier) == 3);
    unlink(audit.c_str());
    s.cust.bytes[0] = 'X'; assert(arm(s, carrier) == 1);
    assert(access(audit.c_str(), F_OK) != 0);
    unsetenv("FAKE_TA_AUDIT");
    assert(child(500, [] { return 255; }) == 255); // stock error is not a signal
    puts("PASS arm: 2405-before-2404/readback, partial failure, changed-state refusal, exit255");
    file(base + carrier, "carrier-image"); file(fsg, "generic-image and trailing bytes");
    assert(compare(carrier, base.c_str(), fsg.c_str()) == 0);
    file(fsg, "carrier-image arbitrary trailing partition bytes");
    assert(compare(carrier, base.c_str(), fsg.c_str()) == 1);
    file(fsg, "short"); assert(compare(carrier, base.c_str(), fsg.c_str()) == -1);
    unlink((base + carrier).c_str()); assert(compare(carrier, base.c_str(), fsg.c_str()) == -2);
    assert(!symlink(fsg.c_str(), (base + carrier).c_str()));
    assert(compare(carrier, base.c_str(), fsg.c_str()) == -1);
    unlink((base + carrier).c_str()); unlink(fsg.c_str());
    puts("PASS physical fsg SHA256 prefix: generic/match+tail/short/missing/symlink");

    const std::string st = base + "modem_switcher_status";
    assert(!refresh_status(carrier, st.c_str()));            // absent file: never created here
    file(st, "2,default"); assert(status(st.c_str()) == 2);
    assert(!refresh_status("../evil", st.c_str()));           // unsafe name refused
    assert(status(st.c_str()) == 2);
    assert(refresh_status(carrier, st.c_str()) && status(st.c_str()) == 0);
    { std::ifstream f(st, std::ios::binary); std::string got((std::istreambuf_iterator<char>(f)), {});
      assert(got == "0," + carrier); }                        // exact stock format, no newline
    unlink(st.c_str());
    puts("PASS status refresh: stale 2 -> \"0,<image>\" exact format; absent/unsafe refused");

    for (int i = 0; i < 13; ++i) {
        assert(history(tmp)); dprintf(logfd, "%d\n", i); close(logfd); logfd = -1;
    }
    int count = 0; DIR* dir = opendir(tmp); assert(dir);
    while (auto* entry = readdir(dir)) if (!strncmp(entry->d_name, "boot.", 5)) ++count;
    closedir(dir); assert(count == 10);
    for (int i = 0; i < 10; ++i) {
        auto p = base + "boot." + std::to_string(i) + ".log";
        std::ifstream f(p); int generation = -1; f >> generation; assert(generation == 12 - i);
        struct stat st{}; assert(!stat(p.c_str(), &st) && (st.st_mode & 0777) == 0600);
        unlink(p.c_str());
    }
    rmdir(tmp);
    puts("PASS history: 13 generations retain exactly last 10, mode 0600");

    const char* image = carrier.c_str();
    assert(!strcmp(repair_apn(image, "wrong-carrier", "other.apn", 1), "fast.t-mobile.com"));
    assert(!strcmp(repair_apn(image, image, "tello", 1), "fast.t-mobile.com"));
    assert(!strcmp(repair_apn(image, image, "fast.t-mobile.com", 1), "fast.t-mobile.com"));
    assert(!strcmp(repair_apn(image, "", "", 0), "fast.t-mobile.com"));
    assert(repair_apn("", image, "fast.t-mobile.com", 1) == nullptr);
    assert(repair_apn("unknown-image", image, "fast.t-mobile.com", 1) == nullptr);
    assert(!strcmp(repair_apn("unknown-image", "unknown-image", "known.attach", 1), "known.attach"));
    assert(!auxiliary_read_required(1) && auxiliary_read_required(0));
    puts("PASS attach decisions: wrong/legacy snapshot/default/unknown/profile5 repair-only tolerance");
    /* poplar: profile 5 does not exist -> absent, exact wire values captured on device
     * (oq raw 1 0x2b 01:0005 -> result=1 error=0x51, TLV 0xE0 = 0x0005). */
    assert(profile_absent(1, 0x51, 1, 5));
    assert(!profile_absent(0, 0, 0, 0));      /* success is not "absent" */
    assert(!profile_absent(1, 0x51, 0, 5));   /* no extended TLV -> unreadable */
    assert(!profile_absent(1, 0x51, 1, 4));   /* other extended code -> unreadable */
    assert(!profile_absent(1, 0x52, 1, 5));   /* other QMI error -> unreadable */
    assert(!profile_absent(-1, -1, 0, 0));    /* transport failure -> unreadable */
    /* poplar: empty Sony profile 1 + no evidence-backed default -> skip, never write */
    assert(skip_unprimable(1, nullptr));
    assert(!skip_unprimable(1, "fast.t-mobile.com"));  /* repairable instead */
    assert(!skip_unprimable(0, nullptr));               /* garbage APN stays a refusal */
    assert(repair_apn("amss_fsg_poplar_tar.mbn", "", "", 0) == nullptr);
    assert(repair_apn("amss_fsg_poplar_ir51_ir92_ims_tar.mbn", "", "", 0) == nullptr);
    assert(!strcmp(repair_apn("amss_fsg_poplar_dsds_tmobile_us_ims_tar.mbn", "", "", 0),
                   "fast.t-mobile.com"));
    assert(!strcmp(repair_apn("amss_fsg_poplar_dsds_tmobile_us_tar.mbn", "", "", 0),
                   "fast.t-mobile.com"));
    assert(repair_apn("amss_fsg_poplar_tmobile_austria_tar.mbn", "", "", 0) == nullptr);
    puts("PASS poplar attach decisions: profile5 absent vs unreadable, empty-APN skip, dsds T-Mobile default");
    puts("ALL HOST TESTS PASSED (no Android/phone/QMI execution)");
}
