#ifndef OPENGL_H
#define OPENGL_H

#include "../crack.h"
#ifdef LSP_INCLUDE
#include "./render.h"
#include "render_core.h"
#endif

#if WINDOWS
#include <windows.h>
#include <gl/GL.h>
#endif

#define GL_NUM_EXTENSIONS 0x821D

#define GL_MAX_COLOR_ATTACHMENTS            0x8CDF
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D

#define GL_TEXTURE_3D 0x806F

#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE1                 0x84C1
#define GL_TEXTURE2                 0x84C2
#define GL_TEXTURE3                 0x84C3
#define GL_TEXTURE4                 0x84C4
#define GL_TEXTURE5                 0x84C5
#define GL_TEXTURE6                 0x84C6
#define GL_TEXTURE7                 0x84C7
#define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF

#define GL_WRITE_ONLY          0x88B9
#define GL_PIXEL_UNPACK_BUFFER 0x88EC

#define GL_DEBUG_SEVERITY_HIGH         0x9146
#define GL_DEBUG_SEVERITY_MEDIUM       0x9147
#define GL_DEBUG_SEVERITY_LOW          0x9148
#define GL_DEBUG_TYPE_MARKER           0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP       0x8269
#define GL_DEBUG_TYPE_POP_GROUP        0x826A
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B

#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_ARRAY_BUFFER             0x8892
#define GL_ELEMENT_ARRAY_BUFFER     0x8893
#define GL_STREAM_DRAW              0x88E0
#define GL_STREAM_READ              0x88E1
#define GL_STREAM_COPY              0x88E2
#define GL_STATIC_DRAW              0x88E4
#define GL_STATIC_READ              0x88E5
#define GL_STATIC_COPY              0x88E6
#define GL_DYNAMIC_DRAW             0x88E8
#define GL_DYNAMIC_READ             0x88E9
#define GL_DYNAMIC_COPY             0x88EA

#define GL_CLAMP_TO_EDGE      0x812F
#define GL_TEXTURE_MIN_LOD    0x813A
#define GL_TEXTURE_MAX_LOD    0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL  0x813D
#define GL_TEXTURE_WRAP_R     0x8072

#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_SRGB8            0x8C41
#define GL_SRGB             0x8C40
#define GL_SRGB8_ALPHA8     0x8C43

#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_VERTEX_SHADER            0x8B31
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82
#define GL_VALIDATE_STATUS          0x8B83

#define GL_TEXTURE_2D_ARRAY 0x8C1A

#define GL_FRAMEBUFFER          0x8D40
#define GL_READ_FRAMEBUFFER     0x8CA8
#define GL_DRAW_FRAMEBUFFER     0x8CA9
#define GL_COLOR_ATTACHMENT0    0x8CE0
#define GL_COLOR_ATTACHMENT1    0x8CE1
#define GL_COLOR_ATTACHMENT2    0x8CE2
#define GL_COLOR_ATTACHMENT3    0x8CE3
#define GL_COLOR_ATTACHMENT4    0x8CE4
#define GL_COLOR_ATTACHMENT5    0x8CE5
#define GL_COLOR_ATTACHMENT6    0x8CE6
#define GL_COLOR_ATTACHMENT7    0x8CE7
#define GL_COLOR_ATTACHMENT8    0x8CE8
#define GL_COLOR_ATTACHMENT9    0x8CE9
#define GL_COLOR_ATTACHMENT10   0x8CEA
#define GL_COLOR_ATTACHMENT11   0x8CEB
#define GL_COLOR_ATTACHMENT12   0x8CEC
#define GL_COLOR_ATTACHMENT13   0x8CED
#define GL_COLOR_ATTACHMENT14   0x8CEE
#define GL_COLOR_ATTACHMENT15   0x8CEF
#define GL_COLOR_ATTACHMENT16   0x8CF0
#define GL_COLOR_ATTACHMENT17   0x8CF1
#define GL_COLOR_ATTACHMENT18   0x8CF2
#define GL_COLOR_ATTACHMENT19   0x8CF3
#define GL_COLOR_ATTACHMENT20   0x8CF4
#define GL_COLOR_ATTACHMENT21   0x8CF5
#define GL_COLOR_ATTACHMENT22   0x8CF6
#define GL_COLOR_ATTACHMENT23   0x8CF7
#define GL_COLOR_ATTACHMENT24   0x8CF8
#define GL_COLOR_ATTACHMENT25   0x8CF9
#define GL_COLOR_ATTACHMENT26   0x8CFA
#define GL_COLOR_ATTACHMENT27   0x8CFB
#define GL_COLOR_ATTACHMENT28   0x8CFC
#define GL_COLOR_ATTACHMENT29   0x8CFD
#define GL_COLOR_ATTACHMENT30   0x8CFE
#define GL_COLOR_ATTACHMENT31   0x8CFF
#define GL_DEPTH_ATTACHMENT     0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

