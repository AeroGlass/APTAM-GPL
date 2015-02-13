// Copyright 2015 ICGJKU
#include <stdlib.h>

std::string vertextexturetransform = "\
attribute vec4 position;                // vertex position attribute\n\
attribute vec2 texCoord;                // vertex texture coordinate attribute\n\
 \n\
uniform mat4 ModelViewMatrix;                 // shader modelview matrix uniform\n\
uniform mat4 ProjectionMatrix;                // shader projection matrix uniform\n\
 \n\
varying vec2 texCoordVar;               // vertex texture coordinate varying\n\
 \n\
void main()\n\
{\n\
    vec4 p = ModelViewMatrix * vec4(position.xyz,1);      // transform vertex position with modelview matrix\n\
    gl_Position = ProjectionMatrix * p;       // project the transformed position and write it to gl_Position\n\
    texCoordVar = texCoord;             // assign the texture coordinate attribute to its varying\n\
}\n\
";

std::string vertexsimpletransform = "precision mediump float;\n\
attribute vec4 position;                // vertex position attribute\n\
 \n\
uniform mat4 ModelViewMatrix;                 // shader modelview matrix uniform\n\
uniform mat4 ProjectionMatrix;                // shader projection matrix uniform\n\
 \n\
void main()\n\
{\n\
    vec4 p = ModelViewMatrix * vec4(position.xyz,1);      // transform vertex position with modelview matrix\n\
    gl_Position = ProjectionMatrix * p;       // project the transformed position and write it to gl_Position\n\
}\n\
";

std::string vertexpointstransform = "\
attribute vec4 position;                // vertex position attribute\n\
attribute vec4 vertColor;\n\
varying vec4 pVertColor;\n\
 \n\
uniform mat4 ModelViewMatrix;                 // shader modelview matrix uniform\n\
uniform mat4 ProjectionMatrix;                // shader projection matrix uniform\n\
uniform float PointSize;\n\
 \n\
void main()\n\
{\n\
	pVertColor = vertColor;\n\
    vec4 p = ModelViewMatrix * vec4(position.xyz,1);      // transform vertex position with modelview matrix\n\
    gl_Position = ProjectionMatrix * p;       // project the transformed position and write it to gl_Position\n\
	gl_PointSize = PointSize;\n\
}\n\
";

std::string fragmentsimpletexture = "\
precision mediump float;        // set default precision for floats to medium \n\
uniform sampler2D mytexture;      // shader texture uniform \n\
varying vec2 texCoordVar;       // fragment texture coordinate varying \n\
void main() \n\
{ \n\
	//vec4 col = texture2D( mytexture, texCoordVar); \n\
    //col.w = 1.0f;\n\
	gl_FragColor =  texture2D( mytexture, texCoordVar);\n\
}\n\
";

std::string fragmenttext = "\
precision mediump float;        // set default precision for floats to medium \n\
uniform vec4 GlobalColor; \n\
uniform sampler2D mytexture;      // shader texture uniform \n\
varying vec2 texCoordVar;       // fragment texture coordinate varying \n\
void main() \n\
{ \n\
	vec4 cc = GlobalColor*texture2D( mytexture, texCoordVar).a;\n\
    gl_FragColor = cc; \n\
}\n\
";

std::string fragmentglobalcolor = "precision mediump float;\n\
uniform vec4 GlobalColor; \n\
void main() \n\
{ \n\
	gl_FragColor = GlobalColor; \n\
}\n\
";

std::string fragmentvertexcolor = "precision mediump float;\n\
varying vec4 pVertColor; \n\
void main() \n\
{ \n\
	gl_FragColor = pVertColor; \n\
}\n\
";

//http://stackoverflow.com/questions/22456884/how-to-render-androids-yuv-nv21-camera-image-on-the-background-in-libgdx-with-o
std::string fragmentyuv = "\
precision mediump float;        // set default precision for floats to medium \n\
uniform sampler2D mytexture;      // shader texture uniform \n\
uniform sampler2D uvtexture;\n\
varying vec2 texCoordVar;       // fragment texture coordinate varying \n\
void main() \n\
{ \n\
   float r, g, b, y, u, v;\n\
   y = texture2D(mytexture, texCoordVar).r;\n\
   u = texture2D(uvtexture, texCoordVar).a - 0.5;\n\
   v = texture2D(uvtexture, texCoordVar).r - 0.5;\n\
   r = y + 1.13983*v;\n\
   g = y - 0.39465*u - 0.58060*v;\n\
   b = y + 2.03211*u;\n\
   gl_FragColor = vec4(r, g, b, 1.0);\n\
}\n\
";

std::string vertexlightingtransform = "precision mediump float;\n\
attribute vec4 position;                // vertex position attribute\n\
attribute vec3 normal;                // vertex normal attribute\n\
 \n\
varying vec3 tnormal; \n\
varying vec3 eyevec; \n\
varying vec3 lightvec; \n\
 \n\
uniform mat4 ModelViewMatrix;                 // shader modelview matrix uniform\n\
uniform mat4 ProjectionMatrix;                // shader projection matrix uniform\n\
uniform mat3 NormalMatrix;                 // shader modelview matrix uniform\n\
\n\
uniform vec4 lightPosition;//todo\n\
 \n\
void main()\n\
{\n\
    vec4 p = ModelViewMatrix * vec4(position.xyz,1);      // transform vertex position with modelview matrix\n\
	eyevec = -p.xyz; \n\
	lightvec = eyevec;//lightPosition-p; //todo: pass real light vec!\n\
	tnormal = NormalMatrix * normal; //todo: thats not 100% correct!!!\n\
    gl_Position = ProjectionMatrix * p;       // project the transformed position and write it to gl_Position\n\
}\n\
";

std::string fragmentlighting = "precision mediump float;\n\
uniform vec4 GlobalColor; \n\
\n\
varying vec3 tnormal; \n\
varying vec3 eyevec; \n\
varying vec3 lightvec; \n\
\n\
void main() \n\
{ \n\
	gl_FragColor = vec4(0.2*GlobalColor.xyz+0.8*max(0.0,dot(normalize(tnormal),normalize(lightvec)))*GlobalColor.xyz,GlobalColor.w); \n\
}\n\
";
