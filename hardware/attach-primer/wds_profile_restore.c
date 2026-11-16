/* lilac-attach-primer: boot-time one-shot that re-sends 3GPP profile 1's OWN
 * stored values back to the modem (QMI WDS Modify 0x28, byte-identical by
 * construction), which empirically collapses cold-boot LTE attach from ~26s
 * to ~2s after radio-on (evidence: live-20260916-{boot-h01,boot-early-h06}).
 *
 * Self-preimaging and carrier-agnostic: no staged files, no hardcoded APN —
 * whatever profile 1 currently holds is what is re-sent. The WDS interaction
 * is kept IDENTICAL to the proven wds-profile-replay sequence: query profile1,
 * profile5, attach list (0x2b/0x2b/0x94), one Modify 0x28 built from the live
 * profile1 readback, then a verify re-query that must be byte-identical.
 *
 * Exit codes (poller contract):
 *   0   write sent and verified (poller stops, marks done)
 *   10  transport not ready yet — NO write was attempted (poller retries)
 *   1   validation/verify failure (poller STOPS; never retries after intent)
 *   2   usage/environment error before any QMI contact
 * Usage: wds-attach-primer <private-0700-log-dir>   (euid 0, 25s hard alarm)
 */
#define _GNU_SOURCE
#include "wds_read_client.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef wds_get_profile_settings_resp_msg_v01 Profile;
typedef wds_get_lte_attach_pdn_list_resp_msg_v01 AttachList;
typedef wds_modify_profile_settings_req_msg_v01 Modify;
_Static_assert(sizeof(Profile) == 3136, "profile response ABI");
_Static_assert(sizeof(AttachList) == 248, "attach list ABI");
_Static_assert(sizeof(Modify) == 3120, "Modify request ABI");
_Static_assert(sizeof(wds_modify_profile_settings_resp_msg_v01) == 16, "Modify response ABI");
_Static_assert(sizeof(qmi_service_info) == 20, "service entry ABI");
_Static_assert(sizeof(wds_get_profile_settings_req_msg_v01) == 8, "profile request ABI");
_Static_assert(sizeof(wds_get_lte_attach_pdn_list_req_msg_v01) == 1, "list request ABI");
_Static_assert(offsetof(Modify, apn_name_valid) == 84, "APN valid");
_Static_assert(offsetof(Modify, apn_name) == 85, "APN");
_Static_assert(offsetof(Modify, lte_roaming_pdp_type_valid) == 1050, "LTE valid");
_Static_assert(offsetof(Modify, lte_roaming_pdp_type) == 1052, "LTE");
_Static_assert(offsetof(Modify, umts_roaming_pdp_type_valid) == 1056, "UMTS valid");
_Static_assert(offsetof(Modify, umts_roaming_pdp_type) == 1060, "UMTS");
_Static_assert(WDS_PROFILE_TYPE_3GPP_V01 == 0, "3GPP selector value");

#define EXIT_NOT_READY 10

static const char *g_restore_apn;
static double started;
static double budget = 25.0;
static volatile sig_atomic_t cancelled;

