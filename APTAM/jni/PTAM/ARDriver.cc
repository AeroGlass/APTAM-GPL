// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

#define GL_GLEXT_PROTOTYPES 1
#include "ARDriver.h"
#include "Map.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif

//#define ENABLE_TIMING
#include "Timing.h"

namespace APTAM {

using namespace GVars3;
using namespace CVD;
using namespace std;

static bool CheckFramebufferStatus();


ARDriver::ARDriver(const ATANCamera &cam, ImageRef irFrameSize, GLWindow2 &glw, Map &map)
  :mCamera(cam), mGLWindow(glw), mMap(map)
{
  mirFrameSize = irFrameSize;
  mCamera.SetImageSize(mirFrameSize);
  mbInitialised = false;
}

void ARDriver::Init()
{
  mbInitialised = true;
#ifdef __ANDROID__
  mirFBSize = GV3::get<ImageRef>("ARDriver.FrameBufferSize", ImageRef(1024,1024), SILENT);
  glGenTextures(1, &mnFrameTex);
  glBindTexture(GL_TEXTURE_2D,mnFrameTex);
    /*glTexImage2D(GL_TEXTURE_2D, 0,
  	       GL_RGBA, mirFrameSize.x, mirFrameSize.y, 0,
  	       GL_RGBA, GL_UNSIGNED_BYTE, NULL);*/
    glTexImage2D(GL_TEXTURE_2D, 0,
     	       GL_LUMINANCE, mirFrameSize.x, mirFrameSize.y, 0,
     	       GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    CheckGLError("ARDriver Init");

    glGenTextures(1, &mnFrameTexUV);
  glBindTexture(GL_TEXTURE_2D,mnFrameTexUV);
	/*glTexImage2D(GL_TEXTURE_2D, 0,
		   GL_RGBA, mirFrameSize.x, mirFrameSize.y, 0,
		   GL_RGBA, GL_UNSIGNED_BYTE, NULL);*/
	glTexImage2D(GL_TEXTURE_2D, 0,
			GL_LUMINANCE_ALPHA, mirFrameSize.x/2, mirFrameSize.y/2, 0,
			GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	CheckGLError("ARDriver Init UV");
#else
  mirFBSize = GV3::get<ImageRef>("ARDriver.FrameBufferSize", ImageRef(1200,900), SILENT);
  glGenTextures(1, &mnFrameTex);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB,mnFrameTex);
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0,
	       GL_RGBA, mirFrameSize.x, mirFrameSize.y, 0,
	       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#endif
  MakeFrameBuffer();
}

void ARDriver::Reset()
{
  //mGame.Reset();
  mnCounter = 0;
}

void ARDriver::Render(Image<Rgb<CVD::byte> > &imFrame, SE3<> se3CfromW)
{
  if(!mbInitialised)
  {
     Init();
     Reset();
  };
  
  mnCounter++;

  TIMER_INIT
  TIMER_START
#ifdef __ANDROID__
  // Upload the image to our frame texture
    glBindTexture(GL_TEXTURE_2D, mnFrameTex);
    glTexSubImage2D(GL_TEXTURE_2D,
  		  0, 0, 0,
  		  mirFrameSize.x, mirFrameSize.y,
  		  //GL_RGBA,
  		  GL_LUMINANCE,
  		  GL_UNSIGNED_BYTE,
  		  imFrame.data());
    CheckGLError("ARDriver Load Texture");

        glBindTexture(GL_TEXTURE_2D, mnFrameTexUV);
        glTexSubImage2D(GL_TEXTURE_2D,
      		  0, 0, 0,
      		  mirFrameSize.x/2, mirFrameSize.y/2,
      		  //GL_RGBA,
      		  GL_LUMINANCE_ALPHA,
      		  GL_UNSIGNED_BYTE,
      		  ((byte*)imFrame.data())+mirFrameSize.x*mirFrameSize.y);

        CheckGLError("ARDriver Load Texture UV");
    TIMER_STOP("Upload image")
    TIMER_START

    // Set up rendering to go the FBO, draw undistorted video frame into BG
#ifdef USE_OGL2
    glBindFramebuffer(GL_FRAMEBUFFER,mnFrameBuffer);
#else
    glBindFramebufferOES(GL_FRAMEBUFFER_OES,mnFrameBuffer);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CheckFramebufferStatus();
    glViewport(0,0,mirFBSize.x,mirFBSize.y);
    DrawFBBackGround();
    glClearDepthf(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    CheckGLError("ARDriver Draw Background");
#else
  // Upload the image to our frame texture
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mnFrameTex);
  glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,
		  0, 0, 0,
		  mirFrameSize.x, mirFrameSize.y,
		  GL_RGB,
		  GL_UNSIGNED_BYTE,
		  imFrame.data());
  TIMER_STOP("Upload image")
  TIMER_START

  // Set up rendering to go the FBO, draw undistorted video frame into BG
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,mnFrameBuffer);
  CheckFramebufferStatus();
  glViewport(0,0,mirFBSize.x,mirFBSize.y);
  DrawFBBackGround();
  glClearDepth(1);
  glClear(GL_DEPTH_BUFFER_BIT);
#endif
  TIMER_STOP("Draw color image")
  TIMER_START

