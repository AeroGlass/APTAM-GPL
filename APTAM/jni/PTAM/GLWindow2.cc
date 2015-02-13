// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

#include "OpenGL.h"
#include "GLWindow2.h"
#include "GLWindowMenu.h"
#include <stdlib.h>
#include <gvars3/GStringUtil.h>
#include <gvars3/instances.h>
#include <TooN/helpers.h>

namespace APTAM {

using namespace CVD;
using namespace std;
using namespace GVars3;
using namespace TooN;

GLWindow2::GLWindow2(ImageRef irSize, string sTitle)
#ifndef __ANDROID__
  : GLWindow(irSize, sTitle)
#endif
{
#ifdef __ANDROID__
	mirWindowSize = irSize;
#endif
#ifdef WIN32
  // On windows, have to initialise GLEW at the start to enable access
  // to GL extensions
  static bool bGLEWIsInit = false;
  if(!bGLEWIsInit)
  {
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
		exit(0);
	}
	bGLEWIsInit = true;
  }
#endif

  mirVideoSize = irSize;
  GUI.RegisterCommand("GLWindow.AddMenu", GUICommandCallBack, this);
#ifndef __ANDROID__
  glSetFont("sans");
#endif
  mvMCPoseUpdate=Zeros;
  mvLeftPoseUpdate=Zeros;
};

#ifdef __ANDROID__
CVD::ImageRef GLWindow2::size() const
{
	return mirWindowSize;
}

void GLWindow2::resize(int w, int h)
{
	double var = (double)mirVideoSize.x/mirVideoSize.y;
	double war = (double)w/h;
	if(var < war)
	{
		mirWindowSize.x = h*var;
		mirWindowSize.y = h;
	}
	else
	{
		mirWindowSize.x = w;
		mirWindowSize.y = w/var;
	}
}
#endif

void GLWindow2::AddMenu(string sName, string sTitle)
{
  GLWindowMenu* pMenu = new GLWindowMenu(sName, sTitle); 
  mvpGLWindowMenus.push_back(pMenu);
}

void GLWindow2::GUICommandCallBack(void* ptr, string sCommand, string sParams)
{
  ((GLWindow2*) ptr)->GUICommandHandler(sCommand, sParams);
}

void GLWindow2::GUICommandHandler(string sCommand, string sParams)  // Called by the callback func..
{
  vector<string> vs=ChopAndUnquoteString(sParams);
  if(sCommand=="GLWindow.AddMenu")
    {
      switch(vs.size())
	{
	case 1:
	  AddMenu(vs[0], "Root");
	  return;
	case 2:
	  AddMenu(vs[0], vs[1]);
	  return;
	default:
	  cout << "? AddMenu: need one or two params (internal menu name, [caption])." << endl;
	  return;
	};
    };
  
  // Should have returned to caller by now - if got here, a command which 
  // was not handled was registered....
  cout << "! GLWindow::GUICommandHandler: unhandled command "<< sCommand << endl;
  exit(1);
}; 

void GLWindow2::DrawMenus()
{
#ifdef __ANDROID__
	glDisable(GL_STENCIL_TEST);
	  glDisable(GL_DEPTH_TEST);
#ifndef USE_OGL2
	  glDisable(GL_TEXTURE_2D);
#endif
	  //glDisable(GL_TEXTURE_CROP_RECT_OES);
	  //glDisable(GL_LINE_SMOOTH);
	  //glDisable(GL_POLYGON_SMOOTH);
	  glEnable(GL_BLEND);
	  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	  glColorMask(1,1,1,1);
	  glMatrixMode(GL_MODELVIEW);
	  glLoadIdentity();
	  SetupWindowOrtho();
	  //return;
	  glLineWidth(1);
#else
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_DEPTH_TEST);
#ifndef USE_OGL2
  glDisable(GL_TEXTURE_2D);
#endif
  glDisable(GL_TEXTURE_RECTANGLE_ARB);
  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_POLYGON_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColorMask(1,1,1,1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  SetupWindowOrtho();
  glLineWidth(1);
#endif
  
  int nTop = 30;
  int nHeight = 30;
  for(vector<GLWindowMenu*>::iterator i = mvpGLWindowMenus.begin();
      i!= mvpGLWindowMenus.end();
      i++)
    {
      (*i)->Render(nTop, nHeight, size()[0], *this);
      nTop+=nHeight+1;
    }
  
}

void GLWindow2::SetupUnitOrtho()
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
#ifdef __ANDROID__
  glOrthof(0,1,1,0,0,1);
#else
  glOrtho(0,1,1,0,0,1);
#endif
}

void GLWindow2::SetupWindowOrtho()
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
#ifdef __ANDROID__
  ImageRef isize = size();
  glOrthof( -0.375, isize.x - 0.375, isize.y - 0.375, -0.375, -1, 1 );
#else
  glOrtho(size());
#endif
}

