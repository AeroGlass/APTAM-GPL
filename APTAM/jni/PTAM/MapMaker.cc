// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

#include "MapMaker.h"
#include "MapPoint.h"
#include "Bundle.h"
#include "PatchFinder.h"
#include "SmallMatrixOpts.h"
#include "HomographyInit.h"

#include <cvd/vector_image_ref.h>
#include <cvd/vision.h>
#include <cvd/image_interpolate.h>

#include <TooN/SVD.h>
#include <TooN/SymEigen.h>

#include <gvars3/instances.h>
#include <fstream>
#include <algorithm>
#include <unordered_map>

#include "GLWindow2.h"
#include "MapSerialization.h"

#include "Tracker.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

//#define ENABLE_TIMING
#include "Timing.h"

#ifdef WIN32
#include <Windows.h>
#endif

namespace APTAM {

using namespace CVD;
using namespace std;
using namespace GVars3;

// Constructor sets up internal reference variable to Map.
// Most of the intialisation is done by Reset()..
MapMaker::MapMaker(Map& m, const ATANCamera &cam)
  : mMap(m), mCamera(cam),
    mbResetRequested(false),
    mbResetDone(true),
    mbBundleAbortRequested(false),
    mbBundleRunning(false),
    mbBundleRunningIsRecent(false),
    mbCleanUpRequested(false),
	updatePositionOnly(false),
	mbLockMap(false)
{
  pthread_mutex_init(&kflock, NULL);
  
  Reset();
  
  start(); // This CVD::thread func starts the map-maker thread with function run()
  //GUI.RegisterCommand("SaveMap", GUICommandCallBack, this);
  //GUI.RegisterCommand("LoadMap", GUICommandCallBack, this);
  GV3::Register(mgvdWiggleScale, "MapMaker.WiggleScale", 0.3, SILENT); // Default to 10cm between keyframes

#ifdef __ANDROID__
  useSensorDataForInitialization = true;
#endif
};


void MapMaker::Reset()
{
  // This is only called from within the mapmaker thread...
  mMap.Reset();
  mvFailureQueue.clear();
  while(!mqNewQueue.empty()) mqNewQueue.pop();
  mMap.vpKeyFrames.clear(); // TODO: actually erase old keyframes
  mvpKeyFrameQueue.clear(); // TODO: actually erase old keyframes
  mbBundleRunning = false;
  mbBundleConverged_Full = true;
  mbBundleConverged_Recent = true;
  mbResetDone = true;
  mbResetRequested = false;
  mbBundleAbortRequested = false;
}

void MapMaker::RequestCleanUp()
{
	mbCleanUpRequested = true;
}

void MapMaker::CleanUpMap()
{
	cout << "Cleaning Map..." << endl;
	mbCleanUpRequested = false;

	//heuristic test remove all map points detected in only 2 keyframes
	//TODO weight based on number of keyframes??

	// delete points that were not refound in at least x keyframes
  for( unsigned int i = 0; i < mMap.vpPoints.size(); i++ )
  {
    MapPoint *p = mMap.vpPoints[i];
	int numKF = 0;
	for( unsigned int j = 0; j < mMap.vpKeyFrames.size(); j++)
    {
		KeyFrame &k = *mMap.vpKeyFrames[j];
		if(k.mMeasurements.count(p)) {
			numKF++;
		}
    }
    if(numKF < 4) {
      p->bBad = true;
    }
  }

  HandleBadPoints();

}

void MapMaker::RequestContinue()
{
	mbRequestContinue = true;
	while(mbRequestContinue)
		sleep(5);
}

// CHECK_RESET is a handy macro which makes the mapmaker thread stop
// what it's doing and reset, if required.
#define CHECK_RESET if(mbResetRequested) {Reset(); continue;};

#define CHECK_CONTINUE if(mbRequestContinue) { mbRequestContinue = false; continue; }

#define DOCHECKS CHECK_RESET CHECK_CONTINUE 

/**
 * Run the map maker thread
 */
void MapMaker::run()
{
	TIMER_INIT

#ifdef WIN32
  // For some reason, I get tracker thread starvation on Win32 when
  // adding key-frames. Perhaps this will help:
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
  ///@TODO Try setting this to THREAD_PRIORITY_HIGHEST to see how it effects your performance.
#endif
	bool printDebug = true;

  while(!shouldStop())  // ShouldStop is a CVD::Thread func which return true if the thread is told to exit.
    {
      DOCHECKS;
      sleep(5); // Sleep not really necessary, especially if mapmaker is busy
      DOCHECKS;
      
      // Handle any GUI commands encountered..
      mmCommands.lock();
      while(!mvQueuedCommands.empty())
	{
	  GUICommandHandler(mvQueuedCommands.begin()->sCommand, mvQueuedCommands.begin()->sParams);
	  mvQueuedCommands.erase(mvQueuedCommands.begin());
	}
      mmCommands.unlock();
      
      if(!mMap.IsGood())  // Nothing to do if there is no map yet!
      {
    	  //__android_log_print(ANDROID_LOG_INFO, "sleep", "sleeping");
    	  //sleep(500);
    	  continue;
      }

      if(mbLockMap)
      		  continue;

	  if(mbCleanUpRequested)
		  CleanUpMap();
      
      // From here on, mapmaker does various map-maintenance jobs in a certain priority
      // Hierarchy. For example, if there's a new key-frame to be added (QueueSize() is >0)
      // then that takes high priority.
      
      DOCHECKS;
      // Should we run local bundle adjustment?
      if(!mbBundleConverged_Recent /*&& QueueSize() == 0*/)
	  {
		TIMER_START  
		BundleAdjustRecent();   
		TIMER_STOP("BundleAdjustRecent")
	  }
      
      DOCHECKS;
      // Are there any newly-made map points which need more measurements from older key-frames?
      if(/*mbBundleConverged_Recent &&*/ QueueSize() == 0 && !mqNewQueue.empty())
	  {
		TIMER_START
	    ReFindNewlyMade();  
		TIMER_STOP("ReFindNewlyMade")
	  }
      
      DOCHECKS;
      // Run global bundle adjustment?
      if(mbBundleConverged_Recent && !mbBundleConverged_Full && QueueSize() == 0)
	  {
		TIMER_START
		BundleAdjustAll();
		TIMER_STOP("BundleAdjustAll")
	  }
      
      DOCHECKS;
      // Very low priorty: re-find measurements marked as outliers
      if(mbBundleConverged_Recent && mbBundleConverged_Full && rand()%20 == 0 && QueueSize() == 0)
	  {
		//TIMER_START
		ReFindFromFailureQueue();
		//TIMER_STOP("ReFindFromFailureQueue")
	  }
      
      DOCHECKS;
      HandleBadPoints();
      
      DOCHECKS;
      // Any new key-frames to be added?
      if(QueueSize() > 0)
	  {
		TIMER_START
		AddKeyFrameFromTopOfQueue(); // Integrate into map data struct, and process
		TIMER_STOP("AddKeyFrameFromTopOfQueue")
	  }
    }
}


//Tracker calls this to demand a reset
void MapMaker::RequestReset()
{
  mbResetDone = false;
  mbResetRequested = true;
}

bool MapMaker::ResetDone()
{
  return mbResetDone;
}


// HandleBadPoints() Does some heuristic checks on all points in the map to see if
// they should be flagged as bad, based on tracker feedback.
void MapMaker::HandleBadPoints()
{
  // Did the tracker see this point as an outlier more often than as an inlier?
  for( unsigned int i = 0; i < mMap.vpPoints.size(); i++ )
  {
    MapPoint &p = *mMap.vpPoints[i];
    if(p.nMEstimatorOutlierCount > 20 && p.nMEstimatorOutlierCount > p.nMEstimatorInlierCount) {
      p.bBad = true;
    }

	if(p.nAttempted-p.nFound > 20 && p.nAttempted-p.nFound > p.nFound) {
      //p.bBad = true;
	  //cout << "delpt";
    }

	if(p.nAttempted>100)
	{
		p.nAttempted = 0;
		p.nFound = 0;
	}

	if(p.nMEstimatorInlierCount+p.nMEstimatorOutlierCount>100)
	{
		p.nMEstimatorInlierCount = p.nMEstimatorOutlierCount = 0;
	}
  }
  
  // All points marked as bad will be erased - erase all records of them
  // from keyframes in which they might have been measured.
  for( unsigned int i = 0; i < mMap.vpPoints.size(); i++ )
  {
    if(mMap.vpPoints[i]->bBad)
    {
      MapPoint *p = mMap.vpPoints[i];

      for( unsigned int j = 0; j < mMap.vpKeyFrames.size(); j++)
      {
        KeyFrame &k = *mMap.vpKeyFrames[j];
        if(k.mMeasurements.count(p)) {
          k.mMeasurements.erase(p);
        }
      }
    }
  }
  
  // Move bad points to the trash list.
  mMap.LockMap();
  mMap.MoveBadPointsToTrash();
  mMap.UnlockMap();
}


/**
 * Destructor
 */
MapMaker::~MapMaker()
{
  mbBundleAbortRequested = true;
  stop(); // makes shouldStop() return true
  cout << "Waiting for mapmaker to die.." << endl;
  join();
  cout << " .. mapmaker has died." << endl;
  pthread_mutex_destroy(&kflock);
}


// Finds 3d coords of point in reference frame B from two z=1 plane projections
Vector<3> MapMaker::ReprojectPoint(SE3<> se3AfromB, const Vector<2> &v2A, const Vector<2> &v2B)
{
  Matrix<3,4> PDash;
  PDash.slice<0,0,3,3>() = se3AfromB.get_rotation().get_matrix();
  PDash.slice<0,3,3,1>() = se3AfromB.get_translation().as_col();
  
  Matrix<4> A;
  A[0][0] = -1.0; A[0][1] =  0.0; A[0][2] = v2B[0]; A[0][3] = 0.0;
  A[1][0] =  0.0; A[1][1] = -1.0; A[1][2] = v2B[1]; A[1][3] = 0.0;
  A[2] = v2A[0] * PDash[2] - PDash[0];
  A[3] = v2A[1] * PDash[2] - PDash[1];

  SVD<4,4> svd(A);
  Vector<4> v4Smallest = svd.get_VT()[3];
  if(v4Smallest[3] == 0.0)
    v4Smallest[3] = 0.00001;
  return project(v4Smallest);
}

bool MapMaker::InitFromLoad()
{
	cout << "MAP is initialized from Load!" << endl;
	//mdWiggleScale = *mgvdWiggleScale; // Cache this for the new map. TODO load this value??
	//cout << "wigglescale: " << mdWiggleScale << endl;
	if( mMap.vpKeyFrames.size()<2)
		return false;
	//mdWiggleScaleDepthNormalized = mdWiggleScale /  mMap.vpKeyFrames[0]->dSceneDepthMean;
	//mdFirstKeyFrameDist = KeyFrameLinearDist(*mMap.vpKeyFrames[0],*mMap.vpKeyFrames[1]);
	mMap.bGood = true;
	//se3TrackerPose = mMap.vpKeyFrames[0]->se3CfromW;
	return true;
}

// InitFromStereo() generates the initial match from two keyframes
// and a vector of image correspondences. Uses the 
bool MapMaker::InitFromStereo(KeyFrame &kF,
			      KeyFrame &kS,
			      vector<pair<ImageRef, ImageRef> > &vTrailMatches,
			      //vector<pair<Vector<2>, Vector<2>> > &vTrailMatches,
			      SE3<> &se3TrackerPose)
{
  mdWiggleScale = *mgvdWiggleScale; // Cache this for the new map.

  mCamera.SetImageSize(kF.aLevels[0].im.size());

  const int traillevel = GV2.GetInt("Tracker.TrailLevel", 0, SILENT);

  vector<HomographyMatch> vMatches;
  for(unsigned int i=0; i<vTrailMatches.size(); i++)
    {
      HomographyMatch m;
      m.v2CamPlaneFirst = mCamera.UnProject(LevelZeroPos(vTrailMatches[i].first,traillevel));
      m.v2CamPlaneSecond = mCamera.UnProject(LevelZeroPos(vTrailMatches[i].second,traillevel));
      m.m2PixelProjectionJac = mCamera.GetProjectionDerivs();
      vMatches.push_back(m);
    }

  SE3<> se3;
  bool bGood;
  HomographyInit HomographyInit;
  bGood = HomographyInit.Compute(vMatches, 5.0, se3);
  if(!bGood)
    {
      cout << "  Could not init from stereo pair, try again." << endl;
      return false;
    }
  
#ifdef __ANDROID__
  if(useSensorDataForInitialization)
  {
	  __android_log_print(ANDROID_LOG_INFO, "rot1", "%f %f %f",se3.get_rotation().ln()[0],se3.get_rotation().ln()[1],se3.get_rotation().ln()[2]);
	  __android_log_print(ANDROID_LOG_INFO, "rotsensor", "%f %f %f",se3TrackerPose.get_rotation().ln()[0],se3TrackerPose.get_rotation().ln()[1],se3TrackerPose.get_rotation().ln()[2]);
	  se3.get_rotation() = SO3<>(se3TrackerPose.get_rotation()).inverse();
	  __android_log_print(ANDROID_LOG_INFO, "transl", "%f %f %f",se3.get_translation()[0],se3.get_translation()[1],se3.get_translation()[2]);
	  Vector<3> it;
	  it[1] = it[2] = 0;
	  it[0] = mdWiggleScale;
	  se3.get_translation() = it;
	  se3 = se3.inverse();
	  __android_log_print(ANDROID_LOG_INFO, "transl set", "%f %f %f",se3.get_translation()[0],se3.get_translation()[1],se3.get_translation()[2]);
  }
#endif

  // Check that the initialiser estimated a non-zero baseline
  double dTransMagn = sqrt(se3.get_translation() * se3.get_translation());
  if(dTransMagn == 0)
    {
      cout << "  Estimated zero baseline from stereo pair, try again." << endl;
      return false;
    }
  // change the scale of the map so the second camera is wiggleScale away from the first
  se3.get_translation() *= mdWiggleScale/dTransMagn;

  
  KeyFrame *pkFirst = new KeyFrame();
  KeyFrame *pkSecond = new KeyFrame();
  *pkFirst = kF;
  *pkSecond = kS;
  
  
  pkFirst->bFixed = true;
  pkFirst->se3CfromW = SE3<>();
  
  pkSecond->bFixed = false;
  pkSecond->se3CfromW = se3;
  
  // Construct map from the stereo matches.
  PatchFinder finder;

  for(unsigned int i=0; i<vMatches.size(); i++)
    {
      MapPoint *p = new MapPoint();
      
      // Patch source stuff:
      p->pPatchSourceKF = pkFirst;
      p->nSourceLevel = traillevel;
      p->v3Normal_NC = makeVector( 0,0,-1);
      p->irCenter = vTrailMatches[i].first;
      p->v3Center_NC = unproject(mCamera.UnProject(p->irCenter));
      p->v3OneDownFromCenter_NC = unproject(mCamera.UnProject(p->irCenter + ImageRef(0,1)));
      p->v3OneRightFromCenter_NC = unproject(mCamera.UnProject(p->irCenter + ImageRef(1,0)));
      normalize(p->v3Center_NC);
      normalize(p->v3OneDownFromCenter_NC);
      normalize(p->v3OneRightFromCenter_NC);
      p->RefreshPixelVectors();

      // Do sub-pixel alignment on the second image
      finder.MakeTemplateCoarseNoWarp(*p);
      finder.MakeSubPixTemplate();
      finder.SetSubPixPos(LevelZeroPos(vTrailMatches[i].second,traillevel));
      bool bGood = finder.IterateSubPixToConvergence(*pkSecond,10);
      if(!bGood)
	{ 
	  delete p; continue;
	}
      
      // Triangulate point:
      Vector<2> v2SecondPos = finder.GetSubPixPos();
      p->v3WorldPos = ReprojectPoint(se3, mCamera.UnProject(v2SecondPos), vMatches[i].v2CamPlaneFirst);
      if(p->v3WorldPos[2] < 0.0)
       	{
 	  delete p; continue;
 	}
      
      // Not behind map? Good, then add to map.
      p->pMMData = new MapMakerData();
      mMap.vpPoints.push_back(p);


      
      // Construct first two measurements and insert into relevant DBs:
      Measurement mFirst;
      mFirst.nLevel = traillevel;
      mFirst.Source = Measurement::SRC_ROOT;
      mFirst.v2RootPos = LevelZeroPos(vTrailMatches[i].first,traillevel);
      mFirst.bSubPix = true;
      pkFirst->mMeasurements[p] = mFirst;
      p->pMMData->sMeasurementKFs.insert(pkFirst);
      
      Measurement mSecond;
      mSecond.nLevel = 0;
      mSecond.Source = Measurement::SRC_TRAIL;
      mSecond.v2RootPos = finder.GetSubPixPos();
      mSecond.bSubPix = true;
      pkSecond->mMeasurements[p] = mSecond;
      p->pMMData->sMeasurementKFs.insert(pkSecond);

    }

  mMap.vpKeyFrames.push_back(pkFirst);
  mMap.vpKeyFrames.push_back(pkSecond);
  pkFirst->MakeKeyFrame_Rest();
  pkSecond->MakeKeyFrame_Rest();
  
#ifdef __ANDROID__
  if(useSensorDataForInitialization)
  {
	  updatePositionOnly = true;
	  mbBundleConverged_Full = false;

	  for(int i=0; i<50 && !mbBundleConverged_Full; i++)
	  //while(!mMap.bBundleConverged_Full)
	  {
		BundleAdjustAll();
		if(mbResetRequested)
			return false;
	  }

	  __android_log_print(ANDROID_LOG_INFO, "first ba 1", "%f %f %f",pkFirst->se3CfromW.get_translation()[0],pkFirst->se3CfromW.get_translation()[1],pkFirst->se3CfromW.get_translation()[2]);
	  __android_log_print(ANDROID_LOG_INFO, "first ba 2", "%f %f %f",pkSecond->se3CfromW.get_translation()[0],pkSecond->se3CfromW.get_translation()[1],pkSecond->se3CfromW.get_translation()[2]);

	  //pkSecond->bFixed = true;
  }
#endif

  updatePositionOnly = false;

  for(int i=0; i<5; i++)
      BundleAdjustAll();

//#endif
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "s ba 1", "%f %f %f",pkFirst->se3CfromW.get_translation()[0],pkFirst->se3CfromW.get_translation()[1],pkFirst->se3CfromW.get_translation()[2]);
  __android_log_print(ANDROID_LOG_INFO, "s ba 2", "%f %f %f",pkSecond->se3CfromW.get_translation()[0],pkSecond->se3CfromW.get_translation()[1],pkSecond->se3CfromW.get_translation()[2]);
#endif