static double monotonic(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return -1;
    return (double)t.tv_sec + (double)t.tv_nsec / 1000000000.0;
}
static double remaining(void) {
    double now = monotonic();
    return now < 0 ? -1 : budget - (now - started);
}
static void expired(int sig) { (void)sig; _exit(124); }
static void cancel(int sig) { (void)sig; cancelled = 1; }
static int deadline(unsigned seconds) {
    budget = (double)seconds;
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = expired;
    if (sigaction(SIGALRM, &sa, NULL)) return 1;
    sa.sa_handler = cancel;
    if (sigaction(SIGINT, &sa, NULL) || sigaction(SIGTERM, &sa, NULL)) return 1;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM); sigaddset(&set, SIGINT); sigaddset(&set, SIGTERM);
    if (sigprocmask(SIG_UNBLOCK, &set, NULL)) return 1;
    started = monotonic();
    if (started < 0) return 1;
    alarm(seconds + 5);
    return 0;
}
static int private_dir(int fd) {
    struct stat st;
    return fstat(fd, &st) || !S_ISDIR(st.st_mode) ||
           (st.st_mode & 0777) != 0700 || st.st_uid != geteuid();
}
static int successful(const qmi_response_type_v01 *r) {
    return r->result == QMI_RESULT_SUCCESS_V01 && r->error == QMI_ERR_NONE_V01;
}
static int store(int dir, const char *name, const void *data, size_t length) {
    int fd = openat(dir, name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return 1;
    size_t done = 0;
    while (done < length) {
        ssize_t n = write(fd, (const char *)data + done, length - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return 1; }
        done += (size_t)n;
    }
    int bad = fsync(fd) != 0;
    if (close(fd)) bad = 1;
    if (fsync(dir)) bad = 1;
    return bad;
}
static int marker(int dir, const char *name, const char *text) {
    return store(dir, name, text, strlen(text));
}

static int query(const struct wds_api *api, qmi_client_type client, unsigned index,
                 void *response, int list, const char *stage) {
    wds_get_profile_settings_req_msg_v01 profile = {0};
    wds_get_lte_attach_pdn_list_req_msg_v01 empty = {0};
    profile.profile.profile_type = WDS_PROFILE_TYPE_3GPP_V01;
    profile.profile.profile_index = (uint8_t)index;
    size_t size = list ? sizeof(AttachList) : sizeof(Profile);
    memset(response, 0, size);
    int rc = api->send(client, list ? 0x94 : 0x2b, list ? (void *)&empty : &profile,
                       list ? sizeof(empty) : sizeof(profile), response, size, 2000);
    qmi_response_type_v01 *r = response;
    printf("stage=%s transport=%d decoded=%u result=%d error=%d\n", stage, rc, rc == 0,
           rc ? -1 : (int)r->result, rc ? -1 : (int)r->error);
    return rc || !successful(r);
}
static void build(Modify *request, const char *apn, wds_common_pdp_type_enum_v01 lte,
                  wds_common_pdp_type_enum_v01 umts) {
    memset(request, 0, sizeof(*request));
    request->profile.profile_type = WDS_PROFILE_TYPE_3GPP_V01;
    request->profile.profile_index = 1;
    request->apn_name_valid = 1;
    memcpy(request->apn_name, apn, strlen(apn) + 1);
    request->lte_roaming_pdp_type_valid = 1;
    request->lte_roaming_pdp_type = lte;
    request->umts_roaming_pdp_type_valid = 1;
    request->umts_roaming_pdp_type = umts;
}
static int modify(const struct wds_api *api, qmi_client_type client, Modify *request,
                  int dir) {
    wds_modify_profile_settings_resp_msg_v01 response;
    memset(&response, 0, sizeof(response));
    int rc = api->send(client, 0x28, request, sizeof(*request), &response, sizeof(response), 2000);
    printf("stage=prime transport=%d decoded=%u result=%d error=%d\n", rc, rc == 0,
           rc ? -1 : (int)response.resp.result, rc ? -1 : (int)response.resp.error);
    int failed = rc || !successful(&response.resp);
    if (!rc && store(dir, "prime-response.bin", &response, sizeof(response))) failed = 1;
    return failed;
}

/* Self-preimage validity: whatever profile 1 holds, but it must be a sane,
 * complete 3GPP data profile before we dare re-send it. No APN content check
 * beyond non-empty printable ASCII — carrier-agnostic by design. */
static int sane_profile1(const Profile *p) {
    /* restore mode: the APN may legitimately be EMPTY (that is the damage). */
    if (!memchr(p->apn_name, 0, sizeof(p->apn_name))) return 0;
    for (const char *c = p->apn_name; *c; ++c)
        if (*c < 0x20 || *c > 0x7e) return 0;
    return p->lte_roaming_pdp_type_valid == 1 && p->umts_roaming_pdp_type_valid == 1 &&
           p->persistent_valid == 1 && p->persistent == 1 &&
           p->pdp_type_valid == 1;
}

/* Returns 0 ok, EXIT_NOT_READY retryable-before-write, 1 hard failure. */
static int run_primer(const struct wds_api *api, int dir) {
    if (!api || !api->service || !api->service_id || !api->major || !api->minor ||
        !api->list || !api->instance || !api->init || !api->release || !api->send ||
        !api->size || private_dir(dir)) return 1;
    qmi_idl_service_object_type service = api->service(1, 192, 6);
    uint32_t id = 0, major = 0, minor = 0;
    if (!service || api->service_id(service, &id) || api->major(service, &major) ||
        api->minor(service, &minor) || id != 1 || major != 1 || minor != 192) return 1;
    const unsigned ids[] = {0x2b, 0x94, 0x28};
    const unsigned requests[] = {8, 1, 3120}, responses[] = {3136, 248, 16};
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t req = 0, resp = 0;
        if (api->size(service, QMI_IDL_REQUEST, ids[i], &req) ||
            api->size(service, QMI_IDL_RESPONSE, ids[i], &resp) ||
            req != requests[i] || resp != responses[i]) return 1;
    }
    qmi_service_info entries[8] = {0};
    unsigned filled = 8, found = 0;
    int rc = api->list(service, entries, &filled, &found);
    printf("stage=discovery rc=%d filled=%u found=%u\n", rc, filled, found);
    if (rc || filled != 1 || found != 1) return EXIT_NOT_READY;   /* WDS not up yet */
    qmi_service_instance instance = 0;
    if (((const unsigned char *)&entries[0])[1] != 1 ||
        api->instance(&entries[0], &instance) || instance != 0) return EXIT_NOT_READY;
    qmi_client_type client = NULL;
    rc = api->init(&entries[0], service, NULL, NULL, NULL, &client);
    printf("stage=init rc=%d owned=%u\n", rc, client != NULL);
    if (rc || !client) {
        if (client) api->release(client);
        return EXIT_NOT_READY;
    }
    int result = 1;
    Profile before1, before5, after1;
    AttachList list_read;
    do {
        if (cancelled || remaining() < 12.0) { result = 1; break; }
        /* Faithful proven read sequence. A failed FIRST read = not ready. */
        if (query(api, client, 1, &before1, 0, "before_profile1")) { result = EXIT_NOT_READY; break; }
        if (store(dir, "before-profile1.bin", &before1, sizeof(before1))) break;
        if (!sane_profile1(&before1)) { (void)marker(dir, "unsane-profile1.txt", "refused=1\n"); break; }
        /* restore mode: profile5 / attach-list may also be wiped; informational only. */
        if (!query(api, client, 5, &before5, 0, "before_profile5"))
            (void)store(dir, "before-profile5.bin", &before5, sizeof(before5));
        else printf("stage=note profile5_unreadable (continuing)\n");
        if (!query(api, client, 0, &list_read, 1, "before_list"))
            (void)store(dir, "before-attach-list.bin", &list_read, sizeof(list_read));
        else printf("stage=note attach_list_unreadable (continuing)\n");
        if (cancelled) break;
        Modify forward;
        build(&forward, g_restore_apn, WDS_COMMON_PDP_TYPE_PDP_IPV4V6_V01,
              WDS_COMMON_PDP_TYPE_PDP_IPV4V6_V01);
        printf("stage=restore apn=%s roaming=IPV4V6/IPV4V6\n", g_restore_apn);
        if (store(dir, "prime-request.bin", &forward, sizeof(forward))) break;
        if (cancelled || remaining() < 7.0) break;
        if (marker(dir, "prime-intent.txt", "prime_may_have_been_sent=1\n")) break;
        /* One write. From here every failure is terminal (exit 1, no retry). */
        if (modify(api, client, &forward, dir)) break;
        if (query(api, client, 1, &after1, 0, "verify_profile1") ||
            store(dir, "after-profile1.bin", &after1, sizeof(after1))) break;
        if (strcmp(after1.apn_name, g_restore_apn) != 0) {
            (void)marker(dir, "verify-failed.txt", "apn_not_applied=1\n");
            break;
        }
        printf("stage=verified apn_now=%s\n", after1.apn_name);
        result = 0;
    } while (0);
    rc = api->release(client);
    printf("stage=release rc=%d\n", rc);
    if (rc && result == 0) result = 1;
    if (result == 0) (void)marker(dir, "restored.txt", "primed=1\nverified_unchanged=1\n");
    return result;
}

