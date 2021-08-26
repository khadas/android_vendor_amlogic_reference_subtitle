LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= ApiTest.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../

LOCAL_SHARED_LIBRARIES :=       \
  liblog    \
  libutils  \
  libcutils \
  libSubtitleClient

LOCAL_C_FLAGS := -Wno-unused-variable   \
                 -Wno-unused-parameter

LOCAL_MODULE := subtitle_api_test
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0 SPDX-license-identifier-BSD SPDX-license-identifier-LGPL legacy_by_exception_only
LOCAL_LICENSE_CONDITIONS := by_exception_only notice restricted
LOCAL_MODULE_TAGS := optional
LOCAL_SYSTEM_EXT_MODULE := true

LOCAL_SANITIZE := address

include $(BUILD_EXECUTABLE)
