#include "VideoSource.h"
#include <android/log.h>

using namespace CVD;
using namespace std;
using namespace APTAM;


VideoSource::VideoSource()
{
    int tmp[2];
    getSize(tmp);

    int width = tmp[0];
    int height = tmp[1];

    mirSize = ImageRef(width, height);
    __android_log_print(ANDROID_LOG_INFO, "video source init", "w: %d", mirSize.x);
    __android_log_print(ANDROID_LOG_INFO, "video source init", "h: %d", mirSize.y);
}

void VideoSource::GetAndFillFrameBWandRGB(Image<byte> &imBW, Image<Rgb<byte> > &imRGB)
{
	getFrame(&imBW, &imRGB, mirSize.x, mirSize.y);
}

ImageRef VideoSource::Size()
{
  return mirSize;
};

//JNI functions are defined in ptam-main.cc, TODO clean this up a bit...
