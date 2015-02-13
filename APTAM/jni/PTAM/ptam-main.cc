#include <string.h>
//#include <sstream>
#include <iostream>     // std::streambuf, std::cout
#include <fstream>      // std::ofstream
#include <stdlib.h>
#include <android/log.h>
#include <gvars3/instances.h>
#include <jni.h>
#include "OpenGL.h"
#include "CameraCalibrator.h"
#include "System.h"
#include "Tracker.h"

//#define ENABLE_TIMING
#include "Timing.h"

using namespace std;
using namespace GVars3;
using namespace APTAM;

extern "C"{

JavaVM* jvm=NULL;
System* msys;
CameraCalibrator* mcal;
bool docalibration = false;

static double last = now_ms();

std::streambuf *psbuf, *coutbackup, *cerrbackup;
std::ofstream filestr;

//init PTAM
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeInit( JNIEnv* env, jobject thiz , jboolean calib)
{
	//init jni
	env->GetJavaVM(&jvm);

	//redirect cout and cerr for debugging purposes
	 filestr.open(GLWindow2::getFDir()+"coutlog.txt", std::ofstream::out | std::ofstream::app);
	 __android_log_print(ANDROID_LOG_INFO, "coutfile", "%s", (GLWindow2::getFDir()+"coutlog.txt").c_str());

	 // back up old streambuffers
	 coutbackup = std::cout.rdbuf();
	 cerrbackup = std::cerr.rdbuf();

	 //change streambuffers
	 psbuf = filestr.rdbuf();
	 std::cout.rdbuf(psbuf);
	 std::cerr.rdbuf(psbuf);

	 //add log time
	 time_t _tm =time(NULL );
	 struct tm * curtime = localtime ( &_tm );
	cout<<endl<<endl;
	cout << "==========================================================" << endl;
	cout << "New Log: " << asctime(curtime) << endl;
	cout << "==========================================================" << endl;

	 //redirect done...

	//init PTAM
	if(!docalibration)
	{
		GUI.LoadFile(GLWindow2::getFDir()+"camera.cfg");
		GUI.LoadFile(GLWindow2::getFDir()+"settings.cfg");
	}
	else
	{
		//GV3::set_var("Camera.Parameters","[0.5 0.75 0.5 0.5 0.1 ]");
		//GV3::set_var("Camera.UseOpenCVDistortion","0");
		GUI.LoadFile(GLWindow2::getFDir()+"calibrator_settings.cfg");
		//GUI.StopParserThread();
	}

	GUI.StartParserThread(); // Start parsing of the console input
	atexit(GUI.StopParserThread);

	docalibration = calib;

	if(docalibration)
	{
		 GV3::get<Vector<NUMTRACKERCAMPARAMETERS> >("Camera.Parameters", ATANCamera::mvDefaultParams, SILENT);
	}

	try
	{
		if(docalibration)
			mcal = new CameraCalibrator();
		else
			msys = new System();
	}
	catch(CVD::Exceptions::All e)
	{
		cout << endl;
		cout << "Error when initializing System: " << e.what << std::endl;
	}
}

//clean up
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeDestroy( JNIEnv* env, jobject thiz )
{
	if(docalibration)
	{
		delete mcal;
		mcal = NULL;
	}
	else
	{
		delete msys;
		msys = NULL;
	}
	if(filestr.is_open())
	{
		std::cout.rdbuf(coutbackup);        // restore cout's original streambuf
		std::cerr.rdbuf(cerrbackup);        // restore cout's original streambuf
		filestr.close();
	}
}

//initialize OpenGL
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeInitGL( JNIEnv* env, jobject thiz )
{
	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

	#ifdef USE_OGL2
	gles2h.InitializeShaders();
	#endif
}

static int framecount = 0;

//render and process a new frame
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeRender( JNIEnv* env, jobject thiz )
{
	framecount++;
	if(now_ms()-last>1000)
	{
		__android_log_print(ANDROID_LOG_INFO, "FPS", "%d",framecount);
		cout << "FPS: " << framecount << endl;
		last = now_ms();
		framecount = 0;
	}

	if(docalibration)
		mcal->Run();
	else
		msys->Run();
}

//check if finished
JNIEXPORT bool JNICALL
Java_at_jku_ptam_PTAM_nativeFinish( JNIEnv* env, jobject thiz )
{
	if(docalibration)
		return true;
	else
	{
		msys->requestFinish = true;
		msys->Run();
		return msys->finished;
	}
}

//forward mouse click to ptam
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeClick( JNIEnv* env, jobject thiz , jint x, jint y)
{
	if(docalibration)
		mcal->mGLWindow.on_mouse_down(x,y);
	else
		msys->mGLWindow.on_mouse_down(x,y);
}

//forward keyboard to ptam
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeKey( JNIEnv* env, jobject thiz , jint keycode)
{
	if(docalibration)
		mcal->mGLWindow.on_key_down(keycode);
	else
		msys->mGLWindow.on_key_down(keycode);
}

//resize window (might only work once)
JNIEXPORT void JNICALL
Java_at_jku_ptam_PTAM_nativeResize( JNIEnv* env, jobject thiz , jint w, jint h)
{
	if(docalibration)
		mcal->mGLWindow.resize(w,h);
	else
		msys->mGLWindow.resize(w,h);
}

bool cachedfdir = false;
std::string fdir = "";

//get config dir
std::string APTAM::GLWindow2::getFDir()
{
	if(!cachedfdir)
	{
		//todo check if smth wrong in here as i had segfault without caching fdir

		if(jvm==NULL) return "";
		JNIEnv* env;
		jvm->AttachCurrentThread(&env, NULL);
		jclass main = env->FindClass("at/jku/ptam/PTAM");
		jmethodID getFDir = env->GetStaticMethodID(main, "getFDir",
				"()Ljava/lang/String;");
		jstring result = (jstring)env->CallStaticObjectMethod(main, getFDir);

		jboolean iscopy;
		const char* elems = env->GetStringUTFChars(result,&iscopy);
		//std::string fdir;
		fdir.append(elems,env->GetStringUTFLength(result));
		env->ReleaseStringUTFChars(result,elems);
		fdir = fdir+"/";
		cachedfdir = true;
	}
	return fdir;
}

//hacky solution to allow rendering text in opengl es
JNICALL const void APTAM::GLWindow2::drawTextJava(std::string s,CVD::ImageRef irPos, int shaderid) const{
	if(jvm==NULL) return;
	JNIEnv* env;
	jvm->AttachCurrentThread(&env, NULL);
	jclass main = env->FindClass("at/jku/ptam/PTAM");
	jmethodID drawText = env->GetStaticMethodID(main, "drawText", "(Ljava/lang/String;III)V");
	jstring text = env->NewStringUTF(s.c_str());
	jint x = irPos.x;
	jint y = irPos.y;
	env->CallStaticVoidMethod(main, drawText,text,x,y,(jint)shaderid);
	env->DeleteLocalRef(text);

}

//get image size
JNICALL void VideoSource::getSize(int * sizeBuffer) {
	if(jvm==NULL) return;
	JNIEnv* env;
	jvm->AttachCurrentThread(&env, NULL);
	jclass main = env->FindClass("at/jku/ptam/PTAM");
	jmethodID getVideoSource = env->GetStaticMethodID(main, "getVideoSource", "()Lat/jku/ptam/VideoSource;");
	jobject vs = env->CallStaticObjectMethod(main, getVideoSource);
	jclass videosource = env->FindClass("at/jku/ptam/VideoSource");
	jmethodID getSize = env->GetMethodID(videosource, "getSize", "()[I");
	jintArray result = (jintArray) env->CallObjectMethod(vs, getSize);
	int len = env->GetArrayLength(result);

	jboolean iscopy;
	jint* elems = env->GetIntArrayElements(result,&iscopy);
	sizeBuffer[0] = elems[0];
	sizeBuffer[1] = elems[1];
	env->ReleaseIntArrayElements(result,elems,JNI_ABORT);
}

//get rotation data gained from androids sensors matching the last requested image
JNICALL void Tracker::getRotation(double * rotMat) {
	if(jvm==NULL) return;
	JNIEnv* env;
	jvm->AttachCurrentThread(&env, NULL);
	jclass main = env->FindClass("at/jku/ptam/PTAM");
	jmethodID getVideoSource = env->GetStaticMethodID(main, "getVideoSource", "()Lat/jku/ptam/VideoSource;");
	jobject vs = env->CallStaticObjectMethod(main, getVideoSource);
	jclass videosource = env->FindClass("at/jku/ptam/VideoSource");
	jmethodID getRotationJ = env->GetMethodID(videosource, "getRotation", "()[F");
	jfloatArray result = (jfloatArray) env->CallObjectMethod(vs, getRotationJ);
	int len = env->GetArrayLength(result);

	jboolean iscopy;
	jfloat* elems = env->GetFloatArrayElements(result,&iscopy);
	for(int i = 0; i < 9; i++)
		rotMat[i] = elems[i];
	env->ReleaseFloatArrayElements(result,elems,JNI_ABORT);
}

//adapt camera brightness correction (unused)
JNICALL void VideoSource::changeBrightness(int change)
{
	if(jvm==NULL) return;
	JNIEnv* env;
	jvm->AttachCurrentThread(&env, NULL);
	jclass main = env->FindClass("at/jku/ptam/PTAM");
	jmethodID getVideoSource = env->GetStaticMethodID(main, "getVideoSource", "()Lat/jku/ptam/VideoSource;");
	jobject vs = env->CallStaticObjectMethod(main, getVideoSource);
	jclass videosource = env->FindClass("at/jku/ptam/VideoSource");
	jmethodID getFrame = env->GetMethodID(videosource, "changeBrightness", "(I)V");
	env->CallVoidMethod(vs, getFrame, change);
}

//get frame data from java
JNICALL void VideoSource::getFrame(CVD::Image<CVD::byte> * imBW,
		CVD::Image<CVD::Rgb<CVD::byte> > * imRGB, int width, int height) {
	if(jvm==NULL) return;
	TIMER_INIT
	TIMER_START
	JNIEnv* env;
	jvm->AttachCurrentThread(&env, NULL);
	jclass main = env->FindClass("at/jku/ptam/PTAM");
	jmethodID getVideoSource = env->GetStaticMethodID(main, "getVideoSource", "()Lat/jku/ptam/VideoSource;");
	jobject vs = env->CallStaticObjectMethod(main, getVideoSource);
	jclass videosource = env->FindClass("at/jku/ptam/VideoSource");
	jmethodID getFrame = env->GetMethodID(videosource, "getFrame", "()[B");
	jbyteArray array = (jbyteArray) env->CallObjectMethod(vs, getFrame);
	TIMER_STOP("Call Java")
	TIMER_START

	int len = env->GetArrayLength(array);
	jboolean frame_copy;

	imBW->resize(mirSize);

	env->GetByteArrayRegion(array,0,width * height,(jbyte*)imBW->data());

	TIMER_STOP("BW image")
	TIMER_START

	if(imRGB!=NULL)
	{
		//use yuv shader to render image directly instead of converting it to rgb first (which is slow)
		//hack use rgb image for yuv data!
		imRGB->resize(mirSize);
		env->GetByteArrayRegion(array,0,width * height * 1.5,(jbyte*)imRGB->data());

	}

	TIMER_STOP("RGB image")
}

}
