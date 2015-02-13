// Copyright 2008 Isis Innovation Limited
// Modified by ICGJKU 2015

#include "ATANCamera.h"
#include <TooN/helpers.h>
#include <cvd/vector_image_ref.h>
#include <iostream>
#include <algorithm>
#include <gvars3/instances.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace APTAM {

using namespace std;
using namespace CVD;
using namespace GVars3;

ATANCamera::ATANCamera()
{
	msName = "";
	mvCameraParams = mvDefaultParams;
	mvImageSize[0] = 640.0;
	mvImageSize[1] = 480.0;
	useOpenCVDistortion=false;
	RefreshParams();
}

ATANCamera::ATANCamera(string sName)
{
  // The camera name is used to find the camera's parameters in a GVar.
  msName = sName;
  GV2.Register(mgvvCameraParams, sName+".Parameters", mvDefaultParams, HIDDEN | FATAL_IF_NOT_DEFINED);
  mvCameraParams = *mgvvCameraParams;
  mvImageSize[0] = 640.0; //todo??
  mvImageSize[1] = 480.0;
  useOpenCVDistortion = GV2.GetInt(sName+".UseOpenCVDistortion",false);
  if(useOpenCVDistortion)
     {
		 opencv_camparams = GV3::get<Vector<9>>(sName+".OpenCVParameters",opencv_camparams,HIDDEN | FATAL_IF_NOT_DEFINED);

		 //cout << opencv_camparams[4];

#ifdef __ANDROID__
	  __android_log_print(ANDROID_LOG_INFO, "Camera", "OpenCV is used");
#endif

  	   /*mvCameraParams[0] = opencv_camparams[0]/mvImageSize[0];
  	   mvCameraParams[1] = opencv_camparams[1]/mvImageSize[1];
  	   mvCameraParams[2] = (opencv_camparams[2]+0.5)/mvImageSize[0];
  	   mvCameraParams[3] = (opencv_camparams[3]+0.5)/mvImageSize[1];
  	   mvCameraParams[4] = 0;*/
     }
  RefreshParams();
}


ATANCamera::ATANCamera(string sName, ImageRef irSize, Vector<NUMTRACKERCAMPARAMETERS> vParams)
{
  msName = sName;
  GV2.Register(mgvvCameraParams, sName+".Parameters", vParams, HIDDEN );
  mvCameraParams = vParams; //force overwrite (this is a hack!!!)
  mvImageSize =  vec(irSize);
  useOpenCVDistortion = false;
  RefreshParams();
}

ATANCamera::ATANCamera(string sName, ImageRef irSize, Vector<9> vParams)
{
  msName = sName;
  GV2.Register(mgvvCameraParams, sName+".Parameters", mvDefaultParams, HIDDEN ); //required??
  opencv_camparams = vParams; 
  mvImageSize =  vec(irSize);
  useOpenCVDistortion = true;

  /*if(useOpenCVDistortion)
   {
	   mvCameraParams[0] = opencv_camparams[0]/mvImageSize[0];
	   mvCameraParams[1] = opencv_camparams[1]/mvImageSize[1];
	   mvCameraParams[2] = (opencv_camparams[2]+0.5)/mvImageSize[0];
	   mvCameraParams[3] = (opencv_camparams[3]+0.5)/mvImageSize[1];
	   mvCameraParams[4] = 0;
   }*/

  RefreshParams();
}


void ATANCamera::SetImageSize(Vector<2> vImageSize)
{
  mvImageSize = vImageSize;
  RefreshParams();
};

