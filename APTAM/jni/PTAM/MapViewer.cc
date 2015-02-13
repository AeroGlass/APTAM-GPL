// -*- c++ -*-
// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

#include "MapViewer.h"
#include "MapPoint.h"
#include "KeyFrame.h"
#include "LevelHelpers.h"
#include <iomanip>
#ifdef __ANDROID__
#include "OpenGL.h"
#else
#include <cvd/gl_helpers.h>
#endif

namespace APTAM {

using namespace CVD;
using namespace std;


MapViewer::MapViewer(Map &map, GLWindow2 &glw):
  mMap(map), mGLWindow(glw)
{
  mse3ViewerFromWorld = 
    SE3<>::exp(makeVector(0,0,10,0,0,0)) * SE3<>::exp(makeVector(0,0,0,0.8 * M_PI,0,0));
}


void MapViewer::DrawMapDots()
{
  SetupFrustum();
  SetupModelView();
#ifdef __ANDROID__
  int nForMass = 0;
    glColor4f(0,1,1,1);
    glPointSize(3);
    mv3MassCenter = Zeros;
    mMap.LockMap();
    int numpts = mMap.vpPoints.size();
    GLfloat pts[3*numpts];
    GLfloat col[4*numpts];
    for(size_t i=0; i<numpts; i++)
    {
        Vector<3> v3Pos = mMap.vpPoints[i]->v3WorldPos;
        Vector<3> v3Col = gavLevelColors[mMap.vpPoints[i]->nSourceLevel];
        if(v3Pos * v3Pos < 10000)
		{
		  nForMass++;
		  mv3MassCenter += v3Pos;
		}
		for (int j=0; j<3; j++)
		{
		  pts[3*i+j]= v3Pos[j];
		  col[4*i+j]= v3Col[j];
		}
		col[4*i+3]= 1.0f;
	}
    mMap.UnlockMap();
#ifndef USE_OGL2
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
#else
    gles2h.UseShader(3);
    gles2h.SetDefaultUniforms();
#endif
    glVertexPointer(3, GL_FLOAT, 0, pts);
    glColorPointer(4, GL_FLOAT, 0, col);
    glDrawArrays(GL_POINTS,0,numpts);
#ifndef USE_OGL2
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif
    mv3MassCenter = mv3MassCenter / (0.1 + nForMass);
#else
  int nForMass = 0;
  glColor3f(0,1,1);
  glPointSize(3);
  glBegin(GL_POINTS);
  mv3MassCenter = Zeros;
  for(size_t i=0; i<mMap.vpPoints.size(); i++)
  {
    Vector<3> v3Pos = mMap.vpPoints[i]->v3WorldPos;
    glColor(gavLevelColors[mMap.vpPoints[i]->nSourceLevel]);
    if( (v3Pos * v3Pos) < 10000)
    {
      nForMass++;
      mv3MassCenter += v3Pos;
    }
    glVertex(v3Pos);
  }
  glEnd();
  mv3MassCenter = mv3MassCenter / (0.1 + nForMass);
#endif
}


void MapViewer::DrawGrid()
{
  SetupFrustum();
  SetupModelView();
  glLineWidth(1);
  
#ifdef __ANDROID__
  GLfloat pts[3*84];
    GLfloat col[4*84];

    // Draw a larger grid around the outside..
    double dGridInterval = 0.1;

    double dMin = -100.0 * dGridInterval;
    double dMax =  100.0 * dGridInterval;

    for(int x=-10;x<=10;x+=1)
      {
        float val;
        if(x==0)
  	val=1.0f;
        else
  	val=0.3;
  	  for (int j=0; j<3; j++)
  	{
  	  col[8*(x+10)+j]=val;
  	  col[8*(x+10)+j+4]=val;
  	}
  	  col[8*(x+10)+3]=1;
  	  col[8*(x+10)+7]=1;

  	  pts[6*(x+10)]=x * 10 * dGridInterval;
  	  pts[6*(x+10)+1]=dMin;
  	  pts[6*(x+10)+2]=0;
  	  pts[6*(x+10)+3]=x * 10 * dGridInterval;
  	  pts[6*(x+10)+4]=dMax;
  	  pts[6*(x+10)+5]=0;
      }
    for(int y=-10;y<=10;y+=1)
      {
        float val;
        if(y==0)
  	val=1.0f;
        else
  	val=0.3;
  	  for (int j=0; j<3; j++)
  	{
  	  col[8*21+8*(y+10)+j]=val;
  	  col[8*21+8*(y+10)+j+4]=val;
  	}
  	  col[8*21+8*(y+10)+3]=1;
  	  col[8*21+8*(y+10)+7]=1;

  	  pts[6*21+6*(y+10)]=dMin;
  	  pts[6*21+6*(y+10)+1]=y * 10 * dGridInterval;
  	  pts[6*21+6*(y+10)+2]=0;
  	  pts[6*21+6*(y+10)+3]=dMax;
  	  pts[6*21+6*(y+10)+4]=y * 10 * dGridInterval;
  	  pts[6*21+6*(y+10)+5]=0;
      }
#ifndef USE_OGL2
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
#else
    gles2h.UseShader(3);
    gles2h.SetDefaultUniforms();
#endif
    glVertexPointer(3, GL_FLOAT, 0, pts);
    glColorPointer(4, GL_FLOAT, 0, col);
    glDrawArrays(GL_LINES,0,84);
#ifndef USE_OGL2
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif

    GLfloat pts2[3*84+3*6];
    GLfloat col2[4*84+4*6];
    dMin = -10.0 * dGridInterval;
    dMax =  10.0 * dGridInterval;

    for(int x=-10;x<=10;x++)
      {
        float val;
        if(x==0)
  	val=1.0f;
        else
  	val=0.5;
        for (int j=0; j<3; j++)
  	{
  	  col2[8*(x+10)+j]=val;
  	  col2[8*(x+10)+j+4]=val;
  	}
  	  col2[8*(x+10)+3]=1;
  	  col2[8*(x+10)+7]=1;

        pts2[6*(x+10)]=x * dGridInterval;
  	  pts2[6*(x+10)+1]=dMin;
  	  pts2[6*(x+10)+2]=0;
  	  pts2[6*(x+10)+3]=x * dGridInterval;
  	  pts2[6*(x+10)+4]=dMax;
  	  pts2[6*(x+10)+5]=0;
      }
    for(int y=-10;y<=10;y++)
      {
        float val;
        if(y==0)
  	val=1.0f;
        else
  	val=0.5;
  	  for (int j=0; j<3; j++)
  	{
  	  col2[8*21+8*(y+10)+j]=val;
  	  col2[8*21+8*(y+10)+j+4]=val;
  	}
  	  col2[8*21+8*(y+10)+3]=1;
  	  col2[8*21+8*(y+10)+7]=1;
  	  pts2[6*21+6*(y+10)]=dMin;
  	  pts2[6*21+6*(y+10)+1]=y * dGridInterval;
  	  pts2[6*21+6*(y+10)+2]=0;
  	  pts2[6*21+6*(y+10)+3]=dMax;
  	  pts2[6*21+6*(y+10)+4]=y * dGridInterval;
  	  pts2[6*21+6*(y+10)+5]=0;
      }

    //l1
    col2[8*42]=1.0f;
    col2[8*42+1]=0.0f;
    col2[8*42+2]=0.0f;
    col2[8*42+3]=1.0f;
    pts2[6*42+0]=0.0f;
    pts2[6*42+1]=0.0f;
    pts2[6*42+2]=0.0f;

    col2[8*42+4]=1.0f;
	col2[8*42+4+1]=0.0f;
	col2[8*42+4+2]=0.0f;
	col2[8*42+4+3]=1.0f;
	pts2[6*42+3+0]=1.0f;
	pts2[6*42+3+1]=0.0f;
	pts2[6*42+3+2]=0.0f;

	//l2
	col2[8*43]=0.0f;
	col2[8*43+1]=1.0f;
	col2[8*43+2]=0.0f;
	col2[8*43+3]=1.0f;
	pts2[6*43+0]=0.0f;
	pts2[6*43+1]=0.0f;
	pts2[6*43+2]=0.0f;

	col2[8*43+4]=0.0f;
	col2[8*43+4+1]=1.0f;
	col2[8*43+4+2]=0.0f;
	col2[8*43+4+3]=1.0f;
	pts2[6*43+3+0]=0.0f;
	pts2[6*43+3+1]=1.0f;
	pts2[6*43+3+2]=0.0f;

	//l3
	col2[8*44]=1.0f;
	col2[8*44+1]=1.0f;
	col2[8*44+2]=1.0f;
	col2[8*44+3]=1.0f;
	pts2[6*44+0]=0.0f;
	pts2[6*44+1]=0.0f;
	pts2[6*44+2]=0.0f;

	col2[8*44+4]=1.0f;
	col2[8*44+4+1]=1.0f;
	col2[8*44+4+2]=1.0f;
	col2[8*44+4+3]=1.0f;
	pts2[6*44+3+0]=0.0f;
	pts2[6*44+3+1]=0.0f;
	pts2[6*44+3+2]=1.0f;

#ifndef USE_OGL2
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
#else
    gles2h.UseShader(3);
    gles2h.SetDefaultUniforms();
#endif
    glVertexPointer(3, GL_FLOAT, 0, pts2);
    glColorPointer(4, GL_FLOAT, 0, col2);
    glDrawArrays(GL_LINES,0,90);
#ifndef USE_OGL2
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif

     /*glColor3f(0.8,0.8,0.8);
     glRasterPos3f(1.1,0,0);
     mGLWindow.PrintString("x");
     glRasterPos3f(0,1.1,0);
     mGLWindow.PrintString("y");
     glRasterPos3f(0,0,1.1);
     mGLWindow.PrintString("z");*/
#else
  glBegin(GL_LINES);
  
  // Draw a larger grid around the outside..
  double dGridInterval = 0.1;
  
  double dMin = -100.0 * dGridInterval;
  double dMax =  100.0 * dGridInterval;
  
  for(int x=-10;x<=10;x+=1)
    {
      if(x==0)
	glColor3f(1,1,1);
      else
	glColor3f(0.3,0.3,0.3);
      glVertex3d((double)x * 10 * dGridInterval, dMin, 0.0);
      glVertex3d((double)x * 10 * dGridInterval, dMax, 0.0);
    }
  for(int y=-10;y<=10;y+=1)
    {
      if(y==0)
	glColor3f(1,1,1);
      else
	glColor3f(0.3,0.3,0.3);
      glVertex3d(dMin, (double)y * 10 *  dGridInterval, 0.0);
      glVertex3d(dMax, (double)y * 10 * dGridInterval, 0.0);
    }
  
  glEnd();

  glBegin(GL_LINES);
  dMin = -10.0 * dGridInterval;
  dMax =  10.0 * dGridInterval;
  
  for(int x=-10;x<=10;x++)
    {
      if(x==0)
	glColor3f(1,1,1);
      else
	glColor3f(0.5,0.5,0.5);
      
      glVertex3d((double)x * dGridInterval, dMin, 0.0);
      glVertex3d((double)x * dGridInterval, dMax, 0.0);
    }
  for(int y=-10;y<=10;y++)
    {
      if(y==0)
	glColor3f(1,1,1);
      else
	glColor3f(0.5,0.5,0.5);
      glVertex3d(dMin, (double)y * dGridInterval, 0.0);
      glVertex3d(dMax, (double)y * dGridInterval, 0.0);
    }
  
  glColor3f(1,0,0);
  glVertex3d(0,0,0);
  glVertex3d(1,0,0);
  glColor3f(0,1,0);
  glVertex3d(0,0,0);
  glVertex3d(0,1,0);
  glColor3f(1,1,1);
  glVertex3d(0,0,0);
  glVertex3d(0,0,1);
  glEnd();
  
//   glColor3f(0.8,0.8,0.8);
//   glRasterPos3f(1.1,0,0);
//   mGLWindow.PrintString("x");
//   glRasterPos3f(0,1.1,0);
//   mGLWindow.PrintString("y");
//   glRasterPos3f(0,0,1.1);
//   mGLWindow.PrintString("z");
#endif
}


void MapViewer::DrawMap(SE3<> se3CamFromWorld)
{
  mMessageForUser.str(""); // Wipe the user message clean
  
  // Update viewer position according to mouse input:
  {
    pair<Vector<6>, Vector<6> > pv6 = mGLWindow.GetMousePoseUpdate();
    SE3<> se3CamFromMC;
    se3CamFromMC.get_translation() = mse3ViewerFromWorld * mv3MassCenter;
    mse3ViewerFromWorld = SE3<>::exp(pv6.first) * 
      se3CamFromMC * SE3<>::exp(pv6.second) * se3CamFromMC.inverse() * mse3ViewerFromWorld;
  }

  mGLWindow.SetupViewport();
  glClearColor(0,0,0,0);
#ifdef __ANDROID__
  glClearDepthf(1);
#else
  glClearDepth(1);
#endif
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
#ifndef USE_OGL2
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_LINE_SMOOTH);
#endif
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColorMask(1,1,1,1);

