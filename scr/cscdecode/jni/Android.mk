# 定义本地路径
LOCAL_PATH := $(call my-dir)

# 初始化变量
include $(CLEAR_VARS)

# 模块名称
LOCAL_MODULE := decode_csc

# 源代码文件
LOCAL_SRC_FILES := decode_csc.c

LOCAL_LDLIBS := -lz  # 链接 zlib 库

# 包含头文件目录（可选）
# LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

# 编译类型：构建可执行文件
include $(BUILD_EXECUTABLE)