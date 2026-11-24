# Clean CyanogenMod 7 for Sony Xperia Miro (mesona) - no GApps
CM_BUILD := mesona
$(call inherit-product, device/sony/mesona/full_mesona.mk)
$(call inherit-product, vendor/cyanogen/products/common_full.mk)
$(call inherit-product, vendor/cyanogen/products/gsm.mk)

PRODUCT_NAME := cm_mesona
PRODUCT_DEVICE := mesona
PRODUCT_BRAND := Sony
PRODUCT_MODEL := Xperia Miro
PRODUCT_MANUFACTURER := Sony

PRODUCT_PACKAGES += Torch

PRODUCT_COPY_FILES += \
    vendor/cyanogen/prebuilt/mdpi/media/bootanimation.zip:system/media/bootanimation.zip

PRODUCT_RELEASE_NAME := XperiaMiro-ST23i
-include vendor/cyanogen/products/common_versions.mk
