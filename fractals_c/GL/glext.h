// Minimal glext.h for required OpenGL extensions
#ifndef __glext_h_
#define __glext_h_

#include <GL/gl.h>

// Essential GL enum definitions not provided by minimal header
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GLAPI
#define GLAPI extern
#endif

typedef char GLchar;

// Function prototypes (GL_GLEXT_PROTOTYPES must be defined before including this header)
GLAPI GLuint APIENTRY glCreateShader (GLenum type);
GLAPI void APIENTRY glShaderSource (GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
GLAPI void APIENTRY glCompileShader (GLuint shader);
GLAPI void APIENTRY glGetShaderiv (GLuint shader, GLenum pname, GLint *params);
GLAPI void APIENTRY glGetShaderInfoLog (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLAPI GLuint APIENTRY glCreateProgram (void);
GLAPI void APIENTRY glAttachShader (GLuint program, GLuint shader);
GLAPI void APIENTRY glLinkProgram (GLuint program);
GLAPI void APIENTRY glGetProgramiv (GLuint program, GLenum pname, GLint *params);
GLAPI void APIENTRY glGetProgramInfoLog (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
GLAPI void APIENTRY glUseProgram (GLuint program);
GLAPI GLint APIENTRY glGetUniformLocation (GLuint program, const GLchar *name);
GLAPI void APIENTRY glUniform1i (GLint location, GLint v0);
GLAPI void APIENTRY glUniform1f (GLint location, GLfloat v0);
GLAPI void APIENTRY glUniform2f (GLint location, GLfloat v0, GLfloat v1);
GLAPI void APIENTRY glUniform1d (GLint location, GLdouble v0);
GLAPI void APIENTRY glUniform2d (GLint location, GLdouble v0, GLdouble v1);
GLAPI void APIENTRY glUniform4f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

#ifdef __cplusplus
}
#endif

#endif // __glext_h_
