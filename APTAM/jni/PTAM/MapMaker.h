// -*- c++ -*-
// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

//
// This header declares the MapMaker class
// MapMaker makes and maintains the Map struct
// Starting with stereo initialisation from a bunch of matches
// over keyframe insertion, continual bundle adjustment and 
// data-association refinement.
// MapMaker runs in its own thread, although some functions
// (notably stereo init) are called by the tracker and run in the 
// tracker's thread.

#ifndef __MAPMAKER_H
#define __MAPMAKER_H
#include <cvd/image.h>
#include <cvd/byte.h>
#include <cvd/thread.h>

#include "Map.h"
#include "KeyFrame.h"
#include "MapPoint.h"
#include "ATANCamera.h"
#include <queue>
#include <pthread.h>          // std::mutex

#include <unordered_set>
#include "threadpool.h"

#include <MapSerialization.h>

namespace APTAM {

// Each MapPoint has an associated MapMakerData class
// Where the mapmaker can store extra information
 
struct MapMakerData
{
  std::set<KeyFrame*> sMeasurementKFs;   // Which keyframes has this map point got measurements in?
  std::set<KeyFrame*> sNeverRetryKFs;    // Which keyframes have measurements failed enough so I should never retry?
  inline int GoodMeasCount()            
  {  return sMeasurementKFs.size(); }

  inline XMLElement* save(MapSerializationHelper& helper)
  {
	  XMLDocument* doc = helper.GetXMLDocument();
	  XMLElement *mmdata = doc->NewElement("MapMakerData");

	  XMLElement *mkfs = doc->NewElement("MeasurementKFs");
	  mkfs->SetAttribute("size",sMeasurementKFs.size());

	  stringstream ss;
	  //for(int i = 0; i < sMeasurementKFs.size(); i++)
	  for(auto iter=sMeasurementKFs.begin(); iter!=sMeasurementKFs.end(); ++iter)
	  {
		  ss << helper.GetKeyFrameID(*iter) << " ";
	  }
	  mkfs->SetText(ss.str().c_str());

	  mmdata->InsertEndChild(mkfs);

	  XMLElement *nrkfs = doc->NewElement("NeverRetryKFs");
	  nrkfs->SetAttribute("size",sNeverRetryKFs.size());

	  stringstream ss2;
	  //for(int i = 0; i < sNeverRetryKFs.size(); i++)
      for(auto iter=sNeverRetryKFs.begin(); iter!=sNeverRetryKFs.end(); ++iter)
	  {
		  ss2 << helper.GetKeyFrameID(*iter) << " ";
	  }
	  nrkfs->SetText(ss2.str().c_str());

	  mmdata->InsertEndChild(nrkfs);

      return mmdata;
  }

  inline void load(const XMLElement* mmdata, MapSerializationHelper& helper)
  {


	  const XMLElement *mkfs = mmdata->FirstChildElement("MeasurementKFs");
	  int size;
	  mkfs->QueryAttribute("size",&size);

	  if(size>0)
	  {
		  stringstream ss(mkfs->GetText());
		  for(int i = 0; i < size; i++)
		  //for(auto iter=sMeasurementKFs.begin(); iter!=sMeasurementKFs.end(); ++iter)
		  {
			  int kfid;
			  ss >> kfid;
			  sMeasurementKFs.insert(helper.GetKeyFrame(kfid));
		  }
	  }

	  const XMLElement *nrkfs = mmdata->FirstChildElement("NeverRetryKFs");
	  nrkfs->QueryAttribute("size",&size);

	  if(size>0)
	  {
		  stringstream ss2(nrkfs->GetText());
		  for(int i = 0; i < size; i++)
		  //for(auto iter=sNeverRetryKFs.begin(); iter!=sNeverRetryKFs.end(); ++iter)
		  {
			  int kfid;
			  ss2 >> kfid;
			  sNeverRetryKFs.insert(helper.GetKeyFrame(kfid));
		  }
	  }


  }
};

// MapMaker dervives from CVD::Thread, so everything in void run() is its own thread.
class MapMaker : protected CVD::Thread
{
  public:
    MapMaker(Map &m, const ATANCamera &cam);
    ~MapMaker();
    
    // Make a map from scratch. Called by the tracker.
    bool InitFromStereo(KeyFrame &kFirst, KeyFrame &kSecond, 
                        std::vector<std::pair<CVD::ImageRef, CVD::ImageRef> > &vMatches,
    					//std::vector<std::pair<TooN::Vector<2>, TooN::Vector<2>> > &vMatches,
                        SE3<> &se3CameraPos);

	bool InitFromLoad(); //Init after map loaded
   
    
    void AddKeyFrame(KeyFrame &k, void* kfsource);   // Add a key-frame to the map. Called by the tracker.

    void RequestReset();   // Request that the we reset. Called by the tracker.
    bool ResetDone();      // Returns true if the has been done.
	int  QueueSize() { return mvpKeyFrameQueue.size() ;} // How many KFs in the queue waiting to be added?
	void RequestCleanUp();