void ATANCamera::RefreshParams() 
{
  // This updates internal member variables according to the current camera parameters,
  // and the currently selected target image size.
  //


   if(useOpenCVDistortion)
   {
	   mvCameraParams[0] = opencv_camparams[0]/mvImageSize[0];
	   mvCameraParams[1] = opencv_camparams[1]/mvImageSize[1];
	   mvCameraParams[2] = (opencv_camparams[2]+0.5)/mvImageSize[0];
	   mvCameraParams[3] = (opencv_camparams[3]+0.5)/mvImageSize[1];
	   mvCameraParams[4] = 0;
   }

  Vector<NUMTRACKERCAMPARAMETERS> cameraParams = mvCameraParams;

  // First: Focal length and image center in pixel coordinates
  mvFocal[0] = mvImageSize[0] * (cameraParams)[0];
  mvFocal[1] = mvImageSize[1] * (cameraParams)[1];
  mvCenter[0] = mvImageSize[0] * (cameraParams)[2] - 0.5;
  mvCenter[1] = mvImageSize[1] * (cameraParams)[3] - 0.5;
  
  // One over focal length
  mvInvFocal[0] = 1.0 / mvFocal[0];
  mvInvFocal[1] = 1.0 / mvFocal[1];

  // Some radial distortion parameters..
  mdW =  (cameraParams)[4];
  if(mdW != 0.0)
    {
      md2Tan = 2.0 * tan(mdW / 2.0);
      mdOneOver2Tan = 1.0 / md2Tan;
      mdWinv = 1.0 / mdW;
      mdDistortionEnabled = 1.0;
    }
  else
    {
      mdWinv = 0.0;
      md2Tan = 0.0;
      mdDistortionEnabled = 0.0;
    }
  
  // work out biggest radius in image
  Vector<2> v2;
  v2[0]= max((cameraParams)[2], (DefaultPrecision)1.0 - (cameraParams)[2]) / (cameraParams)[0];
  v2[1]= max((cameraParams)[3], (DefaultPrecision)1.0 - (cameraParams)[3]) / (cameraParams)[1];
  mdLargestRadius = invrtrans(sqrt(v2*v2));
  
  // At what stage does the model become invalid?
  mdMaxR = 1.5 * mdLargestRadius; // (pretty arbitrary)

  // work out world radius of one pixel
  // (This only really makes sense for square-ish pixels)
  {
    Vector<2> v2Center = UnProject(mvImageSize / 2);
    Vector<2> v2RootTwoAway = UnProject(mvImageSize / 2 + vec(ImageRef(1,1)));
    Vector<2> v2Diff = v2Center - v2RootTwoAway;
    mdOnePixelDist = sqrt(v2Diff * v2Diff) / sqrt(2.0);
  }
  
  // Work out the linear projection values for the UFB
  {
    // First: Find out how big the linear bounding rectangle must be
    vector<Vector<2> > vv2Verts;
    if(useOpenCVDistortion)
    {
    	vv2Verts.push_back(UnProject(makeVector( -0.5, -0.5+mvImageSize[1]/2)));
    	vv2Verts.push_back(UnProject(makeVector( mvImageSize[0]/2-0.5, -0.5)));
    	vv2Verts.push_back(UnProject(makeVector( mvImageSize[0]-0.5, mvImageSize[1]/2-0.5)));
    	vv2Verts.push_back(UnProject(makeVector( mvImageSize[0]/2-0.5, mvImageSize[1]-0.5)));
    	//todo: still not perfect
    }
    else
    {
		vv2Verts.push_back(UnProject(makeVector( -0.5, -0.5)));
		vv2Verts.push_back(UnProject(makeVector( mvImageSize[0]-0.5, -0.5)));
		vv2Verts.push_back(UnProject(makeVector( mvImageSize[0]-0.5, mvImageSize[1]-0.5)));
		vv2Verts.push_back(UnProject(makeVector( -0.5, mvImageSize[1]-0.5)));
    }
    Vector<2> v2Min = vv2Verts[0];
    Vector<2> v2Max = vv2Verts[0];
    for(int i=0; i<4; i++)
      for(int j=0; j<2; j++)
	{
	  if(vv2Verts[i][j] < v2Min[j]) v2Min[j] = vv2Verts[i][j];
	  if(vv2Verts[i][j] > v2Max[j]) v2Max[j] = vv2Verts[i][j];
	}
    if(useOpenCVDistortion)
    {
    	v2Min = v2Min*1.01; //hack!!!!
    	v2Max = v2Max*1.01;
    }
    mvImplaneTL = v2Min;
    mvImplaneBR = v2Max;
    
    // Store projection parameters to fill this bounding box
    Vector<2> v2Range = v2Max - v2Min;
    mvUFBLinearInvFocal = v2Range;
    mvUFBLinearFocal[0] = 1.0 / mvUFBLinearInvFocal[0];
    mvUFBLinearFocal[1] = 1.0 / mvUFBLinearInvFocal[1];
    mvUFBLinearCenter[0] = -1.0 * v2Min[0] * mvUFBLinearFocal[0];
    mvUFBLinearCenter[1] = -1.0 * v2Min[1] * mvUFBLinearFocal[1];
  }
  
}