  updatePositionOnly = false;

  double dNewTransMagn = sqrt(pkSecond->se3CfromW.get_translation() * pkSecond->se3CfromW.get_translation());
  double rescale = mdWiggleScale/dNewTransMagn;

  pkSecond->se3CfromW.get_translation()*=rescale;

  for(unsigned int i=0; i<mMap.vpPoints.size(); i++)
  {
	  mMap.vpPoints[i]->v3WorldPos *= rescale;
  }

#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "rescale", "%f",rescale);
  __android_log_print(ANDROID_LOG_INFO, "kftest", "%f/%f", dNewTransMagn,mdWiggleScale );
#endif

  // Estimate the feature depth distribution in the first two key-frames
  // (Needed for epipolar search)
  RefreshSceneDepth(pkFirst);
  RefreshSceneDepth(pkSecond);
  mdWiggleScaleDepthNormalized = mdWiggleScale / pkFirst->dSceneDepthMean;


  AddSomeMapPoints(0);
  AddSomeMapPoints(3);
  AddSomeMapPoints(1);
  AddSomeMapPoints(2);
  
  mbBundleConverged_Full = false;
  mbBundleConverged_Recent = false;
  
  while(!mbBundleConverged_Full)
    {
      BundleAdjustAll();
      if(mbResetRequested)
    	  return false;
    }