#define GL_DEPTH_COMPONENT16  0x81A5
#define GL_DEPTH_COMPONENT24  0x81A6
#define GL_DEPTH_COMPONENT32  0x81A7
#define GL_DEPTH_COMPONENT32F 0x8CAC

#define GL_RED_INTEGER   0x8D94
#define GL_GREEN_INTEGER 0x8D95
#define GL_BLUE_INTEGER  0x8D96

#define GL_RG             0x8227
#define GL_RGBA32F        0x8814
#define GL_RGB32F         0x8815
#define GL_RGBA16F        0x881A
#define GL_RGB16F         0x881B
#define GL_R8             0x8229
#define GL_R16            0x822A
#define GL_RG8            0x822B
#define GL_RG16           0x822C
#define GL_R16F           0x822D
#define GL_R32F           0x822E
#define GL_RG16F          0x822F
#define GL_RG32F          0x8230
#define GL_R8I            0x8231
#define GL_R8UI           0x8232
#define GL_R16I           0x8233
#define GL_R16UI          0x8234
#define GL_R32I           0x8235
#define GL_R32UI          0x8236
#define GL_RG8I           0x8237
#define GL_RG8UI          0x8238
#define GL_RG16I          0x8239
#define GL_RG16UI         0x823A
#define GL_RG32I          0x823B
#define GL_RG32UI         0x823C
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_BGR            0x80E0

#define GL_MULTISAMPLE               0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE  0x809E
#define GL_SAMPLE_ALPHA_TO_ONE       0x809F
#define GL_SAMPLE_COVERAGE           0x80A0
#define GL_SAMPLE_BUFFERS            0x80A8
#define GL_SAMPLES                   0x80A9
#define GL_SAMPLE_COVERAGE_VALUE     0x80AA
#define GL_SAMPLE_COVERAGE_INVERT    0x80AB
#define GL_TEXTURE_2D_MULTISAMPLE    0x9100
#define GL_MAX_SAMPLES               0x8D57
#define GL_MAX_COLOR_TEXTURE_SAMPLES 0x910E
#define GL_MAX_DEPTH_TEXTURE_SAMPLES 0x910F
#define GL_SHADER_STORAGE_BUFFER     0x90D2

#define GL_MAP_PERSISTENT_BIT     0x0040
#define GL_MAP_COHERENT_BIT       0x0080
#define GL_MAP_WRITE_BIT          0x0002
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020

// WINDOWS
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB   0x2093
#define WGL_CONTEXT_FLAGS_ARB         0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126

#define WGL_CONTEXT_DEBUG_BIT_ARB              0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

typedef const char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

typedef void WINAPI type_glTexImage2DMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                                 GLsizei width, GLsizei height, GLboolean fixedsamplelocations);
typedef void WINAPI type_glBindFramebuffer(GLenum target, GLuint framebuffer);
typedef void WINAPI type_glGenFramebuffers(GLsizei n, GLuint *framebuffers);
typedef void WINAPI type_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                                GLint level);
typedef GLenum WINAPI type_glCheckFramebufferStatus(GLenum target);
typedef void WINAPI type_glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                                           GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
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
typedef void WINAPI type_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                                            const GLfloat *value);
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
typedef void WINAPI type_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                      GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                      const void *pixels);
typedef void WINAPI type_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                         GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                         const void *pixels);
typedef void WINAPI type_glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                                  GLint basevertex);
typedef void WINAPI type_glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                                 GLsizei primcount);
typedef void WINAPI type_glDrawElementsInstancedBaseVertex(GLenum mode, GLsizei count, GLenum type,
                                                           GLvoid *indices, GLsizei primcount, GLint basevertex);
typedef void WINAPI type_glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef const GLubyte *WINAPI type_glGetStringi(GLenum name, GLuint index);
typedef void WINAPI type_glVertexAttribDivisor(GLuint index, GLuint divisor);
typedef void WINAPI type_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data);
typedef void *WINAPI type_glMapBuffer(GLenum target, GLenum access);
typedef GLboolean WINAPI type_glUnmapBuffer(GLenum target);
typedef void WINAPI type_glMultiDrawElementsBaseVertex(GLenum mode, const GLsizei *count, GLenum type,
                                                       const void *const *indices, GLsizei drawcount,
                                                       const GLint *basevertex);
typedef void WINAPI type_glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
                                             const GLvoid *const *indices, GLsizei drawcount);
typedef void WINAPI type_glUniform1iv(GLint location, GLsizei count, const GLint *value);

