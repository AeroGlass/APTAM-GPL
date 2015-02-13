//Copyright ICGJKU 2015
#ifndef __TIMING__
#define __TIMING__

#ifdef __ANDROID__
static inline double
now_ms(void)
{
	struct timespec res;
	clock_gettime(CLOCK_REALTIME, &res);
	return 1000.0*res.tv_sec + (double)res.tv_nsec/1e6;
}
#else
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static inline double
now_ms(void)
{
	LARGE_INTEGER frequency; 
	LARGE_INTEGER t1; 
	double elapsedTime; 
	QueryPerformanceFrequency(&frequency);

	QueryPerformanceCounter(&t1); 
	elapsedTime = (float)(t1.QuadPart) / (frequency.QuadPart / 1000.0);

	return elapsedTime;
}
#endif
#endif

#ifdef ENABLE_TIMING
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/** Use to init the clock */
#define TIMER_INIT LARGE_INTEGER frequency;LARGE_INTEGER t1,t2;double elapsedTime;QueryPerformanceFrequency(&frequency);

/** Use to start the performance timer */
#define TIMER_START QueryPerformanceCounter(&t1);

/** Use to stop the performance timer and output the result to the standard stream. Less verbose than \c TIMER_STOP_VERBOSE */
//#define TIMER_STOP QueryPerformanceCounter(&t2);elapsedTime=(float)(t2.QuadPart-t1.QuadPart)/(frequency.QuadPart/1000.0);std::wcout<<elapsedTime<<L" msec"<<endl;
#define TIMER_STOP(NAME) QueryPerformanceCounter(&t2);elapsedTime=(float)(t2.QuadPart-t1.QuadPart)/(frequency.QuadPart/1000.0);//std::wcout<<NAME<<": "<<elapsedTime<<L" msec"<<endl;
#else

#define TIMER_INIT double t1,t2;
#define TIMER_START t1=now_ms();
#ifdef __ANDROID__
#include <android/log.h>
#define TIMER_STOP(NAME) t2=now_ms(); __android_log_print(ANDROID_LOG_INFO, "Timer", "%s: %f msec",NAME,(t2-t1));
#else
#define TIMER_STOP(NAME) t2=now_ms(); std::cout<<NAME<<": "<<(t2-t1)<<" msec"<<std::endl;
#endif
#endif

#else
#define TIMER_INIT
#define TIMER_START
#define TIMER_STOP(NAME)
#endif

#endif