// Project from the camera z=1 plane to image pixels,
// while storing intermediate calculation results in member variables
Vector<2> ATANCamera::Project(const Vector<2>& vCam){

	double ox, oy;
	if(useOpenCVDistortion)
	{

		  mvLastCam = vCam;
		  mdLastR = sqrt(vCam * vCam);
		  mbInvalid = (mdLastR > mdMaxR);

		  /*double x = mvLastCam[0];//(x - cx)*ifx;
		  double y = mvLastCam[1];//(y - cy)*ify;

		  int iters = 5;
		  // compensate distortion iteratively
		  for(int j = 0; j < iters; j++ )
		  {
				  double r2 = x*x + y*y;
				  double icdist = 1./(1 + ((opencv_camparams[8]*r2 + opencv_camparams[5])*r2 + opencv_camparams[4])*r2);
				  double deltaX = 2*opencv_camparams[6]*x*y + opencv_camparams[7]*(r2 + 2*x*x);
				  double deltaY = opencv_camparams[6]*(r2 + 2*y*y) + 2*opencv_camparams[7]*x*y;
				  x = (mvLastCam[0] - deltaX)*icdist;
				  y = (mvLastCam[1] - deltaY)*icdist;
		  }

		  mvLastDistCam[0] = x;
		  mvLastDistCam[1] = y;//*/

		  DefaultPrecision r2 = mvLastCam * mvLastCam;
		  DefaultPrecision r4 = r2*r2;
		  DefaultPrecision r6 = r4*r2;
		  mdLastFactor = 1+opencv_camparams[4]*r2+opencv_camparams[5]*r4+opencv_camparams[8]*r6;

		  mvLastDistCam[0] = mdLastFactor * mvLastCam[0] + 2*opencv_camparams[6]*mvLastCam[0]*mvLastCam[1] + opencv_camparams[7]*(r2 + 2*mvLastCam[0]*mvLastCam[0]);
		  mvLastDistCam[1] = mdLastFactor * mvLastCam[1] + opencv_camparams[6]*(r2 + 2*mvLastCam[1]*mvLastCam[1]) + 2*opencv_camparams[7]*mvLastCam[0]*mvLastCam[1];//*/

		  //ox=mvLastIm[0] = opencv_camparams[2] + opencv_camparams[0] * mvLastDistCam[0];
		  //oy=mvLastIm[1] = opencv_camparams[3] + opencv_camparams[1] * mvLastDistCam[1];*/

		  mvLastIm[0] = mvCenter[0] + mvFocal[0] * mvLastDistCam[0];
		  mvLastIm[1] = mvCenter[1] + mvFocal[1] * mvLastDistCam[1];

		  return mvLastIm;
	}
	else
	{
	  mvLastCam = vCam;
	  mdLastR = sqrt(vCam * vCam);
	  mbInvalid = (mdLastR > mdMaxR);

	  mdLastFactor = rtrans_factor(mdLastR);
	  mdLastDistR = mdLastFactor * mdLastR;
	  mvLastDistCam = mdLastFactor * mvLastCam;

	  mvLastIm[0] = mvCenter[0] + mvFocal[0] * mvLastDistCam[0];
	  mvLastIm[1] = mvCenter[1] + mvFocal[1] * mvLastDistCam[1];

#ifdef __ANDROID__
	  //__android_log_print(ANDROID_LOG_INFO, "Project", "normal %f %f opencv %f %f",mvLastIm[0],mvLastIm[1],ox,oy);
#endif

	  return mvLastIm;
	}
}