typedef void WINAPI type_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                        GLsizei height, GLsizei depth);
typedef void WINAPI type_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                         GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                         const GLvoid *pixels);
typedef void WINAPI type_glTexImage3D(GLenum target, GLint level, GLint internalFormat, GLsizei width,
                                      GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                      const GLvoid *data);
typedef void WINAPI type_glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
typedef void WINAPI type_glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
typedef void WINAPI type_glBufferStorage(GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void *WINAPI type_glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

#define GL_DEBUG_CALLBACK(name)                                                                                   \
    void WINAPI name(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,                      \
                     const GLchar *message, const void *userParam)

typedef GL_DEBUG_CALLBACK(GLDebugCallback);
typedef void WINAPI type_glDebugMessageCallback(GLDebugCallback callback, void *userParam);

typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
global wgl_swap_interval_ext *wglSwapIntervalEXT;

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext, const int *attribList);
global wgl_create_context_attribs_arb *wglCreateContextAttribsARB;

#define Win32GetOpenGLFunction(name) openGL->name = (type_##name *)wglGetProcAddress(#name)

#define OpenGLFunction(name) type_##name *name

enum R_ShaderType
{
    R_ShaderType_UI,
    R_ShaderType_3D,
    R_ShaderType_Instanced3D,
    R_ShaderType_StaticMesh,
    R_ShaderType_SkinnedMesh,
    R_ShaderType_Terrain,
    R_ShaderType_Count,
};

struct R_Shader
{
    GLuint id;

    u64 globalsLastModified;
    u64 vsLastModified;
    u64 fsLastModified;
};

struct R_OpenGL_Buffer
{
    R_OpenGL_Buffer *next;
    u64 size;
    u64 generation;

    GLuint id;
    R_BufferType type;
};

struct R_OpenGL_Texture
{
    R_OpenGL_Texture *next;
    u64 generation;
    u32 width;
    u32 height;
    GLuint id;
    R_TexFormat format;
};

enum R_TextureLoadStatus
{
    R_TextureLoadStatus_Untransferred,
    R_TextureLoadStatus_Transferred,
};

struct R_Texture2DArrayNode;
struct R_OpenGL_TextureOp
{
    u8 *buffer;
    void *data;
    R_OpenGL_Texture *texture;
    struct
    {
        u32 hashIndex;
        u32 slice;
    } texSlice;

    R_TextureLoadStatus status;
    u32 pboIndex;

    b8 usesArray;
};

struct R_OpenGL_TextureQueue
{
    R_OpenGL_TextureOp ops[64];
    u32 numOps;

    // Loaded -> GPU
    u32 finalizePos;
    // Unloaded -> Loading
    u32 loadPos;
    u32 writePos;
    u32 endPos;
};

struct R_OpenGL_BufferOp
{
    // R_Handle data;
    void *data;
    R_OpenGL_Buffer *buffer;
};

struct R_OpenGL_BufferQueue
{
    R_OpenGL_BufferOp ops[64];
    u32 numOps;

    u32 readPos;
    u32 writePos;
    u32 endPos;
};

// NOTE: this cannot be padded
struct R_Texture2DArrayTopology
{
    GLsizei levels;
    R_TexFormat internalFormat;
    GLsizei width;
    GLsizei height;
};

StaticAssert(sizeof(R_Texture2DArrayTopology) == 16, R_Texture2DArrayTopologySize);

struct R_Texture2DArray
{
    R_Texture2DArrayTopology topology;
    GLsizei *freeList;
    u32 freeListCount;

    GLsizei depth;
    GLuint id;
    u32 hash;
};

struct R_Texture2DArrayMap
{
    R_Texture2DArray *arrayList;
    u32 maxSlots;

    TicketMutex mutex;
};

struct OpenGL
{
    Arena *arena;
    GLuint vao;

    GLuint pbos[4];
    u32 pboIndex;
    u64 firstUsedPboIndex;

    R_Shader shaders[R_ShaderType_Count];

    R_Handle whiteTextureHandle;
    u32 srgb8TextureFormat;
    u32 srgba8TextureFormat;

    GLuint scratchVbo;
    GLuint scratchEbo;
    GLuint scratchInstance;
    u64 scratchVboSize;
    u64 scratchEboSize;
    u64 scratchInstanceSize;

    TicketMutex mutex;
    R_OpenGL_Buffer *freeBuffers;
    R_OpenGL_Texture *freeTextures;

    R_OpenGL_TextureQueue textureQueue;
    R_OpenGL_BufferQueue bufferQueue;

