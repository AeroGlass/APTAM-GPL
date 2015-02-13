// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

#include "KeyFrame.h"
#include "ShiTomasi.h"
#include "SmallBlurryImage.h"
#include <cvd/vision.h>
#include <cvd/fast_corner.h>
#ifdef __ANDROID__
#include <agast_corner_detect.h>
#else
#include <agast/agast_corner_detect.h>
#endif

#include <pthread.h>
//#define ENABLE_TIMING
#include "Timing.h"

#include <gvars3/instances.h>

namespace APTAM {

using namespace CVD;
using namespace std;
using namespace GVars3;


void findCorners(Level &lev)
{
	  // .. and detect and store FAST corner points.
	  // I use a different threshold on each level; this is a bit of a hack
	  // whose aim is to balance the different levels' relative feature densities.
	  lev.vCorners.clear();
	  lev.vCandidates.clear();
	  lev.vMaxCorners.clear();

	  void (*pFASTFunc)(const CVD::BasicImage<CVD::byte> &, std::vector<CVD::ImageRef> &,int)=NULL;

		pFASTFunc=&fast_corner_detect_9_nonmax;

		//MAKE SURE METHOD IS THREAD SAVE IF USING MULTITHREADING DURING KEYFRAME CREATE!!!

		//std::string FASTMethod = "AGAST12d";//"OAST16";
		std::string FASTMethod = GV2.GetString("Tracker.FeatureDetector","AGAST12d",SILENT);
		//BADLIGHTHACK FAST9 seemed to be slightly better in bad light, but that is probably very scene dependent
		//std::string FASTMethod = "FAST9";

		//cout << FASTMethod << endl;

		if (FASTMethod=="FAST9")
			pFASTFunc=&fast_corner_detect_9;
		else if (FASTMethod=="FAST10")
			pFASTFunc=&fast_corner_detect_10;
		else if (FASTMethod=="FAST9_nonmax")
			pFASTFunc=&fast_corner_detect_9_nonmax;
		else if (FASTMethod=="AGAST12d")
			pFASTFunc=&agast::agast7_12d::agast_corner_detect12d;
		else if (FASTMethod=="OAST16")
			pFASTFunc=&agast::oast9_16::oast_corner_detect16;
		else
		{
			//cout << "bad feature detector, using default..." << endl;
			pFASTFunc=&fast_corner_detect_9; //default if wrong settings
		}

		const bool skipLevel0Features = GV2.GetInt("Tracker.SkipLevel0Features", 0, SILENT); //do not generate lvl 0 features as they are bad when noise

		double factor = 1;//0.5;//BADLIGHTHACK 0.5 maybe reduce threshold in bad light when skipping lvl 0 features
		if(lev.index == 0 && !skipLevel0Features)
			pFASTFunc(lev.im, lev.vCorners, factor*20);//10 //20
		if(lev.index == 1)
			pFASTFunc(lev.im, lev.vCorners, factor*30);//15 //30
		if(lev.index == 2)
			pFASTFunc(lev.im, lev.vCorners, factor*30);//15 //30
		if(lev.index == 3)
			pFASTFunc(lev.im, lev.vCorners, factor*20);//10 //20

	  /*
	  if(i == 0)
	fast_corner_detect_10(lev.im, lev.vCorners, 10);
	  if(i == 1)
	fast_corner_detect_10(lev.im, lev.vCorners, 15);
	  if(i == 2)
	fast_corner_detect_10(lev.im, lev.vCorners, 15);
	  if(i == 3)
	fast_corner_detect_10(lev.im, lev.vCorners, 10);*/

	  // Generate row look-up-table for the FAST corner points: this speeds up
	  // finding close-by corner points later on.
	  unsigned int v=0;
	  lev.vCornerRowLUT.clear();
	  for(int y=0; y<lev.im.size().y; y++)
		{
		  while(v < lev.vCorners.size() && y > lev.vCorners[v].y)
			v++;
		  lev.vCornerRowLUT.push_back(v);
		}
}

void* findCornersThreaded(void * arg)
{
	Level * lev = (Level*)arg;
	findCorners(*lev);
	return NULL;
}

void KeyFrame::MakeKeyFrame_Lite(BasicImage<CVD::byte> &im)
{
	TIMER_INIT
	TIMER_START

  // First, copy out the image data to the pyramid's zero level.
  aLevels[0].im.resize(im.size());
  copy(im, aLevels[0].im);

  //pthread_t  p_thread[LEVELS];

  // Then, for each level...
  for(int i=0; i<LEVELS; i++)
  {
	  Level &lev = aLevels[i];
	  if(i!=0)
		{  // .. make a half-size image from the previous level..
		  lev.im.resize(aLevels[i-1].im.size() / 2);
		  halfSample(aLevels[i-1].im, lev.im);
		}
  }
  TIMER_STOP("half sample")
  TIMER_START
  for(int i=0; i<LEVELS; i++)
  {
	  //pthread_create(&(p_thread[i]), NULL, findCornersThreaded, (void*)&(aLevels[i]));
	  //findCorners(lev);
	  Level &lev = aLevels[i];
	  findCorners(lev);
  }
  TIMER_STOP("thread start")
    TIMER_START

  /*for(int i=0; i<LEVELS; i++)
	  pthread_join(p_thread[i],NULL);//*/
  TIMER_STOP("find corners")
}

void KeyFrame::MakeKeyFrame_Rest()
{
  static gvar3<double> gvdCandidateMinSTScore("MapMaker.CandidateMinShiTomasiScore", 70, SILENT);
  // For each level...
  for(int l=0; l<LEVELS; l++)
    {
      Level &lev = aLevels[l];
      // .. find those FAST corners which are maximal..
	  fast_nonmax(lev.im, lev.vCorners, 10, lev.vMaxCorners);
      // .. and then calculate the Shi-Tomasi scores of those, and keep the ones with
      // a suitably high score as Candidates, i.e. points which the mapmaker will attempt
      // to make new map points out of.
      for(vector<ImageRef>::iterator i=lev.vMaxCorners.begin(); i!=lev.vMaxCorners.end(); i++)
	  //for(vector<ImageRef>::iterator i=lev.vCorners.begin(); i!=lev.vCorners.end(); i++)
	{
	  if(!lev.im.in_image_with_border(*i, 10))
	    continue;
	  double dSTScore = FindShiTomasiScoreAtPoint(lev.im, 3, *i);
	  if(dSTScore > *gvdCandidateMinSTScore)
	    {
	      Candidate c;
	      c.irLevelPos = *i;
	      c.dSTScore = dSTScore;
	      lev.vCandidates.push_back(c);
	    }
	}
    }
  
  // Also, make a SmallBlurryImage of the keyframe: The relocaliser uses these.
  pSBI = new SmallBlurryImage(*this);  
  // Relocaliser also wants the jacobians..
  pSBI->MakeJacs();
}


Level& Level::operator=(const Level &rhs)
{
  // Operator= should physically copy pixels, not use CVD's reference-counting image copy.
  im.resize(rhs.im.size());
  copy(rhs.im, im);
  
  vCorners = rhs.vCorners;
  vMaxCorners = rhs.vMaxCorners;
  vCornerRowLUT = rhs.vCornerRowLUT;
  return *this;
}

// -------------------------------------------------------------
// Some useful globals defined in LevelHelpers.h live here:
Vector<3> gavLevelColors[LEVELS];

// These globals are filled in here. A single static instance of this struct is run before main()
struct LevelHelpersFiller // Code which should be initialised on init goes here; this runs before main()
{
  LevelHelpersFiller()
  {
    for(int i=0; i<LEVELS; i++)
      {
	if(i==0)  gavLevelColors[i] = makeVector( 1.0, 0.0, 0.0);
	else if(i==1)  gavLevelColors[i] = makeVector( 1.0, 1.0, 0.0);
	else if(i==2)  gavLevelColors[i] = makeVector( 0.0, 1.0, 0.0);
	else if(i==3)  gavLevelColors[i] = makeVector( 0.0, 0.0, 0.7);
	else gavLevelColors[i] =  makeVector( 1.0, 1.0, 0.7); // In case I ever run with LEVELS > 4
      }
  }
};
static LevelHelpersFiller foo;

}
