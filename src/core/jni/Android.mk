LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE    := csc_tool
LOCAL_SRC_FILES := csc_tool.c lib/cJSON.c
LOCAL_LDLIBS    := -lz
include $(BUILD_EXECUTABLE)