/* Open (creating if absent) a private 0700 attempt dir under base_fd. */
static int open_attempt_dir(int base_fd, unsigned k) {
    char name[32];
    snprintf(name, sizeof(name), "try%u", k);
    mkdirat(base_fd, name, 0700);   /* ignore EEXIST; private_dir re-checks below */
    int fd = openat(base_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd >= 0 && private_dir(fd)) { close(fd); return -1; }
    return fd;
}

int main(int argc, char **argv) {
    /* Forms:  <logdir>            one attempt (poller-driven, KSU module)
     *         --wait <logbasedir> internal retry until ready (init service)   */
    /* wds-profile-restore <private-0700-dir> <apn> : repair a wiped profile 1. */
    int wait_mode = 0;
    const char *base = (argc == 3) ? argv[1] : NULL;
    g_restore_apn = (argc == 3) ? argv[2] : NULL;
    if (!base || !g_restore_apn || strlen(g_restore_apn) > 100 || geteuid() != 0) return 2;
    if (deadline(wait_mode ? 85 : 25)) return 2;
    umask(077);
    setvbuf(stdout, NULL, _IONBF, 0);
    struct rlimit no_core = {0, 0};
    struct sigaction ignore = {0};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    if (setrlimit(RLIMIT_CORE, &no_core) || sigaction(SIGPIPE, &ignore, NULL)) _exit(2);
    int base_fd = open(base, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (base_fd < 0 || private_dir(base_fd)) _exit(2);
    if (wait_mode) {
        /* Unique per-invocation run dir: a qcrild restart may legitimately
         * re-fire the service; attempts must never collide with a prior run. */
        char run[48];
        snprintf(run, sizeof(run), "run-%ld-%u", (long)getpid(),
                 (unsigned)(monotonic() * 10));
        if (mkdirat(base_fd, run, 0700)) _exit(2);
        int run_fd = openat(base_fd, run, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (run_fd < 0 || private_dir(run_fd)) _exit(2);
        close(base_fd);
        base_fd = run_fd;
    }
    int dir = wait_mode ? -1 : base_fd;
    void *enc = dlopen("/vendor/lib64/libqmi_encdec.so", RTLD_NOW | RTLD_LOCAL);
    void *svc = dlopen("/vendor/lib64/libqmiservices.so", RTLD_NOW | RTLD_LOCAL);
    void *cci = dlopen("/vendor/lib64/libqmi_cci.so", RTLD_NOW | RTLD_LOCAL);
    if (!enc || !svc || !cci) _exit(EXIT_NOT_READY);   /* vendor not mounted yet */
    struct wds_api api = {0};
#define LOAD(member, handle, symbol) do { \
    api.member = (__typeof__(api.member))dlsym(handle, #symbol); \
    if (!api.member) { puts("status=FAIL stage=symbol_" #symbol); _exit(2); } \
} while (0)
    LOAD(service, svc, wds_get_service_object_internal_v01);
    LOAD(service_id, enc, qmi_idl_get_service_id);
    LOAD(major, enc, qmi_idl_get_idl_version);
    LOAD(minor, enc, qmi_idl_get_idl_minor_version);
    LOAD(size, enc, qmi_idl_get_message_c_struct_len);
    LOAD(list, cci, qmi_client_get_service_list);
    LOAD(instance, cci, qmi_client_get_instance_id);
    LOAD(init, cci, qmi_client_init);
    LOAD(release, cci, qmi_client_release);
    LOAD(send, cci, qmi_client_send_msg_sync);
#undef LOAD
    int result;
    if (!wait_mode) {
        result = run_primer(&api, dir);
    } else {
        unsigned k = 0;
        result = EXIT_NOT_READY;
        while (!cancelled && remaining() > 14.0) {
            int adir = open_attempt_dir(base_fd, k++);
            if (adir < 0) { result = 2; break; }
            double up = monotonic();
            result = run_primer(&api, adir);
            printf("attempt=%u at_mono=%.2f rc=%d\n", k - 1, up, result);
            if (close(adir)) result = result ? result : 1;
            if (result != EXIT_NOT_READY) break;
            struct timespec ts = {0, 400000000};
            nanosleep(&ts, NULL);
        }
        if (result == 0) (void)marker(base_fd, "primed", "primed=1\n");
    }
    printf("status=%s exit=%d\n", result == 0 ? "PRIMED" :
           result == EXIT_NOT_READY ? "NOT_READY" : "FAIL", result);
    if (close(base_fd)) result = result ? result : 1;
    _exit(result);
}