  //update for need new key frame
  RefreshSceneDepth(pkFirst);
  RefreshSceneDepth(pkSecond);
  mdWiggleScaleDepthNormalized = mdWiggleScale / pkFirst->dSceneDepthMean;
  
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "final ba 1", "%f %f %f",pkFirst->se3CfromW.get_translation()[0],pkFirst->se3CfromW.get_translation()[1],pkFirst->se3CfromW.get_translation()[2]);
  __android_log_print(ANDROID_LOG_INFO, "final ba 2", "%f %f %f",pkSecond->se3CfromW.get_translation()[0],pkSecond->se3CfromW.get_translation()[1],pkSecond->se3CfromW.get_translation()[2]);
#endif

  // Rotate and translate the map so the dominant plane is at z=0:
  ApplyGlobalTransformationToMap(CalcPlaneAligner());
  mMap.bGood = true;
  se3TrackerPose = pkSecond->se3CfromW;

  cout << "WiggleScale:" << mdWiggleScale <<endl;
  cout << "Dist:" << KeyFrameLinearDist(*pkFirst,*pkSecond) <<endl;
  cout << "Tdist:" << KeyFrameLinearTargetDist(*pkFirst,*pkSecond) <<endl;

  mdFirstKeyFrameDist = KeyFrameLinearDist(*pkFirst,*pkSecond);
  
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "fdist", "%f",mdFirstKeyFrameDist);
  __android_log_print(ANDROID_LOG_INFO, "final ba gtm 1", "%f %f %f",pkFirst->se3CfromW.get_translation()[0],pkFirst->se3CfromW.get_translation()[1],pkFirst->se3CfromW.get_translation()[2]);
  __android_log_print(ANDROID_LOG_INFO, "final ba gtm 2", "%f %f %f",pkSecond->se3CfromW.get_translation()[0],pkSecond->se3CfromW.get_translation()[1],pkSecond->se3CfromW.get_translation()[2]);
#endif

  cout << "  MapMaker: made initial map with " << mMap.vpPoints.size() << " points." << endl;
  return true; 
}

// ThinCandidates() Thins out a key-frame's candidate list.
// Candidates are those salient corners where the mapmaker will attempt 
// to make a new map point by epipolar search. We don't want to make new points
// where there are already existing map points, this routine erases such candidates.
// Operates on a single level of a keyframe.
void MapMaker::ThinCandidates(KeyFrame &k, int nLevel)
{
  vector<Candidate> &vCSrc = k.aLevels[nLevel].vCandidates;
  vector<Candidate> vCGood;
  vector<ImageRef> irBusyLevelPos;
  // Make a list of `busy' image locations, which already have features at the same level
  // or at one level higher.
  for(meas_it it = k.mMeasurements.begin(); it!=k.mMeasurements.end(); it++)
    {
      if(!(it->second.nLevel == nLevel || it->second.nLevel == nLevel + 1))
	continue;
      irBusyLevelPos.push_back(ir_rounded(it->second.v2RootPos / LevelScale(nLevel)));
    }
  
  // Only keep those candidates further than 10 pixels away from busy positions.
  unsigned int nMinMagSquared = 7*7;//10*10;
  for(unsigned int i=0; i<vCSrc.size(); i++)
    {
      ImageRef irC = vCSrc[i].irLevelPos;
      bool bGood = true;
      for(unsigned int j=0; j<irBusyLevelPos.size(); j++)
	{
	  ImageRef irB = irBusyLevelPos[j];
	  if((irB - irC).mag_squared() < nMinMagSquared)
	    {
	      bGood = false;
	      break;
	    }
	}
      if(bGood)
		vCGood.push_back(vCSrc[i]);
    } 
  vCSrc = vCGood;
}

// Adds map points by epipolar search to the last-added key-frame, at a single
// specified pyramid level. Does epipolar search in the target keyframe as closest by
// the ClosestKeyFrame function.
void MapMaker::AddSomeMapPoints(int nLevel)
{
	/*if(nLevel==0) //hack avoid level 0 points as they are often bad when noise
		return;*/

  KeyFrame &kSrc = *(mMap.vpKeyFrames[mMap.vpKeyFrames.size() - 1]); // The new keyframe
  KeyFrame &kTarget = *(ClosestKeyFrame(kSrc));   
  KeyFrame &kTarget2 = *(ClosestKeyFrameAdvanced(kSrc));
  vector<KeyFrame*> ktargets = NClosestKeyFramesTarget(kSrc,3);
  Level &l = kSrc.aLevels[nLevel];

  ThinCandidates(kSrc, nLevel);

  vector<pair<double, KeyFrame* > > vKFandScores;
  for(unsigned int i=0; i<ktargets.size(); i++)
    { 
      double dDist = KeyFrameLinearDist(kSrc, *ktargets[i]);
      vKFandScores.push_back(make_pair(dDist, ktargets[i]));
    }
  sort(vKFandScores.begin(), vKFandScores.end());
  
  ktargets.clear();
  for(unsigned int i=0; i<vKFandScores.size(); i++)
    ktargets.push_back(vKFandScores[i].second);

  numTotalCandidates += l.vCandidates.size();
  
  for(unsigned int i = 0; i<l.vCandidates.size(); i++)
  {
    //AddPointEpipolar(kSrc, kTarget, nLevel, i);
	//if(&kTarget != &kTarget2)
	bool found = false;
	int j = ktargets.size()-1;
	while(!found && j>=0)
	{
		found = AddPointEpipolar(kSrc, *ktargets[j], nLevel, i);
		j--;
	}
	if(found)
		numAcceptedCandidates++;
  }
  
  RefreshSceneDepth(&kSrc);
  for(int i = 0; i < ktargets.size(); i++)
  {
	  RefreshSceneDepth(ktargets[i]);
  }
  //RefreshSceneDepth(&kTarget);
  //RefreshSceneDepth(&kTarget2);
};

// Rotates/translates the whole map and all keyframes
void MapMaker::ApplyGlobalTransformationToMap(SE3<> se3NewFromOld)
{
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    mMap.vpKeyFrames[i]->se3CfromW = mMap.vpKeyFrames[i]->se3CfromW * se3NewFromOld.inverse();
  
  SO3<> so3Rot = se3NewFromOld.get_rotation();
  for(unsigned int i=0; i<mMap.vpPoints.size(); i++)
    {
      mMap.vpPoints[i]->v3WorldPos =
	se3NewFromOld * mMap.vpPoints[i]->v3WorldPos;
      mMap.vpPoints[i]->RefreshPixelVectors();
    }
}

// Applies a global scale factor to the map
void MapMaker::ApplyGlobalScaleToMap(double dScale)
{
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    mMap.vpKeyFrames[i]->se3CfromW.get_translation() *= dScale;
  
  for(unsigned int i=0; i<mMap.vpPoints.size(); i++)
    {
      (*mMap.vpPoints[i]).v3WorldPos *= dScale;
      (*mMap.vpPoints[i]).v3PixelRight_W *= dScale;
      (*mMap.vpPoints[i]).v3PixelDown_W *= dScale;
      (*mMap.vpPoints[i]).RefreshPixelVectors();
    }
}

// The tracker entry point for adding a new keyframe;
// the tracker thread doesn't want to hang about, so
// just dumps it on the top of the mapmaker's queue to
// be dealt with later, and return.
void MapMaker::AddKeyFrame(KeyFrame &k, void* source)
{
  if(mbLockMap)
	  return;
  
  KeyFrame *pK = new KeyFrame();

  //TODO make better clone BUT BE SHURE TO MAKE A DEEP COPY!!!!!!!!
  pK->se3CfromW = k.se3CfromW;
  pK->bFixed = k.bFixed;
  pK->dSceneDepthMean = k.dSceneDepthMean;
  pK->dSceneDepthSigma = k.dSceneDepthSigma;
  pK->mMeasurements.insert(k.mMeasurements.begin(),k.mMeasurements.end());
  for(int i = 0; i < LEVELS; i++)
	  pK->aLevels[i] = k.aLevels[i];
  if(pK->pSBI != NULL)
  {
    delete pK->pSBI;
    pK->pSBI = NULL; // Mapmaker uses a different SBI than the tracker, so will re-gen its own
  }

  pthread_mutex_lock(&kflock);
  
  mvpKeyFrameQueue.push_back(pK);

  pthread_mutex_unlock(&kflock);
  
  if(mbBundleRunning)   // Tell the mapmaker to stop doing low-priority stuff and concentrate on this KF first.
    mbBundleAbortRequested = true;
}

/**
 * Mapmaker's code to handle incoming keyframes.
 */
void MapMaker::AddKeyFrameFromTopOfQueue()
{
  if(mvpKeyFrameQueue.size() == 0)
    return;
  

  pthread_mutex_lock(&kflock);

  KeyFrame *pK = mvpKeyFrameQueue[0];
  mvpKeyFrameQueue.erase(mvpKeyFrameQueue.begin());

  pthread_mutex_unlock(&kflock);

  pK->MakeKeyFrame_Rest();

  mMap.LockMap();
  mMap.vpKeyFrames.push_back(pK);
  mMap.UnlockMap();

  // Any measurements? Update the relevant point's measurement counter status map
  for(meas_it it = pK->mMeasurements.begin();
      it!=pK->mMeasurements.end();
      it++)
    {
      it->first->pMMData->sMeasurementKFs.insert(pK);
      it->second.Source = Measurement::SRC_TRACKER;
    }

  // And maybe we missed some - this now adds to the map itself, too.
  ReFindInSingleKeyFrame(*pK);

  numAcceptedCandidates = 0;
  numTotalCandidates = 0;
  
  AddSomeMapPoints(3);       // .. and add more map points by epipolar search.
  AddSomeMapPoints(0);
  AddSomeMapPoints(1);
  AddSomeMapPoints(2);

  double pct = ((double)numAcceptedCandidates)/numTotalCandidates;

  cout << "Num accepted new: " << pct << endl;

  /*if(pct < 0.5)
  {
	  RemoveKeyFrame(pK);
	  cout << "KF REMOVED!!!" << endl;
  }*/
  
  mbBundleConverged_Full = false;
  mbBundleConverged_Recent = false;
}

