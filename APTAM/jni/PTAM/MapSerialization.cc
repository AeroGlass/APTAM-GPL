/*
 * MapSerialization.cc
 *
 *  Created on: 06.02.2015
 *      Author: ICGJKU
 */

#include "MapSerialization.h"
#include <sys/stat.h>
#include <cvd/image_io.h>

#ifdef __ANDROID__
#include <android/log.h>
#include <sys/syslimits.h>
#include <unistd.h>
#include <dirent.h>
#endif

namespace APTAM{

using namespace std;
using namespace TooN;
using namespace CVD;


MapSerializationHelper::MapSerializationHelper(string folder)
{
	keyframeid=0;
	mappointid=0;
	mapfolder = folder;
}

void MapSerializationHelper::CreateAndCleanUpMapFolder()
{
	mkdir(mapfolder.c_str(), 0755);
	mkdir((mapfolder+"/keyframes").c_str(), 0755);

	//delete old keyframes (if there are any)
	DIR * kffolder = opendir ((mapfolder+"/keyframes").c_str());

	if(kffolder==NULL)
		return;

	struct dirent *dp;

	do {
		errno = 0;
		if ((dp = readdir(kffolder)) != NULL) {

			unlink((mapfolder+"/keyframes/"+dp->d_name).c_str());

		}
	} while (dp != NULL);
	//unlink

	closedir(kffolder);
}

void MapSerializationHelper::RegisterKeyFrame(KeyFrame* kf)
{
	mKeyFrameForwardMap[kf]=keyframeid;
	mKeyFrameBackwardMap.push_back(kf);
	keyframeid++;
}

void MapSerializationHelper::RegisterMapPoint(MapPoint* mp)
{
	mMapPointForwardMap[mp]=mappointid;
	mMapPointBackwardMap.push_back(mp);
	mappointid++;
}

int MapSerializationHelper::GetKeyFrameID(KeyFrame* kf)
{
	return mKeyFrameForwardMap[kf];
}

KeyFrame* MapSerializationHelper::GetKeyFrame(int kfid)
{
	if(kfid>=mKeyFrameBackwardMap.size()||kfid<0)
			__android_log_print(ANDROID_LOG_INFO, "Serialization", "KeyFrame out of range (%d/%d)",kfid,mKeyFrameBackwardMap.size());
	return mKeyFrameBackwardMap[kfid];
}

int MapSerializationHelper::GetMapPointID(MapPoint* mp)
{
	return mMapPointForwardMap[mp];
}

MapPoint* MapSerializationHelper::GetMapPoint(int mpid)
{
	if(mpid>=mMapPointBackwardMap.size()||mpid<0)
		__android_log_print(ANDROID_LOG_INFO, "Serialization", "MapPoint out of range (%d/%d)",mpid,mMapPointBackwardMap.size());
	return mMapPointBackwardMap[mpid];
}

XMLDocument* MapSerializationHelper::GetXMLDocument()
{
	return &doc;
}

bool MapSerializationHelper::SaveXMLDocument()
{
	return doc.SaveFile((mapfolder+"/map.xml").c_str())==XML_NO_ERROR;
}

bool MapSerializationHelper::LoadXMLDocument()
{
	return doc.LoadFile((mapfolder+"/map.xml").c_str())==XML_NO_ERROR;
}

void MapSerializationHelper::SaveImage(CVD::Image<CVD::byte> im, int imageid)
{
	stringstream kfname;
	kfname << mapfolder << "/keyframes/image-" << imageid << ".png";
	img_save(im, kfname.str());
}

void MapSerializationHelper::LoadImage(CVD::Image<CVD::byte> &im,int imageid)
{
	stringstream kfname;
	__android_log_print(ANDROID_LOG_INFO, "Serialization", "Loading Image (%d)",imageid);
	kfname << mapfolder << "/keyframes/image-" << imageid << ".png";
	img_load(im,kfname.str());
	__android_log_print(ANDROID_LOG_INFO, "Serialization", "Image Size (%d,%d)",im.size().x,im.size().y);
}

}
