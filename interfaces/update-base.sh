#!/bin/bash

# WARNING: vendor-base is placed here only for testing.
# It should typically be created elsewhere.

options="-Lexport-header \
         -r vendor.amlogic.hardware:vendor/amlogic/reference/subtitle/interfaces \
         -r android.hidl:system/libhidl/transport\
         -r android.hardware:hardware/interfaces"

hidl-gen $options \
         -o system/tools/hidl/test/vendor/1.0/vendor-base.h