  // Set up 3D projection
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glMultMatrix(mCamera.MakeUFBLinearFrustumMatrix(0.005, 1000));

  //glMatrixMode(GL_MODELVIEW); //WARNING: this might stop other Games than SamplingOverlay to work
  //glLoadIdentity();
  glMultMatrix(se3CfromW);

 DrawFadingGrid();

  mGame.DrawStuff(se3CfromW,mMap);
  
  

  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Set up for drawing 2D stuff:
#ifdef __ANDROID__
#ifdef USE_OGL2
  glBindFramebuffer(GL_FRAMEBUFFER,0);
#else
  glBindFramebufferOES(GL_FRAMEBUFFER_OES,0);
#endif
#else
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
#endif

  TIMER_STOP("Draw 3D")
  TIMER_START

  DrawDistortedFB();
  
  TIMER_STOP("Draw Framebuffer to Screen")
  TIMER_START

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  mGLWindow.SetupViewport();
  mGLWindow.SetupVideoOrtho();
  mGLWindow.SetupVideoRasterPosAndZoom();

  TIMER_STOP("Draw End")

}


void ARDriver::MakeFrameBuffer()
{
  // Needs nvidia drivers >= 97.46
  cout << "  ARDriver: Creating FBO... ";

#ifdef __ANDROID__
  glGenTextures(1, &mnFrameBufferTex);
    glBindTexture(GL_TEXTURE_2D,mnFrameBufferTex);
    glTexImage2D(GL_TEXTURE_2D, 0,
  	       GL_RGBA, mirFBSize.x, mirFBSize.y, 0,
  	       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLuint DepthBuffer;
#ifdef USE_OGL2
    glGenRenderbuffers(1, &DepthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, DepthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, mirFBSize.x, mirFBSize.y);

	glGenFramebuffers(1, &mnFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mnFrameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, mnFrameBufferTex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
					   GL_RENDERBUFFER, DepthBuffer);
#else
    glGenRenderbuffersOES(1, &DepthBuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, DepthBuffer);
    glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT24_OES, mirFBSize.x, mirFBSize.y);

    glGenFramebuffersOES(1, &mnFrameBuffer);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, mnFrameBuffer);
    glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES,
  			    GL_TEXTURE_2D, mnFrameBufferTex, 0);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES,
    			       GL_RENDERBUFFER_OES, DepthBuffer);
#endif

    CheckFramebufferStatus();
    cout << " .. created FBO." << endl;
#ifdef USE_OGL2
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
#endif
    CheckGLError("ARDriver Make Framebuffer");
#else
  glGenTextures(1, &mnFrameBufferTex);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB,mnFrameBufferTex);
  glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0,
	       GL_RGBA, mirFBSize.x, mirFBSize.y, 0,
	       GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  GLuint DepthBuffer;
  glGenRenderbuffersEXT(1, &DepthBuffer);
  glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, DepthBuffer);
  glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, mirFBSize.x, mirFBSize.y);

  glGenFramebuffersEXT(1, &mnFrameBuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mnFrameBuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			    GL_TEXTURE_RECTANGLE_ARB, mnFrameBufferTex, 0);
  glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
  			       GL_RENDERBUFFER_EXT, DepthBuffer);

  CheckFramebufferStatus();
  cout << " .. created FBO." << endl;
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif
}