/*void MapMaker::RemoveKeyFrame(KeyFrame *pK)
{
	for(meas_it it = pK->mMeasurements.begin();
      it!=pK->mMeasurements.end();
      it++)
    {
		it->first->pMMData->sMeasurementKFs.erase(pK);//.insert(pK);
		if(it->first->pMMData->sMeasurementKFs.size()==1)
			it->first->bBad = true;
    }

	mMap.LockMap();
	mMap.vpKeyFrames.erase(std::find(mMap.vpKeyFrames.begin(),mMap.vpKeyFrames.end(),pK)); //todo check exist
	mMap.UnlockMap();

	//todo delete kf or add to some vector
}*/


// Tries to make a new map point out of a single candidate point
// by searching for that point in another keyframe, and triangulating
// if a match is found.
bool MapMaker::AddPointEpipolar(KeyFrame &kSrc, 
				KeyFrame &kTarget, 
				int nLevel,
				int nCandidate)
{
   static Image<Vector<2> > imUnProj;
   static bool bMadeCache = false;
   if(!bMadeCache)
     {
       imUnProj.resize(kSrc.aLevels[0].im.size());
       ImageRef ir;
       do imUnProj[ir] = mCamera.UnProject(ir);
       while(ir.next(imUnProj.size()));
       bMadeCache = true;
     }
  
  int nLevelScale = LevelScale(nLevel);
  Candidate &candidate = kSrc.aLevels[nLevel].vCandidates[nCandidate];
  ImageRef irLevelPos = candidate.irLevelPos;
  Vector<2> v2RootPos = LevelZeroPos(irLevelPos, nLevel);
  
  Vector<3> v3Ray_SC = unproject(mCamera.UnProject(v2RootPos));
  normalize(v3Ray_SC);
  Vector<3> v3LineDirn_TC = kTarget.se3CfromW.get_rotation() * (kSrc.se3CfromW.get_rotation().inverse() * v3Ray_SC);

  // Restrict epipolar search to a relatively narrow depth range
  // to increase reliability
  double dMean = kSrc.dSceneDepthMean;
  double dSigma = kSrc.dSceneDepthSigma;
  //double dStartDepth = max(mdWiggleScale, dMean - dSigma);
  //double dEndDepth = min(40 * mdWiggleScale, dMean + dSigma);
  double dStartDepth = mdWiggleScale;
  double dEndDepth = 4000 * mdWiggleScale;
  
  Vector<3> v3CamCenter_TC = kTarget.se3CfromW * kSrc.se3CfromW.inverse().get_translation(); // The camera end
  Vector<3> v3RayStart_TC = v3CamCenter_TC + dStartDepth * v3LineDirn_TC;                               // the far-away end
  Vector<3> v3RayEnd_TC = v3CamCenter_TC + dEndDepth * v3LineDirn_TC;                               // the far-away end

  
  if(v3RayEnd_TC[2] <= v3RayStart_TC[2])  // it's highly unlikely that we'll manage to get anything out if we're facing backwards wrt the other camera's view-ray
    return false;
  if(v3RayEnd_TC[2] <= 0.0 )  return false;
  if(v3RayStart_TC[2] <= 0.0)
    v3RayStart_TC += v3LineDirn_TC * (0.001 - v3RayStart_TC[2] / v3LineDirn_TC[2]);
  
  Vector<2> v2A = project(v3RayStart_TC);
  Vector<2> v2B = project(v3RayEnd_TC);
  Vector<2> v2AlongProjectedLine = v2A-v2B;
  
  if( (v2AlongProjectedLine * v2AlongProjectedLine) < 0.00000001)
  {
    cout << "v2AlongProjectedLine too small." << endl;
    return false;
  }
  
  normalize(v2AlongProjectedLine);
  Vector<2> v2Normal;
  v2Normal[0] = v2AlongProjectedLine[1];
  v2Normal[1] = -v2AlongProjectedLine[0];
  
  double dNormDist = v2A * v2Normal;
  if(fabs(dNormDist) > mCamera.LargestRadiusInImage() )
    return false;
  
  double dMinLen = min(v2AlongProjectedLine * v2A, v2AlongProjectedLine * v2B) - 0.05;
  double dMaxLen = max(v2AlongProjectedLine * v2A, v2AlongProjectedLine * v2B) + 0.05;
  if(dMinLen < -2.0)  dMinLen = -2.0;
  if(dMaxLen < -2.0)  dMaxLen = -2.0;
  if(dMinLen > 2.0)   dMinLen = 2.0;
  if(dMaxLen > 2.0)   dMaxLen = 2.0;

  // Find current-frame corners which might match this
  PatchFinder Finder;
  Finder.MakeTemplateCoarseNoWarp(kSrc, nLevel, irLevelPos);
  if(Finder.TemplateBad())  return false;
  
  vector<Vector<2> > &vv2Corners = kTarget.aLevels[nLevel].vImplaneCorners;
  vector<ImageRef> &vIR = kTarget.aLevels[nLevel].vCorners;
  if(!kTarget.aLevels[nLevel].bImplaneCornersCached)
    {
      for(unsigned int i=0; i<vIR.size(); i++)   // over all corners in target img..
	//vv2Corners.push_back(kTarget.Camera.UnProject(ir(LevelZeroPos(vIR[i], nLevel)))); //TODOREMOVE
	vv2Corners.push_back(imUnProj[ir(LevelZeroPos(vIR[i], nLevel))]);
      kTarget.aLevels[nLevel].bImplaneCornersCached = true;
    }
  
  int nBest = -1;
#ifdef COMPUTE_NCC
  float nBestNCC = NCC_T;
#else
  int nBestZMSSD = Finder.mnMaxSSD + 1;
#endif
  double dMaxDistDiff = mCamera.OnePixelDist() * (4.0 + 1.0 * nLevelScale);
  double dMaxDistSq = dMaxDistDiff * dMaxDistDiff;
  
  for(unsigned int i=0; i<vv2Corners.size(); i++)   // over all corners in target img..
    {
      Vector<2> v2Im = vv2Corners[i];
      double dDistDiff = dNormDist - v2Im * v2Normal;
      if( (dDistDiff * dDistDiff) > dMaxDistSq)       continue; // skip if not along epi line
      if( (v2Im * v2AlongProjectedLine) < dMinLen)    continue; // skip if not far enough along line
      if( (v2Im * v2AlongProjectedLine) > dMaxLen)    continue; // or too far
#ifdef COMPUTE_NCC
	  float nNCC = Finder.NCCAtPoint(kTarget.aLevels[nLevel].im, vIR[i]);
      if(nNCC > nBestNCC)
	{
	  nBest = i;
	  nBestNCC = nNCC;
	}
#else
      int nZMSSD = Finder.ZMSSDAtPoint(kTarget.aLevels[nLevel].im, vIR[i]);
      if(nZMSSD < nBestZMSSD)
	{
	  nBest = i;
	  nBestZMSSD = nZMSSD;
	}
#endif
    } 
  
  if(nBest == -1)   return false;   // Nothing found.
  
  //  Found a likely candidate along epipolar ray
  Finder.MakeSubPixTemplate();
  Finder.SetSubPixPos(LevelZeroPos(vIR[nBest], nLevel));
  bool bSubPixConverges = Finder.IterateSubPixToConvergence(kTarget,10);
  if(!bSubPixConverges)
    return false;

  //find patch in 0 level for better accuracy?
  /*if(nLevel > 0)
  {
	  PatchFinder FinderDetail;

	  CVD::ImageRef detailPos(v2RootPos[0],v2RootPos[1]);
	  FinderDetail.MakeTemplateCoarseNoWarp(kSrc, 0, detailPos);

	  Vector<2> v2TargetRootPos = Finder.GetSubPixPos();
	  CVD::ImageRef detailTargetPos(v2TargetRootPos[0]+0.5,v2TargetRootPos[1]+0.5);
	  if(!FinderDetail.FindPatchCoarse(detailTargetPos,kTarget,4))
		  return false;

	  FinderDetail.MakeSubPixTemplate();
	  //FinderDetail.SetSubPixPos(LevelZeroPos(vIR[nBest], nLevel));
	  bSubPixConverges = FinderDetail.IterateSubPixToConvergence(kTarget,10);
	  if(!bSubPixConverges)
		return false;

	  Finder.SetSubPixPos(FinderDetail.GetSubPixPos());
  }*/
  
  // Now triangulate the 3d point...
  Vector<3> v3New;
  v3New = kTarget.se3CfromW.inverse() *  
    ReprojectPoint(kSrc.se3CfromW * kTarget.se3CfromW.inverse(),
		   mCamera.UnProject(v2RootPos),
		   mCamera.UnProject(Finder.GetSubPixPos()));
  
  MapPoint *pNew = new MapPoint;
  pNew->v3WorldPos = v3New;
  pNew->pMMData = new MapMakerData();
  
  // Patch source stuff:
  pNew->pPatchSourceKF = &kSrc;
  pNew->nSourceLevel = nLevel;
  pNew->v3Normal_NC = makeVector( 0,0,-1);
  pNew->irCenter = irLevelPos;
  pNew->v3Center_NC = unproject(mCamera.UnProject(v2RootPos));
  pNew->v3OneRightFromCenter_NC = unproject(mCamera.UnProject(v2RootPos + vec(ImageRef(nLevelScale,0))));
  pNew->v3OneDownFromCenter_NC  = unproject(mCamera.UnProject(v2RootPos + vec(ImageRef(0,nLevelScale))));
  
  normalize(pNew->v3Center_NC);
  normalize(pNew->v3OneDownFromCenter_NC);
  normalize(pNew->v3OneRightFromCenter_NC);
  
  pNew->RefreshPixelVectors();
    
  mMap.LockMap();
  mMap.vpPoints.push_back(pNew);
  mMap.UnlockMap();



  mqNewQueue.push(pNew);
  Measurement m;
  m.Source = Measurement::SRC_ROOT;
  m.v2RootPos = v2RootPos;
  m.nLevel = nLevel;
  m.bSubPix = true;
  kSrc.mMeasurements[pNew] = m;

  m.Source = Measurement::SRC_EPIPOLAR;
  m.v2RootPos = Finder.GetSubPixPos();
  kTarget.mMeasurements[pNew] = m;
  
  pNew->pMMData->sMeasurementKFs.insert(&kSrc);
  pNew->pMMData->sMeasurementKFs.insert(&kTarget);
  
  return true;
}


