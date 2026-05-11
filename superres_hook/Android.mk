# Android NDK构建配置
# 用于Android Studio或ndk-build集成

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := superres
LOCAL_SRC_FILES := src/superres_core.c src/superres_hook.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_LDLIBS := -llog -lEGL -lGLESv3 -landroid
LOCAL_SHARED_LIBRARIES := libEGL libGLESv3
LOCAL_CFLAGS := -Wall -Wextra -O2
LOCAL_ARM_MODE := arm
include $(BUILD_SHARED_LIBRARY)
