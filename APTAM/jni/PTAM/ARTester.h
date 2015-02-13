// Copyright 2015 ICGJKU
//
// ARTester.h
// Some moveable testobject.
//
#ifndef __ARTESTER_H
#define __ARTESTER_H

#include <TooN/TooN.h>
#include "OpenGL.h"
#include "Map.h"

namespace APTAM {

using namespace TooN;

class ARTester 
{
  public:
	ARTester( );
    void DrawStuff( SE3<> se3CfromW, Map &map );
    void KeyPressed( std::string key );

  protected:
    int selectionStatus;
    SE3<> lastCameraPose;

    SE3<> objectPose;

    bool keyPressed;

};

}

#endif
