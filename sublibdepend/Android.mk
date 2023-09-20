#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libsubtitle_depend
LOCAL_SRC_FILES := subtitle_depend.cpp
LOCAL_VENDOR_MODULE := true

LOCAL_SHARED_LIBRARIES += libsubtitlemanager_jni

LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0 SPDX-license-identifier-BSD
LOCAL_LICENSE_CONDITIONS := notice

include $(BUILD_SHARED_LIBRARY)

