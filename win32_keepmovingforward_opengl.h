#ifndef WIN32_KEEPMOVINGFORWARD_OPENGL_H
#define WIN32_KEEPMOVINGFORWARD_OPENGL_H

#include "keepmovingforward_common.h"
#include <windows.h>

#include <gl/GL.h>

#define GL_NUM_EXTENSIONS 0x821D

#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D

#define GL_TEXTURE_3D 0x806F

#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_TEXTURE6 0x84C6
#define GL_TEXTURE7 0x84C7

#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B

#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STREAM_READ 0x88E1
#define GL_STREAM_COPY 0x88E2
#define GL_STATIC_DRAW 0x88E4
#define GL_STATIC_READ 0x88E5
#define GL_STATIC_COPY 0x88E6
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_DYNAMIC_READ 0x88E9
#define GL_DYNAMIC_COPY 0x88EA

#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_WRAP_R 0x8072

#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_SRGB8_ALPHA8 0x8C43

#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83

#define GL_TEXTURE_2D_ARRAY 0x8C1A

#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_COLOR_ATTACHMENT3 0x8CE3
#define GL_COLOR_ATTACHMENT4 0x8CE4
#define GL_COLOR_ATTACHMENT5 0x8CE5
#define GL_COLOR_ATTACHMENT6 0x8CE6
#define GL_COLOR_ATTACHMENT7 0x8CE7
#define GL_COLOR_ATTACHMENT8 0x8CE8
#define GL_COLOR_ATTACHMENT9 0x8CE9
#define GL_COLOR_ATTACHMENT10 0x8CEA
#define GL_COLOR_ATTACHMENT11 0x8CEB
#define GL_COLOR_ATTACHMENT12 0x8CEC
#define GL_COLOR_ATTACHMENT13 0x8CED
#define GL_COLOR_ATTACHMENT14 0x8CEE
#define GL_COLOR_ATTACHMENT15 0x8CEF
#define GL_COLOR_ATTACHMENT16 0x8CF0
#define GL_COLOR_ATTACHMENT17 0x8CF1
#define GL_COLOR_ATTACHMENT18 0x8CF2
#define GL_COLOR_ATTACHMENT19 0x8CF3
#define GL_COLOR_ATTACHMENT20 0x8CF4
#define GL_COLOR_ATTACHMENT21 0x8CF5
#define GL_COLOR_ATTACHMENT22 0x8CF6
#define GL_COLOR_ATTACHMENT23 0x8CF7
#define GL_COLOR_ATTACHMENT24 0x8CF8
#define GL_COLOR_ATTACHMENT25 0x8CF9
#define GL_COLOR_ATTACHMENT26 0x8CFA
#define GL_COLOR_ATTACHMENT27 0x8CFB
#define GL_COLOR_ATTACHMENT28 0x8CFC
#define GL_COLOR_ATTACHMENT29 0x8CFD
#define GL_COLOR_ATTACHMENT30 0x8CFE
#define GL_COLOR_ATTACHMENT31 0x8CFF
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_DEPTH_COMPONENT32F 0x8CAC

#define GL_RED_INTEGER 0x8D94
#define GL_GREEN_INTEGER 0x8D95
#define GL_BLUE_INTEGER 0x8D96

#define GL_RGBA32F 0x8814
#define GL_RGB32F 0x8815
#define GL_RGBA16F 0x881A
#define GL_RGB16F 0x881B
#define GL_R8 0x8229
#define GL_R16 0x822A
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C
#define GL_R11F_G11F_B10F 0x8C3A

#define GL_MULTISAMPLE 0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_SAMPLE_ALPHA_TO_ONE 0x809F
#define GL_SAMPLE_COVERAGE 0x80A0
#define GL_SAMPLE_BUFFERS 0x80A8
#define GL_SAMPLES 0x80A9
#define GL_SAMPLE_COVERAGE_VALUE 0x80AA
#define GL_SAMPLE_COVERAGE_INVERT 0x80AB
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_MAX_SAMPLES 0x8D57
#define GL_MAX_COLOR_TEXTURE_SAMPLES 0x910E
#define GL_MAX_DEPTH_TEXTURE_SAMPLES 0x910F

typedef const char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

