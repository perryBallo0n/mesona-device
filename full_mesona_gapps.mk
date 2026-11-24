$(call inherit-product, device/sony/mesona/full_mesona.mk)

PRODUCT_NAME := full_mesona_gapps

# Gingerbread 2.3 Google Apps + OldMarket 2.3 (kept at original GB versions, no MarketUpdater)
PRODUCT_PACKAGES += \
    CarHomeLauncher \
    GenieWidget \
    GoogleBackupTransport \
    GoogleCalendarSyncAdapter \
    GoogleContactsSyncAdapter \
    GoogleFeedback \
    GooglePartnerSetup \
    GoogleQuickSearchBox \
    GoogleServicesFramework \
    LatinImeTutorial \
    MediaUploader \
    NetworkLocation \
    OldMarket \
    OneTimeInitializer \
    SetupWizard \
    Talk \
    Vending

PRODUCT_COPY_FILES += \
    device/sony/mesona/gapps_gb/framework/com.google.android.maps.jar:system/framework/com.google.android.maps.jar \
    device/sony/mesona/gapps_gb/etc/permissions/com.google.android.maps.xml:system/etc/permissions/com.google.android.maps.xml \
    device/sony/mesona/gapps_gb/etc/permissions/features.xml:system/etc/permissions/features.xml \
    device/sony/mesona/gapps_gb/etc/permissions/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
    device/sony/mesona/gapps_gb/lib/libmicrobes_jni.so:system/lib/libmicrobes_jni.so \
    device/sony/mesona/gapps_gb/lib/libvoicesearch.so:system/lib/libvoicesearch.so

PRODUCT_PROPERTY_OVERRIDES += \
    ro.setupwizard.mode=OPTIONAL
