//Copyright ICGJKU 2015
//A little bit hacky solution to get OpenGL ES 2.0 support.
#include "OpenGL.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <android/log.h>
#include "DefaultShaders.h"
#include <stdexcept>
#ifdef USE_OGL2

OpenGLES2Helper gles2h = OpenGLES2Helper();

using namespace glm;

OpenGLES2Helper::OpenGLES2Helper()
{
	activeMode = 0;
	pointSize = 1;
	activeShader = -1;
	modelViewMatrix = mat4();
	projectionMatrix = mat4();

	globalColor = vec4(1,1,1,1);
}

OpenGLES2Helper::~OpenGLES2Helper()
{

}

void OpenGLES2Helper::glPointSize( GLfloat size)
{
	pointSize = size;
}

void OpenGLES2Helper::glLoadIdentity()
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = mat4();
}

void OpenGLES2Helper::glMultMatrixf(GLfloat *m )
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = curmat*make_mat4(m);
}

void OpenGLES2Helper::glFrustumf(GLfloat left,GLfloat right,GLfloat	bottom,GLfloat top,GLfloat nearVal,	GLfloat farVal)
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = frustum(left,right,bottom,top,nearVal,farVal);
}

void OpenGLES2Helper::glOrthof(GLfloat left,GLfloat right,GLfloat bottom, GLfloat top, GLfloat znear, GLfloat zfar)
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = ortho(left,right,bottom,top,znear,zfar);
}

void OpenGLES2Helper::glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = translate(curmat,vec3(x,y,z));
}

void OpenGLES2Helper::glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = rotate(curmat,glm::radians(angle),vec3(x,y,z));
}

void OpenGLES2Helper::glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4& curmat = GetMatrix(activeMode);
	curmat = scale(curmat,vec3(x,y,z));
}

void OpenGLES2Helper::getMatrix(float *m)
{
	mat4& curmat = GetMatrix(activeMode);
	memcpy(m,glm::value_ptr(curmat),sizeof(float)*16);
}

void OpenGLES2Helper::glColor4f(GLfloat r,GLfloat g,GLfloat b,GLfloat a)
{
	globalColor = vec4(r,g,b,a);
}

void OpenGLES2Helper::glMatrixMode(GLenum mode)
{
	activeMode = mode;
}

void OpenGLES2Helper::glPushMatrix()
{
	if(activeMode==GL_PROJECTION)
		projectionMatrixStack.push_back(projectionMatrix);
	else
		modelViewMatrixStack.push_back(modelViewMatrix);
}

void OpenGLES2Helper::glPopMatrix()
{
	if(activeMode==GL_PROJECTION)
	{
		if(projectionMatrixStack.size()>0)
		{
			projectionMatrix = projectionMatrixStack.back();
			projectionMatrixStack.pop_back();
		}
	}
	else
	{
		if(modelViewMatrixStack.size()>0)
		{
			modelViewMatrix = modelViewMatrixStack.back();
			modelViewMatrixStack.pop_back();
		}
	}
}

glm::mat4& OpenGLES2Helper::GetMatrix(int mode)
{
	if(mode==GL_PROJECTION)
		return projectionMatrix;
	else
		return modelViewMatrix;
}

void OpenGLES2Helper::glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLuint positionIndex = GetDefaultVertexIndex();
	glEnableVertexAttribArray( positionIndex);
	glVertexAttribPointer( positionIndex, size, type, GL_FALSE, stride, pointer);
}

void OpenGLES2Helper::glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLuint positionIndex = GetDefaultTextureCoordinateIndex();
	glEnableVertexAttribArray( positionIndex);
	glVertexAttribPointer( positionIndex, size, type, GL_FALSE, stride, pointer);
}

void OpenGLES2Helper::glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLuint positionIndex = GetDefaultColorIndex();
	glEnableVertexAttribArray( positionIndex);
	glVertexAttribPointer( positionIndex, size, type, GL_TRUE, stride, pointer);
}

void OpenGLES2Helper::glNormalPointer (GLenum type, GLsizei stride, const GLvoid *pointer)
{
	GLuint positionIndex = GetDefaultNormalIndex();
	glEnableVertexAttribArray( positionIndex);
	glVertexAttribPointer( positionIndex, 3, type, GL_TRUE, stride, pointer);
}

//todo colorpointer

GLuint OpenGLES2Helper::GetDefaultVertexIndex()
{
	return glGetAttribLocation(activeShader, "position");
}

GLuint OpenGLES2Helper::GetDefaultTextureCoordinateIndex()
{
	return glGetAttribLocation(activeShader, "texCoord");
}

GLuint OpenGLES2Helper::GetDefaultColorIndex()
{
	return glGetAttribLocation(activeShader, "vertColor");
}

GLuint OpenGLES2Helper::GetDefaultNormalIndex()
{
	return glGetAttribLocation(activeShader, "normal");
}

