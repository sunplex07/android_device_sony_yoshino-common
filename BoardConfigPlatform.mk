PLATFORM_PATH := device/sony/yoshino-common

include vendor/sony/yoshino-common/BoardConfigVendor.mk

BOARD_USES_QCOM_HARDWARE := true
BOARD_VENDOR := sony
TARGET_BOARD_PLATFORM := msm8998
TARGET_BOARD_PLATFORM_GPU := qcom-adreno540

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := cortex-a73

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := cortex-a73

TARGET_KERNEL_SOURCE  := kernel/sony/msm8998
TARGET_KERNEL_VERSION := 4.4
TARGET_KERNEL_CLANG_VERSION := r536225

BOARD_KERNEL_CMDLINE += ehci-hcd.park=3
BOARD_KERNEL_CMDLINE += lpm_levels.sleep_disabled=1
BOARD_KERNEL_CMDLINE += sched_enable_hmp=0
BOARD_KERNEL_CMDLINE += sched_enable_power_aware=1
BOARD_KERNEL_CMDLINE += service_locator.enable=1
BOARD_KERNEL_CMDLINE += swiotlb=2048
BOARD_KERNEL_CMDLINE += androidboot.configfs=true
BOARD_KERNEL_CMDLINE += androidboot.usbcontroller=a800000.dwc3
BOARD_KERNEL_CMDLINE += loop.max_part=7
BOARD_KERNEL_CMDLINE += androidboot.init_fatal_reboot_target=recovery

BOARD_KERNEL_BASE        := 0x00000000
BOARD_KERNEL_PAGESIZE    := 4096

BOARD_KERNEL_IMAGE_NAME := Image.gz-dtb

BOARD_BOOTIMAGE_PARTITION_SIZE := 67108864
BOARD_CACHEIMAGE_PARTITION_SIZE := 398458880
BOARD_FLASH_BLOCK_SIZE := 131072 # (BOARD_KERNEL_PAGESIZE * 64)
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 67108864

BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
TARGET_USERIMAGES_USE_EXT4 := true

ifneq ($(filter maple maple_dsds, $(TARGET_DEVICE)),)
BOARD_VENDORIMAGE_EXTFS_INODE_COUNT   := 4096
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE    := squashfs
BOARD_VENDORIMAGE_JOURNAL_SIZE        := 0
BOARD_VENDORIMAGE_SQUASHFS_COMPRESSOR := lz4
else
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
endif
TARGET_COPY_OUT_VENDOR := vendor

BUILD_BROKEN_USES_BUILD_COPY_HEADERS := true

BOARD_QTI_CAMERA_32BIT_ONLY := true
MALLOC_SVELTE_FOR_LIBC32 := true

USE_DEVICE_SPECIFIC_DISPLAY := true
DEVICE_SPECIFIC_DISPLAY_PATH := $(PLATFORM_PATH)/hardware/qcom-caf/display
TARGET_USES_HWC2 := true
TARGET_USES_GRALLOC1 := true
TARGET_ADDITIONAL_GRALLOC_10_USAGE_BITS := 0x20200000U

TARGET_FS_CONFIG_GEN := \
    $(PLATFORM_PATH)/fs/config.aid \
    $(PLATFORM_PATH)/fs/config.fs

TARGET_USES_ION := true
TARGET_USES_NEW_ION_API := true

BOARD_VNDK_VERSION := current
DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE := \
    $(PLATFORM_PATH)/device_framework_matrix.xml \
    hardware/qcom-caf/common/vendor_framework_compatibility_matrix.xml \
    hardware/qcom-caf/common/vendor_framework_compatibility_matrix_legacy.xml
ifneq ($(filter poplar_dsds maple_dsds, $(TARGET_DEVICE)),)
DEVICE_MANIFEST_FILE := $(PLATFORM_PATH)/manifest_dsds.xml
else
DEVICE_MANIFEST_FILE := $(PLATFORM_PATH)/manifest.xml
endif
DEVICE_MATRIX_FILE := hardware/qcom-caf/common/compatibility_matrix.xml