// Un-project from image pixel coords to the camera z=1 plane
// while storing intermediate calculation results in member variables
Vector<2> ATANCamera::UnProject(const Vector<2>& v2Im)
{
	double ox,oy,ox2,oy2;
	if(useOpenCVDistortion)
	{
		 mvLastIm = v2Im;
		  //mvLastDistCam[0] = (mvLastIm[0] - opencv_camparams[2]) / opencv_camparams[0];
		  //mvLastDistCam[1] = (mvLastIm[1] - opencv_camparams[3]) / opencv_camparams[1];
		  mvLastDistCam[0] = (mvLastIm[0] - mvCenter[0]) * mvInvFocal[0];
		  mvLastDistCam[1] = (mvLastIm[1] - mvCenter[1]) * mvInvFocal[1];
		  mdLastDistR = sqrt(mvLastDistCam * mvLastDistCam);

		  double x = mvLastDistCam[0];//(x - cx)*ifx;
		  double y = mvLastDistCam[1];//(y - cy)*ify;

		  int iters = 5;
		  // compensate distortion iteratively
		  for(int j = 0; j < iters; j++ )
		  {
		          double r2 = x*x + y*y;
		          double icdist = 1./(1 + ((opencv_camparams[8]*r2 + opencv_camparams[5])*r2 + opencv_camparams[4])*r2);
		          double deltaX = 2*opencv_camparams[6]*x*y + opencv_camparams[7]*(r2 + 2*x*x);
		          double deltaY = opencv_camparams[6]*(r2 + 2*y*y) + 2*opencv_camparams[7]*x*y;
		          x = (mvLastDistCam[0] - deltaX)*icdist;
		          y = (mvLastDistCam[1] - deltaY)*icdist;
		  }

		  /*DefaultPrecision r2 = mvLastDistCam * mvLastDistCam;
		  DefaultPrecision r4 = r2*r2;
		  DefaultPrecision r6 = r4*r2;
		  mdLastFactor = 1+opencv_distparams[0]*r2+opencv_distparams[1]*r4+opencv_distparams[4]*r6;

		  ox2 = mdLastFactor * mvLastDistCam[0] + 2*opencv_distparams[2]*mvLastDistCam[0]*mvLastDistCam[1] + opencv_distparams[3]*(r2 + 2*mvLastDistCam[0]*mvLastDistCam[0]);
		  oy2 = mdLastFactor * mvLastDistCam[1] + opencv_distparams[2]*(r2 + 2*mvLastDistCam[1]*mvLastDistCam[1]) + 2*opencv_distparams[3]*mvLastDistCam[0]*mvLastDistCam[1];*/


		  ox=mvLastCam[0] = x;
		  oy=mvLastCam[1] = y;
		  return mvLastCam;
	}
	else
	{
	  mvLastIm = v2Im;
	  mvLastDistCam[0] = (mvLastIm[0] - mvCenter[0]) * mvInvFocal[0];
	  mvLastDistCam[1] = (mvLastIm[1] - mvCenter[1]) * mvInvFocal[1];
	  mdLastDistR = sqrt(mvLastDistCam * mvLastDistCam);

	  mdLastR = invrtrans(mdLastDistR);
	  double dFactor;
	  if(mdLastDistR > 0.01)
		dFactor =  mdLastR / mdLastDistR;
	  else
		dFactor = 1.0;
	  mdLastFactor = 1.0 / dFactor;
	  mvLastCam = dFactor * mvLastDistCam;
	#ifdef __ANDROID__
		  //__android_log_print(ANDROID_LOG_INFO, "UnProject", "normal %f %f opencv %f %f opencv2 %f %f",mvLastCam[0],mvLastCam[1],ox,oy,ox2,oy2);
	#endif
	  return mvLastCam;
	}
}

// Utility function for easy drawing with OpenGL
// C.f. comment in top of ATANCamera.h
Matrix<4> ATANCamera::MakeUFBLinearFrustumMatrix(double near, double far)
{
  Matrix<4> m4 = Zeros;
  

  double left = mvImplaneTL[0] * near;
  double right = mvImplaneBR[0] * near;
  double top = mvImplaneTL[1] * near;
  double bottom = mvImplaneBR[1] * near;

  
  // The openGhelL frustum manpage is A PACK OF LIES!!
  // Two of the elements are NOT what the manpage says they should be.
  // Anyway, below code makes a frustum projection matrix
  // Which projects a RHS-coord frame with +z in front of the camera
  // Which is what I usually want, instead of glFrustum's LHS, -z idea.
  m4[0][0] = (2 * near) / (right - left);
  m4[1][1] = (2 * near) / (top - bottom);
  
  m4[0][2] = (right + left) / (left - right);
  m4[1][2] = (top + bottom) / (bottom - top);
  m4[2][2] = (far + near) / (far - near);
  m4[3][2] = 1;
  
  m4[2][3] = 2*near*far / (near - far);

  return m4;
};