typedef void WINAPI type_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                                 GLsizei height, GLboolean fixedsamplelocations);
typedef void WINAPI type_glBindFramebuffer(GLenum target, GLuint framebuffer);
typedef void WINAPI type_glGenFramebuffers(GLsizei n, GLuint *framebuffers);
typedef void WINAPI type_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                                GLint level);
typedef GLenum WINAPI type_glCheckFramebufferStatus(GLenum target);
typedef void WINAPI type_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
                                           GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void WINAPI type_glAttachShader(GLuint program, GLuint shader);
typedef void WINAPI type_glCompileShader(GLuint shader);
typedef GLuint WINAPI type_glCreateProgram(void);
typedef GLuint WINAPI type_glCreateShader(GLenum type);
typedef void WINAPI type_glLinkProgram(GLuint program);
typedef void WINAPI type_glShaderSource(GLuint shader, GLsizei count, GLchar **string, GLint *length);
typedef void WINAPI type_glUseProgram(GLuint program);
typedef void WINAPI type_glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void WINAPI type_glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void WINAPI type_glValidateProgram(GLuint program);
typedef void WINAPI type_glGetProgramiv(GLuint program, GLenum pname, GLint *params);
typedef GLint WINAPI type_glGetUniformLocation(GLuint program, const GLchar *name);
typedef void WINAPI type_glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
typedef void WINAPI type_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void WINAPI type_glUniform1i(GLint location, GLint v0);
typedef void WINAPI type_glUniform1f(GLint location, GLfloat v0);
typedef void WINAPI type_glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
typedef void WINAPI type_glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
typedef void WINAPI type_glEnableVertexAttribArray(GLuint index);
typedef void WINAPI type_glDisableVertexAttribArray(GLuint index);
typedef GLint WINAPI type_glGetAttribLocation(GLuint program, const GLchar *name);
typedef void WINAPI type_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                               GLsizei stride, const void *pointer);
typedef void WINAPI type_glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                                const void *pointer);
typedef void WINAPI type_glBindVertexArray(GLuint array);
typedef void WINAPI type_glGenVertexArrays(GLsizei n, GLuint *arrays);
typedef void WINAPI type_glBindBuffer(GLenum target, GLuint buffer);
typedef void WINAPI type_glGenBuffers(GLsizei n, GLuint *buffers);
typedef void WINAPI type_glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void WINAPI type_glActiveTexture(GLenum texture);
typedef void WINAPI type_glDeleteProgram(GLuint program);
typedef void WINAPI type_glDeleteShader(GLuint shader);
typedef void WINAPI type_glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
typedef void WINAPI type_glDrawBuffers(GLsizei n, const GLenum *bufs);
typedef void WINAPI type_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                      GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void WINAPI type_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                         GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                         const void *pixels);
typedef void WINAPI type_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                                  GLint basevertex);
typedef void WINAPI type_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

#define Win32GetOpenGLFunction(name) openGL->name = (type_##name *)wglGetProcAddress(#name)
#define OpenGLFunction(name) type_##name *name

struct OpenGL
{
    GLuint vao;
    GLuint shaderProgram;

    OpenGLFunction(glGenBuffers);
    OpenGLFunction(glBindBuffer);
    OpenGLFunction(glBufferData);
    OpenGLFunction(glCreateShader);
    OpenGLFunction(glCompileShader);
    OpenGLFunction(glShaderSource);
    OpenGLFunction(glCreateProgram);
    OpenGLFunction(glAttachShader);
    OpenGLFunction(glLinkProgram);
    OpenGLFunction(glGetProgramiv);
    OpenGLFunction(glGetProgramInfoLog);
    OpenGLFunction(glGetShaderInfoLog);
    OpenGLFunction(glUseProgram);
    OpenGLFunction(glDeleteShader);
    OpenGLFunction(glVertexAttribPointer);
    OpenGLFunction(glEnableVertexAttribArray);
    OpenGLFunction(glGenVertexArrays);
    OpenGLFunction(glBindVertexArray);
    OpenGLFunction(glGetUniformLocation);
    OpenGLFunction(glUniform4f);
    OpenGLFunction(glUniform1f);
    OpenGLFunction(glActiveTexture);
    OpenGLFunction(glUniformMatrix4fv);
};

internal void Setup();

#endif
