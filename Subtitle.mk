PRODUCT_SUPPORT_SUBTITLE?=true
ifeq ($(PRODUCT_SUPPORT_SUBTITLE), true)
PRODUCT_PACKAGES += \
    subtitleserver \
    libSubtitleClient \
    libsubtitlebinder \
    vendor.amlogic.hardware.subtitleserver@1.0 \
    libsubtitlemanager_jni \
    libsubtitlemanagerproduct_jni \
    libsubtitle_depend
endif