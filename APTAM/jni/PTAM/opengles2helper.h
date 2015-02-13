//Copyright ICGJKU 2015
//A little bit hacky solution to get OpenGL ES 2.0 support.
#ifndef __OPENGLES2HELPER_H
#define __OPENGLES2HELPER_H

#define GL_GLEXT_PROTOTYPES 1
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <stdlib.h>
#include <vector>

class OpenGLES2Helper
{
  public:
	OpenGLES2Helper();
	~OpenGLES2Helper();

	void glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a); //set some global color

	void glMatrixMode(GLenum mode);
	void glLoadIdentity();
	void glMultMatrixf(GLfloat *m );
	void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
	void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
	void glScalef(GLfloat x, GLfloat y, GLfloat z);

	void getMatrix(float *m);

	void glFrustumf(GLfloat left,GLfloat right,GLfloat	bottom,GLfloat top,GLfloat nearVal,	GLfloat farVal);
	void glOrthof(GLfloat left,GLfloat right,GLfloat bottom, GLfloat top, GLfloat znear, GLfloat zfar);

	void glPointSize( GLfloat size);

	void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer);
	void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
	void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
	void glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);

	//void glDrawArrays(GLenum mode, GLint first, GLsizei count);
	//void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

	void SetDefaultUniforms();

	GLuint GetDefaultVertexIndex();
	GLuint GetDefaultTextureCoordinateIndex();
	GLuint GetDefaultColorIndex();
	GLuint GetDefaultNormalIndex();

	void UseShader(int id);
	void DisableShader();

	void InitializeShaders();

	void glPushMatrix();
	void glPopMatrix();

	int GetActiveShader();

  private:
	GLenum activeMode;
	GLuint activeShader;

	glm::mat4& GetMatrix(int mode);

	std::vector<GLuint> shaders;
	std::vector<glm::mat4> modelViewMatrixStack;
	std::vector<glm::mat4> projectionMatrixStack;

	glm::mat4 modelViewMatrix;
	glm::mat4 projectionMatrix;

	glm::vec4 globalColor;
	GLfloat pointSize;
};

#define GL_PROJECTION 0
#define GL_MODELVIEW 1

extern OpenGLES2Helper gles2h;

inline void glMatrixMode(GLenum mode) {gles2h.glMatrixMode(mode);}
inline void glLoadIdentity() {gles2h.glLoadIdentity();}
inline void glMultMatrixf(GLfloat *m) {gles2h.glMultMatrixf(m);}
inline void glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a){gles2h.glColor4f(r,g,b,a);};
inline void glTranslatef(GLfloat x, GLfloat y, GLfloat z){gles2h.glTranslatef(x,y,z);};
inline void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z){gles2h.glRotatef(angle,x,y,z);};
inline void glScalef(GLfloat x, GLfloat y, GLfloat z){gles2h.glScalef(x,y,z);};
inline void glFrustumf(GLfloat left,GLfloat right,GLfloat	bottom,GLfloat top,GLfloat nearVal,	GLfloat farVal){gles2h.glFrustumf(left,right,bottom,top,nearVal,farVal);};
inline void glOrthof(GLfloat left,GLfloat right,GLfloat bottom, GLfloat top, GLfloat znear, GLfloat zfar){gles2h.glOrthof(left,right,bottom,top,znear,zfar);};
inline void glPointSize( GLfloat size){gles2h.glPointSize(size);};
inline void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer){gles2h.glNormalPointer(type,stride,pointer);};
inline void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer){gles2h.glVertexPointer(size,type,stride,pointer);};
inline void glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer){gles2h.glTexCoordPointer(size,type,stride,pointer);};
inline void glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer){gles2h.glColorPointer(size,type,stride,pointer);};
inline void glPushMatrix(){gles2h.glPushMatrix();};
inline void glPopMatrix(){gles2h.glPopMatrix();};
#endif
