/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ATTACH_REPAIR_DECISION_H
#define ATTACH_REPAIR_DECISION_H
#include <string.h>
/* ATTACH APN, not the user's internet/data APN (e.g. tello). Deliberately
 * narrow: add another carrier only with captured known-good attach evidence. */
static inline const char *carrier_attach_default(const char *image) {
    /* lilac: captured T-Mobile-accepted attach. poplar dsds_tmobile_us*: Sony's own
     * Data_Profiles/Profile1 inside these images is fast.t-mobile.com (extracted from the
     * stock G8342 fsg; modem-lab/donors/x), and that APN reached RACH/auth on poplar (Exp E). */
    return !strcmp(image, "amss_fsg_lilac_tmobile_us_ims_tar.mbn") ||
           !strcmp(image, "amss_fsg_lilac_tmobile_us_tar.mbn") ||
           !strcmp(image, "amss_fsg_poplar_dsds_tmobile_us_ims_tar.mbn") ||
           !strcmp(image, "amss_fsg_poplar_dsds_tmobile_us_tar.mbn")
           ? "fast.t-mobile.com" : NULL;
}
static inline const char *repair_apn(const char *image, const char *saved_image,
                                     const char *saved_apn, int have_snapshot) {
    if (!image || !image[0]) return NULL; /* no verified carrier this boot */
    const char *fallback = carrier_attach_default(image);
    if (have_snapshot && saved_image && !strcmp(image, saved_image) &&
        saved_apn && saved_apn[0] && (!fallback || !strcmp(saved_apn, fallback)))
        return saved_apn;
    return fallback; /* wrong-carrier/legacy snapshot never silently reused */
}
static inline int auxiliary_read_required(int repairing) { return !repairing; }
/* A 3GPP profile that does not exist is "absent", not "unreadable". poplar's generic/EU
 * images define only profile 1 (WDS profile list = [1]); a get-settings for profile 5 is
 * answered result=FAILURE(1), error=QMI_ERR_EXTENDED_INTERNAL(0x51) with extended error
 * WDS_EEC_DS_PROFILE_REG_RESULT_ERR_INVAL_PROFILE_NUM(5). Any other failure (transport,
 * other error/extended code, missing extended TLV) is still treated as unreadable. */
static inline int profile_absent(int result, int error, int ext_valid, int ext) {
    return result == 1 && error == 0x51 && ext_valid == 1 && ext == 5;
}
/* Profile 1 holds an empty attach APN and this carrier image has no evidence-backed
 * attach default: nothing may be written (no guessing), and that is a skip, not a
 * failure. Sony ships empty-APN profile 1 in its own images (e.g. generic, ir51_ir92_ims). */
static inline int skip_unprimable(int apn_wiped, const char *chosen) {
    return apn_wiped && !chosen;
}
#endif