double MapMaker::KeyFrameLinearDist(KeyFrame &k1, KeyFrame &k2)
{
  Vector<3> v3KF1_CamPos = k1.se3CfromW.inverse().get_translation();
  Vector<3> v3KF2_CamPos = k2.se3CfromW.inverse().get_translation();
  Vector<3> v3Diff = v3KF2_CamPos - v3KF1_CamPos;
  double dDist = sqrt(v3Diff * v3Diff);
  return dDist;
}

vector<KeyFrame*> MapMaker::NClosestKeyFrames(KeyFrame &k, unsigned int N)
{
  vector<pair<double, KeyFrame* > > vKFandScores;
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    {
      if(mMap.vpKeyFrames[i] == &k)
	continue;
      double dDist = KeyFrameLinearDist(k, *mMap.vpKeyFrames[i]);
      vKFandScores.push_back(make_pair(dDist, mMap.vpKeyFrames[i]));
    }
  if(N > vKFandScores.size())
    N = vKFandScores.size();
  partial_sort(vKFandScores.begin(), vKFandScores.begin() + N, vKFandScores.end());
  
  vector<KeyFrame*> vResult;
  for(unsigned int i=0; i<N; i++)
    vResult.push_back(vKFandScores[i].second);
  return vResult;
}

vector<KeyFrame*> MapMaker::NClosestKeyFramesTarget(KeyFrame &k, unsigned int N)
{
  vector<pair<double, KeyFrame* > > vKFandScores;
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    {
      if(mMap.vpKeyFrames[i] == &k)
	continue;
      //double dDist = KeyFrameLinearDist(k, *mMap.vpKeyFrames[i]);
	  double dDist = KeyFrameLinearTargetDist(k, *mMap.vpKeyFrames[i]);
      vKFandScores.push_back(make_pair(dDist, mMap.vpKeyFrames[i]));
    }
  if(N > vKFandScores.size())
    N = vKFandScores.size();
  partial_sort(vKFandScores.begin(), vKFandScores.begin() + N, vKFandScores.end());
  
  vector<KeyFrame*> vResult;
  for(unsigned int i=0; i<N; i++)
    vResult.push_back(vKFandScores[i].second);
  return vResult;
}

KeyFrame* MapMaker::ClosestKeyFrame(KeyFrame &k)
{
  double dClosestDist = 9999999999.9;
  int nClosest = -1;
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    {
      if(mMap.vpKeyFrames[i] == &k)
		continue;

      double dDist = KeyFrameLinearDist(k, *mMap.vpKeyFrames[i]);

      if(dDist < dClosestDist)
	{
	  dClosestDist = dDist;
	  nClosest = i;
	}
    }
  assert(nClosest != -1);
  return mMap.vpKeyFrames[nClosest];
}

double MapMaker::KeyFrameLinearTargetDist(KeyFrame &k1, KeyFrame &k2)
{
  Vector<3> v3KF1_CamTPos = k1.se3CfromW.inverse()*makeVector(0,0,k1.dSceneDepthMean);
  Vector<3> v3KF2_CamTPos = k2.se3CfromW.inverse()*makeVector(0,0,k2.dSceneDepthMean);
  Vector<3> v3Diff = v3KF2_CamTPos - v3KF1_CamTPos;
  double dDist = sqrt(v3Diff * v3Diff);
  return dDist;
}

KeyFrame* MapMaker::ClosestKeyFrameAdvanced(KeyFrame &k, double minDist)
{
  double dClosestDist = 9999999999.9;
  double dClosestDistMinDist = 9999999999.9;
  int nClosest = -1;
  int nClosestMinDist = -1;
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    {
      if(mMap.vpKeyFrames[i] == &k)
		continue;
	  
      double dDist = KeyFrameLinearDist(k, *mMap.vpKeyFrames[i]);
	  double dTDist = KeyFrameLinearTargetDist(k, *mMap.vpKeyFrames[i]);

      if(dTDist < dClosestDist)
	{
	  dClosestDist = dDist;
	  nClosest = i;
	}
	  if(dTDist < dClosestDistMinDist && dDist > minDist)
	{
	  dClosestDistMinDist = dDist;
	  nClosestMinDist = i;
	}
    }
  assert(nClosest != -1);
  if(nClosestMinDist!=-1)
	  nClosest = nClosestMinDist;
  return mMap.vpKeyFrames[nClosest];
}

double MapMaker::DistToNearestKeyFrame(KeyFrame &kCurrent)
{
  KeyFrame *pClosest = ClosestKeyFrame(kCurrent);
  double dDist = KeyFrameLinearDist(kCurrent, *pClosest);
  return dDist;
}

bool MapMaker::NeedNewKeyFrame(KeyFrame &kCurrent)
{
  if(mbLockMap)
	 return false;

	mMap.LockMap();
  KeyFrame *pClosest = ClosestKeyFrame(kCurrent);
  mMap.UnlockMap();
  double dDist = KeyFrameLinearDist(kCurrent, *pClosest);
  //dDist *= (1.0 / kCurrent.dSceneDepthMean);
  
  //__android_log_print(ANDROID_LOG_INFO, "kftest", "%f/%f %f", dDist,GV2.GetDouble("MapMaker.MaxKFDistWiggleMult",1.0,SILENT),mdWiggleScale );

  if(dDist > GV2.GetDouble("MapMaker.MaxKFDistWiggleMult",0.2,SILENT))// * mdWiggleScaleDepthNormalized)
    return true;

  //target criterion
  mMap.LockMap();
  vector<KeyFrame*> ktargets = NClosestKeyFramesTarget(kCurrent,3);
  mMap.UnlockMap();

  vector<pair<double, KeyFrame* > > vKFandScores;
  for(unsigned int i=0; i<ktargets.size(); i++)
    { 
      double dDist = KeyFrameLinearDist(kCurrent, *ktargets[i]);
      vKFandScores.push_back(make_pair(dDist, ktargets[i]));
    }
  sort(vKFandScores.begin(), vKFandScores.end());
  
  ktargets.clear();
  for(unsigned int i=0; i<vKFandScores.size(); i++)
    ktargets.push_back(vKFandScores[i].second);

  pClosest = ktargets[0];

  dDist = KeyFrameLinearDist(kCurrent, *pClosest);
  //dDist *= (1.0 / kCurrent.dSceneDepthMean);

  double dTDist = KeyFrameLinearTargetDist(kCurrent, *pClosest);
  dTDist *= (1.0 / kCurrent.dSceneDepthMean);

  if(dTDist > 0.1 && dDist > GV2.GetDouble("MapMaker.MaxKFDistWiggleMult",0.2,SILENT))// * mdWiggleScaleDepthNormalized)
  {
	 //cout << "d:" << dDist;
	 return true;
	
  }

  return false;
}

// Perform bundle adjustment on all keyframes, all map points
void MapMaker::BundleAdjustAll()
{
	TIMER_INIT
	TIMER_START
  // construct the sets of kfs/points to be adjusted:
  // in this case, all of them
  unordered_set<KeyFrame*> sAdj;
  unordered_set<KeyFrame*> sFixed;
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    if(mMap.vpKeyFrames[i]->bFixed)
      sFixed.insert(mMap.vpKeyFrames[i]);
    else
      sAdj.insert(mMap.vpKeyFrames[i]);
  
  unordered_set<MapPoint*> sMapPoints;
  for(unsigned int i=0; i<mMap.vpPoints.size();i++)
    sMapPoints.insert(mMap.vpPoints[i]);
  
  TIMER_STOP("Prepare BundleAdjustAll")
  BundleAdjust(sAdj, sFixed, sMapPoints, false);
}

// Peform a local bundle adjustment which only adjusts
// recently added key-frames
void MapMaker::BundleAdjustRecent()
{
  if(mMap.vpKeyFrames.size() < 8)
    { // Ignore this unless map is big enough
      mbBundleConverged_Recent = true;
      return;
    }

  // First, make a list of the keyframes we want adjusted in the adjuster.
  // This will be the last keyframe inserted, and its four nearest neighbors
  unordered_set<KeyFrame*> sAdjustSet;
  KeyFrame *pkfNewest = mMap.vpKeyFrames.back();
  sAdjustSet.insert(pkfNewest);
  vector<KeyFrame*> vClosest = NClosestKeyFrames(*pkfNewest, 4);

  vector<KeyFrame*> vClosest2 = NClosestKeyFramesTarget(*pkfNewest, 4);
  vClosest.insert(vClosest.end(),vClosest2.begin(),vClosest2.end());

  sort( vClosest.begin(), vClosest.end() );
  vClosest.erase( unique( vClosest.begin(), vClosest.end() ), vClosest.end() );

  for(int i=0; i<vClosest.size(); i++)
    if(vClosest[i]->bFixed == false)
      sAdjustSet.insert(vClosest[i]);
  
  // Now we find the set of features which they contain.
  unordered_set<MapPoint*> sMapPoints;
  for(unordered_set<KeyFrame*>::iterator iter = sAdjustSet.begin();
      iter!=sAdjustSet.end();
      iter++)
    {
      map<MapPoint*,Measurement> &mKFMeas = (*iter)->mMeasurements;
      for(meas_it jiter = mKFMeas.begin(); jiter!= mKFMeas.end(); jiter++)
	sMapPoints.insert(jiter->first);
    };
  
  // Finally, add all keyframes which measure above points as fixed keyframes
  unordered_set<KeyFrame*> sFixedSet;
  for(vector<KeyFrame*>::iterator it = mMap.vpKeyFrames.begin(); it!=mMap.vpKeyFrames.end(); it++)
    {
      if(sAdjustSet.count(*it))
	continue;
      bool bInclude = false;
      for(meas_it jiter = (*it)->mMeasurements.begin(); jiter!= (*it)->mMeasurements.end(); jiter++)
	if(sMapPoints.count(jiter->first))
	  {
	    bInclude = true;
	    break;
	  }
      if(bInclude)
	sFixedSet.insert(*it);
    }
  BundleAdjust(sAdjustSet, sFixedSet, sMapPoints, true);
}