  glEnable(GL_DEPTH_TEST);
  DrawGrid();
  DrawMapDots();

    DrawCamera(se3CamFromWorld);
  
  for(size_t i=0; i<mMap.vpKeyFrames.size(); i++)
    DrawCamera(mMap.vpKeyFrames[i]->se3CfromW, true);
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  mMessageForUser << " Map: "
      << mMap.vpPoints.size() << "P, " << mMap.vpKeyFrames.size() << "KF";
  mMessageForUser << setprecision(4);
  mMessageForUser << "   Camera Pos: " << se3CamFromWorld.inverse().get_translation();
}


/*void MapViewer::DrawMap(std::vector<SE3<>> se3CamFromWorld)
{
  mMessageForUser.str(""); // Wipe the user message clean
  
  // Update viewer position according to mouse input:
  {
    pair<Vector<6>, Vector<6> > pv6 = mGLWindow.GetMousePoseUpdate();
    SE3<> se3CamFromMC;
    se3CamFromMC.get_translation() = mse3ViewerFromWorld * mv3MassCenter;
    mse3ViewerFromWorld = SE3<>::exp(pv6.first) * 
      se3CamFromMC * SE3<>::exp(pv6.second) * se3CamFromMC.inverse() * mse3ViewerFromWorld;
  }

  mGLWindow.SetupViewport();
  glClearColor(0,0,0,0);
#ifdef __ANDROID__
  glClearDepthf(1);
#else
  glClearDepth(1);
#endif
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
#ifndef USE_OGL2
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_LINE_SMOOTH);
#endif
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColorMask(1,1,1,1);

  glEnable(GL_DEPTH_TEST);
  DrawGrid();
  DrawMapDots();

  if( mpViewingMap == mpMap ) {
	  for(int i = 0; i < se3CamFromWorld.size(); i++)
	  {
		DrawCamera(se3CamFromWorld[i]);
	  }
  }
  
  for(size_t i=0; i<mMap.vpKeyFrames.size(); i++)
    DrawCamera(mMap.vpKeyFrames[i]->se3CfromW, true);
  glDisable(GL_DEPTH_TEST);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  
  mMessageForUser << " Map " << mMap.MapID() << ": "
      << mMap.vpPoints.size() << "P, " << mMap.vpKeyFrames.size() << "KF";
  mMessageForUser << setprecision(4);
  mMessageForUser << "   Camera Pos: " << se3CamFromWorld[0].inverse().get_translation();
}*/


