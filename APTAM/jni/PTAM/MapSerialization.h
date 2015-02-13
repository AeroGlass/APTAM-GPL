/*
 * MapSerialization.h
 *
 *  Created on: 06.02.2015
 *      Author: ICGJKU
 */

#ifndef MAPSERIALIZATION_H_
#define MAPSERIALIZATION_H_

#include <map>
#include <tinyxml2.h>
#include <TooN/TooN.h>
#include <cvd/image_ref.h>
#include <cvd/image.h>
#include <cvd/byte.h>
#include <limits>

namespace APTAM{
using namespace TooN;
using namespace CVD;
using namespace std;
using namespace tinyxml2;

class KeyFrame;
class MapPoint;

class MapSerializationHelper
{
public:
	MapSerializationHelper(string folder);

	void RegisterKeyFrame(KeyFrame* kf);
	void RegisterMapPoint(MapPoint* mp);

	int GetKeyFrameID(KeyFrame* kf);
	KeyFrame* GetKeyFrame(int kfid);

	int GetMapPointID(MapPoint* mp);
	MapPoint* GetMapPoint(int mpid);

	void CreateAndCleanUpMapFolder();

	XMLDocument* GetXMLDocument();
	bool SaveXMLDocument();
	bool LoadXMLDocument();

	void SaveImage(CVD::Image<CVD::byte> im, int id);
	void LoadImage(CVD::Image<CVD::byte> &im,int id);

	template<int N> string saveVector(Vector<N> v)
	{
		stringstream ss;
		ss.precision(numeric_limits<double>::digits10+1);
		for(int i=0; i < N; i++)
		{
			ss << v[i] << " ";
		}
		return ss.str();
	}

	template<int N> Vector<N> loadVector(string data)
	{
		Vector<N> v;
		stringstream ss(data);
		for(int i=0; i < N; i++)
		{
			ss >> v[i];
		}
		return v;
	}

	string saveImageRef(ImageRef v)
	{
		stringstream ss;
		ss << v.x << " " << v.y;
		return ss.str();
	}

	ImageRef loadImageRef(string data)
	{
		ImageRef v;
		stringstream ss(data);
		ss >> v.x >> v.y;
		return v;
	}

protected:
	int keyframeid;
	int mappointid;

	string mapfolder;

	std::map<KeyFrame*,int> mKeyFrameForwardMap;
	std::vector<KeyFrame*> mKeyFrameBackwardMap;
	std::map<MapPoint*,int> mMapPointForwardMap;
	std::vector<MapPoint*> mMapPointBackwardMap;

	XMLDocument doc;
};

}

#endif /* MAPSERIALIZATION_H_ */