// Common bundle adjustment code. This creates a bundle-adjust instance, populates it, and runs it.
void MapMaker::BundleAdjust(unordered_set<KeyFrame*> sAdjustSet, unordered_set<KeyFrame*> sFixedSet, unordered_set<MapPoint*> sMapPoints, bool bRecent)
{
	TIMER_INIT
	TIMER_START
  Bundle b(mCamera);   // Our bundle adjuster
	if(updatePositionOnly)
		b.updatePositionOnly = true;

  mbBundleRunning = true;
  mbBundleRunningIsRecent = bRecent;
  
  // The bundle adjuster does different accounting of keyframes and map points;
  // Translation maps are stored:
  //map<MapPoint*, int> mPoint_BundleID;
  unordered_map<MapPoint*, int> mPoint_BundleID;
  //map<int, MapPoint*> mBundleID_Point;
  vector<MapPoint*> mBundleID_Point;
  //map<KeyFrame*, int> mView_BundleID;
  unordered_map<KeyFrame*, int> mView_BundleID;
  //map<int, KeyFrame*> mBundleID_View;
  vector<KeyFrame*> mBundleID_View;

  mBundleID_View.resize(sAdjustSet.size()+sFixedSet.size());
  mBundleID_Point.resize(sMapPoints.size());
  
  // Add the keyframes' poses to the bundle adjuster. Two parts: first nonfixed, then fixed.
  for(unordered_set<KeyFrame*>::iterator it = sAdjustSet.begin(); it!= sAdjustSet.end(); it++)
    {
      int nBundleID = b.AddCamera((*it)->se3CfromW, (*it)->bFixed);
      mView_BundleID[*it] = nBundleID;
      mBundleID_View[nBundleID] = *it;
    }
  for(unordered_set<KeyFrame*>::iterator it = sFixedSet.begin(); it!= sFixedSet.end(); it++)
    {
      int nBundleID = b.AddCamera((*it)->se3CfromW, true);
      mView_BundleID[*it] = nBundleID;
      mBundleID_View[nBundleID] = *it;
    }
  TIMER_STOP("Bundle prepare0");
	//TIMER_START
	//b.AllocatePoints(sMapPoints.size());
	//TIMER_STOP("Bundle prepare1.1");
	TIMER_START
  // Add the points' 3D position
  for(unordered_set<MapPoint*>::iterator it = sMapPoints.begin(); it!=sMapPoints.end(); it++)
    {
      int nBundleID = b.AddPoint((*it)->v3WorldPos);
      mPoint_BundleID[*it] = nBundleID;
      mBundleID_Point[nBundleID] = *it;
    }
  TIMER_STOP("Bundle prepare1");
  
	TIMER_START
  
  // Add the relevant point-in-keyframe measurements
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    {
      if(mView_BundleID.count(mMap.vpKeyFrames[i]) == 0)
	continue;
      
      int nKF_BundleID = mView_BundleID[mMap.vpKeyFrames[i]];

      for(meas_it it= mMap.vpKeyFrames[i]->mMeasurements.begin();
	  it!= mMap.vpKeyFrames[i]->mMeasurements.end();
	  it++)
	{
	  if(mPoint_BundleID.count(it->first) == 0)
	    continue;
	  int nPoint_BundleID = mPoint_BundleID[it->first];

	  //b.AddMeas(nKF_BundleID, nPoint_BundleID, it->second.v2ImplanePos, LevelScale(it->second.nLevel) * LevelScale(it->second.nLevel), it->second.m2CamDerivs);
	  //TODO check pow!!!!
	  //b.AddMeas(nKF_BundleID, nPoint_BundleID, it->second.v2ImplanePos, 1/LevelScale(it->second.nLevel)*pow(1.1,min(5,(int)(it->first->pMMData->sMeasurementKFs.size()))), it->second.m2CamDerivs);
	  b.AddMeas(nKF_BundleID, nPoint_BundleID, it->second.v2RootPos, LevelScale(it->second.nLevel) * LevelScale(it->second.nLevel));

	}
    }
  
   TIMER_STOP("Bundle prepare2");

   /*TIMER_START
  
  // Add the relevant point-in-keyframe measurements
  for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
    {
      if(mView_BundleID.count(mMap.vpKeyFrames[i]) == 0)
	continue;
      
      int nKF_BundleID = mView_BundleID[mMap.vpKeyFrames[i]];

      for(meas_it it= mMap.vpKeyFrames[i]->mMeasurements.begin();
	  it!= mMap.vpKeyFrames[i]->mMeasurements.end();
	  it++)
	{
	  if(mPoint_BundleID.count(it->first) == 0)
	    continue;
	  int nPoint_BundleID = mPoint_BundleID[it->first];

	  b.AddMeasTest(nKF_BundleID, nPoint_BundleID, it->second.v2ImplanePos, LevelScale(it->second.nLevel) * LevelScale(it->second.nLevel), it->second.m2CamDerivs);
	}
    }
  
   TIMER_STOP("Bundle prepare2 test");*/
	TIMER_START
  // Run the bundle adjuster. This returns the number of successful iterations
  int nAccepted = b.Compute(&mbBundleAbortRequested);
  TIMER_STOP("Bundle");
  TIMER_START
  
  if(nAccepted < 0)
    {
      // Crap: - LM Ran into a serious problem!
      // This is probably because the initial stereo was messed up.
      // Get rid of this map and start again! 
      cout << "!! MapMaker: Cholesky failure in bundle adjust. " << endl
	   << "   The map is probably corrupt: Ditching the map. " << endl;
      mbResetRequested = true;
      return;
    }

  // Bundle adjustment did some updates, apply these to the map
  if(nAccepted > 0)
    {
      
      for(unordered_map<MapPoint*,int>::iterator itr = mPoint_BundleID.begin();
	  itr!=mPoint_BundleID.end();
	  itr++)
	  {
		itr->first->v3WorldPos = b.GetPoint(itr->second);

	  }
      
      for(unordered_map<KeyFrame*,int>::iterator itr = mView_BundleID.begin();
	  itr!=mView_BundleID.end();
	  itr++)
	  {
		itr->first->se3CfromW = b.GetCamera(itr->second);


	  }

      if(bRecent)
        mbBundleConverged_Recent = false;
      mbBundleConverged_Full = false;
    };
  
  if(b.Converged())
    {
      mbBundleConverged_Recent = true;
      if(!bRecent)
        mbBundleConverged_Full = true;
    }
  
  mbBundleRunning = false;
  mbBundleAbortRequested = false;
  
  // Handle outlier measurements:
  vector<pair<int,int> > vOutliers_PC_pair = b.GetOutlierMeasurements();
  for(unsigned int i=0; i<vOutliers_PC_pair.size(); i++)
    {
      MapPoint *pp = mBundleID_Point[vOutliers_PC_pair[i].first];
      KeyFrame *pk = mBundleID_View[vOutliers_PC_pair[i].second];
      Measurement &m = pk->mMeasurements[pp];
      if(pp->pMMData->GoodMeasCount() <= 2 || m.Source == Measurement::SRC_ROOT)   // Is the original source kf considered an outlier? That's bad.
	pp->bBad = true;
      else
	{
	  // Do we retry it? Depends where it came from!!
	  if(m.Source == Measurement::SRC_TRACKER || m.Source == Measurement::SRC_EPIPOLAR)
             mvFailureQueue.push_back(pair<KeyFrame*,MapPoint*>(pk,pp));
	  else
	    pp->pMMData->sNeverRetryKFs.insert(pk);
	  pk->mMeasurements.erase(pp);
	  pp->pMMData->sMeasurementKFs.erase(pk);
	}
    }
  TIMER_STOP("Bundle postprocess");
}

