#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../asset.h"
#include "../asset_cache.h"
#include "./render.h"
#include "./opengl.h"
#include "vertex_cache.h"
#endif

// TODO: this will go away

#define GL_TEXTURE_ARRAY_HANDLE_FLAG 0x8000000000000000
global i32 win32OpenGLAttribs[] = {
    WGL_CONTEXT_MAJOR_VERSION_ARB,
    3,
    WGL_CONTEXT_MINOR_VERSION_ARB,
    0,
    WGL_CONTEXT_FLAGS_ARB,
    WGL_CONTEXT_DEBUG_BIT_ARB,
    WGL_CONTEXT_PROFILE_MASK_ARB,
    WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
    0,
};

global readonly i32 r_sizeFromFormatTable[R_TexFormat_Count] = {1, 4, 4};

global string r_opengl_g_globalsPath = Str8Lit("src/shaders/global.glsl");

global string r_opengl_g_vsPath[] = {Str8Lit("src/shaders/ui.vs"), Str8Lit("src/shaders/basic_3d.vs"),
                                     Str8Lit("src/shaders/basic_3d.vs"), Str8Lit(""),
                                     Str8Lit("src/shaders/model.vs")};
global string r_opengl_g_fsPath[] = {Str8Lit("src/shaders/ui.fs"), Str8Lit("src/shaders/basic_3d.fs"),
                                     Str8Lit("src/shaders/basic_3d.fs"), Str8Lit(""),
                                     Str8Lit("src/shaders/model.fs")};

internal void VSyncToggle(b32 enable)
{
#if WINDOWS
    if (!wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
    }
    if (wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(enable ? 1 : 0);
    }
#else
    Assert(!"VSync not supported on platform");
#endif
}

internal GLuint R_OpenGL_CompileShader(char *globals, char *vs, char *fs)
{
    GLuint vertexShaderId      = openGL->glCreateShader(GL_VERTEX_SHADER);
    GLchar *vertexShaderCode[] = {
        globals,
        vs,
    };
    openGL->glShaderSource(vertexShaderId, ArrayLength(vertexShaderCode), vertexShaderCode, 0);
    openGL->glCompileShader(vertexShaderId);

    GLuint fragmentShaderId      = openGL->glCreateShader(GL_FRAGMENT_SHADER);
    GLchar *fragmentShaderCode[] = {
        globals,
        fs,
    };
    openGL->glShaderSource(fragmentShaderId, ArrayLength(fragmentShaderCode), fragmentShaderCode, 0);
    openGL->glCompileShader(fragmentShaderId);

    GLuint shaderProgramId = openGL->glCreateProgram();
    openGL->glAttachShader(shaderProgramId, vertexShaderId);
    openGL->glAttachShader(shaderProgramId, fragmentShaderId);
    openGL->glLinkProgram(shaderProgramId);

    openGL->glValidateProgram(shaderProgramId);
    GLint success = false;
    openGL->glGetProgramiv(shaderProgramId, GL_LINK_STATUS, &success);
    if (!success)
    {
        char vertexShaderErrors[4096];
        char fragmentShaderErrors[4096];
        char programErrors[4096];
        openGL->glGetShaderInfoLog(vertexShaderId, 4096, 0, vertexShaderErrors);
        openGL->glGetShaderInfoLog(fragmentShaderId, 4096, 0, fragmentShaderErrors);
        openGL->glGetProgramInfoLog(shaderProgramId, 4096, 0, programErrors);

        Printf("Vertex shader errors: %s\n", vertexShaderErrors);
        Printf("Fragment shader errors: %s\n", fragmentShaderErrors);
        Printf("Program errors: %s\n", programErrors);
    }

    openGL->glDeleteShader(vertexShaderId);
    openGL->glDeleteShader(fragmentShaderId);

    return shaderProgramId;
}

internal GLuint R_OpenGL_CreateShader(string globalsPath, string vsPath, string fsPath, string preprocess)
{
    TempArena temp = ScratchStart(0, 0);

    Printf("Vertex shader: %S\n", vsPath);
    Printf("Fragment shader: %S\n", fsPath);
    string gTemp = OS_ReadEntireFile(temp.arena, globalsPath);
    string vs    = OS_ReadEntireFile(temp.arena, vsPath);
    string fs    = OS_ReadEntireFile(temp.arena, fsPath);

    string globals = StrConcat(temp.arena, gTemp, preprocess);

    GLuint id = R_OpenGL_CompileShader((char *)globals.str, (char *)vs.str, (char *)fs.str);

    ScratchEnd(temp);
    return id;
}

internal void R_OpenGL_HotloadShaders()
{

    for (R_ShaderType type = (R_ShaderType)0; type < R_ShaderType_Count; type = (R_ShaderType)(type + 1))
    {
        R_Shader *shader        = openGL->shaders + type;
        u64 vsLastModified      = OS_GetLastWriteTime(r_opengl_g_vsPath[type]);
        u64 fsLastModified      = OS_GetLastWriteTime(r_opengl_g_fsPath[type]);
        u64 globalsLastModified = OS_GetLastWriteTime(r_opengl_g_globalsPath);
        if (vsLastModified != shader->vsLastModified || fsLastModified != shader->fsLastModified ||
            globalsLastModified != shader->globalsLastModified)
        {
            openGL->glDeleteProgram(shader->id);
            shader->vsLastModified      = vsLastModified;
            shader->fsLastModified      = fsLastModified;
            shader->globalsLastModified = globalsLastModified;
            string preprocess           = Str8Lit("");
            switch (type)
            {
                case R_ShaderType_Instanced3D: preprocess = Str8Lit("#define instanced 1\n"); break;
                case R_ShaderType_StaticMesh: continue;
                default: break;
            }
            openGL->shaders[type].id = R_OpenGL_CreateShader(r_opengl_g_globalsPath, r_opengl_g_vsPath[type],
                                                             r_opengl_g_fsPath[type], preprocess);
        }
    }
}

GL_DEBUG_CALLBACK(R_OpenGL_DebugCallback)
{
    if (severity == GL_DEBUG_SEVERITY_HIGH)
    {
        Printf("Id: %u\nMessage: %s\n", id, (char *)message);
    }
}

