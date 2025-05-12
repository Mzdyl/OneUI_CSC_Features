LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := auto_modify_jsoncsc
LOCAL_SRC_FILES := auto_modify_jsoncsc.c

include $(BUILD_EXECUTABLE)