void OpenGLES2Helper::UseShader(int id)
{
	activeShader = shaders[id];
	glUseProgram(activeShader);
}

void OpenGLES2Helper::DisableShader()
{
	glUseProgram(GL_NONE);
}

void OpenGLES2Helper::SetDefaultUniforms()
{
	glUniformMatrix4fv(glGetUniformLocation(activeShader,"ProjectionMatrix"),1,GL_FALSE,glm::value_ptr<GLfloat>(projectionMatrix));
	glUniformMatrix4fv(glGetUniformLocation(activeShader,"ModelViewMatrix"),1,GL_FALSE,glm::value_ptr<GLfloat>(modelViewMatrix));
	glm::mat3 normalMatrix = glm::mat3(modelViewMatrix);
	normalMatrix = glm::transpose(glm::inverse(normalMatrix));
	glUniformMatrix3fv(glGetUniformLocation(activeShader,"NormalMatrix"),1,GL_FALSE,glm::value_ptr<GLfloat>(normalMatrix));
	glUniform4fv(glGetUniformLocation(activeShader,"GlobalColor"),1, glm::value_ptr<GLfloat>(globalColor));
	glUniform1f(glGetUniformLocation(activeShader,"PointSize"),pointSize);
	glUniform1i(glGetUniformLocation(activeShader,"mytexture"),0);
}

std::string getInfoLog(GLuint shaderId) {
	GLint infolen;
	glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infolen);
	if(infolen <= 0)
		return "";

	char *log = new char[infolen];
	assert(log);
	glGetShaderInfoLog(shaderId, infolen, NULL, log);
	std::string slog(log);
	delete [] log;

	return slog;
}

GLuint LoadShader(const std::string& code,GLenum type) {
	GLint status;
	const GLchar* tmp[2] = {code.c_str(), 0};
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, tmp, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	std::string log = getInfoLog(shader);

	if(!status) {
		//LOG_ERROR << filename << "(" << toString(type) << "): can't compile shader: \n" << log << std::endl;
		__android_log_print(ANDROID_LOG_INFO, "opengl shader error", "%s",log.c_str());
		throw std::runtime_error("can't compile shader: " + log);
	} else {
		/* maybe there are warnings? */
		if(log == "" || log == "No errors.") {
			//LOG_DEBUG << filename << "(" << toString(type) << "): compiled shader: -> no warnings" << std::endl;
			__android_log_print(ANDROID_LOG_INFO, "opengl shader", "compiled shader: -> no warnings");
		} else {
			//LOG_WARN << filename << "(" << toString(type) << "): shader warnings:\n" << log << std::endl;
			__android_log_print(ANDROID_LOG_INFO, "opengl shader warning", "%s",log.c_str());
		}
	}

	return shader;
}

GLuint CompileShader(std::string vertexShader, std::string fragmentShader)
{
	//create program
	GLuint simpleshaderprogram = glCreateProgram();

	//compile shaders
	GLuint vertexshader = LoadShader(vertexShader,GL_VERTEX_SHADER);
	GLuint fragmentshader = LoadShader(fragmentShader,GL_FRAGMENT_SHADER);

	//attach shaders
	glAttachShader(simpleshaderprogram, vertexshader);
	glAttachShader(simpleshaderprogram, fragmentshader);

	//link program
	GLint status;
	glLinkProgram(simpleshaderprogram);

	//check for link errors
	glGetProgramiv(simpleshaderprogram, GL_LINK_STATUS, &status);
	if (!status) {
		std::string error = "can't link program: ";
		//LOG_ERROR << error << std::endl;
		__android_log_print(ANDROID_LOG_INFO, "opengl shader link error", "%s",error.c_str());
		throw std::runtime_error(error);
	} else {
		//LOG_DEBUG << "linked program - no errors" << std::endl;
		__android_log_print(ANDROID_LOG_INFO, "opengl shader link", "no errors");
	}
	return simpleshaderprogram;
}

int OpenGLES2Helper::GetActiveShader()
{
	return activeShader;
}

void OpenGLES2Helper::InitializeShaders()
{
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "simple-global");
	shaders.push_back(CompileShader(vertexsimpletransform,fragmentglobalcolor));
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "texture");
	shaders.push_back(CompileShader(vertextexturetransform,fragmentsimpletexture));
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "point-global");
	shaders.push_back(CompileShader(vertexpointstransform,fragmentglobalcolor)); //todo maybe combine with first
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "point-vertexcolor");
	shaders.push_back(CompileShader(vertexpointstransform,fragmentvertexcolor));//todo points and lines
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "text");
	shaders.push_back(CompileShader(vertextexturetransform,fragmenttext));//todo points and lines
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "yuv convert");
	shaders.push_back(CompileShader(vertextexturetransform,fragmentyuv));//todo points and lines
	__android_log_print(ANDROID_LOG_INFO, "opengl shader", "lighting simple");
	shaders.push_back(CompileShader(vertexlightingtransform,fragmentlighting));
}














#endif