internal void R_OpenGL_Init()
{
    openGL->arena = ArenaAlloc();

    // Debug info hopefully
    {
        if (openGL->glDebugMessageCallback)
        {
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            openGL->glDebugMessageCallback(R_OpenGL_DebugCallback, 0);
        }
    }
    // Initialize scratch buffers
    {
        openGL->glGenBuffers(1, &openGL->scratchVbo);
        openGL->glGenBuffers(1, &openGL->scratchEbo);
        // openGL->glGenBuffers(1, &openGL->scratchInstance);

        openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
        openGL->scratchVboSize = kilobytes(64);
        openGL->glBufferData(GL_ARRAY_BUFFER, openGL->scratchVboSize, 0, GL_DYNAMIC_DRAW);

        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEbo);
        openGL->scratchEboSize = kilobytes(64);
        openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEboSize, 0, GL_DYNAMIC_DRAW);

        // openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchEbo);
        // openGL->scratchEboSize = kilobytes(64);
        // openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEboSize, 0, GL_DYNAMIC_DRAW);

        openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    // Pbos
    openGL->glGenBuffers(ArrayLength(openGL->pbos), openGL->pbos);
    openGL->pboIndex          = 0;
    openGL->firstUsedPboIndex = 0;

    // Shaders
    {
        for (R_ShaderType type = (R_ShaderType)0; type < R_ShaderType_Count; type = (R_ShaderType)(type + 1))
        {
            string preprocess = Str8Lit("");
            switch (type)
            {
                case R_ShaderType_Instanced3D: preprocess = Str8Lit("#define instanced 1\n"); break;
                case R_ShaderType_StaticMesh: continue;
                default: break;
            }
            openGL->shaders[type].id = R_OpenGL_CreateShader(r_opengl_g_globalsPath, r_opengl_g_vsPath[type],
                                                             r_opengl_g_fsPath[type], preprocess);
            openGL->shaders[type].globalsLastModified = OS_GetLastWriteTime(r_opengl_g_globalsPath);
            openGL->shaders[type].vsLastModified      = OS_GetLastWriteTime(r_opengl_g_vsPath[type]);
            openGL->shaders[type].fsLastModified      = OS_GetLastWriteTime(r_opengl_g_fsPath[type]);
        }
    }

    // Texture/buffer queues
    {
        openGL->textureQueue.numOps = ArrayLength(openGL->textureQueue.ops);
        openGL->bufferQueue.numOps  = ArrayLength(openGL->bufferQueue.ops);
    }
    // Default white texture
    {
        u32 *data                  = PushStruct(openGL->arena, u32);
        data[0]                    = 0xffffffff;
        openGL->whiteTextureHandle = R_AllocateTexture2D((u8 *)data, 1, 1, R_TexFormat_RGBA8);
    }

    // Texture arrays
    {
        // glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &openGL->maxSlices);
        openGL->maxSlices            = 16;
        openGL->textureMap.maxSlots  = 32;
        openGL->textureMap.arrayList = PushArray(openGL->arena, R_Texture2DArray, openGL->textureMap.maxSlots);
    }

    VSyncToggle(1);
}

struct OpenGLInfo
{
    char *version;
    char *shaderVersion;
    b32 framebufferArb;
    b32 textureExt;
    b32 shaderDrawParametersArb;
    b32 persistentMap;
    b8 sparse;
};

global OpenGLInfo openGLInfo;

internal void OpenGLGetInfo()
{
    // TODO: I think you have to check wgl get extension string first? who even knows let's just use vulkan
    if (openGL->glGetStringi)
    {
        openGLInfo.version       = (char *)glGetString(GL_VERSION);
        openGLInfo.shaderVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        GLint numExtensions      = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        loopi(0, (u32)numExtensions)
        {
            char *extension = (char *)openGL->glGetStringi(GL_EXTENSIONS, i);
            if (Str8C(extension) == Str8Lit("GL_EXT_framebuffer_sRGB")) openGLInfo.framebufferArb = true;
            else if (Str8C(extension) == Str8Lit("GL_ARB_framebuffer_sRGB")) openGLInfo.framebufferArb = true;
            else if (Str8C(extension) == Str8Lit("GL_EXT_texture_sRGB")) openGLInfo.textureExt = true;
            else if (Str8C(extension) == Str8Lit("GL_ARB_shader_draw_parameters"))
            {
                openGLInfo.shaderDrawParametersArb = true;
                Printf("gl_DrawID should work\n");
            }
            else if (Str8C(extension) == Str8Lit("GL_ARB_texture_non_power_of_two"))
            {
                // Printf("whoopie!\n");
            }
            else if (Str8C(extension) == Str8Lit("GL_ARB_buffer_storage"))
            {
                openGLInfo.persistentMap = true;
            }
        }
    }
};

internal void R_Init(Arena *arena, OS_Handle handle)
{
    renderState = PushStruct(arena, RenderState);
#if WINDOWS
    R_Win32_OpenGL_Init(handle);
    R_AllocateTexture = R_AllocateTextureInArray;
#else
#error OS not implemented
#endif
}