void GLWindow2::SetupVideoOrtho()
{
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
#ifdef __ANDROID__
  glOrthof(-0.5,(double)mirVideoSize.x - 0.5, (double) mirVideoSize.y - 0.5, -0.5, -1.0, 1.0);
#else
  glOrtho(-0.5,(double)mirVideoSize.x - 0.5, (double) mirVideoSize.y - 0.5, -0.5, -1.0, 1.0);
#endif
}

void GLWindow2::SetupVideoRasterPosAndZoom()
{ 
#ifdef __ANDROID__
	//TODO
#else
  glRasterPos2d(-0.5,-0.5);
  double adZoom[2];
  adZoom[0] = (double) size()[0] / (double) mirVideoSize[0];
  adZoom[1] = (double) size()[1] / (double) mirVideoSize[1];
  glPixelZoom(adZoom[0], -adZoom[1]);
#endif
}

void GLWindow2::SetupViewport()
{
  glViewport(0, 0, size()[0], size()[1]);
}

void GLWindow2::PrintString(CVD::ImageRef irPos, std::string s)
{
  glMatrixMode(GL_PROJECTION);

  glPushMatrix();

#ifdef __ANDROID__
  gles2h.UseShader(4);
  gles2h.SetDefaultUniforms();
  drawTextJava(s,irPos,gles2h.GetActiveShader()); //bad hack
#else
  glTranslatef(irPos.x, irPos.y, 0.0);
  glScalef(8,-8,1);
  glDrawText(s, NICE, 1.6, 0.1);
#endif
  glPopMatrix();
}

void GLWindow2::DrawCaption(string s)
{
  if(s.length() == 0)
    return;
  
  SetupWindowOrtho();
  // Find out how many lines are in the caption:
  // Count the endls
  int nLines = 0;
  {
    string sendl("\n");
    string::size_type st=0;
    while(1)
      {
	nLines++;
	st = s.find(sendl, st);
	if(st==string::npos)
	  break;
	else
	  st++;
      }
  }
  
  int nTopOfBox = size().y - nLines * 17;
  
#ifdef __ANDROID__
  // Draw a grey background box for the text
    glColor4f(0,0,0,0.4);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GLfloat vtx[] = {
      -0.5,                           static_cast<GLfloat>(nTopOfBox),
      static_cast<GLfloat>(size().x), static_cast<GLfloat>(nTopOfBox),
      static_cast<GLfloat>(size().x), static_cast<GLfloat>(size().y),
      -0.5,                           static_cast<GLfloat>(size().y)
    };
    GLfloat col[] = {
      0,0,0,0.4,
      0,0,0,0.4,
      0,0,0,0.4,
      0,0,0,0.4
    };
#ifndef USE_OGL2
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
#else
    gles2h.UseShader(0);
    gles2h.SetDefaultUniforms();
#endif

    glVertexPointer(2, GL_FLOAT, 0, vtx);
    //glColorPointer(4, GL_FLOAT, 0, col);
    glDrawArrays(GL_TRIANGLE_FAN,0,4);

#ifndef USE_OGL2
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
#endif

#else
  // Draw a grey background box for the text
  glColor4f(0,0,0,0.4);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_QUADS);
  glVertex2d(-0.5, nTopOfBox);
  glVertex2d(size().x, nTopOfBox);
  glVertex2d(size().x, size().y);
  glVertex2d(-0.5, size().y);
  glEnd();
#endif
  
  // Draw the caption text in yellow
  glColor4f(1,1,0,1);
  PrintString(ImageRef(10,nTopOfBox + 13), s);
}


void GLWindow2::HandlePendingEvents()
{
#ifndef __ANDROID__
  handle_events(*this);
#endif
}

#ifdef __ANDROID__
void GLWindow2::on_mouse_move(GLWindow2& win, CVD::ImageRef where, int state)
#else
void GLWindow2::on_mouse_move(GLWindow& win, CVD::ImageRef where, int state)
#endif
{
  ImageRef irMotion = where - mirLastMousePos;
  mirLastMousePos = where;
  
#ifdef __ANDROID__
  //TODO
#else
  double dSensitivity = 0.01;
  if(state & BUTTON_LEFT && ! (state & BUTTON_RIGHT))
    {
      mvMCPoseUpdate[3] -= irMotion[1] * dSensitivity;
      mvMCPoseUpdate[4] += irMotion[0] * dSensitivity;
    }
  else if(!(state & BUTTON_LEFT) && state & BUTTON_RIGHT)
    {
      mvLeftPoseUpdate[4] -= irMotion[0] * dSensitivity;
      mvLeftPoseUpdate[3] += irMotion[1] * dSensitivity;
    }
  else if(state & BUTTON_MIDDLE  || (state & BUTTON_LEFT && state & BUTTON_RIGHT))
    {
      mvLeftPoseUpdate[5] -= irMotion[0] * dSensitivity;
      mvLeftPoseUpdate[2] += irMotion[1] * dSensitivity;
    }
#endif
  
}

