LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_VENDOR_MODULE := true
LOCAL_C_INCLUDES += hardware/libhardware/include
LOCAL_CFLAGS := -Wconversion -Wall -Werror -Wno-sign-conversion -Wno-error
LOCAL_CLANG  := true
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_HEADER_LIBRARIES := libhardware_headers
LOCAL_SRC_FILES := memtrack_msm.c kgsl.c
LOCAL_MODULE := memtrack.$(TARGET_BOARD_PLATFORM)
include $(BUILD_SHARED_LIBRARY)
