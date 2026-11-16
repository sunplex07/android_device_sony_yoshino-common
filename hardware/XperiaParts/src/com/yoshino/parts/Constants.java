/*
 * Copyright (c) 2020, Shashank Verma (shank03) <shashank.verma2002@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

package com.yoshino.parts;

public class Constants {

    public static final String MODEM_SWITCHER = "modem_sw";

    public static final String GLOVE_MODE = "glove_mode";
    public static final String GLOVE_PROP = "persist.sys.touch.glove_mode";

    public static final String SMART_STAMINA_MODE = "smart_stamina_mode";
    public static final String SMART_STAMINA_PROP = "vendor.smartstamina.touchreport";

    public static final String CS_LOG = "cs_log";
    public static final String CS_STATUS = "cs_status";
    public static final String CS_NOTIFICATION = "cs_notification";
    public static final String CS_IMS = "cs_ims";
    public static final String CS_RE_APPLY_MODEM = "cs_re_apply_modem";

    public static final String NS_SLOT = "ns_slot";
    public static final String NS_SERVICE = "ns_service";

    /*
     * NFC controller selection. lilac ships as one ROM for two different NFC
     * chips — the G8441's NXP PN553 and the docomo SO-02K's Sony CXD224x +
     * ROHM BD7602 (FeliCa) — and init.lilac-nfc.sh picks one at boot.
     *
     * NFC_VARIANT_PROP is the user's override (auto|nxp|felica), read by that
     * script. NFC_HW_PROP is what it actually selected this boot. NFC_CFG_PROP
     * is AOSP's own hook for naming the NCI config file
     * (packages/modules/Nfc/.../nfc_config.cc), which has to match the chip.
     */
    public static final String NFC_VARIANT = "nfc_variant";
    public static final String NFC_VARIANT_PROP = "persist.sys.nfc_variant";
    public static final String NFC_HW_PROP = "vendor.nfc.variant";
    public static final String NFC_CFG_PROP = "persist.nfc_cfg.config_file_name";
    public static final String NFC_CFG_FELICA = "libnfc-nci-cxd224x.conf";

    /*
     * CPU/GPU tuning profile (battery|balanced|performance).
     *
     * This app never touches cpufreq or kgsl nodes. It only records the choice
     * in PERF_PROFILE_PROP; init.lilac-perf.rc maps that to one of the
     * powerhint files shipped in /vendor/etc and restarts the power HAL, which
     * stays the single writer of those nodes. Being a persist. property, the
     * choice is re-applied by init on every boot without help from this app.
     */
    public static final String PERF_PROFILE = "perf_profile";
    public static final String PERF_PROFILE_PROP = "persist.sys.perf_profile";
    public static final String PERF_PROFILE_DEFAULT = "balanced";
    public static final String PERF_PROFILE_PERFORMANCE = "performance";
}