internal void R_Win32_OpenGL_Init(OS_Handle handle)
{
    HWND window = (HWND)handle.handle;
    HDC dc      = GetDC(window);

    PIXELFORMATDESCRIPTOR desiredPixelFormat = {};
    desiredPixelFormat.nSize                 = sizeof(desiredPixelFormat);
    desiredPixelFormat.nVersion              = 1;
    desiredPixelFormat.dwFlags               = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    desiredPixelFormat.iPixelType            = PFD_TYPE_RGBA;
    desiredPixelFormat.cColorBits            = 32;
    desiredPixelFormat.cAlphaBits            = 8;
    desiredPixelFormat.iLayerType            = PFD_MAIN_PLANE;

    int suggestedPixelFormatIndex = ChoosePixelFormat(dc, &desiredPixelFormat);
    PIXELFORMATDESCRIPTOR suggestedPixelFormat;
    DescribePixelFormat(dc, suggestedPixelFormatIndex, sizeof(suggestedPixelFormat), &suggestedPixelFormat);
    SetPixelFormat(dc, suggestedPixelFormatIndex, &suggestedPixelFormat);

    HGLRC rc = wglCreateContext(dc);
    if (wglMakeCurrent(dc, rc))
    {
        wglCreateContextAttribsARB =
            (wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");
        if (wglCreateContextAttribsARB)
        {
            HGLRC shareContext = 0;
            HGLRC modernGLRC   = wglCreateContextAttribsARB(dc, shareContext, win32OpenGLAttribs);
            if (modernGLRC)
            {
                if (wglMakeCurrent(dc, modernGLRC))
                {
                    wglDeleteContext(rc);
                    rc = modernGLRC;
                }
            }
        }
        Win32GetOpenGLFunction(glGenBuffers);
        Win32GetOpenGLFunction(glBindBuffer);
        Win32GetOpenGLFunction(glBufferData);
        Win32GetOpenGLFunction(glCreateShader);
        Win32GetOpenGLFunction(glShaderSource);
        Win32GetOpenGLFunction(glCompileShader);
        Win32GetOpenGLFunction(glCreateProgram);
        Win32GetOpenGLFunction(glAttachShader);
        Win32GetOpenGLFunction(glLinkProgram);
        Win32GetOpenGLFunction(glGetProgramiv);
        Win32GetOpenGLFunction(glGetShaderInfoLog);
        Win32GetOpenGLFunction(glGetProgramInfoLog);
        Win32GetOpenGLFunction(glUseProgram);
        Win32GetOpenGLFunction(glDeleteShader);
        Win32GetOpenGLFunction(glVertexAttribPointer);
        Win32GetOpenGLFunction(glEnableVertexAttribArray);
        Win32GetOpenGLFunction(glDisableVertexAttribArray);
        Win32GetOpenGLFunction(glGenVertexArrays);
        Win32GetOpenGLFunction(glBindVertexArray);
        Win32GetOpenGLFunction(glGetUniformLocation);
        Win32GetOpenGLFunction(glUniform4f);
        Win32GetOpenGLFunction(glUniform1f);
        Win32GetOpenGLFunction(glActiveTexture);
        Win32GetOpenGLFunction(glUniformMatrix4fv);
        Win32GetOpenGLFunction(glUniform3fv);
        Win32GetOpenGLFunction(glGetAttribLocation);
        Win32GetOpenGLFunction(glValidateProgram);
        Win32GetOpenGLFunction(glVertexAttribIPointer);
        Win32GetOpenGLFunction(glUniform1i);
        Win32GetOpenGLFunction(glDeleteProgram);
        Win32GetOpenGLFunction(glGetStringi);
        Win32GetOpenGLFunction(glDrawElementsInstancedBaseVertex);
        Win32GetOpenGLFunction(glVertexAttribDivisor);
        Win32GetOpenGLFunction(glDrawElementsInstanced);
        Win32GetOpenGLFunction(glBufferSubData);
        Win32GetOpenGLFunction(glMapBuffer);
        Win32GetOpenGLFunction(glUnmapBuffer);
        Win32GetOpenGLFunction(glMultiDrawElements);
        Win32GetOpenGLFunction(glUniform1iv);
        Win32GetOpenGLFunction(glTexStorage3D);
        Win32GetOpenGLFunction(glTexSubImage3D);
        Win32GetOpenGLFunction(glTexImage3D);
        Win32GetOpenGLFunction(glBindBufferBase);
        Win32GetOpenGLFunction(glDrawArraysInstanced);
        Win32GetOpenGLFunction(glDebugMessageCallback);
        Win32GetOpenGLFunction(glBufferStorage);
        Win32GetOpenGLFunction(glMapBufferRange);
        Win32GetOpenGLFunction(glMultiDrawElementsBaseVertex);

        OpenGLGetInfo();

        openGL->defaultTextureFormat = GL_RGBA8;
        if (openGLInfo.textureExt)
        {
            openGL->defaultTextureFormat = GL_SRGB8_ALPHA8;
        }
    }
    else
    {
        Unreachable;
    }
    R_OpenGL_Init();
    ReleaseDC(window, dc);
};

// global RenderState *lastState;
// global RenderState *newState;

enum DatumType
{
    DatumType_AnimationTransform,
    DatumType_Position,
};

struct RenderDatum
{
    union
    {
        AnimationTransform transform;
        V3 position;
    };
    DatumType type;
};

struct RenderData
{
    RenderDatum *datum;
    u32 datumCount;
};

// i really just have to be ok with writing code that isn't perfectly 100% optimal perfect
// actually nevermind, each frame when an entities data is submitted, the "delta" stuff (positions,
// transformations, etc.) meaning data that visually changes is submitted. this information i guess is then
// sorted by the entities id or something, and then sent to the renderer. the renderer then steps through
// this newly submitted state and the old state, and if the entity id matches, it interpolates the data
// using a lerp. if there's a teleportation or something, then instead of lerping, the entities "personal"
// generation id is incremented (is this the same generation id as the one used to allocate/destroy
// entities???????? i think a separate one). so basically if the entity id matches, it then checks the personal id
// or whatever term you want, and then uses that to determine the interpolation pattern (lerp vs discrete).
// finally this submits the data to the render which somehow uses this to do good in the world. god knows we need
// something.

// basically what this will do is take the render state as it is and flatten it so that it can be memcopied or
// something

// THIS IS DONE IN THE SIMULATION TO SUBMIT A RENDER STATE

#if 0
// this contains static data used in the game. this doesn't change from frame to frame
enum R_RenderObjectType
{
    R_RenderType_SkinnedModel,
};

// contains data for skinned models
struct R_RenderData
{

};

typedef void R_ExtractData(
struct R_RenderFeature 
{
     
};

// takes data from skinning matrices/game state/dynamic shit or something and then
//
// map individual data to a render feature or something, where the render feature defines entry points for
// A. what data is taken from the game state
// B. what data is put in the atomic ring buffer
// C. how data is shoved into the renderer and how it is rendererd :)

internal void R_SubmitRenderPass(RenderState *state)
{
    // TODO: maybe also sort here some how
    AtomicRing *ring = &shared->g2rRing;

    R_Pass3D *pass3D = state->passes[R_PassType_3D];
    for (u32 i = 0; i < pass3D->groups.numGroups; i++)
    {
        R_Batch3DParams *params = &pass3D->groups[i].params;

        params->
    }

    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        state->passes[type];
    }
    pass->
}

internal u8 *R_Interpolate(Arena *arena, RenderState *oldState, RenderState *newState, f32 dt)
{
    u8 *pushBuffer = PushArray(arena, u8, newState->size);
    u64 cursor     = 0;

    f32 oldTimestamp = oldState->timestamp;
    f32 newTimestamp = newState->timestamp;

    for (u32 i = 0; i < ArrayLength(state->objects); i++)
    {
        RenderData *oldData = &oldState->objects[i].data;
        RenderData *newData = &newState->objects[i].data;

        // but this might not be true and for good reason. what if a thing gains attributes or something?
        // who even knows
        Assert(oldData->numPieces == newData->numPieces);
        for (u32 datumIndex = 0; datumIndex < oldData->numPieces; datumIndex++)
        {
            RenderDatum *oldDatum = &oldData->piece[datumIndex];
            RenderDatum *newDatum = &newData->piece[datumIndex];

            u64 datumSize = 0;
            switch (newDatum->type)
            {
                case DatumType_AnimationTransform:
                {
                    AnimationTransform *oldTform = (AnimationTransform *)oldDatum;
                    AnimationTransform *newTform = (AnimationTransform *)newDatum;

                    AnimationTransform result = Lerp(oldTform, newTform, dt);

                    MemoryCopy(pushBuffer + cursor, &result, sizeof(result));
                    cursor += AlignPow2(sizeof(result), 8);

                    break;
                }
                case DatumType_Position:
                {
                    V3 *oldPosition = (V3 *)oldDatum;
                    V3 *newPosition = (V3 *)newDatum;

                    V3 result = Lerp(oldPosition, newPosition, dt);
                    datumSize = sizeof(AnimationTransform);

                    MemoryCopy(pushBuffer + cursor, &result, sizeof(result));
                    cursor += AlignPow2(sizeof(result), 8);
                    break;
                }
                default: Assert(!"Not valid data");
            }
        }
    }
    return pushBuffer;
}

internal void R_SubmitFrame(RenderState *state, f32 accumulator, f32 dt)
{
    f32 alpha = accumulator / dt;

    R_Interpolate(lastState, newState, alpha);
    // TODO: Ideally this is just a memcopy.
}

internal void R_EntryPoint(void *p)
{
    for (; shared->running;)
    {
        // StartAtomicRead(&shared->g2rRing, sizeof(u64));
        // u64 readPos = shared->readPos;
        // u64 size;
        // RingReadStruct(
        // R_EndFrame(state);
    }
}
#endif

internal void R_BeginFrame(i32 width, i32 height)
{
    // openGL->width      = width;
    // openGL->height     = height;
    // RenderGroup *group = &openGL->group;
    // group->indexCount  = 0;
    // group->vertexCount = 0;
    // group->quadCount   = 0;
}

// TODO: no :)
internal void R_EndFrame()
{
#if WINDOWS
    V2 viewport       = OS_GetWindowDimension(shared->windowHandle);
    HWND window       = (HWND)shared->windowHandle.handle;
    HDC deviceContext = GetDC(window);
    R_Win32_OpenGL_EndFrame(deviceContext, (i32)viewport.x, (i32)viewport.y);
    ReleaseDC(window, deviceContext);
#else
#error
#endif
}

internal void R_Win32_OpenGL_EndFrame(HDC deviceContext, int clientWidth, int clientHeight)
{
    TIMED_FUNCTION();
    // Load queued buffers and textures
    {
        R_OpenGL_LoadBuffers();
        openGL->glActiveTexture(GL_TEXTURE0);
        R_OpenGL_LoadTextures();
    }

    TempArena temp    = ScratchStart(0, 0);
    GLint *samplerIds = 0;
    // Initialize texture array (for everyone
    {
        for (u32 i = 0; i < openGL->textureMap.maxSlots; i++)
        {
            R_Texture2DArray *aList = &openGL->textureMap.arrayList[i];
            openGL->glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D_ARRAY, aList->id);
        }
        samplerIds = PushArray(temp.arena, GLint, openGL->textureMap.maxSlots);
        for (u32 i = 0; i < openGL->textureMap.maxSlots; i++)
        {
            samplerIds[i] = i;
        }
    }

    // INITIALIZE
    {
        // as_state = renderState->as_state;
        glViewport(0, 0, clientWidth, clientHeight);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
        // TODO: NOT SAFE!
        if (openGLInfo.framebufferArb)
        {
            glEnable(GL_FRAMEBUFFER_SRGB);
        }
        glCullFace(GL_BACK);
        glClearColor(0.5f, 0.5f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // RENDER MODEL

    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        R_Pass *pass = &renderState->passes[type];
        switch (type)
        {
            case R_PassType_UI:
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                GLuint id = openGL->shaders[R_ShaderType_UI].id;
                openGL->glUseProgram(id);
                openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
                u32 totalOffset = 0;
                // TODO: I got a null reference exception here somehow???
                R_BatchList *list = &pass->passUI->batchList;
                for (R_BatchNode *node = list->first; node != 0; node = node->next)
                {
                    // TODO: persistently mapped buffers for scratch data? that way you can just
                    // memcpy to the buffer instead of calling glBufferSubData repeatedly
                    // also still have to try out sparse bindless textures : ) but apparently
                    // they suck on windows so who know

                    openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, node->val.byteCount, node->val.data);
                    totalOffset += node->val.byteCount;
                }

                openGL->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(R_RectInst),
                                              (void *)Offset(R_RectInst, pos));
                openGL->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R_RectInst),
                                              (void *)Offset(R_RectInst, scale));
                openGL->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(R_RectInst),
                                              (void *)Offset(R_RectInst, handle));
                openGL->glEnableVertexAttribArray(0);
                openGL->glEnableVertexAttribArray(1);
                openGL->glEnableVertexAttribArray(2);

                openGL->glVertexAttribDivisor(0, 1);
                openGL->glVertexAttribDivisor(1, 1);
                openGL->glVertexAttribDivisor(2, 1);

                f32 transform[] = {
                    // 2.f / (f32)clientWidth, 0, 0, -1, 2.f / (f32)clientHeight, 0, 0, -1, 0, 0, 1, 0, 0, 0,
                    // 0, 1,
                    2.f / (f32)clientWidth, 0, 0, 0, 0, 2.f / (f32)clientHeight, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1,
                };
                GLint transformLocation = openGL->glGetUniformLocation(id, "transform");
                openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, transform);

                GLint textureLoc = openGL->glGetUniformLocation(id, "textureMaps");
                openGL->glUniform1iv(textureLoc, openGL->textureMap.maxSlots, samplerIds);

                openGL->glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, list->numInstances);
                R_OpenGL_EndShader(R_ShaderType_UI);
                glDisable(GL_BLEND);
                break;
            }
            case R_PassType_StaticMesh:
            {
                break;
            }
            case R_PassType_SkinnedMesh:
            {
                GLuint modelProgramId = openGL->shaders[R_ShaderType_SkinnedMesh].id;
                openGL->glUseProgram(modelProgramId);

                R_SkinnedMeshParamsList *list = &pass->passSkinned->list;
                // TODO: remove reference to material in here
                for (R_SkinnedMeshParamsNode *node = list->first; node != 0; node = node->next)
                {
                    R_SkinnedMeshParams *params = &node->val;
                    LoadedModel *model          = GetModel(params->model);

                    GLint vertexApiObject = 0;
                    {
                        GPUBuffer *vertexBuffer = VC_GetBufferFromHandle(model->vertexBuffer, BufferType_Vertex);
                        if (vertexBuffer)
                        {
                            vertexApiObject = R_OpenGL_GetBufferFromHandle(vertexBuffer->handle);
                        }
                    }
                    GLint indexApiObject = 0;
                    {
                        GPUBuffer *indexBuffer = VC_GetBufferFromHandle(model->indexBuffer, BufferType_Index);
                        if (indexBuffer)
                        {
                            indexApiObject = R_OpenGL_GetBufferFromHandle(indexBuffer->handle);
                        }
                    }

                    R_BufferHandle handle = model->vertexBuffer;

                    // R_OpenGL_Buffer *vertexBuffer = R_OpenGL_BufferFromHandle(model->vertexBuffer);
                    // R_OpenGL_Buffer *indexBuffer  = R_OpenGL_BufferFromHandle(model->indexBuffer);
                    // openGL->glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->id);
                    // openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->id);

                    openGL->glBindBuffer(GL_ARRAY_BUFFER, vertexApiObject);
                    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexApiObject);
                    R_OpenGL_StartShader(renderState, R_ShaderType_SkinnedMesh, 0);

                    // TODO: uniform blocks so we can just push a struct or something instead of having to do
                    // these all manually

                    // MVP matrix
                    Mat4 newTransform       = renderState->transform * params->transform;
                    GLint transformLocation = openGL->glGetUniformLocation(modelProgramId, "transform");
                    openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, newTransform.elements[0]);

                    // Skinning matrices
                    GLint boneTformLocation = openGL->glGetUniformLocation(modelProgramId, "boneTransforms");
                    openGL->glUniformMatrix4fv(boneTformLocation, params->skinningMatricesCount, GL_FALSE,
                                               params->skinningMatrices[0].elements[0]);

                    GLint modelLoc = openGL->glGetUniformLocation(modelProgramId, "model");
                    openGL->glUniformMatrix4fv(modelLoc, 1, GL_FALSE, params->transform.elements[0]);

                    V3 lightPosition  = MakeV3(5, 5, 0);
                    GLint lightPosLoc = openGL->glGetUniformLocation(modelProgramId, "lightPos");
                    openGL->glUniform3fv(lightPosLoc, 1, lightPosition.elements);

                    GLint viewPosLoc = openGL->glGetUniformLocation(modelProgramId, "viewPos");
                    openGL->glUniform3fv(viewPosLoc, 1, renderState->camera.position.elements);

                    // TEXTURE MAP
                    GLint textureLoc = openGL->glGetUniformLocation(modelProgramId, "textureMaps");
                    openGL->glUniform1iv(textureLoc, openGL->textureMap.maxSlots, samplerIds);

                    // GLint diffuseMapLoc = openGL->glGetUniformLocation(modelProgramId, "diffuseMap");
                    // openGL->glUniform1i(diffuseMapLoc, 0);
                    //
                    // GLint normalMapLoc = openGL->glGetUniformLocation(modelProgramId, "normalMap");
                    // openGL->glUniform1i(normalMapLoc, 1);

                    openGL->glActiveTexture(GL_TEXTURE0);
                    R_OpenGL_Texture *whiteTexture = R_OpenGL_TextureFromHandle(openGL->whiteTextureHandle);
                    glBindTexture(GL_TEXTURE_2D, whiteTexture->id);

