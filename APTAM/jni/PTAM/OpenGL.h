// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015
#ifndef __OPENGL_INCLUDES_H
#define __OPENGL_INCLUDES_H

#ifdef __ANDROID__

#define USE_OGL2

#ifdef USE_OGL2
#include "opengles2helper.h"
#include "gles2_helpers.h"
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <cvd/gles1_helpers.h>
#endif

#include <android/log.h>
#include <cvd/image.h>

inline void CheckGLError(const char* prefix)
{
	GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
	  //cerr << "OpenGL error: " << err << endl;
	  __android_log_print(ANDROID_LOG_INFO, "opengl error", "%s: %d",prefix, err);
  }
}

static inline int cvdalignof(const void* ptr)
			{
				size_t p = (size_t)ptr;

				if(p&3)
					if(p&1)
						return 1;
					else
						return 2;
				else
					if(p&4)
						return 4;
					else
						return 8;
			}

template<class C> inline void glDrawPixels(const CVD::SubImage<C>& im)
{
	CheckGLError("before gltex");

#ifndef USE_OGL2
	glEnable(GL_TEXTURE_2D);
	CheckGLError("gltex2d");
#endif
	glActiveTexture(GL_TEXTURE0);
	CheckGLError("gltex-active");
#ifndef USE_OGL2
	glColor4f(1.0f,1.0f,1.0f,1.0f);
#endif

	GLuint texture;
	glGenTextures(1, &texture);
	CheckGLError("gltex1");
	glBindTexture(GL_TEXTURE_2D, texture);
	CheckGLError("gltex2");
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, CVD::gl::data<C>::format, 1024, 1024, 0, CVD::gl::data<C>::format, CVD::gl::data<C>::type, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, CVD::gl::data<C>::format, im.size().x, im.size().y, 0, CVD::gl::data<C>::format, CVD::gl::data<C>::type, NULL);
	CheckGLError("gltex3");

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	//int cropRect[4] = {0,0,im.size().x,im.size().y};
	//glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cropRect);
	CheckGLError("gltex4");

	glPixelStorei(GL_UNPACK_ALIGNMENT, std::min(cvdalignof(im[0]), cvdalignof(im[1])));
	CheckGLError("gltex4.1");
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, imFrameBW.row_stride());
	//CheckGLError("gltex4.2");

	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imFrameBW.size().x, imFrameBW.size().y, gl::data<byte>::format, gl::data<byte>::type, imFrameBW.data());
	const unsigned char * p = im.data();
	int ww = im.size().x;
	int stride = im.row_stride();
	int hh = im.size().y;
	//__android_log_print(ANDROID_LOG_INFO, "renderim", "%d %d %d t: %d",ww,stride,hh, texture);
	for(int dy = 0; dy < hh; dy++){
		glTexSubImage2D(GL_TEXTURE_2D, 0,   0, hh-dy-1, ww, 1, CVD::gl::data<C>::format, CVD::gl::data<C>::type, p);
		p += stride;
	}
	CheckGLError("gltex5");
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	//CheckGLError("gltex5.1");

	//glDrawTexiOES(0, 0, 0, im.size().x,im.size().y);
	int x = 0;
	int y = 0;
	int w = im.size().x;
	int h = im.size().y;

	GLfloat vtx[] = {
	      static_cast<GLfloat>(x),   static_cast<GLfloat>(y),
	      static_cast<GLfloat>(x+w), static_cast<GLfloat>(y),
	      static_cast<GLfloat>(x+w), static_cast<GLfloat>(y+h),
	      static_cast<GLfloat>(x),   static_cast<GLfloat>(y+h)
	    };

	GLfloat tcoord[] = {
		      static_cast<GLfloat>(0),   static_cast<GLfloat>(1),
		      static_cast<GLfloat>(1), static_cast<GLfloat>(1),
		      static_cast<GLfloat>(1), static_cast<GLfloat>(0),
		      static_cast<GLfloat>(0),   static_cast<GLfloat>(0)
		    };

#ifndef USE_OGL2
	glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#else
    gles2h.UseShader(1);
    gles2h.SetDefaultUniforms();
#endif

    glVertexPointer(2, GL_FLOAT, 0, vtx);
    glTexCoordPointer(2, GL_FLOAT, 0, tcoord);

    //glDrawElements(GL_TRIANGLE_STRIP,idx.size(),GL_UNSIGNED_SHORT,idx.data());
    glDrawArrays(GL_TRIANGLE_FAN,0,4);

#ifndef USE_OGL2
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif

	CheckGLError("gltex6");
	glDeleteTextures(1,&texture);
	CheckGLError("gltex7");
}

#else

#ifdef _LINUX
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#ifdef _OSX
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <GL/glew.h>
#endif

#include <cvd/gl_helpers.h>
#include <iostream>
#include <fstream>

inline void CheckGLError(const char* prefix)
{
	GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
	  std::cerr << "OpenGL error: " << prefix << " " << err << std::endl;
	  //__android_log_print(ANDROID_LOG_INFO, "opengl error", "%s: %d",prefix, err);
  }
}

#endif

#endif