TARGET_ODM_PROP += $(PLATFORM_PATH)/odm.prop
TARGET_SYSTEM_PROP := $(PLATFORM_PATH)/system.prop
TARGET_VENDOR_PROP := $(PLATFORM_PATH)/vendor.prop

ifneq ($(filter maple maple_dsds, $(TARGET_DEVICE)),)
TARGET_RECOVERY_FSTAB := $(PLATFORM_PATH)/recovery/fstab_maple.recovery
else
TARGET_RECOVERY_FSTAB := $(PLATFORM_PATH)/recovery/fstab.recovery
endif

ENABLE_VENDOR_RIL_SERVICE := true
TARGET_PER_MGR_ENABLED := true

include device/qcom/sepolicy-legacy-um/SEPolicy.mk
BOARD_VENDOR_SEPOLICY_DIRS += device/sony/yoshino-common/sepolicy/vendor
PRODUCT_PRIVATE_SEPOLICY_DIRS += device/sony/yoshino-common/sepolicy/private

# SELinux denial-collection build: WITH_SELINUX_COLLECT=true
# Compiles OUT every dontaudit rule (dontaudit.te + the 5 inline ones) so the
# denials they mask surface in the corpus. Opt-in only -- normal builds keep them.
ifeq ($(WITH_SELINUX_COLLECT),true)
BOARD_SEPOLICY_M4DEFS += lilac_collect_denials
endif

BUILD_BROKEN_TREBLE_SYSPROP_NEVERALLOW := true

BUILD_BROKEN_DUP_RULES := true
BUILD_BROKEN_ELF_PREBUILT_PRODUCT_COPY_FILES := true
BUILD_BROKEN_INCORRECT_PARTITION_IMAGES := true

VENDOR_SECURITY_PATCH := 2019-09-01

BOARD_HOSTAPD_DRIVER := NL80211
BOARD_HOSTAPD_PRIVATE_LIB := lib_driver_cmd_qcwcn
BOARD_WLAN_DEVICE := qcwcn
BOARD_WPA_SUPPLICANT_DRIVER := NL80211
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_qcwcn
HOSTAPD_VERSION := VER_0_8_X
WIFI_HIDL_FEATURE_DUAL_INTERFACE := true
WIFI_HIDL_UNIFIED_SUPPLICANT_SERVICE_RC_ENTRY := true
WPA_SUPPLICANT_VERSION := VER_0_8_X

AB_OTA_UPDATER := false

ifeq ($(KSU_ENABLED), true)
    BOARD_KERNEL_CMDLINE += androidboot.ksu=1
   
    PRODUCT_NAME_SUFFIX := -KSU
endif

# SELinux is ENFORCING (the LineageOS charter requires it for official builds).
#
# There is deliberately no androidboot.selinux= line here: system/core/init/
# selinux.cpp defaults to ENFORCING unless the cmdline is literally "permissive",
# so removing the line IS the flip. Do not write "=enforcing" -- that value is
# not special-cased and only misleads the next reader.
#
# To go back to permissive for bring-up, re-add:
#   BOARD_KERNEL_CMDLINE += androidboot.selinux=permissive
# On this userdebug build `adb shell setenforce 0` also works at runtime.
#
# Policy was curated from an 8-scenario denial matrix; see
# ~/Projects/lilac-eis/selinux-corpus/FINDINGS.md and docs/19_SELINUX_ENFORCING.md.
BOARD_KERNEL_CMDLINE += loglevel=8
BOARD_KERNEL_CMDLINE += printk.devkmsg=on
BOARD_KERNEL_CMDLINE += panic_on_oops=1
BOARD_KERNEL_CMDLINE += panic=5
BOARD_KERNEL_CMDLINE += msm_poweroff.panic_to_recovery=1