#define TEXTURES_PER_MATERIAL 2
                    u32 baseTexture     = 0;
                    GLsizei *counts     = PushArray(temp.arena, GLsizei, model->materialCount);
                    void **startIndices = PushArray(temp.arena, void *, model->materialCount);
                    GLint *baseVertices = PushArray(temp.arena, GLint, model->materialCount);

                    R_TexAddress *addresses =
                        PushArray(temp.arena, R_TexAddress, model->materialCount * TextureType_Count);
                    u32 count = 0;

                    VC_Handle vertexHandle = model->vertexBuffer;
                    VC_Handle indexHandle  = model->indexBuffer;

                    i32 vertexOffset =
                        (i32)((vertexHandle >> VERTEX_CACHE_OFFSET_SHIFT) & VERTEX_CACHE_OFFSET_MASK) /
                        sizeof(model->vertices[0]);

                    u64 indexOffset = (u64)((indexHandle >> VERTEX_CACHE_OFFSET_SHIFT) & VERTEX_CACHE_OFFSET_MASK);

                    for (u32 i = 0; i < model->materialCount; i++)
                    {
                        Material *material = model->materials + i;
                        counts[i]          = material->onePlusEndIndex - material->startIndex;
                        u64 startIndex     = sizeof(material->startIndex) * (material->startIndex) + indexOffset;
                        startIndices[i]    = (void *)(startIndex);
                        baseVertices[i]    = vertexOffset;
                        for (u32 j = 0; j < TextureType_Count; j++)
                        {
                            R_Handle textureHandle = GetTextureRenderHandle(material->textureHandles[j]);
                            if (R_HandleMatch(textureHandle, R_HandleZero()))
                            {
                                textureHandle = openGL->whiteTextureHandle;
                            }
                            // Bind texture array
                            if (textureHandle.u64[1] == GL_TEXTURE_ARRAY_HANDLE_FLAG)
                            {
                                // SSBO
                                switch (j)
                                {
                                    case TextureType_Diffuse:
                                    case TextureType_Normal:
                                    {
                                        u32 hashIndex = textureHandle.u32[0];
                                        f32 slice;
                                        MemoryCopy(&slice, &textureHandle.u32[1], sizeof(slice));
                                        // f32 slice = (f32)textureHandle.u32[1];

                                        addresses[count++] = {hashIndex, slice};
                                        break;
                                    }
                                    default: continue;
                                }
                            }
                            // Bind textures individually
                            else
                            {
                                GLuint id = R_OpenGL_TextureFromHandle(textureHandle)->id;

                                switch (j)
                                {
                                    case TextureType_Diffuse:
                                    {
                                        openGL->glActiveTexture(GL_TEXTURE0 + baseTexture);
                                        glBindTexture(GL_TEXTURE_2D, id);
                                        break;
                                    }
                                    case TextureType_Normal:
                                    {
                                        openGL->glActiveTexture(GL_TEXTURE0 + baseTexture + 1);
                                        glBindTexture(GL_TEXTURE_2D, id);
                                        break;
                                    }
                                    default:
                                    {
                                        continue;
                                    }
                                }
                            }
                        }
                        baseTexture += TEXTURES_PER_MATERIAL;
                    }

                    static GLuint ssbo = 0;
                    if (ssbo == 0)
                    {
                        openGL->glGenBuffers(1, &ssbo);
                        openGL->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
                    }
                    openGL->glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
                    openGL->glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(R_TexAddress), addresses,
                                         GL_DYNAMIC_DRAW);

                    // openGL->glMultiDrawElements(GL_TRIANGLES, counts, GL_UNSIGNED_INT, startIndices,
                    //                             model->materialCount);
                    openGL->glMultiDrawElementsBaseVertex(GL_TRIANGLES, counts, GL_UNSIGNED_INT, startIndices,
                                                          model->materialCount, baseVertices);
                }
                R_OpenGL_EndShader(R_ShaderType_SkinnedMesh);
                break;
            }
            case R_PassType_3D:
            {
                R_Pass3D *pass3D = pass->pass3D;
                for (u32 i = 0; i < pass3D->numGroups; i++)
                {
                    R_Batch3DGroup *group = &pass3D->groups[i];
                    GLuint topology;
                    {
                        switch (group->params.topology)
                        {
                            case R_Topology_Points:
                            {
                                topology = GL_POINTS;
                                break;
                            }
                            case R_Topology_Lines:
                            {
                                topology = GL_LINES;
                                break;
                            }
                            case R_Topology_TriangleStrip:
                            {
                                topology = GL_TRIANGLE_STRIP;
                                break;
                            }
                            case R_Topology_Triangles:
                            default:
                            {
                                topology = GL_TRIANGLES;
                                break;
                            }
                        }
                    }
                    switch (group->params.primType)
                    {
                        case R_Primitive_Lines:
                        {
                            openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
                            u32 totalOffset = 0;
                            for (R_BatchNode *node = group->batchList.first; node != 0; node = node->next)
                            {
                                openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, node->val.byteCount,
                                                        node->val.data);
                                totalOffset += node->val.byteCount;
                            }
                            R_OpenGL_StartShader(renderState, R_ShaderType_3D, 0);
                            glDrawArrays(topology, 0, group->batchList.numInstances * 2);
                            R_OpenGL_EndShader(R_ShaderType_3D);
                            break;
                        }
                        case R_Primitive_Points:
                        {
                            openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
                            u32 totalOffset = 0;
                            for (R_BatchNode *node = group->batchList.first; node != 0; node = node->next)
                            {
                                openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, node->val.byteCount,
                                                        node->val.data);
                                totalOffset += node->val.byteCount;
                            }
                            R_OpenGL_StartShader(renderState, R_ShaderType_3D, 0);
                            glDrawArrays(topology, 0, group->batchList.numInstances);
                            R_OpenGL_EndShader(R_ShaderType_3D);
                            break;
                        }
                        case R_Primitive_Sphere:
                        case R_Primitive_Cube:
                        {
                            u32 totalOffset = 0;
                            // Assert that the total size can fit in the scratch buffers
                            Assert(group->batchList.numInstances * group->batchList.bytesPerInstance +
                                       sizeof(group->params.vertices[0]) * group->params.vertexCount <
                                   openGL->scratchVboSize);
                            Assert(sizeof(group->params.indices[0]) * group->params.indexCount <
                                   openGL->scratchEboSize);

                            openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
                            openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset,
                                                    sizeof(group->params.vertices[0]) * group->params.vertexCount,
                                                    group->params.vertices);

                            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEbo);
                            openGL->glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                                                    sizeof(group->params.indices[0]) * group->params.indexCount,
                                                    group->params.indices);

                            totalOffset += sizeof(group->params.vertices[0]) * group->params.vertexCount;
                            for (R_BatchNode *node = group->batchList.first; node != 0; node = node->next)
                            {
                                openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, node->val.byteCount,
                                                        node->val.data);
                                totalOffset += node->val.byteCount;
                            }

                            R_OpenGL_StartShader(renderState, R_ShaderType_Instanced3D, group);
                            openGL->glDrawElementsInstanced(topology, group->params.indexCount, GL_UNSIGNED_INT, 0,
                                                            group->batchList.numInstances);
                            R_OpenGL_EndShader(R_ShaderType_Instanced3D);
                            break;
                        }
                        default: Assert(!"Not implemented");
                    }
                }

                break;
            }
            default:
            {
                Assert(!"Invalid default case");
            }
        }
    }

    ScratchEnd(temp);

    // DOUBLE BUFFER SWAP
    SwapBuffers(deviceContext);

    // TODO: integrate into asset system? but I'll have to queue
    R_OpenGL_HotloadShaders();
}

