LOCAL_PATH:= $(call my-dir)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= ApiTestJni.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../..  \
                    $(JNI_H_INCLUDE)

#LOCAL_C_INCLUDES :=     \
#      vendor/amlogic/common/frameworks/services/subtitleserver/client \
#      $(JNI_H_INCLUDE)

LOCAL_SHARED_LIBRARIES :=       \
      liblog    \
      libSubtitleClient

LOCAL_C_FLAGS += -Wno-unused-variable
LOCAL_HEADER_LIBRARIES := jni_headers
LOCAL_MODULE := libsubtitleApiTestJni
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0 SPDX-license-identifier-BSD SPDX-license-identifier-LGPL legacy_by_exception_only
LOCAL_LICENSE_CONDITIONS := by_exception_only notice restricted
LOCAL_MODULE_TAGS := optional
LOCAL_SYSTEM_EXT_MODULE := true

LOCAL_CPPFLAGS += -std=c++14 -Wno-unused-parameter -Wno-unused-const-variable -O0
include $(BUILD_SHARED_LIBRARY)