// Mapmaker's try-to-find-a-point-in-a-keyframe code. This is used to update
// data association if a bad measurement was detected, or if a point
// was never searched for in a keyframe in the first place. This operates
// much like the tracker! So most of the code looks just like in 
// TrackerData.h.
bool MapMaker::ReFind_Common(KeyFrame &k, MapPoint &p)
{
  // abort if either a measurement is already in the map, or we've
  // decided that this point-kf combo is beyond redemption

  if(p.pMMData->sMeasurementKFs.count(&k))
	  return true;
  if(p.pMMData->sNeverRetryKFs.count(&k))
    return false;
  
  static PatchFinder Finder;
  Vector<3> v3Cam = k.se3CfromW*p.v3WorldPos;
  if(v3Cam[2] < 0.001)
    {
      p.pMMData->sNeverRetryKFs.insert(&k);
      return false;
    }
  Vector<2> v2ImPlane = project(v3Cam);
  if( (v2ImPlane* v2ImPlane) > (mCamera.LargestRadiusInImage() * mCamera.LargestRadiusInImage()) )
    {
      p.pMMData->sNeverRetryKFs.insert(&k);
      return false;
    }
  
  Vector<2> v2Image = mCamera.Project(v2ImPlane);
  if(mCamera.Invalid())
    {
      p.pMMData->sNeverRetryKFs.insert(&k);
      return false;
    }

  ImageRef irImageSize = k.aLevels[0].im.size();
  if(v2Image[0] < 0 || v2Image[1] < 0 || v2Image[0] > irImageSize[0] || v2Image[1] > irImageSize[1])
    {
      p.pMMData->sNeverRetryKFs.insert(&k);
      return false;
    }
  
  Matrix<2> m2CamDerivs = mCamera.GetProjectionDerivs();
  Finder.MakeTemplateCoarse(p, k.se3CfromW, m2CamDerivs);
  
  if(Finder.TemplateBad())
    {
      p.pMMData->sNeverRetryKFs.insert(&k);
      return false;
    }
  
  bool bFound = Finder.FindPatchCoarse(ir(v2Image), k, 4);  // Very tight search radius!
  //bool bFound = Finder.FindPatchCoarse(ir(v2Image), k, 20);  // Very tight search radius!
  if(!bFound)
    {
      p.pMMData->sNeverRetryKFs.insert(&k);
      return false;
    }
  
  // If we found something, generate a measurement struct and put it in the map
  Measurement m;
  m.nLevel = Finder.GetLevel();
  m.Source = Measurement::SRC_REFIND;
  
  if(Finder.GetLevel() > 0)
    {
      Finder.MakeSubPixTemplate();
      Finder.IterateSubPixToConvergence(k,8);
      m.v2RootPos = Finder.GetSubPixPos();
      m.bSubPix = true;
    }
  else
    {
      m.v2RootPos = Finder.GetCoarsePosAsVector();
      m.bSubPix = false;
    };
    
  if(k.mMeasurements.count(&p))
    {
      assert(0); // This should never happen, we checked for this at the start.
    }
  k.mMeasurements[&p] = m;
  p.pMMData->sMeasurementKFs.insert(&k);

  return true;
}

// A general data-association update for a single keyframe
// Do this on a new key-frame when it's passed in by the tracker
int MapMaker::ReFindInSingleKeyFrame(KeyFrame &k)
{
  vector<MapPoint*> vToFind;
  for(unsigned int i=0; i<mMap.vpPoints.size(); i++)
    vToFind.push_back(mMap.vpPoints[i]);
  
  int nFoundNow = 0;
  for(unsigned int i=0; i<vToFind.size(); i++)
    if(ReFind_Common(k,*vToFind[i]))
      nFoundNow++;

  return nFoundNow;
};

// When new map points are generated, they're only created from a stereo pair
// this tries to make additional measurements in other KFs which they might
// be in.
void MapMaker::ReFindNewlyMade()
{
  if(mqNewQueue.empty())
    return;
  int nFound = 0;
  int nBad = 0;
  set<KeyFrame*> updateDepthKF;
  while(!mqNewQueue.empty() && mvpKeyFrameQueue.size() == 0)
    {
		MapPoint* pNew = mqNewQueue.front();
		mqNewQueue.pop();
		if(pNew->bBad)
		{
			nBad++;
			continue;
		}
		for(unsigned int i=0; i<mMap.vpKeyFrames.size(); i++)
			if(ReFind_Common(*mMap.vpKeyFrames[i], *pNew))
			{
				nFound++;
				updateDepthKF.insert(mMap.vpKeyFrames[i]); //TODO: rethink this!!! update when removed??
			}
    }

  //TODO: ask for new recent bundle adjustment??

  set<KeyFrame*>::iterator it;
  for (it=updateDepthKF.begin(); it!=updateDepthKF.end(); ++it)
    RefreshSceneDepth(*it);
};

// Dud measurements get a second chance.
void MapMaker::ReFindFromFailureQueue()
{
  if(mvFailureQueue.size() == 0)
    return;
  sort(mvFailureQueue.begin(), mvFailureQueue.end());
  vector<pair<KeyFrame*, MapPoint*> >::iterator it;
  int nFound=0;
  for(it = mvFailureQueue.begin(); it!=mvFailureQueue.end(); it++)
    if(ReFind_Common(*it->first, *it->second))
      nFound++;
  
  mvFailureQueue.erase(mvFailureQueue.begin(), it);
};

// Is the tracker's camera pose in cloud-cuckoo land?
bool MapMaker::IsDistanceToNearestKeyFrameExcessive(KeyFrame &kCurrent)
{
	mMap.LockMap();
	double ndist = DistToNearestKeyFrame(kCurrent);
	mMap.UnlockMap();
	//if (ndist > mdWiggleScale * 10.0)
	//	cout << "Excessive: " << ndist << ">" << mdWiggleScale;
  return ndist > mdWiggleScale * 10.0;
}

// Find a dominant plane in the map, find an SE3<> to put it as the z=0 plane
SE3<> MapMaker::CalcPlaneAligner()
{
  unsigned int nPoints = mMap.vpPoints.size();
  if(nPoints < 10)
    {
      cout << "  MapMaker: CalcPlane: too few points to calc plane." << endl;
      return SE3<>();
    };
  
  int nRansacs = GV2.GetInt("MapMaker.PlaneAlignerRansacs", 100, HIDDEN|SILENT);
  Vector<3> v3BestMean;
  Vector<3> v3BestNormal;
  double dBestDistSquared = 9999999999999999.9;
  
  for(int i=0; i<nRansacs; i++)
    {
      int nA = rand()%nPoints;
      int nB = nA;
      int nC = nA;
      while(nB == nA)
	nB = rand()%nPoints;
      while(nC == nA || nC==nB)
	nC = rand()%nPoints;
      
      Vector<3> v3Mean = 0.33333333 * (mMap.vpPoints[nA]->v3WorldPos +
				       mMap.vpPoints[nB]->v3WorldPos +
				       mMap.vpPoints[nC]->v3WorldPos);
      
      Vector<3> v3CA = mMap.vpPoints[nC]->v3WorldPos  - mMap.vpPoints[nA]->v3WorldPos;
      Vector<3> v3BA = mMap.vpPoints[nB]->v3WorldPos  - mMap.vpPoints[nA]->v3WorldPos;
      Vector<3> v3Normal = v3CA ^ v3BA;
      if( (v3Normal * v3Normal) == 0 )
	continue;
      normalize(v3Normal);
      
      double dSumError = 0.0;
      for(unsigned int i=0; i<nPoints; i++)
	{
	  Vector<3> v3Diff = mMap.vpPoints[i]->v3WorldPos - v3Mean;
	  double dDistSq = v3Diff * v3Diff;
	  if(dDistSq == 0.0)
	    continue;
	  double dNormDist = fabs(v3Diff * v3Normal);
	  
	  if(dNormDist > 0.05)
	    dNormDist = 0.05;
	  dSumError += dNormDist;
	}
      if(dSumError < dBestDistSquared)
	{
	  dBestDistSquared = dSumError;
	  v3BestMean = v3Mean;
	  v3BestNormal = v3Normal;
	}
    }
  
  // Done the ransacs, now collect the supposed inlier set
  vector<Vector<3> > vv3Inliers;
  for(unsigned int i=0; i<nPoints; i++)
    {
      Vector<3> v3Diff = mMap.vpPoints[i]->v3WorldPos - v3BestMean;
      double dDistSq = v3Diff * v3Diff;
      if(dDistSq == 0.0)
	continue;
      double dNormDist = fabs(v3Diff * v3BestNormal);
      if(dNormDist < 0.05)
	vv3Inliers.push_back(mMap.vpPoints[i]->v3WorldPos);
    }
  
  // With these inliers, calculate mean and cov
  Vector<3> v3MeanOfInliers = Zeros;
  for(unsigned int i=0; i<vv3Inliers.size(); i++)
    v3MeanOfInliers+=vv3Inliers[i];
  v3MeanOfInliers *= (1.0 / vv3Inliers.size());
  
  Matrix<3> m3Cov = Zeros;
  for(unsigned int i=0; i<vv3Inliers.size(); i++)
    {
      Vector<3> v3Diff = vv3Inliers[i] - v3MeanOfInliers;
      m3Cov += v3Diff.as_col() * v3Diff.as_row();
    };
  
  // Find the principal component with the minimal variance: this is the plane normal
  SymEigen<3> sym(m3Cov);
  Vector<3> v3Normal = sym.get_evectors()[0];
  
  // Use the version of the normal which points towards the cam center
  if(v3Normal[2] > 0)
    v3Normal *= -1.0;
  
  Matrix<3> m3Rot = Identity;
  m3Rot[2] = v3Normal;
  m3Rot[0] = m3Rot[0] - (v3Normal * (m3Rot[0] * v3Normal));
  normalize(m3Rot[0]);
  m3Rot[1] = m3Rot[2] ^ m3Rot[0];
  
  SE3<> se3Aligner;
  se3Aligner.get_rotation() = m3Rot;
  Vector<3> v3RMean = se3Aligner * v3MeanOfInliers;
  se3Aligner.get_translation() = -v3RMean;
  
  return se3Aligner;
}

// Calculates the depth(z-) distribution of map points visible in a keyframe
// This function is only used for the first two keyframes - all others
// get this filled in by the tracker
void MapMaker::RefreshSceneDepth(KeyFrame *pKF)
{
  double dSumDepth = 0.0;
  double dSumDepthSquared = 0.0;
  int nMeas = 0;
  for(meas_it it = pKF->mMeasurements.begin(); it!=pKF->mMeasurements.end(); it++)
    {
      MapPoint &point = *it->first;
      Vector<3> v3PosK = pKF->se3CfromW * point.v3WorldPos;
      dSumDepth += v3PosK[2];
      dSumDepthSquared += v3PosK[2] * v3PosK[2];
      nMeas++;
    }
 
  assert(nMeas > 2); // If not then something is seriously wrong with this KF!!
  pKF->dSceneDepthMean = dSumDepth / nMeas;
  pKF->dSceneDepthSigma = sqrt((dSumDepthSquared / nMeas) - (pKF->dSceneDepthMean) * (pKF->dSceneDepthMean));


}

