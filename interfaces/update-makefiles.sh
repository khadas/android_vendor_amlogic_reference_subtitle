#!/bin/bash

source system/tools/hidl/update-makefiles-helper.sh

do_makefiles_update \
  "vendor.amlogic.hardware:vendor/amlogic/reference/subtitle/interfaces" \
  "android.hidl:system/libhidl/transport" \
  "android.hardware:hardware/interfaces"