static bool CheckFramebufferStatus()
{
  GLenum n;
#ifdef __ANDROID__
#ifdef USE_OGL2
  n = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if(n == GL_FRAMEBUFFER_COMPLETE)
        return true; // All good
#else
  n = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
    if(n == GL_FRAMEBUFFER_COMPLETE_OES)
      return true; // All good
#endif

    __android_log_print(ANDROID_LOG_ERROR, "PTAM", "Framebuffer status error");
#else
  n = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if(n == GL_FRAMEBUFFER_COMPLETE_EXT)
    return true; // All good
  
  cout << "glCheckFrameBufferStatusExt returned an error." << endl;
#endif
  return false;
}

struct DistortionVertex
 {
   GLfloat x, y;        //Vertex
   GLfloat s0, t0;         //Texcoord0
 };

void ARDriver::DrawFBBackGround()
{
#ifdef __ANDROID__
	//static GLuint nList;
	  mGLWindow.SetupUnitOrtho();

	  //glDisable(GL_POLYGON_SMOOTH);
#ifndef USE_OGL2
	  glEnable(GL_TEXTURE_2D);
#endif
	  glActiveTexture(GL_TEXTURE0);
	  glBindTexture(GL_TEXTURE_2D, mnFrameTex);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  glActiveTexture(GL_TEXTURE1);
	  glBindTexture(GL_TEXTURE_2D, mnFrameTexUV);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	  glActiveTexture(GL_TEXTURE0);
	  glDisable(GL_BLEND);

	  // How many grid divisions in the x and y directions to use?
	  int nStepsX = 24; // Pretty arbitrary..
	  int nStepsY = (int) (nStepsX * ((double) mirFrameSize.x / mirFrameSize.y)); // Scaled by aspect ratio
	  if(nStepsY < 2)
	  nStepsY = 2;

	  std::vector<DistortionVertex> vtx((nStepsX+1)*(nStepsY+1));
	  std::vector<GLushort> idx(nStepsX*nStepsY*2*3);

	  for(int ystep = 0; ystep <= nStepsY; ystep++)
	  {
		  for(int xstep = 0; xstep <= nStepsX; xstep++)
	  	  {

			Vector<2> v2Iter;
			v2Iter[0] = (double) xstep / nStepsX;
			v2Iter[1] = (double) ystep / nStepsY;

			// If this is a border quad, draw a little beyond the
			// outside of the frame, this avoids strange jaggies
			// at the edge of the reconstructed frame later:
			if(xstep == 0 || ystep == 0 || xstep == nStepsX || ystep == nStepsY)
			  for(int i=0; i<2; i++)
				v2Iter[i] = v2Iter[i] * 1.02 - 0.01;

			Vector<2> v2UFBDistorted = v2Iter;
			Vector<2> v2UFBUnDistorted = mCamera.UFBLinearProject(mCamera.UFBUnProject(v2UFBDistorted));

			vtx[ystep*(nStepsX+1)+xstep].x = v2UFBUnDistorted[0];
			vtx[ystep*(nStepsX+1)+xstep].y = v2UFBUnDistorted[1];

			vtx[ystep*(nStepsX+1)+xstep].s0 = v2UFBDistorted[0];
			vtx[ystep*(nStepsX+1)+xstep].t0 = v2UFBDistorted[1];

			if(xstep < nStepsX && ystep < nStepsY)
			{
				idx[2*3*(ystep*nStepsX+xstep)+0] =  ystep*(nStepsX+1)+xstep;
				idx[2*3*(ystep*nStepsX+xstep)+1] =  ystep*(nStepsX+1)+xstep+1;
				idx[2*3*(ystep*nStepsX+xstep)+2] =  (ystep+1)*(nStepsX+1)+xstep;

				idx[2*3*(ystep*nStepsX+xstep)+3] =  ystep*(nStepsX+1)+xstep+1;
				idx[2*3*(ystep*nStepsX+xstep)+4] =  (ystep+1)*(nStepsX+1)+xstep+1;
				idx[2*3*(ystep*nStepsX+xstep)+5] =  (ystep+1)*(nStepsX+1)+xstep;
			}

	  	  }
	  }

	  glColor4f(1,1,1,1);

#ifndef USE_OGL2
	  glEnableClientState(GL_VERTEX_ARRAY);
	  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif

#ifdef USE_OGL2
	  gles2h.UseShader(5);
	  gles2h.SetDefaultUniforms();
	  glUniform1i(glGetUniformLocation(gles2h.GetActiveShader(),"uvtexture"),1);
#endif

	  glVertexPointer(2, GL_FLOAT, sizeof(DistortionVertex), vtx.data());
	  glTexCoordPointer(2, GL_FLOAT, sizeof(DistortionVertex), &(vtx.data()[0].s0));

	  glDrawElements(GL_TRIANGLES,idx.size(),GL_UNSIGNED_SHORT,idx.data());

#ifndef USE_OGL2
	  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	  glDisableClientState(GL_VERTEX_ARRAY);
#endif

#ifndef USE_OGL2
	  glDisable(GL_TEXTURE_2D);
#endif

	  CheckGLError("ARDriver draw color frame");
#else
  static bool bFirstRun = true;
  static GLuint nList;
  mGLWindow.SetupUnitOrtho();

  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mnFrameTex);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glDisable(GL_POLYGON_SMOOTH);
  glDisable(GL_BLEND);
  // Cache the cpu-intesive projections in a display list..
  if(bFirstRun)
    {
      bFirstRun = false;
      nList = glGenLists(1);
      glNewList(nList, GL_COMPILE_AND_EXECUTE);
      glColor3f(1,1,1);
      // How many grid divisions in the x and y directions to use?
      int nStepsX = 24; // Pretty arbitrary..
      int nStepsY = (int) (nStepsX * ((double) mirFrameSize.x / mirFrameSize.y)); // Scaled by aspect ratio
      if(nStepsY < 2)
	nStepsY = 2;
      for(int ystep = 0; ystep< nStepsY; ystep++)
	{
	  glBegin(GL_QUAD_STRIP);
	  for(int xstep = 0; xstep <= nStepsX; xstep++)
	    for(int yystep = ystep; yystep<=ystep+1; yystep++) // Two y-coords in one go - magic.
	      {
		Vector<2> v2Iter;
		v2Iter[0] = (double) xstep / nStepsX;
		v2Iter[1] = (double) yystep / nStepsY;
		// If this is a border quad, draw a little beyond the
		// outside of the frame, this avoids strange jaggies
		// at the edge of the reconstructed frame later:
		if(xstep == 0 || yystep == 0 || xstep == nStepsX || yystep == nStepsY)
		  for(int i=0; i<2; i++)
		    v2Iter[i] = v2Iter[i] * 1.02 - 0.01;

		Vector<2> v2UFBDistorted = v2Iter;
		Vector<2> v2UFBUnDistorted = mCamera.UFBLinearProject(mCamera.UFBUnProject(v2UFBDistorted));
		glTexCoord2d(v2UFBDistorted[0] * mirFrameSize.x, v2UFBDistorted[1] * mirFrameSize.y);
		glVertex(v2UFBUnDistorted);
	      }
	  glEnd();
	}
      glEndList();
    }
  else
    glCallList(nList);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);