void MapMaker::GUICommandCallBack(void* ptr, string sCommand, string sParams)
{
	((MapMaker*) ptr)->mmCommands.lock();
  Command c;
  c.sCommand = sCommand;
  c.sParams = sParams;
  ((MapMaker*) ptr)->mvQueuedCommands.push_back(c);
  ((MapMaker*) ptr)->mmCommands.unlock();
}

void MapMaker::GUICommandHandler(string sCommand, string sParams)  // Called by the callback func..
{
  cout << "! MapMaker::GUICommandHandler: unhandled command "<< sCommand << endl;
// exit(1);
}; 

void MapMaker::SaveMap(string filename)
{
	//lock map and wait until accepted...
	bool waslocked = mbLockMap;
	mbLockMap = true;
	RequestContinue();

	//TODO delete files in folder

	MapSerializationHelper helper(filename);
	helper.CreateAndCleanUpMapFolder();

	//register all pointers for cross referencing
	for(int i = 0; i < mMap.vpKeyFrames.size(); i++)
		helper.RegisterKeyFrame(mMap.vpKeyFrames[i]);
	for(int i = 0; i < mvpKeyFrameQueue.size(); i++)
			helper.RegisterKeyFrame(mvpKeyFrameQueue[i]);
	for(int i = 0; i < mMap.vpPoints.size(); i++)
		helper.RegisterMapPoint(mMap.vpPoints[i]);
	for(int i = 0; i < mMap.vpPointsTrash.size(); i++)
		helper.RegisterMapPoint(mMap.vpPointsTrash[i]);

	XMLDocument* doc = helper.GetXMLDocument();

	XMLElement *map = doc->NewElement("Map");

	//save camera
	map->InsertEndChild(mCamera.save(helper)); //TODO allow different cameras for each keyframe, at the moment saving the camera is useless!!!

	//save map
	XMLElement *keyframes = doc->NewElement("KeyFrames");
	for(int i = 0; i < mMap.vpKeyFrames.size(); i++)
		keyframes->InsertEndChild(mMap.vpKeyFrames[i]->save(helper));
	map->InsertEndChild(keyframes);

	XMLElement *points = doc->NewElement("Points");
	for(int i = 0; i < mMap.vpPoints.size(); i++)
		points->InsertEndChild(mMap.vpPoints[i]->save(helper));
	map->InsertEndChild(points);

	XMLElement *trashpoints = doc->NewElement("TrashPoints");
	for(int i = 0; i < mMap.vpPointsTrash.size(); i++)
		trashpoints->InsertEndChild(mMap.vpPointsTrash[i]->save(helper));
	map->InsertEndChild(trashpoints);

	//save mapmaker stuff
	XMLElement *newkeyframes = doc->NewElement("NewKeyFrames");
	for(int i = 0; i < mvpKeyFrameQueue.size(); i++)
		newkeyframes->InsertEndChild(mvpKeyFrameQueue[i]->save(helper));
	map->InsertEndChild(newkeyframes);

	XMLElement *failures = doc->NewElement("FailureQueue");
	for(auto iter=mvFailureQueue.begin(); iter!=mvFailureQueue.end(); ++iter)
	{
		XMLElement *failure = doc->NewElement("Failure");
		failure->SetAttribute("KeyFrameID",helper.GetKeyFrameID(iter->first));
		failure->SetAttribute("MapPointID",helper.GetMapPointID(iter->second));
		failures->InsertEndChild(failure);
	}
	map->InsertEndChild(failures);

	XMLElement *newmappoints = doc->NewElement("NewMapPoints"); //TODO save the same way as in mmdata as its just a list of numbers?
	std::queue<MapPoint*> tmpqueue = mqNewQueue;
	while(!tmpqueue.empty())
	{
		XMLElement *newmp = doc->NewElement("NewMapPoint");
		newmp->SetAttribute("MapPointID",helper.GetMapPointID(tmpqueue.front()));
		newmappoints->InsertEndChild(newmp);

		tmpqueue.pop();
	}
	map->InsertEndChild(newmappoints);

	map->SetAttribute("mdWiggleScale",mdWiggleScale);
	map->SetAttribute("mdWiggleScaleDepthNormalized",mdWiggleScaleDepthNormalized);
	map->SetAttribute("mdFirstKeyFrameDist",mdFirstKeyFrameDist);

	doc->InsertEndChild(map);
	helper.SaveXMLDocument();

	mbLockMap = waslocked;
}

void MapMaker::LoadMap(string filename)
{
	bool waslocked = mbLockMap;
	mbLockMap = true;
	RequestContinue();
	mMap.LockMap();
	Reset();

	MapSerializationHelper helper(filename);
	helper.LoadXMLDocument();
	XMLDocument* doc = helper.GetXMLDocument();

	XMLElement *map = doc->FirstChildElement("Map");

	//create empty object for each keyframe and mappoint and register pointer
	XMLElement *keyframes = map->FirstChildElement("KeyFrames");
	XMLElement *keyframe = keyframes->FirstChildElement("KeyFrame");
	while(keyframe!=NULL)
	{
		KeyFrame* kf = new KeyFrame();
		helper.RegisterKeyFrame(kf); //TODO maybe register keyframes and mappoints with saved IDs (instead of assuming to be in same order as during save) to allow easy manual map editing?
		mMap.vpKeyFrames.push_back(kf);
		keyframe = keyframe->NextSiblingElement("KeyFrame");
	}
	keyframes = map->FirstChildElement("NewKeyFrames");
	keyframe = keyframes->FirstChildElement("KeyFrame");
	while(keyframe!=NULL)
	{
		KeyFrame* kf = new KeyFrame();
		helper.RegisterKeyFrame(kf);
		mvpKeyFrameQueue.push_back(kf);
		keyframe = keyframe->NextSiblingElement("KeyFrame");
	}
	XMLElement *mappoints = map->FirstChildElement("Points");
	XMLElement *mappoint = mappoints->FirstChildElement("MapPoint");
	while(mappoint!=NULL)
	{
		MapPoint* mp = new MapPoint();
		helper.RegisterMapPoint(mp);
		mMap.vpPoints.push_back(mp);
		mappoint = mappoint->NextSiblingElement("MapPoint");
	}
	mappoints = map->FirstChildElement("TrashPoints");
	mappoint = mappoints->FirstChildElement("MapPoint");
	while(mappoint!=NULL)
	{
		MapPoint* mp = new MapPoint();
		helper.RegisterMapPoint(mp);
		mMap.vpPointsTrash.push_back(mp);
		mappoint = mappoint->NextSiblingElement("MapPoint");
	}

	//load actual data
	keyframes = map->FirstChildElement("KeyFrames");
	keyframe = keyframes->FirstChildElement("KeyFrame");
	while(keyframe!=NULL)
	{
		int kfid;
		keyframe->QueryAttribute("ID",&kfid);
		helper.GetKeyFrame(kfid)->load(keyframe,helper);
		keyframe = keyframe->NextSiblingElement("KeyFrame");
	}
	keyframes = map->FirstChildElement("NewKeyFrames");
	keyframe = keyframes->FirstChildElement("KeyFrame");
	while(keyframe!=NULL)
	{
		int kfid;
		keyframe->QueryAttribute("ID",&kfid);
		helper.GetKeyFrame(kfid)->load(keyframe,helper);
		keyframe = keyframe->NextSiblingElement("KeyFrame");
	}

	mappoints = map->FirstChildElement("Points");
	mappoint = mappoints->FirstChildElement("MapPoint");
	while(mappoint!=NULL)
	{
		int mpid;
		mappoint->QueryAttribute("ID",&mpid);
		helper.GetMapPoint(mpid)->load(mappoint,helper);
		mappoint = mappoint->NextSiblingElement("MapPoint");
	}
	mappoints = map->FirstChildElement("TrashPoints");
	mappoint = mappoints->FirstChildElement("MapPoint");
	while(mappoint!=NULL)
	{
		int mpid;
		mappoint->QueryAttribute("ID",&mpid);
		helper.GetMapPoint(mpid)->load(mappoint,helper);
		mappoint = mappoint->NextSiblingElement("MapPoint");
	}

	//load other mapmaker stuff
	XMLElement *failures =  map->FirstChildElement("FailureQueue");
	XMLElement *failure = failures->FirstChildElement("Failure");
	while(failure!=NULL)
	{
		int kfid;
		int mpid;
		failure->QueryAttribute("KeyFrameID",&kfid);
		failure->QueryAttribute("MapPointID",&mpid);
		KeyFrame* kf = helper.GetKeyFrame(kfid);
		MapPoint* mp = helper.GetMapPoint(mpid);
		mvFailureQueue.push_back(std::pair<KeyFrame*, MapPoint*>(kf,mp));
		failure = failure->NextSiblingElement("Failure");
	}

	XMLElement *newmappoints = map->FirstChildElement("NewMapPoints");
	XMLElement *newmp = newmappoints->FirstChildElement("NewMapPoint");
	while(newmp!=NULL)
	{
		int mpid;
		newmp->QueryAttribute("MapPointID",&mpid);
		mqNewQueue.push(helper.GetMapPoint(mpid));
		newmp = newmp->NextSiblingElement("NewMapPoint");
	}

	map->QueryAttribute("mdWiggleScale",&mdWiggleScale);
	map->QueryAttribute("mdWiggleScaleDepthNormalized",&mdWiggleScaleDepthNormalized);
	map->QueryAttribute("mdFirstKeyFrameDist",&mdFirstKeyFrameDist);

	//TODO load camera and allow different cameras for each keyframe! At the moment loading the camera just in the mapmaker is useless!!!
	ATANCamera test;
	test.load(map->FirstChildElement("Camera"),helper);

	InitFromLoad();

	mMap.UnlockMap();
	mbLockMap = waslocked;

}

}

