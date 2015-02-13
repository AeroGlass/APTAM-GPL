LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := 
LOCAL_CPP_EXTENSION := .cpp

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_CFLAGS += -DHAVE_NEON=1
    LOCAL_CFLAGS += -DLOCAL_ARM_NEON=1
    #LOCAL_CFLAGS += -D__ARM_NEON__=1
    LOCAL_ARM_NEON  := true
endif

LOCAL_MODULE    := tinyxml2
LOCAL_SRC_FILES := ../tinyxml2-master/tinyxml2.cpp
	
LOCAL_EXPORT_LDLIBS := $(LOCAL_LDLIBS) #export linker cmds

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../tinyxml2-master/
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES) #export includes

include $(BUILD_STATIC_LIBRARY)
