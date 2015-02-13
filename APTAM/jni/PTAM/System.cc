// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015
#include "OpenGL.h"
#include <gvars3/instances.h>
#include <stdlib.h>
#include "System.h"
#include "ATANCamera.h"
#include "MapMaker.h"
#include "Tracker.h"
#include "ARDriver.h"
#include "MapViewer.h"
#include <cvd/image_io.h>
#include <cvd/convolution.h>
#include <cvd/vision.h>

//#define ENABLE_TIMING
#include "Timing.h"


namespace APTAM {

using namespace CVD;
using namespace std;
using namespace GVars3;


System::System()
  : mGLWindow(mVideoSource.Size(), "APTAM")
{

	requestFinish = false;
	finished = false;

  GUI.RegisterCommand("exit", GUICommandCallBack, this);
  GUI.RegisterCommand("quit", GUICommandCallBack, this);

  GUI.RegisterCommand("KeyPress", GUICommandCallBack, this);

  GUI.RegisterCommand("IncreaseBrightness", GUICommandCallBack, this);
  GUI.RegisterCommand("DecreaseBrightness", GUICommandCallBack, this);
  GUI.RegisterCommand("FixBrightness", GUICommandCallBack, this);

  GUI.RegisterCommand("SaveMap", GUICommandCallBack, this);
  GUI.RegisterCommand("LoadMap", GUICommandCallBack, this);

  
  mimFrameBW.resize(mVideoSource.Size());
  mimFrameRGB.resize(mVideoSource.Size());
  // First, check if the camera is calibrated.
  // If not, we need to run the calibration widget.
  Vector<NUMTRACKERCAMPARAMETERS> vTest;
  
  vTest = GV3::get<Vector<NUMTRACKERCAMPARAMETERS> >("Camera.Parameters", ATANCamera::mvDefaultParams, HIDDEN);
  mpCamera = new ATANCamera("Camera");
  mpCamera->SetImageSize(mVideoSource.Size()); // test
  if(vTest == ATANCamera::mvDefaultParams)
  {
    cout << endl;
    cout << "! Camera.Parameters is not set, need to run the CameraCalibrator tool" << endl;
    cout << "  and/or put the Camera.Parameters= line into the appropriate .cfg file." << endl;
    //exit(1);
  }

  mpMap = new Map;
  mpMapMaker = new MapMaker(*mpMap, *mpCamera);
  mpTracker = new Tracker(mVideoSource.Size(), *mpCamera, *mpMap, *mpMapMaker);
  mpARDriver = new ARDriver(*mpCamera, mVideoSource.Size(), mGLWindow, *mpMap);
  mpMapViewer = new MapViewer(*mpMap, mGLWindow);
  

  //These commands have to be registered here as they call the classes created above
  GUI.RegisterCommand("CleanUpMap", GUICommandCallBack, this);

  //create the menus
  GUI.ParseLine("GLWindow.AddMenu Menu Menu");
  GUI.ParseLine("Menu.ShowMenu Root");
  GUI.ParseLine("Menu.AddMenuButton Root Reset Reset Root");
  GUI.ParseLine("Menu.AddMenuButton Root Spacebar PokeTracker Root");
  GUI.ParseLine("DrawAR=0");
  GUI.ParseLine("DrawMap=0");
  GUI.ParseLine("Menu.AddMenuToggle Root \"View Map\" DrawMap Root");
  GUI.ParseLine("Menu.AddMenuToggle Root \"Draw AR\" DrawAR Root");

  GUI.ParseLine("GLWindow.AddMenu Options Options");
  GUI.ParseLine("Options.ShowMenu Root");
  GUI.ParseLine("Rendering=1");
  GUI.ParseLine("UseSensors=0");
  GUI.ParseLine("LockMap=0");
  GUI.ParseLine("Options.AddMenuToggle Root \"Render\" Rendering Root");
  GUI.ParseLine("Options.AddMenuToggle Root \"Sensors\" UseSensors Root");
  GUI.ParseLine("Options.AddMenuToggle Root \"Lock Map\" LockMap Root");

  GUI.ParseLine("GLWindow.AddMenu Map Map");
  //GUI.ParseLine("Map.ShowMenu Root");
  GUI.ParseLine("Map.AddMenuButton Root Save SaveMap Root");
  GUI.ParseLine("Map.AddMenuButton Root Load LoadMap Root");

  mbDone = false;
  bRender = true;
}

void average(CVD::Image<CVD::byte> &im1,CVD::Image<CVD::byte> &im2)
{
	for(int y=0; y < im1.size().y; y++)
	for(int x=0; x < im1.size().x; x++)
		im1[y][x] = ((int)im1[y][x] + (int)im2[y][x])/2;
}


void System::Run()
{
#ifndef __ANDROID__
  while(!mbDone)
#endif
  {
	  TIMER_INIT

      // We use two versions of each video frame:
      // One black and white (for processing by the tracker etc)
      // and one RGB, for drawing.

      if(!requestFinish)
      {
		  // Grab new video frame...
		  TIMER_START
		  mVideoSource.GetAndFillFrameBWandRGB(mimFrameBW, mimFrameRGB);

		  //Image<byte> imBlurred = mim;
		  //imBlurred.make_unique();
		  //convolveGaussian(mimFrameBW, 0.5);


		  TIMER_STOP("Get Frame")
		  static bool bFirstFrame = true;
		  if(bFirstFrame)
		  {
			mpARDriver->Init();
			bFirstFrame = false;


			//mimLastFrameBW.copy_from(mimFrameBW);

			//img_save(mimFrameBW, GLWindow2::getFDir()+"im.bmp");
			//img_save(mimFrameRGB, GLWindow2::getFDir()+"imrgb.bmp");
		  }
      }
      

      mGLWindow.SetupViewport();
      mGLWindow.SetupVideoOrtho();
      mGLWindow.SetupVideoRasterPosAndZoom();
      
      if(!requestFinish)
      {
		  if(!mpMap->IsGood()) {
			  mpARDriver->Reset();
		  }

		  static gvar3<int> gvnDrawMap("DrawMap", 0, HIDDEN|SILENT);
		  static gvar3<int> gvnDrawAR("DrawAR", 0, HIDDEN|SILENT);
		  static gvar3<int> gvnRendering("Rendering", 0, HIDDEN|SILENT);
		  static gvar3<int> gvnUseSensors("UseSensors", 0, HIDDEN|SILENT);
		  static gvar3<int> gvnLockMap("LockMap", 0, HIDDEN|SILENT);

		  mpMapMaker->mbLockMap = *gvnLockMap;

#ifdef __ANDROID__
		  mpTracker->useSensorDataForInitialization = *gvnUseSensors;
		  mpTracker->useSensorDataForTracking = *gvnUseSensors;
#endif

		  bool bDrawMap = mpMap->IsGood() && *gvnDrawMap;
		  bDrawAR = mpMap->IsGood() && *gvnDrawAR;

		  bRender = *gvnRendering;

		  const bool enableMedianFilter = GV2.GetInt("Tracker.EnableMedianFilter", 0, SILENT);

		  if(enableMedianFilter)
		  {
			  CVD::Image<CVD::byte> tmpFrameBW;
			  //tmpFrameBW.copy_from(mimFrameBW);
			  tmpFrameBW.resize(mimFrameBW.size());
			  median_filter_3x3(mimFrameBW,tmpFrameBW);
			  //average(tmpFrameBW,mimLastFrameBW);

			  TIMER_START
			  mpTracker->TrackFrame(tmpFrameBW,mimFrameRGB, !bDrawAR && !bDrawMap && bRender);
			  TIMER_STOP("Tracker")
		  }
		  else
		  {
			  TIMER_START
			  mpTracker->TrackFrame(mimFrameBW,mimFrameRGB, !bDrawAR && !bDrawMap && bRender);
			  TIMER_STOP("Tracker")
		  }

		  //mimLastFrameBW.copy_from(mimFrameBW);

		  TIMER_START

		  if(bDrawMap && bRender) {
			mpMapViewer->DrawMap(mpTracker->GetCurrentPose());
		  }
		  else if(bDrawAR && bRender)
			mpARDriver->Render(mimFrameRGB, mpTracker->GetCurrentPose());


		  string sCaption;
		  if(bDrawMap) {
			sCaption = mpMapViewer->GetMessageForUser();
		  }
		  else {
			sCaption = mpTracker->GetMessageForUser();
		  }
		  mGLWindow.DrawCaption(sCaption);
		  mGLWindow.DrawMenus();

      }
      else
      {
    	  mGLWindow.DrawCaption("Finishing... The application will exit soon...");
    	  finished = true;
      }
      
#ifndef __ANDROID__
      mGLWindow.swap_buffers();
#endif
      mGLWindow.HandlePendingEvents();

      mmCommands.lock();
      while(!mvQueuedCommands.empty())
      	{
      	  GUICommandHandler(mvQueuedCommands.begin()->sCommand, mvQueuedCommands.begin()->sParams);
      	  mvQueuedCommands.erase(mvQueuedCommands.begin());
      	}
      mmCommands.unlock();
    }
}


void System::GUICommandCallBack(void *ptr, string sCommand, string sParams)
{
	static_cast<System*>(ptr)->mmCommands.lock();
	Command c;
	c.sCommand = sCommand;
	c.sParams = sParams;
	static_cast<System*>(ptr)->mvQueuedCommands.push_back(c);
	static_cast<System*>(ptr)->mmCommands.unlock();
}


/**
 * Parse commands sent via the GVars command system.
 * @param ptr Object callback
 * @param sCommand command string
 * @param sParams parameters
 */
void System::GUICommandHandler(std::string sCommand, std::string sParams)
{
  if(sCommand=="quit" || sCommand == "exit") {
    mbDone = true;
  }
   else if( sCommand == "CleanUpMap" )
  {
	  mpMapMaker->RequestCleanUp();
  }
  else if( sCommand == "SaveNextImage")
  {
	  //mbSaveNextImage = true;
  }
  else if( sCommand == "KeyPress" )
  {
    if(sParams == "q" || sParams == "Escape")
    {
      GUI.ParseLine("quit");
      return;
    }

    if(!bDrawAR)
	   mpTracker->KeyPressed( sParams );
	else 
       mpARDriver->KeyPressed( sParams );
    
  }
  else if (sCommand == "IncreaseBrightness")
  {
	  cout << "IncreaseBrightness" << endl;
#ifdef __ANDROID__
	  mVideoSource.changeBrightness(1);
#endif
  }
  else if (sCommand == "DecreaseBrightness")
  {
	  cout << "DecreaseBrightness" << endl;
#ifdef __ANDROID__
	  mVideoSource.changeBrightness(-1);
#endif
  }
  else if (sCommand == "FixBrightness")
  {
	  cout << "FixBrightness" << endl;
#ifdef __ANDROID__
	  mVideoSource.changeBrightness(0);
#endif
  }
  else if(sCommand=="SaveMap")
  {
	  cout << "  Saving the map.... " << endl;
	  mpMapMaker->SaveMap(GLWindow2::getFDir()+"map/");
  }
  else if(sCommand=="LoadMap")
    {
  	  cout << "  Loading the map.... " << endl;
  	  mpMapMaker->LoadMap(GLWindow2::getFDir()+"map/");
    }
    

}



}