#ifdef __ANDROID__
void GLWindow2::on_mouse_down(GLWindow2& win, CVD::ImageRef where, int state, int button)
#else
void GLWindow2::on_mouse_down(GLWindow& win, CVD::ImageRef where, int state, int button)
#endif
{
   bool bHandled = false;
  for(unsigned int i=0; !bHandled && i<mvpGLWindowMenus.size(); i++)
    bHandled = mvpGLWindowMenus[i]->HandleClick(button, state, where.x, where.y);
}

#ifdef __ANDROID__
void GLWindow2::on_mouse_down(int x, int y)
{
	on_mouse_down(*this,CVD::ImageRef(x,y),0,0);
}
#endif


#ifdef __ANDROID__
void GLWindow2::on_event(GLWindow2& win, int event)
{
  //if(event==EVENT_CLOSE) //TODO
    GUI.ParseLine("quit");
}
#else
void GLWindow2::on_event(GLWindow& win, int event)
{
  if(event==EVENT_CLOSE)
    GUI.ParseLine("quit");
}
#endif


pair<Vector<6>, Vector<6> > GLWindow2::GetMousePoseUpdate()
{
  pair<Vector<6>, Vector<6> > result = make_pair(mvLeftPoseUpdate, mvMCPoseUpdate);
  mvLeftPoseUpdate = Zeros;
  mvMCPoseUpdate = Zeros;
  return result;
}


//#ifndef WIN32 || __ANDROID_API__
#if !defined(WIN32) && !defined(__ANDROID__)
#include <X11/keysym.h>
void GLWindow2::on_key_down(GLWindow&, int k)
{
  string s;
  switch(k)
    {
    case XK_a:   case XK_A:  s="a"; break;
    case XK_b:   case XK_B:  s="b"; break;
    case XK_c:   case XK_C:  s="c"; break;
    case XK_d:   case XK_D:  s="d"; break;
    case XK_e:   case XK_E:  s="e"; break;
    case XK_f:   case XK_F:  s="f"; break;
    case XK_g:   case XK_G:  s="g"; break;
    case XK_h:   case XK_H:  s="h"; break;
    case XK_i:   case XK_I:  s="i"; break;
    case XK_j:   case XK_J:  s="j"; break;
    case XK_k:   case XK_K:  s="k"; break;
    case XK_l:   case XK_L:  s="l"; break;
    case XK_m:   case XK_M:  s="m"; break;
    case XK_n:   case XK_N:  s="n"; break;
    case XK_o:   case XK_O:  s="o"; break;
    case XK_p:   case XK_P:  s="p"; break;
    case XK_q:   case XK_Q:  s="q"; break;
    case XK_r:   case XK_R:  s="r"; break;
    case XK_s:   case XK_S:  s="s"; break;
    case XK_t:   case XK_T:  s="t"; break;
    case XK_u:   case XK_U:  s="u"; break;
    case XK_v:   case XK_V:  s="v"; break;
    case XK_w:   case XK_W:  s="w"; break;
    case XK_x:   case XK_X:  s="x"; break;
    case XK_y:   case XK_Y:  s="y"; break;
    case XK_z:   case XK_Z:  s="z"; break;
    case XK_1:   s="1"; break;
    case XK_2:   s="2"; break;
    case XK_3:   s="3"; break;
    case XK_4:   s="4"; break;
    case XK_5:   s="5"; break;
    case XK_6:   s="6"; break;
    case XK_7:   s="7"; break;
    case XK_8:   s="8"; break;
    case XK_9:   s="9"; break;
    case XK_0:   s="0"; break;
    case XK_KP_Prior: case XK_Page_Up:     s="PageUp"; break;
    case XK_KP_Next:  case XK_Page_Down:   s="PageDown"; break;
    case XK_Return: s="Enter"; break;
    case XK_space:  s="Space"; break;
    case XK_BackSpace:  s="BackSpace"; break;
    case XK_Escape:  s="Escape"; break;
    default: ;
    }

  if(s!="")
    GUI.ParseLine("try KeyPress "+s);
}
#else
#ifdef __ANDROID__
void GLWindow2::on_key_down(GLWindow2&, int k)
#else
void GLWindow2::on_key_down(GLWindow&, int k)
#endif
{
  string s;
  // ASCI chars can be mapped directly:
  if( (k >= 48 && k <=57) || ( k >=97 && k <= 122) || (k >= 65 && k <= 90))
  {
	char c = k;
	if(c >= 65 && c <= 90)
		c = c + 32;
	s = c;
  }
  else switch (k) // Some special chars are translated:
  {
    case 33: s="PageUp"; break;
    case 34: s="PageDown"; break;
    case 13: s="Enter"; break;
    case 32:  s="Space"; break;
    case 8:  s="BackSpace"; break;
    case 27:  s="Escape"; break;
    default: break;
  }
  
  if(s!="")
    GUI.ParseLine("try KeyPress "+s);
}
#endif

#ifdef __ANDROID__
void GLWindow2::on_key_down(int keycode)
{
	on_key_down(*this,keycode);
}
#endif


}

