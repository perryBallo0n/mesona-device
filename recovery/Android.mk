LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := recovery_ui.c
LOCAL_MODULE := librecovery_ui_mesona
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += bootable/recovery

include $(BUILD_STATIC_LIBRARY)
