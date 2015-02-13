LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc
LOCAL_C_INCLUDES += $(LOCAL_PATH)/PTAM
LOCAL_CFLAGS += -std=c++11
LOCAL_CPPFLAGS += -std=c++11
LOCAL_MODULE    := PTAM

PTAM_PATH := ./PTAM/
LOCAL_SRC_FILES += $(PTAM_PATH)/opengles2helper.cc                      \
$(PTAM_PATH)/ARDriver.cc                      \
$(PTAM_PATH)/GLWindow2.cc                       \
$(PTAM_PATH)/ATANCamera.cc                       \
$(PTAM_PATH)/Bundle.cc                       \
$(PTAM_PATH)/CalibCornerPatch.cc                      \
$(PTAM_PATH)/CalibImage.cc                      \
$(PTAM_PATH)/CameraCalibrator.cc                      \
$(PTAM_PATH)/ARTester.cc                      \
$(PTAM_PATH)/GLWindowMenu.cc                      \
$(PTAM_PATH)/HomographyInit.cc                      \
$(PTAM_PATH)/KeyFrame.cc                      \
$(PTAM_PATH)/Map.cc                      \
$(PTAM_PATH)/MapMaker.cc                      \
$(PTAM_PATH)/MapPoint.cc                      \
$(PTAM_PATH)/MapViewer.cc                      \
$(PTAM_PATH)/MiniPatch.cc                      \
$(PTAM_PATH)/PatchFinder.cc                      \
$(PTAM_PATH)/Relocaliser.cc                      \
$(PTAM_PATH)/ShiTomasi.cc                      \
$(PTAM_PATH)/SmallBlurryImage.cc                      \
$(PTAM_PATH)/System.cc                      \
$(PTAM_PATH)/Tracker.cc                      \
$(PTAM_PATH)/threadpool.cc \
$(PTAM_PATH)/ptam-main.cc \
$(PTAM_PATH)/VideoSource_Android.cc \
$(PTAM_PATH)/MapSerialization.cc
 
LOCAL_STATIC_LIBRARIES += TooN
LOCAL_STATIC_LIBRARIES += cpufeatures
LOCAL_STATIC_LIBRARIES += cvd
LOCAL_STATIC_LIBRARIES += gvars3
LOCAL_STATIC_LIBRARIES += agast
LOCAL_STATIC_LIBRARIES += glm
LOCAL_STATIC_LIBRARIES += tinyxml2
LOCAL_LDLIBS    += -landroid -lGLESv2
LOCAL_CFLAGS += -D GL_GLEXT_PROTOTYPES -g

#LOCAL_CFLAGS += -fopenmp
#LOCAL_LDFLAGS += -fopenmp

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES) #export includes
LOCAL_EXPORT_LDLIBS := $(LOCAL_LDLIBS) #export linker cmds
LOCAL_EXPORT_CFLAGS := $(LOCAL_CFLAGS) #export c flgs
LOCAL_EXPORT_CPPFLAGS := $(LOCAL_CPPFLAGS) #export cpp flgs
LOCAL_EXPORT_CXXFLAGS := $(LOCAL_CXXFLAGS) #export cpp flgs

include $(BUILD_SHARED_LIBRARY)
 
#define prebuilt lapack because this takes forever to build or even check if it should be rebuilt!!!
ifeq (true,true) 
    include $(CLEAR_VARS)
    LOCAL_MODULE := lapack
    LOCAL_SRC_FILES := $(LOCAL_PATH)/../../prebuild-libs/$(TARGET_ARCH_ABI)/liblapack.a
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../ndk-modules/lapack/jni/clapack/INCLUDE
    LOCAL_STATIC_LIBRARIES := tmglib clapack blas f2c
    include $(PREBUILT_STATIC_LIBRARY)
    
    include $(CLEAR_VARS)
    LOCAL_MODULE := clapack
    LOCAL_SRC_FILES := $(LOCAL_PATH)/../../prebuild-libs/$(TARGET_ARCH_ABI)/libclapack.a
    include $(PREBUILT_STATIC_LIBRARY)
    
    include $(CLEAR_VARS)
    LOCAL_MODULE := blas
    LOCAL_SRC_FILES := $(LOCAL_PATH)/../../prebuild-libs/$(TARGET_ARCH_ABI)/libblas.a
    include $(PREBUILT_STATIC_LIBRARY)
    
    include $(CLEAR_VARS)
    LOCAL_MODULE := f2c
    LOCAL_SRC_FILES := $(LOCAL_PATH)/../../prebuild-libs/$(TARGET_ARCH_ABI)/libf2c.a
    include $(PREBUILT_STATIC_LIBRARY)
    #tmglib clapack blas f2c
else
#if prebuild libs don't work compile them with the following commands and copy then manually to prebuild-libs folder
$(call import-add-path,$(LOCAL_PATH)/../ndk-modules)
$(call import-module,lapack)
endif

$(call import-add-path,$(LOCAL_PATH)/../ndk-modules)
$(call import-module,cvd)
$(call import-module,gvars3)
$(call import-module,TooN)
$(call import-module,agast)
$(call import-module,glm)
$(call import-module,tinyxml2)

$(call import-module,android/cpufeatures)