#endif
}


void ARDriver::DrawDistortedFB()
{
#ifdef __ANDROID__

	mGLWindow.SetupViewport();
	mGLWindow.SetupUnitOrtho();

	  //glDisable(GL_POLYGON_SMOOTH);
#ifndef USE_OGL2
	glEnable(GL_TEXTURE_2D);
#endif
	glBindTexture(GL_TEXTURE_2D, mnFrameBufferTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glDisable(GL_BLEND);

	  // How many grid divisions in the x and y directions to use?
	  int nStepsX = 24; // Pretty arbitrary..
	  int nStepsY = (int) (nStepsX * ((double) mirFrameSize.x / mirFrameSize.y)); // Scaled by aspect ratio
	  if(nStepsY < 2)
	  nStepsY = 2;

	  std::vector<DistortionVertex> vtx((nStepsX+1)*(nStepsY+1));
	  std::vector<GLushort> idx(nStepsX*nStepsY*2*3);

	  for(int ystep = 0; ystep <= nStepsY; ystep++)
	  {
		  for(int xstep = 0; xstep <= nStepsX; xstep++)
	  	  {

			Vector<2> v2Iter;
			v2Iter[0] = (double) xstep / nStepsX;
			v2Iter[1] = (double) ystep / nStepsY;
			Vector<2> v2UFBDistorted = v2Iter;
			Vector<2> v2UFBUnDistorted = mCamera.UFBLinearProject(mCamera.UFBUnProject(v2UFBDistorted));


			vtx[ystep*(nStepsX+1)+xstep].x = v2UFBDistorted[0];
			vtx[ystep*(nStepsX+1)+xstep].y = v2UFBDistorted[1];

			vtx[ystep*(nStepsX+1)+xstep].s0 = v2UFBUnDistorted[0];
			vtx[ystep*(nStepsX+1)+xstep].t0 = (1- v2UFBUnDistorted[1]);
			//vtx[ystep*(nStepsX+1)+xstep].s0 = v2UFBDistorted[0];
			//vtx[ystep*(nStepsX+1)+xstep].t0 = (1- v2UFBDistorted[1]);

			if(xstep < nStepsX && ystep < nStepsY)
			{
				idx[2*3*(ystep*nStepsX+xstep)+0] =  ystep*(nStepsX+1)+xstep;
				idx[2*3*(ystep*nStepsX+xstep)+1] =  ystep*(nStepsX+1)+xstep+1;
				idx[2*3*(ystep*nStepsX+xstep)+2] =  (ystep+1)*(nStepsX+1)+xstep;

				idx[2*3*(ystep*nStepsX+xstep)+3] =  ystep*(nStepsX+1)+xstep+1;
				idx[2*3*(ystep*nStepsX+xstep)+4] =  (ystep+1)*(nStepsX+1)+xstep+1;
				idx[2*3*(ystep*nStepsX+xstep)+5] =  (ystep+1)*(nStepsX+1)+xstep;
			}

	  	  }
	  }

	  glColor4f(1,1,1,1);

#ifndef USE_OGL2
	  glEnableClientState(GL_VERTEX_ARRAY);
	  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif

#ifdef USE_OGL2
	  gles2h.UseShader(1);
	  gles2h.SetDefaultUniforms();
#endif

	  glVertexPointer(2, GL_FLOAT, sizeof(DistortionVertex), vtx.data());
	  glTexCoordPointer(2, GL_FLOAT, sizeof(DistortionVertex), &(vtx.data()[0].s0));

	  glDrawElements(GL_TRIANGLES,idx.size(),GL_UNSIGNED_SHORT,idx.data());

#ifndef USE_OGL2
	  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	  glDisableClientState(GL_VERTEX_ARRAY);
#endif

#ifndef USE_OGL2
	  glDisable(GL_TEXTURE_2D);
#endif

	  CheckGLError("ARDriver draw color frame");

#else
  static bool bFirstRun = true;
  static GLuint nList;
  mGLWindow.SetupViewport();
  mGLWindow.SetupUnitOrtho();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, mnFrameBufferTex);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glDisable(GL_POLYGON_SMOOTH);
  glDisable(GL_BLEND);
  if(bFirstRun)
    {
      bFirstRun = false;
      nList = glGenLists(1);
      glNewList(nList, GL_COMPILE_AND_EXECUTE);
      // How many grid divisions in the x and y directions to use?
      int nStepsX = 24; // Pretty arbitrary..
      int nStepsY = (int) (nStepsX * ((double) mirFrameSize.x / mirFrameSize.y)); // Scaled by aspect ratio
      if(nStepsY < 2)
	nStepsY = 2;
      glColor3f(1,1,1);
      for(int ystep = 0; ystep<nStepsY; ystep++)
	{
	  glBegin(GL_QUAD_STRIP);
	  for(int xstep = 0; xstep<=nStepsX; xstep++)
	    for(int yystep = ystep; yystep<=ystep + 1; yystep++) // Two y-coords in one go - magic.
	      {
		Vector<2> v2Iter;
		v2Iter[0] = (double) xstep / nStepsX;
		v2Iter[1] = (double) yystep / nStepsY;
		Vector<2> v2UFBDistorted = v2Iter;
		Vector<2> v2UFBUnDistorted = mCamera.UFBLinearProject(mCamera.UFBUnProject(v2UFBDistorted));
		glTexCoord2d(v2UFBUnDistorted[0] * mirFBSize.x, (1.0 - v2UFBUnDistorted[1]) * mirFBSize.y);
		glVertex(v2UFBDistorted);
	      }
	  glEnd();
	}
      glEndList();
    }
  else
    glCallList(nList);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);