internal void R_OpenGL_StartShader(RenderState *state, R_ShaderType type, void *inputGroup)
{
    switch (type)
    {
        case R_ShaderType_3D:
        {
            Assert(inputGroup == 0);
            GLuint id = openGL->shaders[R_ShaderType_3D].id;
            openGL->glUseProgram(id);
            openGL->glEnableVertexAttribArray(0);
            openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                                          (void *)Offset(DebugVertex, pos));
            openGL->glEnableVertexAttribArray(1);
            openGL->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                                          (void *)Offset(DebugVertex, color));
            GLint transformLocation = openGL->glGetUniformLocation(id, "transform");
            openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, state->transform.elements[0]);
            break;
        }
        case R_ShaderType_Instanced3D:
        {
            GLuint id = openGL->shaders[R_ShaderType_Instanced3D].id;
            openGL->glUseProgram(id);
            R_Batch3DGroup *group = (R_Batch3DGroup *)inputGroup;
            openGL->glEnableVertexAttribArray(0);
            openGL->glEnableVertexAttribArray(1);
            openGL->glEnableVertexAttribArray(2);
            openGL->glEnableVertexAttribArray(3);
            openGL->glEnableVertexAttribArray(4);
            openGL->glEnableVertexAttribArray(5);

            u64 instanceOffsetStart = sizeof(group->params.vertices[0]) * group->params.vertexCount;
            openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(group->params.vertices[0]), 0);
            openGL->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, group->batchList.bytesPerInstance,
                                          (void *)(instanceOffsetStart));
            openGL->glVertexAttribDivisor(1, 1);

            openGL->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, group->batchList.bytesPerInstance,
                                          (void *)(instanceOffsetStart + sizeof(V4)));
            openGL->glVertexAttribDivisor(2, 1);

            openGL->glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, group->batchList.bytesPerInstance,
                                          (void *)(instanceOffsetStart + sizeof(V4) * 2));
            openGL->glVertexAttribDivisor(3, 1);

            openGL->glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, group->batchList.bytesPerInstance,
                                          (void *)(instanceOffsetStart + sizeof(V4) * 3));
            openGL->glVertexAttribDivisor(4, 1);

            openGL->glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, group->batchList.bytesPerInstance,
                                          (void *)(instanceOffsetStart + sizeof(V4) * 4));
            openGL->glVertexAttribDivisor(5, 1);

            GLint transformLocation =
                openGL->glGetUniformLocation(openGL->shaders[R_ShaderType_Instanced3D].id, "transform");
            openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, state->transform.elements[0]);

            break;
        }
        case R_ShaderType_SkinnedMesh:
        {
            openGL->glEnableVertexAttribArray(0);
            openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, position));

            openGL->glEnableVertexAttribArray(1);
            openGL->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, normal));

            openGL->glEnableVertexAttribArray(2);
            openGL->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, uv));

            openGL->glEnableVertexAttribArray(3);
            openGL->glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, tangent));

            openGL->glEnableVertexAttribArray(4);
            openGL->glVertexAttribIPointer(4, 4, GL_UNSIGNED_INT, sizeof(MeshVertex),
                                           (void *)Offset(MeshVertex, boneIds));

            openGL->glEnableVertexAttribArray(5);
            openGL->glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, boneWeights));
            break;
        }
    }
}

