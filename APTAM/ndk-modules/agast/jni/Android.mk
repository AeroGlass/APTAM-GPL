LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := 

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_CFLAGS += -DHAVE_NEON=1
    LOCAL_CFLAGS += -DLOCAL_ARM_NEON=1
    LOCAL_ARM_NEON  := true
endif

LOCAL_CPP_EXTENSION := .cc
LOCAL_MODULE    := agast

LOCAL_SRC_FILES :=\
../src/agast5_8.cc \
../src/agast5_8_nms.cc \
../src/agast7_12d.cc \
../src/agast7_12d_nms.cc \
../src/agast7_12s.cc \
../src/agast7_12s_nms.cc \
../src/AstDetector.cc \
../src/nonMaximumSuppression.cc \
../src/oast9_16.cc \
../src/oast9_16_nms.cc
	
		
#LOCAL_LDLIBS := -lz
#LOCAL_EXPORT_LDLIBS := $(LOCAL_LDLIBS) #export linker cmds

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include/agast
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES) #export includes

LOCAL_STATIC_LIBRARIES += cpufeatures

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/cpufeatures)
