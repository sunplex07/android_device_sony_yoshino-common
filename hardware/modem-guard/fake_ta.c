/* SPDX-License-Identifier: Apache-2.0 -- HOST TEST ONLY; not an Android module. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
static char current[256] = "amss_fsg_lilac_tmobile_us_ims_tar.mbn";
static char pending[256];
static uint32_t current_n = 37, pending_n = 1;
static const char *mode(void) { const char *m = getenv("FAKE_TA_MODE"); return m ? m : "good"; }
static const char *value(uint32_t unit, uint32_t *n) {
    if (unit == 2212) { *n = 3; return "408"; }
    if (unit == 2404) { *n = current_n; return current; }
    if (unit == 2405) { *n = pending_n; return pending; }
    *n = 0; return "";
}
int miscta_get_unit_size(uint32_t unit, uint32_t *n) {
    if (!strcmp(mode(), "hang")) for (;;) pause();
    if (!strcmp(mode(), "unreachable")) return 1;
    if (unit == 42) { *n = 0; return !strcmp(mode(), "42present") ? 0 : 5; }
    if (unit == 2404 && !strcmp(mode(), "missing")) return 5;
    value(unit, n); return 0;
}
int miscta_read_unit(uint32_t unit, void *buf, uint32_t *n) {
    if (!strcmp(mode(), "readfail")) return 1;
    uint32_t actual; const char *p = value(unit, &actual);
    if (*n < actual) return 1;
    memset(buf, 0, *n); memcpy(buf, p, actual); *n = actual;
    return 0;
}
int miscta_write_unit_critical(uint32_t unit, const void *buf, uint32_t n) {
    /* In-memory fake only. Audit just unit IDs, never device paths. */
    if (n > 256 || (unit != 2404 && unit != 2405)) return 1;
    const char *audit = getenv("FAKE_TA_AUDIT");
    if (audit) { FILE *f = fopen(audit, "a"); if (!f) return 1; fprintf(f, "%u\n", unit); fclose(f); }
    if (unit == 2404 && !strcmp(mode(), "writefail2404")) return 1;
    if (unit == 2405) { memcpy(pending, buf, n); pending_n = n; }
    else { memcpy(current, buf, n); current_n = n; }
    return 0;
}