Matrix<2,2> ATANCamera::GetProjectionDerivs()
{
  // get the derivative of image frame wrt camera z=1 frame at the last computed projection
  // in the form (d im1/d cam1, d im1/d cam2)
  //             (d im2/d cam1, d im2/d cam2)
  
  DefaultPrecision dFracBydx;
  DefaultPrecision dFracBydy;
  

  DefaultPrecision &k = md2Tan;
  DefaultPrecision &x = mvLastCam[0];
  DefaultPrecision &y = mvLastCam[1];
  DefaultPrecision r = mdLastR * mdDistortionEnabled;
  
  if(useOpenCVDistortion)
    {
  	  //TODO
  	  //r=0;
  	  mdLastFactor=1;
  	  mdWinv = 0;
    }

  if(r < 0.01)
    {
      dFracBydx = 0.0;
      dFracBydy = 0.0;
    }
  else
    {
      dFracBydx = mdWinv * (k * x) / (r*r*(1 + k*k*r*r)) - x * mdLastFactor / (r*r);
      dFracBydy = mdWinv * (k * y) / (r*r*(1 + k*k*r*r)) - y * mdLastFactor / (r*r);
    }
  
  Matrix<2> m2Derivs;
  
  m2Derivs[0][0] = mvFocal[0] * (dFracBydx * x + mdLastFactor);  
  m2Derivs[1][0] = mvFocal[1] * (dFracBydx * y);  
  m2Derivs[0][1] = mvFocal[0] * (dFracBydy * x);  
  m2Derivs[1][1] = mvFocal[1] * (dFracBydy * y + mdLastFactor);  
  return m2Derivs;
}

Matrix<2,NUMTRACKERCAMPARAMETERS> ATANCamera::GetCameraParameterDerivs()
{
  // Differentials wrt to the camera parameters
  // Use these to calibrate the camera
  // No need for this to be quick, so do them numerically
  
  Matrix<2, NUMTRACKERCAMPARAMETERS> m2NNumDerivs;

  Vector<NUMTRACKERCAMPARAMETERS> vNNormal = *mgvvCameraParams;
  Vector<2> v2Cam = mvLastCam;
  Vector<2> v2Out = Project(v2Cam);
  for(int i=0; i<NUMTRACKERCAMPARAMETERS; i++)
	{
	  if(i == NUMTRACKERCAMPARAMETERS-1 && mdW == 0.0)
	continue;
	  Vector<NUMTRACKERCAMPARAMETERS> vNUpdate;
	  vNUpdate = Zeros;
	  vNUpdate[i] += 0.001;
	  UpdateParams(vNUpdate);
	  Vector<2> v2Out_B = Project(v2Cam);
	  m2NNumDerivs.T()[i] = (v2Out_B - v2Out) / 0.001;
	  if(!useOpenCVDistortion) //hack!!
	    {
		  *mgvvCameraParams = vNNormal;
		  mvCameraParams = *mgvvCameraParams;
		  RefreshParams();
	    }
	}
  if(mdW == 0.0)
	m2NNumDerivs.T()[NUMTRACKERCAMPARAMETERS-1] = Zeros;
  return m2NNumDerivs;
}

void ATANCamera::UpdateParams(Vector<5> vUpdate)
{
	if(!useOpenCVDistortion)
	{
	  // Update the camera parameters; use this as part of camera calibration.
	  (*mgvvCameraParams) = (*mgvvCameraParams) + vUpdate;
	  mvCameraParams = *mgvvCameraParams;
	  RefreshParams();
	}
}