    R_Texture2DArrayMap textureMap;
    GLint maxSlices;

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
    OpenGLFunction(glDisableVertexAttribArray);
    OpenGLFunction(glGenVertexArrays);
    OpenGLFunction(glBindVertexArray);
    OpenGLFunction(glGetUniformLocation);
    OpenGLFunction(glUniform4f);
    OpenGLFunction(glUniform1f);
    OpenGLFunction(glActiveTexture);
    OpenGLFunction(glUniformMatrix4fv);
    OpenGLFunction(glUniform3fv);
    OpenGLFunction(glGetAttribLocation);
    OpenGLFunction(glValidateProgram);
    OpenGLFunction(glVertexAttribIPointer);
    OpenGLFunction(glUniform1i);
    OpenGLFunction(glDeleteProgram);
    OpenGLFunction(glGetStringi);
    OpenGLFunction(glDrawElementsInstancedBaseVertex);
    OpenGLFunction(glDrawElementsInstanced);
    OpenGLFunction(glVertexAttribDivisor);
    OpenGLFunction(glBufferSubData);
    OpenGLFunction(glMapBuffer);
    OpenGLFunction(glUnmapBuffer);
    OpenGLFunction(glMultiDrawElements);
    OpenGLFunction(glUniform1iv);
    OpenGLFunction(glTexStorage3D);
    OpenGLFunction(glTexSubImage3D);
    OpenGLFunction(glTexImage3D);
    OpenGLFunction(glBindBufferBase);
    OpenGLFunction(glDrawArraysInstanced);
    OpenGLFunction(glDebugMessageCallback);
    OpenGLFunction(glBufferStorage);
    OpenGLFunction(glMapBufferRange);
    OpenGLFunction(glMultiDrawElementsBaseVertex);
};

global OpenGL _openGL;
global OpenGL *openGL = &_openGL;

//////////////////////////////
// Functions
//
internal void R_Init(Arena *arena, OS_Handle handle);
internal void R_OpenGL_Init();
internal void R_Win32_OpenGL_Init(OS_Handle handle);
internal void R_Win32_OpenGL_EndFrame(HDC deviceContext, int clientWidth, int clientHeight);
internal GLuint R_OpenGL_CreateShader(string globalsPath, string vsPath, string fsPath, string preprocess);
internal GLuint R_OpenGL_CompileShader(char *globals, char *vs, char *fs);

internal void R_OpenGL_StartShader(RenderState *state, R_ShaderType type, void *group);
internal void R_OpenGL_EndShader(R_ShaderType type);

r_allocate_texture_2D *R_AllocateTexture;

R_ALLOCATE_TEXTURE_2D(R_AllocateTexture2D);
R_ALLOCATE_TEXTURE_2D(R_AllocateTextureInArray);
R_ALLOCATE_BUFFER(R_AllocateBuffer);
internal void R_OpenGL_LoadBuffers();
internal void R_OpenGL_LoadTextures();

//////////////////////////////
// Handle
//
global R_OpenGL_Buffer r_opengl_bufferNil   = {&r_opengl_bufferNil};
global R_OpenGL_Texture r_opengl_textureNil = {&r_opengl_textureNil};

internal R_Handle R_OpenGL_HandleFromBuffer(R_OpenGL_Buffer *buffer);
internal R_OpenGL_Buffer *R_OpenGL_BufferFromHandle(R_Handle handle);
internal R_Handle R_OpenGL_HandleFromTexture(R_OpenGL_Texture *texture);
internal R_OpenGL_Texture *R_OpenGL_TextureFromHandle(R_Handle handle);

inline GLenum R_OpenGL_GetInternalFormat(R_TexFormat format)
{
    GLenum glFormat;
    switch (format)
    {
        case R_TexFormat_R8: glFormat = GL_R8; break;
        case R_TexFormat_RG8: glFormat = GL_RG8; break;
        case R_TexFormat_RGB8: glFormat = GL_RGB8; break;
        case R_TexFormat_SRGB8: glFormat = openGL->srgb8TextureFormat; break;
        case R_TexFormat_RGBA8: glFormat = GL_RGBA8; break;
        case R_TexFormat_SRGBA8: glFormat = openGL->srgba8TextureFormat; break;
        default: glFormat = GL_RGBA8; break;
    }
    return glFormat;
}

inline GLenum R_OpenGL_GetFormat(R_TexFormat format)
{
    GLenum glFormat;
    switch (format)
    {
        case R_TexFormat_R8: glFormat = GL_RED; break;
        case R_TexFormat_RG8: glFormat = GL_RG; break;

        case R_TexFormat_RGB8:
        case R_TexFormat_SRGB8: glFormat = GL_RGB; break;

        case R_TexFormat_RGBA8:
        case R_TexFormat_SRGBA8: glFormat = GL_RGBA; break;
        default: Assert(!"Invalid format");
    }
    return glFormat;
}

inline GLint R_OpenGL_GetBufferFromHandle(R_BufferHandle handle)
{
    GLint result = (GLint)(handle);
    return result;
}

global RenderState *renderState;

#endif
