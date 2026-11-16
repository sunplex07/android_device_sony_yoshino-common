PRODUCT_PACKAGES += \
    audio.r_submix.default \
    audio.usb.default

PRODUCT_PACKAGES += \
    audio.bluetooth.default \
    libldacBT_bco.vendor

PRODUCT_PACKAGES += \
    libcacao_client \
    libcacaoreffix \
    libsonypiecompat

PRODUCT_PACKAGES += \
    CarrierConfigOverlay

PRODUCT_PACKAGES += \
    charger_res_images

PRODUCT_PACKAGES += \
    FlipFlap

PRODUCT_PACKAGES += \
    vendor.display.config@1.3

PRODUCT_PACKAGES += \
    gralloc.msm8998 \
    hwcomposer.msm8998 \
    memtrack.msm8998 \
    libdisplayconfig

PRODUCT_PACKAGES += \
    ipacm \
    IPACM_cfg.xml \
    libipanat \
    liboffloadhal

PRODUCT_PACKAGES += \
    com.android.nfc_extras \
    Tag

PRODUCT_PACKAGES += \
    CustomizationSelector

PRODUCT_PACKAGES += \
    libqti-perfd-client

PRODUCT_PACKAGES += \
    libsensorndkbridge \
    libsensorfix \
    libpower.vendor

PRODUCT_PACKAGES += \
    android.hardware.audio@6.0-impl:32 \
    android.hardware.audio.service \
    android.hardware.audio.effect@6.0-impl:32 \
    android.hardware.soundtrigger@2.2-impl:32 \
    libaudioroute.vendor

PRODUCT_PACKAGES += \
    android.hardware.bluetooth@1.0.vendor \
    android.hardware.bluetooth.audio-impl

PRODUCT_PACKAGES += \
    android.hardware.camera.provider@2.4 \
    android.hardware.camera.provider@2.4-impl:32 \
    camera.device@3.2-impl:32 \
    vendor.qti.hardware.camera.device@1.0 \
    vendor.qti.hardware.camera.device@1.0.vendor

PRODUCT_PACKAGES += \
    android.hardware.configstore@1.1-service

# LILAC_NO_PANEL=1 (dev only): leave the LiveDisplay SDM HAL out. With the DSI
# panel disconnected its QDCM backend never becomes ready, the service aborts
# in a restart loop ("DisplayModes backend not ready"), and because its VINTF
# fragments still declare the interfaces system_server blocks forever in
# waitForDeclaredService() until the Watchdog kills it. Not declared = no wait.
ifneq ($(LILAC_NO_PANEL),1)
PRODUCT_PACKAGES += \
    vendor.lineage.livedisplay-service.sdm
endif

PRODUCT_PACKAGES += \
    android.hardware.drm@1.3.vendor \
    android.hardware.drm-service.clearkey

PRODUCT_PACKAGES += \
    android.hardware.biometrics.fingerprint@2.1 \
    android.hardware.biometrics.fingerprint@2.1.vendor \
    libhidlbase-v32.vendor

PRODUCT_PACKAGES += \
    android.hardware.gatekeeper@1.0 \
    android.hardware.gatekeeper@1.0.vendor

PRODUCT_PACKAGES += \
    android.hardware.gnss@2.1 \
    android.hardware.gnss@2.1.vendor

PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.composer@2.1-service \
    android.hardware.graphics.mapper@2.0-impl-2.1

PRODUCT_PACKAGES += \
    android.hardware.health@2.1-impl:64 \
    android.hardware.health@2.1-impl.recovery \
    android.hardware.health@2.1-service

PRODUCT_PACKAGES += \
    android.hardware.keymaster@3.0 \
    android.hardware.keymaster@3.0.vendor

PRODUCT_PACKAGES += \
    android.hardware.light@2.0-impl \
    android.hardware.light@2.0-service

PRODUCT_PACKAGES += \
    vendor.lineage.health-service.default

PRODUCT_PACKAGES += \
    android.hardware.memtrack@1.0-impl \
    android.hardware.memtrack@1.0-service

PRODUCT_PACKAGES += \
    android.system.net.netd@1.1.vendor

PRODUCT_PACKAGES += \
    android.hardware.power@1.2 \
    android.hardware.power@1.2.vendor \
    android.hardware.power-service.yoshino-libperfmgr

PRODUCT_PACKAGES += \
    android.hardware.broadcastradio@1.0-impl:64 \
    android.hardware.radio@1.5 \
    android.hardware.radio@1.5.vendor \
    android.hardware.radio.config@1.2 \
    android.hardware.radio.config@1.2.vendor \
    android.hardware.radio.deprecated@1.0 \
    android.hardware.radio.deprecated@1.0.vendor

PRODUCT_PACKAGES += \
    android.hardware.secure_element@1.2 \
    android.hardware.secure_element@1.2.vendor

PRODUCT_PACKAGES += \
    android.frameworks.sensorservice@1.0 \
    android.frameworks.sensorservice@1.0.vendor \
    android.hardware.sensors@1.0-impl:64 \
    android.hardware.sensors@1.0-service

PRODUCT_PACKAGES += \
    android.hardware.usb@1.3-service.dual_role_usb

PRODUCT_PACKAGES += \
    android.hardware.vibrator-service.legacy

PRODUCT_PACKAGES += \
    android.hardware.weaver@1.0

PRODUCT_PACKAGES += \
    android.hardware.wifi-service

PRODUCT_PACKAGES += \
    TetheringConfigOverlay

PRODUCT_PACKAGES += \
    hostapd \
    libwpa_client \
    wificond \
    WifiOverlay \
    YoshinoNetworkStackOverlay \
    wpa_supplicant \
    wpa_supplicant.conf

PRODUCT_PACKAGES += \
    XperiaParts

PRODUCT_PACKAGES += \
    wds-attach-primer \
    wds-profile-restore \
    modem-switcher-guard

# Sony stock camera PARKED 2026-09-18 (user: ship stable fixes only).
# Restore: uncomment below, or use hardware.mk.bak-camera-20260918
#PARKED-20260918 # Sony stock camera app + required addon library (BAKE-LIST 1a; re-enabled 2026-09-18)
#PARKED-20260918 PRODUCT_PACKAGES += \
#PARKED-20260918     SemcCameraUI \
#PARKED-20260918     com.sonymobile.camera.addon_impl
#PARKED-20260918 PRODUCT_COPY_FILES += \
#PARKED-20260918     device/sony/yoshino-common/config/permissions/privapp-permissions-semccamera.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/privapp-permissions-semccamera.xml \
#PARKED-20260918     device/sony/yoshino-common/config/permissions/default-permissions-addoncamera.xml:$(TARGET_COPY_OUT_SYSTEM)/etc/permissions/default-permissions-addoncamera.xml