internal void R_OpenGL_EndShader(R_ShaderType type)
{
    openGL->glUseProgram(0);
    switch (type)
    {
        case R_ShaderType_UI:
        {
            openGL->glVertexAttribDivisor(0, 0);
            openGL->glVertexAttribDivisor(1, 0);
            openGL->glVertexAttribDivisor(2, 0);
            openGL->glDisableVertexAttribArray(0);
            openGL->glDisableVertexAttribArray(1);
            openGL->glDisableVertexAttribArray(2);
            break;
        }
        case R_ShaderType_3D:
        {
            openGL->glDisableVertexAttribArray(0);
            openGL->glDisableVertexAttribArray(1);
            openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
            break;
        }
        case R_ShaderType_Instanced3D:
        {
            openGL->glDisableVertexAttribArray(0);
            openGL->glDisableVertexAttribArray(1);
            openGL->glDisableVertexAttribArray(2);
            openGL->glDisableVertexAttribArray(3);
            openGL->glDisableVertexAttribArray(4);
            openGL->glDisableVertexAttribArray(5);
            openGL->glVertexAttribDivisor(1, 0);
            openGL->glVertexAttribDivisor(2, 0);
            openGL->glVertexAttribDivisor(3, 0);
            openGL->glVertexAttribDivisor(4, 0);
            openGL->glVertexAttribDivisor(5, 0);
            openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            break;
        }
        case R_ShaderType_SkinnedMesh:
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            openGL->glDisableVertexAttribArray(0);
            openGL->glDisableVertexAttribArray(1);
            openGL->glDisableVertexAttribArray(2);
            openGL->glDisableVertexAttribArray(3);
            openGL->glDisableVertexAttribArray(4);
            openGL->glDisableVertexAttribArray(5);
            break;
        }
    }
}

//////////////////////////////
// Texture loading
//
inline GLuint GetPbo(u32 index)
{
    u64 ringIndex = index & (ArrayLength(openGL->pbos) - 1);
    GLuint pbo    = openGL->pbos[ringIndex];
    return pbo;
}

// Allocate individual texture
R_ALLOCATE_TEXTURE_2D(R_AllocateTexture2D)
{
    R_OpenGL_Texture *texture = openGL->freeTextures;
    while (texture && AtomicCompareExchangePtr(&openGL->freeTextures, texture->next, texture) != texture)
    {
        texture = openGL->freeTextures;
    }
    if (texture)
    {
        u64 gen = texture->generation;
        MemoryZeroStruct(texture);
        texture->generation = gen;
    }
    else
    {
        BeginTicketMutex(&openGL->mutex);
        texture = PushStruct(openGL->arena, R_OpenGL_Texture);
        EndTicketMutex(&openGL->mutex);
    }
    texture->width  = width;
    texture->height = height;
    texture->generation += 1;
    texture->format = format;

    R_OpenGL_TextureQueue *queue = &openGL->textureQueue;
    u32 writePos                 = AtomicIncrementU32(&queue->writePos) - 1;
    for (;;)
    {
        u32 availableSpots = queue->numOps - (writePos - queue->finalizePos);
        if (availableSpots >= 1)
        {
            u32 ringIndex          = (writePos & (queue->numOps - 1));
            R_OpenGL_TextureOp *op = queue->ops + ringIndex;
            op->data               = data;
            op->texture            = texture;
            op->status             = R_TextureLoadStatus_Untransferred;
            op->usesArray          = 0;
            while (AtomicCompareExchangeU32(&queue->endPos, writePos + 1, writePos) != writePos)
            {
                _mm_pause();
            }
            break;
        }
        _mm_pause();
    }

    R_Handle handle = R_OpenGL_HandleFromTexture(texture);
    return handle;
}