#endif
}

void ARDriver::DrawFadingGrid()
{
#ifdef __ANDROID__
	double dStrength;
	  if(mnCounter >= 60)
	    return;
	  if(mnCounter < 30)
	    dStrength = 1.0;
	  dStrength = (60 - mnCounter) / 30.0;

	  glColor4f(1,1,1,dStrength);
	  int nHalfCells = 8;
	  if(mnCounter < 8)
	    nHalfCells = mnCounter + 1;
	  int nTot = nHalfCells * 2 + 1;
	  Vector<3>  aaVertex[17][17];
	  for(int i=0; i<nTot; i++)
	    for(int j=0; j<nTot; j++)
	      {
		Vector<3> v3;
		v3[0] = (i - nHalfCells) * 0.1;
		v3[1] = (j - nHalfCells) * 0.1;
		v3[2] = 0.0;
		aaVertex[i][j] = v3;
	      }

	  //glEnable(GL_LINE_SMOOTH);
	  glEnable(GL_BLEND);
	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	  glLineWidth(2);
	  for(int i=0; i<nTot; i++)
	    {
	      GLfloat vtx1[3*nTot];
	      for(int j=0; j<nTot; j++)
		    for(int k=0; k<3; k++)
		      vtx1[3*j+k] = aaVertex[i][j][k];
#ifndef USE_OGL2
		  glEnableClientState(GL_VERTEX_ARRAY);
#endif

#ifdef USE_OGL2
	  gles2h.UseShader(1);
	  gles2h.SetDefaultUniforms();
#endif
	      glVertexPointer(3, GL_FLOAT, 0, vtx1);

	      glDrawArrays(GL_LINE_STRIP,0,nTot);
#ifndef USE_OGL2
	      glDisableClientState(GL_VERTEX_ARRAY);
#endif

	      GLfloat vtx2[3*nTot];
	      for(int j=0; j<nTot; j++)
		    for(int k=0; k<3; k++)
		      vtx2[3*j+k] = aaVertex[j][i][k];
#ifndef USE_OGL2
		  glEnableClientState(GL_VERTEX_ARRAY);
#endif

#ifdef USE_OGL2
	      gles2h.UseShader(0);
	      gles2h.SetDefaultUniforms();
#endif

	      glVertexPointer(3, GL_FLOAT, 0, vtx2);

	      glDrawArrays(GL_LINE_STRIP,0,nTot);
#ifndef USE_OGL2
	      glDisableClientState(GL_VERTEX_ARRAY);
#endif
	    };
	  CheckGLError("ARDriver draw grid");
#else
  double dStrength;
  if(mnCounter >= 60)
    return;
  if(mnCounter < 30)
    dStrength = 1.0;
  dStrength = (60 - mnCounter) / 30.0;

  glColor4f(1,1,1,dStrength);
  int nHalfCells = 8;
  if(mnCounter < 8)
    nHalfCells = mnCounter + 1;
  int nTot = nHalfCells * 2 + 1;
  Vector<3>  aaVertex[17][17];
  for(int i=0; i<nTot; i++)
    for(int j=0; j<nTot; j++)
      {
	Vector<3> v3;
	v3[0] = (i - nHalfCells) * 0.1;
	v3[1] = (j - nHalfCells) * 0.1;
	v3[2] = 0.0;
	aaVertex[i][j] = v3;
      }

  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth(2);
  for(int i=0; i<nTot; i++)
    {
      glBegin(GL_LINE_STRIP);
      for(int j=0; j<nTot; j++)
	glVertex(aaVertex[i][j]);
      glEnd();

      glBegin(GL_LINE_STRIP);
      for(int j=0; j<nTot; j++)
	glVertex(aaVertex[j][i]);
      glEnd();
    };
#endif
};

void ARDriver::KeyPressed( std::string key )
{
    mGame.KeyPressed( key );
}
}
