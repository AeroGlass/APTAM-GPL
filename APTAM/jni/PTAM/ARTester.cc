// Copyright 2015 ICGJKU
#include "ARTester.h"
#include "Map.h"
#include "MapPoint.h"

namespace APTAM {

using namespace CVD;


ARTester::ARTester()
{
	keyPressed = false;
	selectionStatus = 0;
}

extern Vector<3> gavLevelColors[];

void ARTester::DrawStuff( SE3<> se3CfromW, Map &map)
{
  Vector<3> v3CameraPos = se3CfromW.inverse().get_translation();

  CheckGLError("Draw3D Begin");
  glEnable(GL_BLEND);
#ifndef USE_OGL2 //todo
  glEnable(GL_CULL_FACE);
  glFrontFace(GL_CW);
#endif
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  CheckGLError("draw3d states done");

  if(keyPressed)
  {
	  __android_log_print(ANDROID_LOG_INFO, "ARTester", "key pressed");
	  if(selectionStatus==0)
	  {
		  selectionStatus = 1;
		  lastCameraPose = se3CfromW;
	  }
	  else
	  {
		  objectPose.get_translation() += (v3CameraPos - lastCameraPose.inverse().get_translation());
		  objectPose.get_rotation() = objectPose.get_rotation()*(lastCameraPose.get_rotation().inverse()*se3CfromW.get_rotation()).inverse();
		  selectionStatus=0;
	  }
	  keyPressed = false;
  }

  Vector<3> translation = objectPose.get_translation();
  SO3<> rotation = objectPose.get_rotation();
  if(selectionStatus==1)
  {
	  translation = translation + (v3CameraPos - lastCameraPose.inverse().get_translation());
	  rotation = rotation*(lastCameraPose.get_rotation().inverse()*se3CfromW.get_rotation()).inverse();
  }

    //draw features

  //render points
  map.LockMap();

  			//TODO: test if points are visible

  			//glLoadIdentity();
  			glEnable(GL_DEPTH_TEST);
  			//glDisable(GL_DEPTH_TEST);

  			int nForMass = 0;
  			glPointSize(8);

  			const int npts = map.vpPoints.size();

  			std::vector<GLfloat> pcol(4 * npts);
  			std::vector<GLfloat> pvtx(3 * npts);

  			int pcount = 0;

  			for (size_t i = 0; i<npts; i++)
  			{

  				Vector<3> v3Pos = map.vpPoints[i]->v3WorldPos;

  				if(selectionStatus==1)
  				{
  					pcol[pcount * 4 + 0] = 0.5;//gavLevelColors[map.vpPoints[i]->nSourceLevel][0];
  					pcol[pcount * 4 + 1] = 0.5;//gavLevelColors[map.vpPoints[i]->nSourceLevel][1];
  					pcol[pcount * 4 + 2] = 0.5;//gavLevelColors[map.vpPoints[i]->nSourceLevel][2];
  					pcol[pcount * 4 + 3] = 1;

					if (norm(v3Pos - translation) <= 0.5)
					{
						pcol[pcount * 4 + 0] = 1;
						pcol[pcount * 4 + 1] = 0;
						pcol[pcount * 4 + 2] = 0;
						pcol[pcount * 4 + 3] = 1;
					}
  				}
  				else
  				{
  					pcol[pcount * 4 + 0] = gavLevelColors[map.vpPoints[i]->nSourceLevel][0];
  					pcol[pcount * 4 + 1] = gavLevelColors[map.vpPoints[i]->nSourceLevel][1];
  					pcol[pcount * 4 + 2] = gavLevelColors[map.vpPoints[i]->nSourceLevel][2];
  					pcol[pcount * 4 + 3] = 1;
  					//if (norm(v3Pos - translation) > 0.5)
					//	pcol[i * 4 + 3] = 0.0;

  					if(!map.vpPoints[i]->bFoundRecent)
  					{
  						pcol[pcount * 4 + 3] = 0;
  						pcount--;
  					}
  				}

  				if(pcount >= 0)
  				{
					pvtx[pcount * 3 + 0] = v3Pos[0];
					pvtx[pcount * 3 + 1] = v3Pos[1];
					pvtx[pcount * 3 + 2] = v3Pos[2];
  				}

  				pcount++;

  			}
  			map.UnlockMap();

  	#ifndef USE_OGL2
  			glEnableClientState(GL_VERTEX_ARRAY);
  			glEnableClientState(GL_COLOR_ARRAY);
  	#else
  			gles2h.UseShader(3);
  			gles2h.SetDefaultUniforms();
  	#endif

  			glColorPointer(4, GL_FLOAT, 0, pcol.data());
  			glVertexPointer(3, GL_FLOAT, 0, pvtx.data());

  			glDrawArrays(GL_POINTS, 0, pcount);

  	#ifndef USE_OGL2
  			glDisableClientState(GL_COLOR_ARRAY);
  			glDisableClientState(GL_VERTEX_ARRAY);
  	#endif

  if(map.bTrackingGood)
  {
    //draw arrows
  	  float scale = 1;
	Vector<3> diff;
	diff[0] = scale;
	diff[1] = -scale;
	diff[2] = scale;


	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslate(translation);
	glMultMatrix(rotation.get_matrix());

	glColor4f(1.0, 0.0, 0.0, 1.0);
	glLineWidth(10);

	float ss = 0.1;

	GLfloat pts[] = {
		0, 0, 0,
		diff[0], 0, 0,
		0, 0, 0,
		0, diff[1], 0,
		0, 0, 0,
		0, 0, diff[2]
	};
	GLfloat acol[] = {
		1, 0, 0, 1,
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 1, 0, 1,
		1, 1, 1, 1,
		1, 1, 1, 1
	};
#ifndef USE_OGL2
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
#else
	gles2h.UseShader(3);
	gles2h.SetDefaultUniforms();
#endif
	glVertexPointer(3, GL_FLOAT, 0, pts);
	glColorPointer(4, GL_FLOAT, 0, acol);
	glDrawArrays(GL_LINES, 0, 2 * 3);
#ifndef USE_OGL2
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
#endif
  }

  CheckGLError("draw3d done");
};


void ARTester::KeyPressed( std::string key )
{
	if(key == "Space")
	{
		keyPressed = true;
	}
	
}

}

