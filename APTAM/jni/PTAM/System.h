// -*- c++ -*-
// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015
//
// System.h
//
// Defines the System class
//
// This stores the main functional classes of the system, like the
// mapmaker, map, tracker etc, and spawns the working threads.
//
#ifndef __SYSTEM_H
#define __SYSTEM_H
#include "VideoSource.h"
#include "GLWindow2.h"

#include <cvd/image.h>
#include <cvd/rgb.h>
#include <cvd/byte.h>

#include "threadpool.h"


namespace APTAM {

class ATANCamera;
class Map;
class MapMaker;
class Tracker;
class ARDriver;
class MapViewer;


class System
{
  public:
    System();
    void Run();
    
  private:
    VideoSource mVideoSource;                       // The video image source
  public:
    GLWindow2 mGLWindow;                            // The OpenGL window

    bool requestFinish;
    bool finished;

private:
  CVD::Image<CVD::Rgb<CVD::byte> > mimFrameRGB;
  CVD::Image<CVD::byte> mimFrameBW;
  
  //CVD::Image<CVD::byte> mimLastFrameBW;

  Map *mpMap; 
  MapMaker *mpMapMaker; 
  Tracker *mpTracker; 
  ATANCamera *mpCamera;
  ARDriver *mpARDriver;
  MapViewer *mpMapViewer;
  
  bool mbDone;
  bool bDrawAR;
  bool bRender;

  void GUICommandHandler(std::string sCommand, std::string sParams);
  static void GUICommandCallBack(void* ptr, std::string sCommand, std::string sParams);

  struct Command {std::string sCommand; std::string sParams; };
  std::vector<Command> mvQueuedCommands;

  Mutex mmCommands;


};

}

#endif