void ATANCamera::DisableRadialDistortion()
{
	if(false) //hack to compare old vs opencv distortion
	{
		//5.3290649350438764e+002 5.3498620721421366e+002 3.1904226905307098e+002 2.4199789799891897e+002
		//3.6044291475063783e-001 -1.6122374725670012e+000 1.8811558792869727e-003 -1.8205822384653062e-003 2.0416196370865922e+000
		opencv_camparams[0] = 5.3290649350438764e+002;
		  opencv_camparams[1] = 5.3498620721421366e+002;
		  opencv_camparams[2] = 3.1904226905307098e+002;
		  opencv_camparams[3] = 2.4199789799891897e+002;

		  opencv_camparams[4] = 3.6044291475063783e-001;
		  opencv_camparams[5] = -1.6122374725670012e+000;
		  opencv_camparams[6] = 1.8811558792869727e-003;
		  opencv_camparams[7] = -1.8205822384653062e-003;
		  opencv_camparams[8] = 2.0416196370865922e+000;

		  /*opencv_camparams[0] = 5.3018815370296795e+002;
		  		  opencv_camparams[1] = 5.3201511700428216e+002;
		  		  opencv_camparams[2] = 3.2233740278066796e+002;
		  		  opencv_camparams[3] = 2.3974472466780679e+002;

		  		  opencv_camparams[4] = 3.7890280416030037e-001;
		  		  opencv_camparams[5] = -1.6539408438529930e+000;
		  		  opencv_camparams[6] = -3.9448023581733103e-003;
		  		  opencv_camparams[7] = 2.4806928917877609e-003;
		  		  opencv_camparams[8] = 1.9966322216759664e+000;*/

		 //cout << opencv_camparams[4];

		  useOpenCVDistortion = true;

	   mvCameraParams[0] = opencv_camparams[0]/mvImageSize[0];
	   mvCameraParams[1] = opencv_camparams[1]/mvImageSize[1];
	   mvCameraParams[2] = (opencv_camparams[2]+0.5)/mvImageSize[0];
	   mvCameraParams[3] = (opencv_camparams[3]+0.5)/mvImageSize[1];
	   mvCameraParams[4] = 0;

		  RefreshParams();
	}
	if(!useOpenCVDistortion)
	{
	  // Set the radial distortion parameter to zero
	  // This disables radial distortion and also disables its differentials
	  (*mgvvCameraParams)[NUMTRACKERCAMPARAMETERS-1] = 0.0;
	  mvCameraParams = *mgvvCameraParams;
	  RefreshParams();
	}
}

Vector<2> ATANCamera::UFBProject(const Vector<2>& vCam)
{
	if(useOpenCVDistortion)
		{
			  mvLastCam = vCam;
			  mdLastR = sqrt(vCam * vCam);
			  mbInvalid = (mdLastR > mdMaxR);

			  DefaultPrecision r2 = mvLastCam * mvLastCam;
			  DefaultPrecision r4 = r2*r2;
			  DefaultPrecision r6 = r4*r2;
			  mdLastFactor = 1+opencv_camparams[4]*r2+opencv_camparams[5]*r4+opencv_camparams[8]*r6;

			  mvLastDistCam[0] = mdLastFactor * mvLastCam[0] + 2*opencv_camparams[6]*mvLastCam[0]*mvLastCam[1] + opencv_camparams[7]*(r2 + 2*mvLastCam[0]*mvLastCam[0]);
			  mvLastDistCam[1] = mdLastFactor * mvLastCam[1] + opencv_camparams[6]*(r2 + 2*mvLastCam[1]*mvLastCam[1]) + 2*opencv_camparams[7]*mvLastCam[0]*mvLastCam[1];

			  //ox=mvLastIm[0] = opencv_camparams[2] + opencv_camparams[0] * mvLastDistCam[0];
			  //oy=mvLastIm[1] = opencv_camparams[3] + opencv_camparams[1] * mvLastDistCam[1];
			  mvLastIm[0] = (mvCameraParams)[2]  + (mvCameraParams)[0] * mvLastDistCam[0];
			  mvLastIm[1] = (mvCameraParams)[3]  + (mvCameraParams)[1] * mvLastDistCam[1];

			  return mvLastIm;
		}
		else
		{
			  // Project from camera z=1 plane to UFB, storing intermediate calc results.
			  mvLastCam = vCam;
			  mdLastR = sqrt(vCam * vCam);
			  mbInvalid = (mdLastR > mdMaxR);
			  mdLastFactor = rtrans_factor(mdLastR);
			  mdLastDistR = mdLastFactor * mdLastR;
			  mvLastDistCam = mdLastFactor * mvLastCam;

			  mvLastIm[0] = (mvCameraParams)[2]  + (mvCameraParams)[0] * mvLastDistCam[0];
			  mvLastIm[1] = (mvCameraParams)[3]  + (mvCameraParams)[1] * mvLastDistCam[1];
			  return mvLastIm;
		}
}

