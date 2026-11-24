$(call inherit-product, device/sony/mesona/full_mesona.mk)

PRODUCT_NAME := full_mesona_gapps

# Gingerbread 2.3 Google Apps + Patched Market 2.3.6 (Vending.apk), no MarketUpdater, no Xperia apps
PRODUCT_PACKAGES += \
    BooksPhone \
    CarHomeGoogle \
    GenieWidget \
    Gmail \
    GoogleBackupTransport \
    GoogleCalendarSyncAdapter \
    GoogleContactsSyncAdapter \
    GoogleFeedback \
    GoogleMusic \
    GooglePartnerSetup \
    GoogleQuickSearchBox \
    GoogleServicesFramework \
    LatinImeTutorial \
    Maps \
    MediaUploader \
    NetworkLocation \
    OneTimeInitializer \
    SetupWizard \
    Street \
    Talk \
    Vending \
    VoiceSearch \
    YouTube

PRODUCT_COPY_FILES += \
    device/sony/mesona/gapps_gb/framework/com.google.android.maps.jar:system/framework/com.google.android.maps.jar \
    device/sony/mesona/gapps_gb/etc/gapps_firstboot.sh:system/etc/gapps_firstboot.sh \
    device/sony/mesona/gapps_gb/etc/permissions/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
    device/sony/mesona/gapps_gb/etc/permissions/com.google.android.maps.xml:system/etc/permissions/com.google.android.maps.xml \
    device/sony/mesona/gapps_gb/etc/permissions/features.xml:system/etc/permissions/features.xml \
    device/sony/mesona/gapps_gb/lib/libtalk_jni.so:system/lib/libtalk_jni.so \
    device/sony/mesona/gapps_gb/lib/libvoicesearch.so:system/lib/libvoicesearch.so

PRODUCT_PROPERTY_OVERRIDES += \
    ro.setupwizard.mode=OPTIONAL