    bool NeedNewKeyFrame(KeyFrame &kCurrent);            // Is it a good camera pose to add another KeyFrame?
    bool IsDistanceToNearestKeyFrameExcessive(KeyFrame &kCurrent);  // Is the camera far away from the nearest KeyFrame (i.e. maybe lost?)
	
	void RequestContinue(); //requests main loop to start again, blocking call

	void SaveMap(string filename);
	void LoadMap(string filename);

#ifdef __ANDROID__
	bool useSensorDataForInitialization;
#endif
  
  protected:
  
   Map &mMap;               // The map
  ATANCamera mCamera;      // Same as the tracker's camera: N.B. not a reference variable!
  
    virtual void run();      // The MapMaker thread code lives here

    // Functions for starting the map from scratch:
    SE3<> CalcPlaneAligner();
    void ApplyGlobalTransformationToMap(SE3<> se3NewFromOld);
    void ApplyGlobalScaleToMap(double dScale);
    
    // Map expansion functions:
    void AddKeyFrameFromTopOfQueue();  
    void ThinCandidates(KeyFrame &k, int nLevel);
    void AddSomeMapPoints(int nLevel);
    bool AddPointEpipolar(KeyFrame &kSrc, KeyFrame &kTarget, int nLevel, int nCandidate);
    // Returns point in ref frame B
    Vector<3> ReprojectPoint(SE3<> se3AfromB, const Vector<2> &v2A, const Vector<2> &v2B);
    
    // Bundle adjustment functions:
    void BundleAdjust(std::unordered_set<KeyFrame*>, std::unordered_set<KeyFrame*>, std::unordered_set<MapPoint*>, bool);
    void BundleAdjustAll();
    void BundleAdjustRecent();

    // Data association functions:
    int ReFindInSingleKeyFrame(KeyFrame &k);
    void ReFindFromFailureQueue();
    void ReFindNewlyMade();
    void ReFindAll();
    bool ReFind_Common(KeyFrame &k, MapPoint &p);
    void SubPixelRefineMatches(KeyFrame &k, int nLevel);
    
    // General Maintenance/Utility:
    void Reset();
	
    void CleanUpMap();
    
    void HandleBadPoints();
    double DistToNearestKeyFrame(KeyFrame &kCurrent);
    double KeyFrameLinearDist(KeyFrame &k1, KeyFrame &k2);
	double KeyFrameLinearTargetDist(KeyFrame &k1, KeyFrame &k2);
    KeyFrame* ClosestKeyFrame(KeyFrame &k);
	KeyFrame* ClosestKeyFrameAdvanced(KeyFrame &k,double minDist=0);
    std::vector<KeyFrame*> NClosestKeyFrames(KeyFrame &k, unsigned int N);
	std::vector<KeyFrame*> NClosestKeyFramesTarget(KeyFrame &k, unsigned int N);
    void RefreshSceneDepth(KeyFrame *pKF);

    // GUI Interface:
    void GUICommandHandler(std::string sCommand, std::string sParams);
    static void GUICommandCallBack(void* ptr, std::string sCommand, std::string sParams);


  // Member variables:
  protected:
    pthread_mutex_t kflock;

    // GUI Interface:
    struct Command {std::string sCommand; std::string sParams; };
    std::vector<Command> mvQueuedCommands;
    Mutex mmCommands;
	
	  // Member variables:
  std::vector<KeyFrame*> mvpKeyFrameQueue;  // Queue of keyframes from the tracker waiting to be processed
  std::vector<std::pair<KeyFrame*, MapPoint*> > mvFailureQueue; // Queue of failed observations to re-find
  std::queue<MapPoint*> mqNewQueue;   // Queue of newly-made map points to re-find in other KeyFrames

  
    double mdWiggleScale;  // Metric distance between the first two KeyFrames (copied from GVar)
                          // This sets the scale of the map
    GVars3::gvar3<double> mgvdWiggleScale;   // GVar for above
    double mdWiggleScaleDepthNormalized;  // The above normalized against scene depth, 
                                          // this controls keyframe separation

    bool mbBundleConverged_Full;    // Has global bundle adjustment converged?
    bool mbBundleConverged_Recent;  // Has local bundle adjustment converged?

	double mdFirstKeyFrameDist;

	int numAcceptedCandidates;
	int numTotalCandidates;

    
    // Thread interaction signalling stuff
    bool mbResetRequested;            // A reset has been requested
    bool mbResetDone;                 // The reset was done.
    bool mbBundleAbortRequested;      // We should stop bundle adjustment
    bool mbBundleRunning;             // Bundle adjustment is running
    bool mbBundleRunningIsRecent;     //    ... and it's a local bundle adjustment.

   
	bool mbCleanUpRequested;

	bool updatePositionOnly;

	bool mbRequestContinue;

  public: //TODO make clean
	bool mbLockMap;
};


}

#endif