string MapViewer::GetMessageForUser()
{
  return mMessageForUser.str();
}



void MapViewer::SetupFrustum()
{
  glMatrixMode(GL_PROJECTION);  
  glLoadIdentity();
  double zNear = 0.03;
#ifdef __ANDROID__
  glFrustumf(-zNear, zNear, 0.75*zNear,-0.75*zNear,zNear,50);
#else
  glFrustum(-zNear, zNear, 0.75*zNear,-0.75*zNear,zNear,50);
#endif
  glScalef(1,1,-1);
  return;
}

void MapViewer::SetupModelView(SE3<> se3WorldFromCurrent)
{
  glMatrixMode(GL_MODELVIEW);  
  glLoadIdentity();
  glMultMatrix(mse3ViewerFromWorld * se3WorldFromCurrent);
  return;
}



void MapViewer::DrawCamera(SE3<> se3CfromW, bool bSmall)
{
  
  SetupModelView(se3CfromW.inverse());
  SetupFrustum();
  
  if(bSmall)
    glLineWidth(1);
  else
    glLineWidth(3);
  
#ifdef __ANDROID__
  GLfloat pts[] = {
      0,   0,   0,
      0.1f,0,   0,
      0,   0,   0,
      0,   0.1f,0,
      0,   0,   0,
      0,   0,   0.1f
    };
    GLfloat col[] = {
      1,0,0,1,
      1,0,0,1,
      0,1,0,1,
      0,1,0,1,
      1,1,1,1,
      1,1,1,1
    };
#ifndef USE_OGL2
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
#else
    gles2h.UseShader(3);
    gles2h.SetDefaultUniforms();
#endif
    glVertexPointer(3, GL_FLOAT, 0, pts);
    glColorPointer(4, GL_FLOAT, 0, col);
    glDrawArrays(GL_LINES,0,6);
#ifndef USE_OGL2
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif


    if(!bSmall)
    {
  	  glLineWidth(1);
  	  glColor4f(0.5,0.5,0.5,1.0f);
  	  SetupModelView();
  	  Vector<2> v2CamPosXY = se3CfromW.inverse().get_translation().slice<0,2>();
        GLfloat col[4*4];
        for (int i=0; i<16; i++)
      {
        col[i]=1.0f;
      }
        GLfloat pts[] = {
          static_cast<GLfloat>(v2CamPosXY[0] - 0.04), static_cast<GLfloat>(v2CamPosXY[1] + 0.04),
          static_cast<GLfloat>(v2CamPosXY[0] + 0.04), static_cast<GLfloat>(v2CamPosXY[1] - 0.04),
          static_cast<GLfloat>(v2CamPosXY[0] - 0.04), static_cast<GLfloat>(v2CamPosXY[1] - 0.04),
          static_cast<GLfloat>(v2CamPosXY[0] + 0.04), static_cast<GLfloat>(v2CamPosXY[1] + 0.04)
        };
#ifndef USE_OGL2
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
#else
        gles2h.UseShader(3);
        gles2h.SetDefaultUniforms();
#endif
        glVertexPointer(2, GL_FLOAT, 0, pts);
        glColorPointer(4, GL_FLOAT, 0, col);
        glDrawArrays(GL_LINES,0,4);
#ifndef USE_OGL2
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
#endif
    }
#else

  glBegin(GL_LINES);
  glColor3f(1,0,0);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.1f, 0.0f, 0.0f);
  glColor3f(0,1,0);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.1f, 0.0f);
  glColor3f(1,1,1);
  glVertex3f(0.0f, 0.0f, 0.0f);
  glVertex3f(0.0f, 0.0f, 0.1f);
  glEnd();

  
  if(!bSmall)
  {
    glLineWidth(1);
    glColor3f(0.5,0.5,0.5);
    SetupModelView();
    Vector<2> v2CamPosXY = se3CfromW.inverse().get_translation().slice<0,2>();
    glBegin(GL_LINES);
    glColor3f(1,1,1);
    glVertex2d(v2CamPosXY[0] - 0.04, v2CamPosXY[1] + 0.04);
    glVertex2d(v2CamPosXY[0] + 0.04, v2CamPosXY[1] - 0.04);
    glVertex2d(v2CamPosXY[0] - 0.04, v2CamPosXY[1] - 0.04);
    glVertex2d(v2CamPosXY[0] + 0.04, v2CamPosXY[1] + 0.04);
    glEnd();
  }
#endif
}

}

