LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := auto_modify_cscfeature
LOCAL_SRC_FILES := auto_modify_cscfeature.c

include $(BUILD_EXECUTABLE)