// Allocate using GL_TEXTURE_2D_ARRAY
// TODO: mipmaps using glcompressedtexture? or something idk
R_ALLOCATE_TEXTURE_2D(R_AllocateTextureInArray)
{
    R_Texture2DArrayTopology topology;
    topology.levels         = 1;
    topology.width          = width;
    topology.height         = height;
    topology.internalFormat = format;

    u32 maxSlots           = openGL->textureMap.maxSlots;
    u32 hash               = (u32)(HashStruct(&topology));
    u32 hashIndex          = hash % maxSlots;
    R_Texture2DArray *list = &openGL->textureMap.arrayList[hashIndex];

    BeginTicketMutex(&openGL->textureMap.mutex);
    while (list->hash != 0 && (list->hash != hash || list->freeListCount == 0))
    {
        hashIndex = (hashIndex + 1) & (maxSlots - 1);
        list      = &openGL->textureMap.arrayList[hashIndex];
    }

    if (list->hash == 0)
    {
        list->topology      = topology;
        list->freeList      = PushArray(openGL->arena, GLsizei, openGL->maxSlices);
        list->freeListCount = openGL->maxSlices;
        list->depth         = openGL->maxSlices;
        for (GLsizei i = openGL->maxSlices - 1; i >= 0; i--)
        {
            list->freeList[openGL->maxSlices - 1 - i] = i;
        }
        list->hash = hash;
    }
    Assert(list->freeListCount != 0);
    GLsizei freeIndex = list->freeList[--list->freeListCount];
    EndTicketMutex(&openGL->textureMap.mutex);

    R_Handle result;
    result.u32[0] = hashIndex;
    f32 slice     = (f32)freeIndex;
    MemoryCopy(&result.u32[1], &slice, sizeof(slice));
    result.u64[1] = GL_TEXTURE_ARRAY_HANDLE_FLAG;

    R_OpenGL_Texture *texture = openGL->freeTextures;
    while (texture && AtomicCompareExchangePtr(&openGL->freeTextures, texture->next, texture) != texture)
    {
        texture = texture->next;
    }
    if (texture)
    {
        u64 gen = texture->generation;
        MemoryZeroStruct(texture);
        texture->generation = gen;
    }
    else
    {
        BeginTicketMutex(&openGL->mutex);
        texture = PushStruct(openGL->arena, R_OpenGL_Texture);
        EndTicketMutex(&openGL->mutex);
    }
    texture->width  = width;
    texture->height = height;
    texture->generation += 1;
    texture->format = format;

    // Add to queue
    R_OpenGL_TextureQueue *queue = &openGL->textureQueue;
    u32 writePos                 = AtomicIncrementU32(&queue->writePos) - 1;
    for (;;)
    {
        u32 availableSpots = queue->numOps - (writePos - queue->finalizePos);
        if (availableSpots >= 1)
        {
            u32 ringIndex          = (writePos & (queue->numOps - 1));
            R_OpenGL_TextureOp *op = queue->ops + ringIndex;
            op->data               = data;
            op->texture            = texture;
            op->texSlice.hashIndex = result.u32[0];
            op->texSlice.slice     = (u32)freeIndex;
            op->status             = R_TextureLoadStatus_Untransferred;
            op->usesArray          = 1;
            while (AtomicCompareExchangeU32(&queue->endPos, writePos + 1, writePos) != writePos)
            {
                _mm_pause();
            }
            break;
        }
        _mm_pause();
    }

    return result;
}

R_DELETE_TEXTURE_2D(R_DeleteTexture2D)
{
    R_OpenGL_Texture *texture = R_OpenGL_TextureFromHandle(handle);
    texture->next             = openGL->freeTextures;
    while (AtomicCompareExchangePtr(openGL->freeTextures, texture, texture->next) != texture->next)
    {
        texture->next = openGL->freeTextures;
    }
}

internal void R_OpenGL_LoadTextures()
{
    R_OpenGL_TextureQueue *queue = &openGL->textureQueue;

    u32 endPos      = queue->endPos;
    u32 loadPos     = queue->loadPos;
    u32 finalizePos = queue->finalizePos;
    // Gets the Pixel Buffer Object to map to
    while (loadPos != endPos)
    {
        u32 ringIndex          = loadPos & (queue->numOps - 1);
        R_OpenGL_TextureOp *op = queue->ops + ringIndex;
        Assert(op->status == R_TextureLoadStatus_Untransferred);

        if (op->usesArray == 0)
        {
            if (op->texture->generation == 1)
            {
                glGenTextures(1, &op->texture->id);
            }
        }
        else
        {
            R_Texture2DArray *array = &openGL->textureMap.arrayList[op->texSlice.hashIndex];
            Assert(array);
            GLenum glFormat = R_OpenGL_GetFormat(array->topology.internalFormat);
            if (array->id == 0)
            {
                glGenTextures(1, &array->id);
                glBindTexture(GL_TEXTURE_2D_ARRAY, array->id);
#if 0
                openGL->glTexImage3D(GL_TEXTURE_2D_ARRAY, array->topology.levels, glFormat,
                array->topology.width,
                                     array->topology.height, array->depth, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif
                openGL->glTexStorage3D(GL_TEXTURE_2D_ARRAY, array->topology.levels, glFormat,
                                       array->topology.width, array->topology.height, array->depth);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
        }

        u64 availableSlots = ArrayLength(openGL->pbos) - (openGL->pboIndex - openGL->firstUsedPboIndex);
        if (availableSlots >= 1)
        {
            loadPos++;
            op->pboIndex = openGL->pboIndex++;
            GLuint pbo   = GetPbo(op->pboIndex);
            openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            i32 size = op->texture->width * op->texture->height * r_sizeFromFormatTable[op->texture->format];
            openGL->glBufferData(GL_PIXEL_UNPACK_BUFFER, size, 0, GL_STREAM_DRAW);
            op->buffer = (u8 *)openGL->glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            Assert(op->buffer);
            MemoryCopy(op->buffer, op->data, size);
            openGL->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            // TODO: this free doesn't work across DLL boundaries.
            // stbi_image_free(op->data);
            op->status = R_TextureLoadStatus_Transferred;
        }
        else
        {
            break;
        }
    }
    // Submits PBO to OpenGL
    while (finalizePos != queue->loadPos)
    {
        u32 ringIndex          = finalizePos++ & (queue->numOps - 1);
        R_OpenGL_TextureOp *op = queue->ops + ringIndex;
        if (op->status == R_TextureLoadStatus_Transferred)
        {
            if (op->usesArray == 0)
            {
                R_OpenGL_Texture *texture = op->texture;
                glBindTexture(GL_TEXTURE_2D, texture->id);

                openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GetPbo(op->pboIndex));

                GLenum glFormat = R_OpenGL_GetFormat(texture->format);

                glTexImage2D(GL_TEXTURE_2D, 0, glFormat, texture->width, texture->height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, 0);

                // TODO: pass these in as params
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glBindTexture(GL_TEXTURE_2D, 0);
                openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                openGL->firstUsedPboIndex++;
            }
            else
            {
                u32 slice               = op->texSlice.slice;
                R_Texture2DArray *array = &openGL->textureMap.arrayList[op->texSlice.hashIndex];

                GLenum format;
                switch (array->topology.internalFormat)
                {
                    case R_TexFormat_R8: format = GL_RED; break;
                    default: format = GL_RGBA; break;
                }
                glBindTexture(GL_TEXTURE_2D_ARRAY, array->id);
                openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GetPbo(op->pboIndex));
                // Printf("%u\n", glGetError());
                openGL->glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, slice, array->topology.width,
                                        array->topology.height, 1, format, GL_UNSIGNED_BYTE, 0);
                // Printf("%u\n", glGetError());
                Printf("Width: %u\nHeight: %u\nFormat: %u\n\n", array->topology.width, array->topology.height,
                       format);

                glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
                openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                openGL->firstUsedPboIndex++;
            }
        }
        else
        {
            break;
        }
    }
    queue->loadPos     = loadPos;
    queue->finalizePos = finalizePos;
}