Vector<2> ATANCamera::UFBUnProject(const Vector<2>& v2Im)
{
	if(useOpenCVDistortion)
		{
			 mvLastIm = v2Im;
			  //mvLastDistCam[0] = (mvLastIm[0] - opencv_camparams[2]) / opencv_camparams[0];
			  //mvLastDistCam[1] = (mvLastIm[1] - opencv_camparams[3]) / opencv_camparams[1];
			 mvLastDistCam[0] = (mvLastIm[0] - (mvCameraParams)[2]) / (mvCameraParams)[0];
			 mvLastDistCam[1] = (mvLastIm[1] - (mvCameraParams)[3]) / (mvCameraParams)[1];

			  double x = mvLastDistCam[0];//(x - cx)*ifx;
			  double y = mvLastDistCam[1];//(y - cy)*ify;

			  int iters = 5;
			  // compensate distortion iteratively
			  for(int j = 0; j < iters; j++ )
			  {
			          double r2 = x*x + y*y;
			          double icdist = 1./(1 + ((opencv_camparams[8]*r2 + opencv_camparams[5])*r2 + opencv_camparams[4])*r2);
			          double deltaX = 2*opencv_camparams[6]*x*y + opencv_camparams[7]*(r2 + 2*x*x);
			          double deltaY = opencv_camparams[6]*(r2 + 2*y*y) + 2*opencv_camparams[7]*x*y;
			          x = (mvLastDistCam[0] - deltaX)*icdist;
			          y = (mvLastDistCam[1] - deltaY)*icdist;
			  }

			 mvLastCam[0] = x;
			 mvLastCam[1] = y;


			  /*DefaultPrecision r2 = mvLastDistCam * mvLastDistCam;
			  DefaultPrecision r4 = r2*r2;
			  DefaultPrecision r6 = r4*r2;
			  mdLastFactor = 1+opencv_distparams[0]*r2+opencv_distparams[1]*r4+opencv_distparams[4]*r6;

			  mvLastCam[0] = mdLastFactor * mvLastDistCam[0] + 2*opencv_distparams[2]*mvLastDistCam[0]*mvLastDistCam[1] + opencv_distparams[3]*(r2 + 2*mvLastDistCam[0]*mvLastDistCam[0]);
			  mvLastCam[1] = mdLastFactor * mvLastDistCam[1] + opencv_distparams[2]*(r2 + 2*mvLastDistCam[1]*mvLastDistCam[1]) + 2*opencv_distparams[3]*mvLastDistCam[0]*mvLastDistCam[1];*/


			  return mvLastCam;
		}
		else
		{
			  mvLastIm = v2Im;
			  mvLastDistCam[0] = (mvLastIm[0] - (mvCameraParams)[2]) / (mvCameraParams)[0];
			  mvLastDistCam[1] = (mvLastIm[1] - (mvCameraParams)[3]) / (mvCameraParams)[1];
			  mdLastDistR = sqrt(mvLastDistCam * mvLastDistCam);
			  mdLastR = invrtrans(mdLastDistR);
			  DefaultPrecision dFactor;
			  if(mdLastDistR > 0.01)
				dFactor =  mdLastR / mdLastDistR;
			  else
				dFactor = 1.0;
			  mdLastFactor = 1.0 / dFactor;
			  mvLastCam = dFactor * mvLastDistCam;
			  return mvLastCam;
		}
}

const Vector<NUMTRACKERCAMPARAMETERS> ATANCamera::mvDefaultParams = makeVector(0.5, 0.75, 0.5, 0.5, 0.1);

}
