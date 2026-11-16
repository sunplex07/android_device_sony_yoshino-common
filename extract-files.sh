#!/bin/bash

set -e

DEVICE_COMMON=yoshino-common
VENDOR=sony

MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."

HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

CLEAN_VENDOR=true

KANG=
SECTION=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n | --no-cleanup )
                CLEAN_VENDOR=false
                ;;
        -k | --kang )
                KANG="--kang"
                ;;
        -s | --section )
                SECTION="${2}"; shift
                CLEAN_VENDOR=false
                ;;
        * )
                SRC="${1}"
                ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    SRC="adb"
fi

setup_vendor "${DEVICE_COMMON}" "${VENDOR}" "${ANDROID_ROOT}" true "${CLEAN_VENDOR}"

extract "${MY_DIR}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"
extract "${MY_DIR}/proprietary-files-vendor.txt" "${SRC}" "${KANG}" --section "${SECTION}"

DEVICE_COMMON_ROOT="${ANDROID_ROOT}"/vendor/"${VENDOR}"/"${DEVICE_COMMON}"/proprietary

sed -i 's/\/lib\/firmware\/ufs/\/etc\/firmware\/ufs/g' "${DEVICE_COMMON_ROOT}"/vendor/bin/ffu

sed -i 's/version\=\"2\.0\"/version\=\"1\.0\"/g' "${DEVICE_COMMON_ROOT}"/product/etc/permissions/vendor.qti.hardware.data.connection-V1.0-java.xml
sed -i 's/version\=\"2\.0\"/version\=\"1\.0\"/g' "${DEVICE_COMMON_ROOT}"/product/etc/permissions/vendor.qti.hardware.data.connection-V1.1-java.xml

sed -i '4 a\    restorecon /persist/wlan' "${DEVICE_COMMON_ROOT}"/vendor/etc/init/taimport_vendor.rc

grep -q "libgui_shim.so" "${DEVICE_COMMON_ROOT}"/system_ext/lib64/lib-imsvideocodec.so || "${PATCHELF}" --add-needed "libgui_shim.so" "${DEVICE_COMMON_ROOT}"/system_ext/lib64/lib-imsvideocodec.so

"${PATCHELF}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/lib/libjni_imageutil.so
"${PATCHELF}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/lib/libjni_snapcammosaic.so
"${PATCHELF}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/lib/libjni_snapcamtinyplanet.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/lib/libseemore.so
"${PATCHELF}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/lib64/libseemore.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_alfortlp.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_alfortlpserv.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_alfortrsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_bordeauxrsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_buttercakersc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_canelersc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_cheesesconersc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_dars.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_darsrsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_marblersc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_melonpanrsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_mugichocorsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_pretzchocorsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_raisinrsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_shortcakersc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_spicarsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_sumomolpserv.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_sumomorsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_topporsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsomc_yummyrsc.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsony_fooddetect.so
"${PATCHELF_0_17_2}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${DEVICE_COMMON_ROOT}"/vendor/lib/libsony_naruto.so

"${PATCHELF_0_17_2}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/bin/sony-modem-switcher
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/com.qualcomm.qti.ant@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/com.qualcomm.qti.bluetooth_audio@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/libMiscTaWrapper.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.qti.hardware.qteeconnector@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.qti.hardware.tui_comm@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.qti.hardware.vpp@1.1.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.semc.hardware.light@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.semc.system.idd@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.somc.hardware.camera.cacao@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.somc.hardware.camera.cacao@2.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.somc.hardware.camera.cacao@3.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.somc.hardware.camera.cacao@3.1.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.somc.hardware.camera.device@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib/vendor.somc.hardware.camera.provider@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/com.qualcomm.qti.ant@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/com.qualcomm.qti.bluetooth_audio@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/libMiscTaWrapper.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.display.color@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.display.color@1.1.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.display.color@1.2.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.display.postproc@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.qti.esepowermanager@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.qti.hardware.qdutils_disp@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.qti.hardware.qteeconnector@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.qti.hardware.tui_comm@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.qti.hardware.vpp@1.1.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.semc.hardware.light@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.semc.system.idd@1.0.so
"${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${DEVICE_COMMON_ROOT}"/lib64/vendor.somc.hardware.security.secd@1.0.so

sed -i 's/writepid\ \/dev\/cpuset\/foreground\/tasks/task_profiles\ ProcessCapacityHigh/g' "${DEVICE_COMMON_ROOT}"/vendor/etc/init/android.hardware.drm@1.1-service.widevine.rc
sed -i 's/writepid\ \/dev\/cpuset\/system-background\/tasks/task_profiles\ ServiceCapacityLow/g' "${DEVICE_COMMON_ROOT}"/vendor/etc/init/init.illumination_service.rc
sed -i 's/writepid\ \/dev\/cpuset\/system-background\/tasks/task_profiles\ ServiceCapacityLow/g' "${DEVICE_COMMON_ROOT}"/vendor/etc/init/init.touchbacklightd.rc
sed -i 's/writepid \/dev\/cpuset\/camera-daemon\/tasks\ \/dev\/stune\/top-app\/tasks/task_profiles\ CameraServiceCapacity\ MaxPerformance/g' "${DEVICE_COMMON_ROOT}"/vendor/etc/init/vendor.somc.hardware.camera.provider@1.0-service.rc

CAM_RC="${DEVICE_COMMON_ROOT}"/vendor/etc/init/vendor.somc.hardware.camera.provider@1.0-service.rc
if ! grep -q "cama15" "${CAM_RC}"; then
  sed -i '/capabilities SYS_NICE/a\    setenv LD_PRELOAD /vendor/lib/libsensorfix.so:/vendor/lib/cama15/libutils.so:/vendor/lib/cama15/libui.so:/vendor/lib/cama15/libc++.so\n    setenv LD_LIBRARY_PATH /vendor/lib/cama15:/vendor/lib:/system/lib' "${CAM_RC}"
fi

# Same AT_SECURE problem as the camera provider: cacaoserver's LD_PRELOAD is
# discarded under SELinux enforcing, so the shims must be real DT_NEEDED entries.
# libcacaoreffix INTERPOSES symbols that libutils already defines (RefBase,
# MemoryHeapBase, MemoryBase), so it has to be searched BEFORE libutils --
# patchelf prepends, hence the reverse order here. Appending would link cleanly
# and silently do nothing. See docs/27 in lilac-eis.
CACAOSERVER="${DEVICE_COMMON_ROOT}"/bin/cacaoserver
if ! "${PATCHELF}" --print-needed "${CACAOSERVER}" | grep -q libcacaoreffix; then
    "${PATCHELF}" --add-needed libsonypiecompat.so "${CACAOSERVER}"
    "${PATCHELF}" --add-needed libcacaoreffix.so "${CACAOSERVER}"
fi

"${MY_DIR}"/setup-makefiles.sh