//////////////////////////////
// Buffer loading
//
internal void R_InitializeBuffer(GPUBuffer *ioBuffer, const BufferUsageType inUsageType, const i32 inSize)
{
    GLenum target;
    switch (ioBuffer->type)
    {
        case BufferType_Vertex:
        {
            target = GL_ARRAY_BUFFER;
            break;
        }
        case BufferType_Index:
        {
            target = GL_ELEMENT_ARRAY_BUFFER;
            break;
        }
    }
    GLenum usage;
    switch (inUsageType)
    {
        case BufferUsage_Static:
        {
            usage = GL_STATIC_DRAW;
            break;
        }
        case BufferUsage_Dynamic:
        {
            usage = GL_DYNAMIC_DRAW;
            break;
        }
    }

    GLenum apiObject;
    openGL->glGenBuffers(1, &apiObject);
    openGL->glBindBuffer(target, apiObject);
    // TODO: AZDO, differentiate between opengl 4 and opengl 3?

    if (openGLInfo.persistentMap)
    {
        GLbitfield flags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;
        openGL->glBufferStorage(target, inSize, 0, flags);
        ioBuffer->mappedBufferBase = (u8 *)openGL->glMapBufferRange(target, 0, inSize, flags);
    }
    else
    {
        openGL->glBufferData(target, inSize, 0, usage);
    }

    R_BufferHandle handle = (u64)apiObject;
    ioBuffer->handle      = handle;
    ioBuffer->size        = inSize;
}

internal void R_MapGPUBuffer(GPUBuffer *buffer)
{
    if (openGLInfo.persistentMap)
    {
        Assert(buffer->mappedBufferBase);
    }
    else
    {
        GLenum target;
        switch (buffer->type)
        {
            case BufferType_Vertex:
            {
                target = GL_ARRAY_BUFFER;
                break;
            }
            case BufferType_Index:
            {
                target = GL_ELEMENT_ARRAY_BUFFER;
                break;
            }
        }

        GLint apiObject = R_OpenGL_GetBufferFromHandle(buffer->handle);
        openGL->glBindBuffer(target, apiObject);
        buffer->mappedBufferBase =
            (u8 *)openGL->glMapBufferRange(target, 0, buffer->size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    }
}

internal void R_UnmapGPUBuffer(GPUBuffer *buffer)
{
    if (openGLInfo.persistentMap)
    {
        Assert(buffer->mappedBufferBase);
    }
    else
    {
        GLenum target;
        switch (buffer->type)
        {
            case BufferType_Vertex:
            {
                target = GL_ARRAY_BUFFER;
                break;
            }
            case BufferType_Index:
            {
                target = GL_ELEMENT_ARRAY_BUFFER;
                break;
            }
        }

        GLint apiObject = R_OpenGL_GetBufferFromHandle(buffer->handle);
        openGL->glBindBuffer(target, apiObject);
        openGL->glUnmapBuffer(target);
        buffer->mappedBufferBase = 0;
    }
}

internal void R_UpdateBuffer(GPUBuffer *buffer, BufferUsageType type, void *data, i32 offset, i32 size)
{
    GLint id = R_OpenGL_GetBufferFromHandle(buffer->handle);
    GLenum target;
    // R_OpenGL_Buffer *buffer = (R_OpenGL_Buffer *)handle.u64[0];
    // TODO: I don't think I want the platform specific handle to have the type definition.

    switch (buffer->type)
    {
        case BufferType_Vertex:
        {
            target = GL_ARRAY_BUFFER;
            break;
        }
        case BufferType_Index:
        {
            target = GL_ELEMENT_ARRAY_BUFFER;
            break;
        }
    }

    if (openGLInfo.persistentMap && buffer->mappedBufferBase)
    {
        void *dest = (u8 *)(buffer->mappedBufferBase) + offset;
        MemoryCopy(dest, data, size);
    }
    else
    {
        openGL->glBindBuffer(target, id);
        openGL->glBufferSubData(target, offset, size, data);
    }
}

R_ALLOCATE_BUFFER(R_AllocateBuffer)
{
    R_OpenGL_Buffer *buffer = openGL->freeBuffers;
    while (buffer && AtomicCompareExchangePtr(&openGL->freeBuffers, buffer->next, buffer) != buffer)
    {
        buffer = buffer->next;
    }
    if (buffer)
    {
        u64 gen = buffer->generation;
        MemoryZeroStruct(buffer);
        buffer->generation = gen;
    }
    else
    {
        BeginTicketMutex(&openGL->mutex);
        buffer = PushStruct(openGL->arena, R_OpenGL_Buffer);
        EndTicketMutex(&openGL->mutex);
    }
    buffer->size = size;
    buffer->generation += 1;
    buffer->type = type;

    R_OpenGL_BufferQueue *queue = &openGL->bufferQueue;
    u32 writePos                = AtomicIncrementU32(&queue->writePos) - 1;
    for (;;)
    {
        u32 availableSpots = queue->numOps - (writePos - queue->readPos);
        if (availableSpots >= 1)
        {
            u32 ringIndex         = (writePos & (queue->numOps - 1));
            R_OpenGL_BufferOp *op = queue->ops + ringIndex;
            op->data              = data;
            op->buffer            = buffer;
            while (AtomicCompareExchangeU32(&queue->endPos, writePos + 1, writePos) != writePos)
            {
                _mm_pause();
            }
            break;
        }
        _mm_pause();
    }

    R_Handle result = R_OpenGL_HandleFromBuffer(buffer);
    return result;
}

internal void R_OpenGL_LoadBuffers()
{
    R_OpenGL_BufferQueue *queue = &openGL->bufferQueue;
    u32 readPos                 = queue->readPos;
    u32 endPos                  = queue->endPos;
    ReadBarrier();
    while (readPos != endPos)
    {
        u32 ringIndex         = readPos++ & (queue->numOps - 1);
        R_OpenGL_BufferOp *op = &queue->ops[ringIndex];
        if (op->buffer->generation == 1)
        {
            openGL->glGenBuffers(1, &op->buffer->id);
        }

        GLenum format;
        switch (op->buffer->type)
        {
            case R_BufferType_Vertex: format = GL_ARRAY_BUFFER; break;
            case R_BufferType_Index: format = GL_ELEMENT_ARRAY_BUFFER; break;
            default: Assert(!"Invalid");
        }
        openGL->glBindBuffer(format, op->buffer->id);
        // R_TempMemoryNode *data = (R_TempMemoryNode *)op->data.u64[0];
        openGL->glBufferData(format, op->buffer->size, op->data, GL_DYNAMIC_DRAW);
        openGL->glBindBuffer(format, 0);

        // R_FreeTemp(ptr);
    }
    queue->readPos = readPos;
}

//////////////////////////////
// HANDLES
//
internal R_Handle R_OpenGL_HandleFromBuffer(R_OpenGL_Buffer *buffer)
{
    R_Handle handle;
    handle.u64[0] = (u64)(buffer);
    handle.u64[1] = buffer->generation;
    return handle;
}

internal R_OpenGL_Buffer *R_OpenGL_BufferFromHandle(R_Handle handle)
{
    R_OpenGL_Buffer *buffer = (R_OpenGL_Buffer *)handle.u64[0];
    if (buffer == 0 || buffer->generation != handle.u64[1])
    {
        buffer = &r_opengl_bufferNil;
    }
    return buffer;
}
internal R_Handle R_OpenGL_HandleFromTexture(R_OpenGL_Texture *texture)
{
    R_Handle handle = {(u64)(texture), texture->generation};
    return handle;
}

internal R_OpenGL_Texture *R_OpenGL_TextureFromHandle(R_Handle handle)
{
    R_OpenGL_Texture *texture = (R_OpenGL_Texture *)(handle.u64[0]);
    if (texture == 0 || texture->generation != handle.u64[1])
    {
        texture = &r_opengl_textureNil;
    }
    return texture;
}
