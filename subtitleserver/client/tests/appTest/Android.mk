LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, java)


LOCAL_STATIC_ANDROID_LIBRARIES := androidx.appcompat_appcompat
LOCAL_SYSTEM_EXT_MODULE := true

LOCAL_PACKAGE_NAME := DemoTest
LOCAL_CERTIFICATE := platform
#LOCAL_SDK_VERSION := current
LOCAL_REQUIRED_MODULES := libsubtitleApiTestJni

LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0 SPDX-license-identifier-BSD SPDX-license-identifier-LGPL legacy_by_exception_only
LOCAL_LICENSE_CONDITIONS := by_exception_only notice restricted

LOCAL_STATIC_JAVA_LIBRARIES := droidlogicLib
LOCAL_PRIVATE_PLATFORM_APIS := true
LOCAL_DEX_PREOPT := false
#LOCAL_PRIVILEGED_MODULE := true
include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
