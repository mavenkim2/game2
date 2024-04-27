#include "opengl.h"

#include "../keepmovingforward_memory.cpp"
#include "../keepmovingforward_string.cpp"
#include "../thread_context.cpp"
#include "vertex_cache.cpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

global const i32 GL_ATTRIB_INDEX_POSITION = 0;
global const i32 GL_ATTRIB_INDEX_NORMAL   = 1;

global const i32 GL_SHADOW_MAP_BINDING      = 0;
global const i32 GL_SKINNING_MATRIX_BINDING = 4;
global const i32 GL_PER_DRAW_BUFFER_BINDING = 3;
global const i32 GL_GLOBALUNIFORMS_BINDING  = 2;

global const i32 MAX_MESHES            = 32;
global const i32 MAX_SKINNING_MATRICES = 4096;

#define GL_TEXTURE_ARRAY_HANDLE_FLAG 0x8000000000000000
const i32 cTexturesPerMaterial = 2;

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

global readonly i32 r_sizeFromFormatTable[R_TexFormat_Count] = {1, 2, 3, 4, 3, 4};

// TODO: absorb this into the shader type, instantiate a table containing the filenames as well as preprocess
// defines also make uniforms more manageable by having

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

internal GLuint R_OpenGL_CompileShader(char *globals, char *vs, char *fs, char *gs)
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

    GLint geometryShaderId = -1;
    if (gs)
    {
        geometryShaderId             = openGL->glCreateShader(GL_GEOMETRY_SHADER);
        GLchar *geometryShaderCode[] = {
            globals,
            gs,
        };
        openGL->glShaderSource(geometryShaderId, ArrayLength(geometryShaderCode), geometryShaderCode, 0);
        openGL->glCompileShader(geometryShaderId);
    }

    GLuint shaderProgramId = openGL->glCreateProgram();
    openGL->glAttachShader(shaderProgramId, vertexShaderId);
    openGL->glAttachShader(shaderProgramId, fragmentShaderId);

    if (gs)
    {
        openGL->glAttachShader(shaderProgramId, geometryShaderId);
    }
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

        if (gs)
        {
            char geometryShaderErrors[4096];
            openGL->glGetShaderInfoLog(geometryShaderId, 4096, 0, geometryShaderErrors);
            Printf("Geometry shader errors: %s\n", geometryShaderErrors);
        }
    }

    openGL->glDeleteShader(vertexShaderId);
    openGL->glDeleteShader(fragmentShaderId);
    if (gs)
    {
        openGL->glDeleteShader(geometryShaderId);
    }

    if (!success)
    {
        openGL->glDeleteProgram(shaderProgramId);
    }

    return shaderProgramId;
}

internal GLuint R_OpenGL_CreateShader(Arena *arena, string globalsPath, string vsPath, string fsPath,
                                      string gsPath, string preprocess)
{
    TempArena temp = ScratchStart(&arena, 1);

    Printf("Vertex shader: %S\n", vsPath);
    Printf("Fragment shader: %S\n", fsPath);

    string gTemp = {};
    if (globalsPath.size != 0)
    {
        gTemp = platform.OS_ReadEntireFile(temp.arena, globalsPath);
    }
    string vs = {};
    if (vsPath.size != 0)
    {
        vs = platform.OS_ReadEntireFile(temp.arena, vsPath);
    }
    string fs = {};
    if (fsPath.size != 0)
    {
        fs = platform.OS_ReadEntireFile(temp.arena, fsPath);
    }
    string gs = {};
    if (gsPath.size != 0)
    {
        gs = platform.OS_ReadEntireFile(temp.arena, gsPath);
    }
    string globals = StrConcat(temp.arena, gTemp, preprocess);

    GLuint id = R_OpenGL_CompileShader((char *)globals.str, (char *)vs.str, (char *)fs.str, (char *)gs.str);

    ScratchEnd(temp);
    return id;
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
        openGL->scratchVboSize = kilobytes(128);
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

    // Texture arrays
    {
        // glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &openGL->maxSlices);
        openGL->maxSlices            = 16;
        openGL->textureMap.maxSlots  = 32;
        openGL->textureMap.arrayList = PushArray(openGL->arena, R_Texture2DArray, openGL->textureMap.maxSlots);
    }

    {
        // see render.cpp same constant should be

        openGL->glGenFramebuffers(1, &openGL->shadowMapPassFBO);
        glGenTextures(1, &openGL->depthMapTextureArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, openGL->depthMapTextureArray);

        // TODO: glTexStorage3D?
        openGL->glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, cShadowMapSize, cShadowMapSize,
                             cNumCascades, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        f32 borderColor[4] = {1.f, 1.f, 1.f, 1.f};
        // sampling outside of the atlas returns not in shadow
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

        openGL->glBindFramebuffer(GL_FRAMEBUFFER, openGL->shadowMapPassFBO);
        openGL->glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, openGL->depthMapTextureArray, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        GLint status = openGL->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            Printf("Error: %u\n", status);
        }
        openGL->glBindFramebuffer(GL_FRAMEBUFFER, 0);

        openGL->glGenBuffers(1, &openGL->lightMatrixUBO);
        openGL->glBindBuffer(GL_UNIFORM_BUFFER, openGL->lightMatrixUBO);
        openGL->glBufferData(GL_UNIFORM_BUFFER, sizeof(Mat4) * 16, 0, GL_DYNAMIC_DRAW);
    }

    // Shaders
    openGL->progManager.Init();

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

    // Environment cubemap
    {
        GenerateIBLFromHDR(&openGL->cubeMap, &openGL->irradianceMap, &openGL->prefilterMap, &openGL->brdfLut);
    }
    VSyncToggle(1);
}

internal void OpenGLGetInfo()
{
    // TODO: I think you have to check wgl get extension string first? who even knows let's just use vulkan
    if (openGL->glGetStringi)
    {
        openGL->openGLInfo.version       = (char *)glGetString(GL_VERSION);
        openGL->openGLInfo.shaderVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        GLint numExtensions              = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        loopi(0, (u32)numExtensions)
        {
            char *extension = (char *)openGL->glGetStringi(GL_EXTENSIONS, i);
            if (Str8C(extension) == Str8Lit("GL_EXT_framebuffer_sRGB")) openGL->openGLInfo.framebufferArb = true;
            else if (Str8C(extension) == Str8Lit("GL_ARB_framebuffer_sRGB")) openGL->openGLInfo.framebufferArb = true;
            else if (Str8C(extension) == Str8Lit("GL_EXT_texture_sRGB")) openGL->openGLInfo.textureExt = true;
            else if (Str8C(extension) == Str8Lit("GL_ARB_shader_draw_parameters"))
            {
                openGL->openGLInfo.shaderDrawParametersArb = true;
                Printf("gl_DrawID should work\n");
            }
            else if (Str8C(extension) == Str8Lit("GL_ARB_texture_non_power_of_two"))
            {
                // Printf("whoopie!\n");
            }
            else if (Str8C(extension) == Str8Lit("GL_ARB_buffer_storage"))
            {
                openGL->openGLInfo.persistentMap = true;
            }
        }
    }
};

Shared *shared;
PlatformApi platform;

DLL R_INIT(R_Init)
{
    if (ioRenderMem->mIsHotloaded || !ioRenderMem->mIsLoaded)
    {
        platform                  = ioRenderMem->mPlatform;
        Printf                    = platform.Printf;
        ioRenderMem->mIsHotloaded = 0;
        shared                    = ioRenderMem->mShared;
        ThreadContextSet(ioRenderMem->mTctx);
        openGL = (OpenGL *)ioRenderMem->mRenderer;
    }

#if WINDOWS
    if (!ioRenderMem->mIsLoaded)
    {
        ioRenderMem->mIsLoaded = 1;
        ioRenderMem->mRenderer = (PlatformRenderer *)R_Win32_OpenGL_Init(handle);
    }
#else
#error OS not implemented
#endif
}

internal OpenGL *R_Win32_OpenGL_Init(OS_Handle handle)
{
    OpenGL *openGL_ = (OpenGL *)platform.OS_Alloc(sizeof(OpenGL));
    openGL          = openGL_;
    HWND window     = (HWND)handle.handle;
    HDC dc          = GetDC(window);

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
        Win32GetOpenGLFunction(glDrawElementsBaseVertex);
        Win32GetOpenGLFunction(glGenFramebuffers);
        Win32GetOpenGLFunction(glBindFramebuffer);
        Win32GetOpenGLFunction(glFramebufferTexture);
        Win32GetOpenGLFunction(glCheckFramebufferStatus);
        Win32GetOpenGLFunction(glUniform1fv);
        Win32GetOpenGLFunction(glMultiDrawElementsIndirect);
        Win32GetOpenGLFunction(glBindRenderbuffer);
        Win32GetOpenGLFunction(glGenRenderbuffers);
        Win32GetOpenGLFunction(glFramebufferRenderbuffer);
        Win32GetOpenGLFunction(glRenderbufferStorage);
        Win32GetOpenGLFunction(glFramebufferTexture2D);
        Win32GetOpenGLFunction(glDeleteFramebuffers);
        Win32GetOpenGLFunction(glDeleteRenderbuffers);
        Win32GetOpenGLFunction(glGenerateMipmap);

        OpenGLGetInfo();

        openGL->srgb8TextureFormat  = GL_RGB8;
        openGL->srgba8TextureFormat = GL_RGBA8;

        if (openGL->openGLInfo.textureExt)
        {
            openGL->srgb8TextureFormat  = GL_SRGB8;
            openGL->srgba8TextureFormat = GL_SRGB8_ALPHA8;
        }
    }
    else
    {
        Unreachable;
    }
    R_OpenGL_Init();
    ReleaseDC(window, dc);
    return openGL_;
};

internal void R_BeginFrame(i32 width, i32 height)
{
    // openGL->width      = width;
    // openGL->height     = height;
    // RenderGroup *group = &openGL->group;
    // group->indexCount  = 0;
    // group->vertexCount = 0;
    // group->quadCount   = 0;
}

DLL R_ENDFRAME(R_EndFrame)
{
#if WINDOWS
    V2 viewport       = platform.OS_GetWindowDimension(shared->windowHandle);
    HWND window       = (HWND)shared->windowHandle.handle;
    HDC deviceContext = GetDC(window);
    R_Win32_OpenGL_EndFrame(renderState, deviceContext, (i32)viewport.x, (i32)viewport.y);
    ReleaseDC(window, deviceContext);
#else
#error
#endif
}

void R_ProgramManager::Init()
{
    R_ShaderLoadInfo shaderLoadInfo[R_ShaderType_Count] =
        {
            {R_ShaderType_Mesh, "src/shaders/model", {{"USE_PBR", "1"}, {"MULTI_DRAW_TEXTURE_ARRAY", "1"}}, ShaderStage_Default, ShaderLoadFlag_TextureArrays},
            {R_ShaderType_UI, "src/shaders/ui", {}, ShaderStage_Default, ShaderLoadFlag_TextureArrays},
            {R_ShaderType_Depth, "src/shaders/depth", {}, ShaderStage_Default | ShaderStage_Geometry, ShaderLoadFlag_ShadowMaps},
            {R_ShaderType_3D, "src/shaders/basic_3d", {}, ShaderStage_Default},
            {R_ShaderType_Instanced3D, "src/shaders/basic_3d", {{"INSTANCED", "1"}}, ShaderStage_Default},
            {R_ShaderType_Terrain, "src/shaders/terrain", {}, ShaderStage_Default},
            {R_ShaderType_Skybox, "src/shaders/skybox", {}, ShaderStage_Default},
        };
    MemoryCopy(mShaderLoadInfo, shaderLoadInfo, sizeof(R_ShaderLoadInfo) * R_ShaderType_Count);
    cGlobalsPath = PushStr8Copy(openGL->arena, Str8Lit("src/shaders/global.glsl"));

    for (i32 i = 0; i < R_ShaderType_Count; i++)
    {
        // TODO: having to manually allocate these strings isn't ideal.
        mShaders[shaderLoadInfo[i].mType].mName = PushStr8Copy(openGL->arena, Str8C(shaderLoadInfo[i].mName));
    }

    LoadPrograms();

    // TODO: make this function platform nonspecific by removing this
    GLuint constantBuffer;
    openGL->glGenBuffers(1, &constantBuffer);

    openGL->glBindBuffer(GL_UNIFORM_BUFFER, constantBuffer);
    openGL->glBufferData(GL_UNIFORM_BUFFER, sizeof(V4) * MAX_GLOBAL_UNIFORMS, 0, GL_DYNAMIC_DRAW);
    mGlobalUniformsBuffer = (R_BufferHandle)constantBuffer;

    // TODO: this should probably just be a ubo, doing this just for padding for now to keep it simple
    GLuint ssbo;
    openGL->glGenBuffers(1, &ssbo);
    openGL->glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    // TODO: persistently map this when available
    openGL->glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_MESHES * sizeof(R_MeshPerDrawParams), 0, GL_DYNAMIC_DRAW);
    mPerDrawBuffer = (R_BufferHandle)ssbo;

    GLuint indirect;
    openGL->glGenBuffers(1, &indirect);
    openGL->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect);
    openGL->glBufferData(GL_DRAW_INDIRECT_BUFFER, MAX_MESHES * sizeof(R_IndirectCmd), 0, GL_DYNAMIC_DRAW);
    mIndirectBuffer = (R_BufferHandle)indirect;
}

void R_ProgramManager::HotloadPrograms()
{
    LoadPrograms();
}

void R_ProgramManager::LoadPrograms()
{
    TempArena temp          = ScratchStart(0, 0);
    u64 lastModifiedGlobals = platform.OS_GetLastWriteTime(cGlobalsPath);
    for (R_ShaderType type = (R_ShaderType)0; type < R_ShaderType_Count; type = (R_ShaderType)(type + 1))
    {
        R_ShaderLoadInfo *loadInfo = &mShaderLoadInfo[type];
        Shader *shader             = &mShaders[loadInfo->mType];

        // TODO: operator overload with +
        string vsPath = StrConcat(temp.arena, shader->mName, ".vs");
        string fsPath = StrConcat(temp.arena, shader->mName, ".fs");

        u64 lastModifiedVS = platform.OS_GetLastWriteTime(vsPath);
        u64 lastModifiedFS = platform.OS_GetLastWriteTime(fsPath);

        u64 lastModifiedGS = 0;
        string gsPath      = "";
        if (loadInfo->mShaderStageFlags & ShaderStage_Geometry)
        {
            gsPath         = StrConcat(temp.arena, shader->mName, ".gs");
            lastModifiedGS = platform.OS_GetLastWriteTime(gsPath);
        }

        if (lastModifiedVS != shader->mLastModifiedVS || lastModifiedFS != shader->mLastModifiedFS || lastModifiedGS != shader->mLastModifiedGS || lastModifiedGlobals != mLastModifiedGlobals)
        {
            mLastModifiedGlobals = lastModifiedGlobals;
            Program *program     = &mPrograms[loadInfo->mType];
            string preprocess    = "";
            for (i32 macroIndex = 0; macroIndex < ArrayLength(loadInfo->mMacros); macroIndex++)
            {
                if (loadInfo->mMacros[macroIndex].mName != 0)
                {
                    preprocess = PushStr8F(temp.arena, "%S#define %s %s\n", preprocess, loadInfo->mMacros[macroIndex].mName, loadInfo->mMacros[macroIndex].mDef);
                }
            }

            GLuint programId = (GLuint)program->mApiObject;
            if (programId != 0)
            {
                openGL->glDeleteProgram(programId);
            }

            shader->mLastModifiedVS = lastModifiedVS;
            shader->mLastModifiedFS = lastModifiedFS;
            shader->mLastModifiedGS = lastModifiedGS;

            programId           = R_OpenGL_CreateShader(temp.arena, cGlobalsPath, vsPath, fsPath, gsPath, preprocess);
            program->mApiObject = (R_BufferHandle)programId;

            GLint *samplerIds = 0;
            samplerIds        = PushArray(temp.arena, GLint, openGL->textureMap.maxSlots);
            for (u32 j = 0; j < openGL->textureMap.maxSlots; j++)
            {
                samplerIds[j] = j;
            }

            if (loadInfo->mShaderLoadFlags)
            {
                openGL->glUseProgram(programId);
            }
            if (loadInfo->mShaderLoadFlags & ShaderLoadFlag_TextureArrays)
            {
                GLint textureLoc = openGL->glGetUniformLocation(programId, "textureMaps");
                openGL->glUniform1iv(textureLoc, openGL->textureMap.maxSlots, samplerIds);
            }
        }
    }
    ScratchEnd(temp);
}

R_BufferHandle R_ProgramManager::GetProgramApiObject(R_ShaderType type)
{
    return mPrograms[type].mApiObject;
}

inline void R_ProgramManager::SetUniform(UniformType type, i32 param, V4 value)
{
    switch (type)
    {
        case UniformType_Global:
        {
            mGlobalUniforms[param] = value;
            mUniformsAdded         = true;
            break;
        }
    }
}

inline void R_ProgramManager::SetUniform(UniformType type, i32 param, V3 value)
{
    SetUniform(type, param, MakeV4(value, 0.f));
}

// memory mapped buffer to avoid two copies? persistent mapping?
inline void R_ProgramManager::SetUniform(UniformType type, UniformParam param, Mat4 *matrix)
{
    for (i32 i = 0; i < 4; i++)
    {
        SetUniform(type, param + i, matrix->columns[i]);
    }
}

inline void R_ProgramManager::SetUniform(UniformType type, UniformParam param, f32 values[4])
{
    SetUniform(type, param, *(V4 *)values);
}

void R_ProgramManager::CommitUniforms(UniformType type)
{
    switch (type)
    {
        case UniformType_Global:
        {
            if (mUniformsAdded)
            {
                openGL->glBindBuffer(GL_UNIFORM_BUFFER, (GLuint)mGlobalUniformsBuffer);
                openGL->glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(V4) * MAX_GLOBAL_UNIFORMS, mGlobalUniforms);
                mUniformsAdded = false;
            }
            break;
        }
    }
}

internal void R_Win32_OpenGL_EndFrame(RenderState *renderState, HDC deviceContext, int clientWidth, int clientHeight)
{
    // TIMED_FUNCTION();
    TempArena temp = ScratchStart(0, 0);

    // Load queued buffers and textures
    {
        R_OpenGL_LoadBuffers();
        openGL->glActiveTexture(GL_TEXTURE0);
        R_OpenGL_LoadTextures();
    }

    // INITIALIZE
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
        // TODO: NOT SAFE!
        if (openGL->openGLInfo.framebufferArb)
        {
            glEnable(GL_FRAMEBUFFER_SRGB);
        }
        glCullFace(GL_BACK);
        glClearColor(0.5f, 0.5f, 0.5f, 1.f);
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Set global uniforms
    openGL->progManager.SetUniform(UniformType_Global, UniformParam_ViewPerspective, &renderState->transform);
    openGL->progManager.SetUniform(UniformType_Global, UniformParam_ViewMatrix, &renderState->viewMatrix);
    openGL->progManager.SetUniform(UniformType_Global, UniformParam_ViewPosition, renderState->camera.position);

    openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_GLOBALUNIFORMS_BINDING, (GLuint)openGL->progManager.mGlobalUniformsBuffer);
    // Shadow map pass
    Mat4 *lightViewProjectionMatrices    = renderState->shadowMapMatrices;
    R_MeshPreparedDrawParams *drawParams = renderState->drawParams; // D_PrepareMeshes();

    f32 *cascadeDistances = renderState->cascadeDistances; //[cNumSplits];
    {
        // openGL->glActiveTexture(GL_TEXTURE0 + 1);
        // glBindTexture(GL_TEXTURE_2D_ARRAY, openGL->depthMapTextureArray);

        openGL->progManager.SetUniform(UniformType_Global, UniformParam_CascadeDist, cascadeDistances);
        openGL->progManager.CommitUniforms(UniformType_Global);

        openGL->glBindFramebuffer(GL_FRAMEBUFFER, openGL->shadowMapPassFBO);
        glViewport(0, 0, cShadowMapSize, cShadowMapSize);
        glClear(GL_DEPTH_BUFFER_BIT);

        R_PassMesh *pass = renderState->passes[R_PassType_Mesh].passMesh;
        ViewLight *light = pass->viewLight;
        for (; light != 0; light = light->next)
        {
            if (light->type == LightType_Directional)
            {
                openGL->progManager.SetUniform(UniformType_Global, UniformParam_LightDir, light->dir);
                break;
            }
        }

        GLuint currentProg = (GLuint)openGL->progManager.GetProgramApiObject(R_ShaderType_Depth);
        openGL->glUseProgram(currentProg);
        openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_SHADOW_MAP_BINDING, openGL->lightMatrixUBO);
        openGL->glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Mat4) * cNumCascades, lightViewProjectionMatrices);

        GLuint jointBuffer = (GLuint)renderState->vertexCache.mFrameData[renderState->vertexCache.mCurrentDrawIndex].mUniformBuffer.mHandle;
        openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_SKINNING_MATRIX_BINDING, jointBuffer);

        // TODO: find a way to collapse this with the render pass below
        R_MeshParamsList *list = &pass->list;

        // TODO: this just uses the static data. need to handle the dynamic data.
        openGL->glBindBuffer(GL_ARRAY_BUFFER, (GLuint)renderState->vertexCache.mStaticData.mVertexBuffer.mHandle);
        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)renderState->vertexCache.mStaticData.mIndexBuffer.mHandle);

        // Skinning matrices
        openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_SKINNING_MATRIX_BINDING, (GLuint)renderState->vertexCache.mFrameData[renderState->vertexCache.mCurrentDrawIndex].mUniformBuffer.mHandle);

        // Indirect buffer
        openGL->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, (GLuint)openGL->progManager.mIndirectBuffer);
        openGL->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, GL_PER_DRAW_BUFFER_BINDING, (GLuint)openGL->progManager.mPerDrawBuffer);

        // Per mesh draw parameters

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

        // TODO: uniform blocks so we can just push a struct or something instead of having to do
        // these all manually

        // Draw shadow only models
        {
            openGL->glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, light->mNumShadowSurfaces * sizeof(R_IndirectCmd), light->drawParams->mIndirectBuffers);
            openGL->glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(R_MeshPerDrawParams) * light->mNumShadowSurfaces, light->drawParams->mPerMeshDrawParams);
            openGL->glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, light->mNumShadowSurfaces, 0);
        }

        {
            openGL->glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, list->mTotalSurfaceCount * sizeof(R_IndirectCmd), drawParams->mIndirectBuffers);
            openGL->glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(R_MeshPerDrawParams) * list->mTotalSurfaceCount, drawParams->mPerMeshDrawParams);
            openGL->glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, list->mTotalSurfaceCount, 0);
        }

        glCullFace(GL_BACK);
        glViewport(0, 0, clientWidth, clientHeight);
        openGL->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        openGL->glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // Bind texture map slots
    {
        for (u32 i = 0; i < openGL->textureMap.maxSlots; i++)
        {
            R_Texture2DArray *aList = &openGL->textureMap.arrayList[i];
            openGL->glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D_ARRAY, aList->id);
        }
    }

    // Height map
    for (R_Command *command = renderState->head; command != 0; command = (R_Command *)command->next)
    {
        switch (command->type)
        {
            case R_CommandType_Null: continue;
            case R_CommandType_Heightmap:
            {
                Heightmap *heightmap = &((R_CommandHeightMap *)command)->heightmap;
                GPUBuffer *buffer    = renderState->vertexCache.VC_GetBufferFromHandle(heightmap->vertexHandle, BufferType_Vertex);
                if (buffer)
                {
                    openGL->glBindBuffer(GL_ARRAY_BUFFER, R_OpenGL_GetBufferFromHandle(buffer->mHandle));
                }

                buffer = renderState->vertexCache.VC_GetBufferFromHandle(heightmap->indexHandle, BufferType_Index);
                if (buffer)
                {
                    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, R_OpenGL_GetBufferFromHandle(buffer->mHandle));
                }

                GLuint id = (GLuint)openGL->progManager.GetProgramApiObject(R_ShaderType_Terrain);
                openGL->glUseProgram(id);

                GLint widthLoc = openGL->glGetUniformLocation(id, "width");
                openGL->glUniform1f(widthLoc, (f32)heightmap->width);

                GLint heightLoc = openGL->glGetUniformLocation(id, "inHeight");
                openGL->glUniform1f(heightLoc, (f32)heightmap->height);

                openGL->glVertexAttribPointer(GL_ATTRIB_INDEX_POSITION, 1, GL_FLOAT, GL_FALSE, sizeof(f32), 0);
                openGL->glEnableVertexAttribArray(GL_ATTRIB_INDEX_POSITION);

                i32 indexSize =
                    (i32)(((heightmap->indexHandle >> VERTEX_CACHE_SIZE_SHIFT) & VERTEX_CACHE_SIZE_MASK) /
                          sizeof(u32));
                u64 indexOffset = (heightmap->indexHandle >> VERTEX_CACHE_OFFSET_SHIFT) & VERTEX_CACHE_OFFSET_MASK;
                i32 baseVertex =
                    (i32)((heightmap->vertexHandle >> VERTEX_CACHE_OFFSET_SHIFT) & VERTEX_CACHE_OFFSET_MASK) /
                    sizeof(f32);

                openGL->glDrawElementsBaseVertex(GL_TRIANGLE_STRIP, indexSize, GL_UNSIGNED_INT,
                                                 (GLvoid *)indexOffset, baseVertex);

                break;
            }
        }
    }

    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        R_Pass *pass = &renderState->passes[type];
        switch (type)
        {
            case R_PassType_UI:
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                GLuint id = (GLuint)openGL->progManager.GetProgramApiObject(R_ShaderType_UI);
                openGL->glUseProgram(id);
                openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
                u32 totalOffset = 0;

                R_BatchList *list = &pass->passUI->batchList;
                for (R_BatchNode *node = list->first; node != 0; node = node->next)
                {
                    openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, node->val.byteCount, node->val.data);
                    totalOffset += node->val.byteCount;
                }

                openGL->glVertexAttribPointer(GL_ATTRIB_INDEX_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(R_RectInst),
                                              (void *)Offset(R_RectInst, pos));
                openGL->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R_RectInst),
                                              (void *)Offset(R_RectInst, scale));
                openGL->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(R_RectInst),
                                              (void *)Offset(R_RectInst, handle));
                openGL->glEnableVertexAttribArray(GL_ATTRIB_INDEX_POSITION);
                openGL->glEnableVertexAttribArray(1);
                openGL->glEnableVertexAttribArray(2);

                openGL->glVertexAttribDivisor(GL_ATTRIB_INDEX_POSITION, 1);
                openGL->glVertexAttribDivisor(1, 1);
                openGL->glVertexAttribDivisor(2, 1);

                f32 transform[] = {
                    2.f / (f32)clientWidth, 0, 0, 0,
                    0, 2.f / (f32)clientHeight, 0, 0,
                    0, 0, 1, 0,
                    -1, -1, 0, 1};

                GLuint loc = openGL->glGetUniformLocation(id, "transform");
                openGL->glUniformMatrix4fv(loc, 1, GL_FALSE, transform);

                openGL->glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, list->numInstances);
                R_OpenGL_EndShader(R_ShaderType_UI);
                glDisable(GL_BLEND);
                break;
            }
            case R_PassType_Mesh:
            {
                GLuint currentProgram = (GLuint)openGL->progManager.GetProgramApiObject(R_ShaderType_Mesh);
                openGL->glUseProgram(currentProgram);
                // TEXTURE MAP

                R_MeshParamsList *list = &pass->passMesh->list;

                GLuint jointBuffer = (GLuint)renderState->vertexCache.mFrameData[renderState->vertexCache.mCurrentDrawIndex].mUniformBuffer.mHandle;
                openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_SKINNING_MATRIX_BINDING, jointBuffer);

                // TODO: this just uses the static data. need to handle the dynamic data.
                openGL->glBindBuffer(GL_ARRAY_BUFFER, (GLuint)renderState->vertexCache.mStaticData.mVertexBuffer.mHandle);
                openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)renderState->vertexCache.mStaticData.mIndexBuffer.mHandle);

                // Skinning matrices
                openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_SKINNING_MATRIX_BINDING, (GLuint)renderState->vertexCache.mFrameData[renderState->vertexCache.mCurrentDrawIndex].mUniformBuffer.mHandle);

                // Indirect buffer
                openGL->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, (GLuint)openGL->progManager.mIndirectBuffer);

                // Per mesh draw parameters
                openGL->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, GL_PER_DRAW_BUFFER_BINDING, (GLuint)openGL->progManager.mPerDrawBuffer);

                R_OpenGL_StartShader(renderState, R_ShaderType_Mesh, 0);

                // TODO: these don't have to be specified per draw. Use SSBO or UBO

                // Light space matrices (for testing fragment depths against shadow atlas)
                openGL->glBindBufferBase(GL_UNIFORM_BUFFER, GL_SHADOW_MAP_BINDING, openGL->lightMatrixUBO);

                // Shadow maps generated in previous pass
                GLint shadowMapLoc = openGL->glGetUniformLocation(currentProgram, "shadowMaps");
                openGL->glUniform1i(shadowMapLoc, 32);
                openGL->glActiveTexture(GL_TEXTURE0 + 32);
                glBindTexture(GL_TEXTURE_2D_ARRAY, openGL->depthMapTextureArray);

                GLint irradiancemapLoc = openGL->glGetUniformLocation(currentProgram, "irradianceMap");
                openGL->glUniform1i(irradiancemapLoc, 33);
                openGL->glActiveTexture(GL_TEXTURE0 + 33);
                glBindTexture(GL_TEXTURE_CUBE_MAP, openGL->irradianceMap);

                GLint brdfLoc = openGL->glGetUniformLocation(currentProgram, "brdfMap");
                openGL->glUniform1i(brdfLoc, 34);
                openGL->glActiveTexture(GL_TEXTURE0 + 34);
                glBindTexture(GL_TEXTURE_2D, openGL->brdfLut);

                GLint prefilterLoc = openGL->glGetUniformLocation(currentProgram, "prefilterMap");
                openGL->glUniform1i(prefilterLoc, 35);
                openGL->glActiveTexture(GL_TEXTURE0 + 35);
                glBindTexture(GL_TEXTURE_CUBE_MAP, openGL->prefilterMap);

                openGL->glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, list->mTotalSurfaceCount, 0);
                R_OpenGL_EndShader(R_ShaderType_Mesh);
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
                            glPointSize(5.f);
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

    // render skybox last
    GLuint shader = (GLuint)openGL->progManager.GetProgramApiObject(R_ShaderType_Skybox);
    openGL->glUseProgram(shader);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    openGL->glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, openGL->cubeMap);

    openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
    openGL->glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cubeVertices), cubeVertices);
    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEbo);
    openGL->glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(cubeIndices), cubeIndices);
    openGL->glEnableVertexAttribArray(0);
    openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V3), 0);

    GLint matrix = openGL->glGetUniformLocation(shader, "projection");
    openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, renderState->projection.elements[0]);
    matrix = openGL->glGetUniformLocation(shader, "view");
    openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, renderState->viewMatrix.elements[0]);

    GLint textureLoc = openGL->glGetUniformLocation(shader, "environmentMap");
    openGL->glUniform1i(textureLoc, 0);

    glDrawElements(GL_TRIANGLES, ArrayLength(cubeIndices), GL_UNSIGNED_SHORT, 0);

    ScratchEnd(temp);

    // DOUBLE BUFFER SWAP
    SwapBuffers(deviceContext);

    openGL->progManager.HotloadPrograms();
}

internal void R_OpenGL_StartShader(RenderState *state, R_ShaderType type, void *inputGroup)
{
    GLuint id = (GLuint)openGL->progManager.GetProgramApiObject(type);
    switch (type)
    {
        case R_ShaderType_3D:
        {
            Assert(inputGroup == 0);
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

            GLint transformLocation = openGL->glGetUniformLocation(id, "transform");
            openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, state->transform.elements[0]);

            break;
        }
        case R_ShaderType_Mesh:
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
        case R_ShaderType_Mesh:
        {
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

internal void R_OpenGL_PushTextureOp(void *inData, R_OpenGL_Texture *inTexture,
                                     const u32 inHashIndex = 0, const u32 inHashSlice = 0, const b8 inUsesArray = 0)
{
    R_OpenGL_TextureQueue *queue = &openGL->textureQueue;
    u32 writePos                 = AtomicIncrementU32(&queue->writePos) - 1;
    for (;;)
    {
        u32 availableSpots = queue->numOps - (writePos - queue->finalizePos);
        if (availableSpots >= 1)
        {
            u32 ringIndex          = (writePos & (queue->numOps - 1));
            R_OpenGL_TextureOp *op = queue->ops + ringIndex;
            op->data               = inData;
            op->texture            = inTexture;
            op->texSlice.hashIndex = inHashIndex;
            op->texSlice.slice     = inHashSlice;
            op->status             = R_TextureLoadStatus_Untransferred;
            op->usesArray          = inUsesArray;
            while (AtomicCompareExchangeU32(&queue->endPos, writePos + 1, writePos) != writePos)
            {
                _mm_pause();
            }
            break;
        }
        _mm_pause();
    }
}

// Allocate individual texture
R_ALLOCATE_TEXTURE_2D(R_AllocateTexture2D)
{
    R_OpenGL_Texture *texture = R_OpenGL_CreateTexture(width, height, format);
    R_OpenGL_PushTextureOp(data, texture);

    R_Handle handle = R_OpenGL_HandleFromTexture(texture);
    return handle;
}

// Allocate using GL_TEXTURE_2D_ARRAY
// TODO: mipmaps using glcompressedtexture? or something idk
DLL R_ALLOCATE_TEXTURE_2D(R_AllocateTextureInArray)
{
    // TODO IMPORTANT: integrate this with R_OpenGL_Texture, create array of texture pointers instead of linked list,
    // use hash array (similar to asset system) to get index of matching texture array, if it doesn't exist
    // atomic increment and create new texture
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
    result.u32[1] = (u32)freeIndex;

    result.u64[1] = GL_TEXTURE_ARRAY_HANDLE_FLAG;

    R_OpenGL_Texture *texture = R_OpenGL_CreateTexture(width, height, format);

    // Add to queue
    R_OpenGL_PushTextureOp(data, texture, result.u32[0], (u32)freeIndex, true);

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
            GLenum glFormat = R_OpenGL_GetInternalFormat(array->topology.internalFormat);
            if (array->id == 0)
            {
                glGenTextures(1, &array->id);
                glBindTexture(GL_TEXTURE_2D_ARRAY, array->id);
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

                GLenum glFormat       = R_OpenGL_GetFormat(texture->format);
                GLenum internalFormat = R_OpenGL_GetInternalFormat(texture->format);

                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texture->width, texture->height, 0, glFormat,
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

                GLenum format = R_OpenGL_GetFormat(array->topology.internalFormat);
                glBindTexture(GL_TEXTURE_2D_ARRAY, array->id);
                openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GetPbo(op->pboIndex));
                // Printf("%u\n", glGetError());
                openGL->glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, slice, array->topology.width,
                                        array->topology.height, 1, format, GL_UNSIGNED_BYTE, 0);
                free(op->data);
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
// Renderer api implementation
//
DLL R_INITIALIZE_BUFFER(R_InitializeBuffer)
{
    GLenum target;
    switch (ioBuffer->mType)
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
        case BufferType_Uniform:
        {
            target = GL_UNIFORM_BUFFER;
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

    if (openGL->openGLInfo.persistentMap)
    {
        GLbitfield flags = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;
        openGL->glBufferStorage(target, inSize, 0, flags);
        ioBuffer->mMappedBufferBase = (u8 *)openGL->glMapBufferRange(target, 0, inSize, flags);
    }
    else
    {
        Assert(!"Not supported yet");
        openGL->glBufferData(target, inSize, 0, usage);
    }

    R_BufferHandle handle = (u64)apiObject;
    ioBuffer->mHandle     = handle;
    ioBuffer->mSize       = inSize;
}

DLL R_MAP_GPU_BUFFER(R_MapGPUBuffer)
{
    if (openGL->openGLInfo.persistentMap)
    {
        Assert(buffer->mMappedBufferBase);
    }
    else
    {
        GLenum target;
        switch (buffer->mType)
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

        GLint apiObject = R_OpenGL_GetBufferFromHandle(buffer->mHandle);
        openGL->glBindBuffer(target, apiObject);
        buffer->mMappedBufferBase =
            (u8 *)openGL->glMapBufferRange(target, 0, buffer->mSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    }
}

DLL R_UNMAP_GPU_BUFFER(R_UnmapGPUBuffer)
{
    if (openGL->openGLInfo.persistentMap)
    {
        Assert(buffer->mMappedBufferBase);
    }
    else
    {
        GLenum target;
        switch (buffer->mType)
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

        GLint apiObject = R_OpenGL_GetBufferFromHandle(buffer->mHandle);
        openGL->glBindBuffer(target, apiObject);
        openGL->glUnmapBuffer(target);
        buffer->mMappedBufferBase = 0;
    }
}

DLL R_UPDATE_BUFFER(R_UpdateBuffer)
{
    GLint id = R_OpenGL_GetBufferFromHandle(buffer->mHandle);
    GLenum target;
    // R_OpenGL_Buffer *buffer = (R_OpenGL_Buffer *)handle.u64[0];
    // TODO: I don't think I want the platform specific handle to have the type definition.

    switch (buffer->mType)
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
        case BufferType_Uniform:
        {
            target = GL_UNIFORM_BUFFER;
            break;
        }
    }

    if (openGL->openGLInfo.persistentMap && buffer->mMappedBufferBase)
    {
        void *dest = (u8 *)(buffer->mMappedBufferBase) + offset;
        MemoryCopy(dest, data, size);
    }
    else
    {
        Assert(!"Not supported yet");
        openGL->glBindBuffer(target, id);
        openGL->glBufferSubData(target, offset, size, data);
    }
}

DLL R_ALLOCATE_BUFFER(R_AllocateBuffer)
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

//////////////////////////////
// Image based lighting
//

internal R_OpenGL_Texture *R_OpenGL_CreateTexture(const i32 inWidth, const i32 inHeight, R_TexFormat format)
{
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
    texture->width  = inWidth;
    texture->height = inHeight;
    texture->generation += 1;
    texture->format = format;

    return texture;
}

enum ImageType
{
    ImageType_2D,
    ImageType_Cubemap,
};

// NOTE: if 0 is passed, allocate but don't upload any data
internal GLuint R_OpenGL_AllocTexture(const ImageType inTexType, const i32 inWidth, const i32 inHeight,
                                      const GLenum inInternalFormat, GLenum inType, const b8 inGenMips = 0,
                                      const void *inData = 0)
{
    GLuint id;
    switch (inTexType)
    {
        case ImageType_2D:
        {
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, inInternalFormat, inWidth, inHeight, 0, inType, GL_FLOAT, inData);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        }
        case ImageType_Cubemap:
        {
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_CUBE_MAP, id);
            for (u32 i = 0; i < 6; i++)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, inInternalFormat, inWidth, inHeight, 0, inType,
                             GL_FLOAT, inData);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            if (inGenMips)
            {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                // openGL->glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
            }
            else
            {
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            }
            break;
        }
    }
    return id;
}

internal void GenerateIBLFromHDR(GLuint *outCubeMap, GLuint *outIrradianceMap, GLuint *outPrefilterCubemap,
                                 GLuint *outBrdfLut)
{
    GLuint hdrTex;
    GLuint envCubemap;
    GLuint shader = 0;

    // Load the hdr file
    stbi_set_flip_vertically_on_load(true);
    i32 width, height, nComponents;
    f32 *data = stbi_loadf("data/industrial_sunset_puresky_8k.hdr", &width, &height, &nComponents, 0);

    if (data)
    {
        hdrTex = R_OpenGL_AllocTexture(ImageType_2D, width, height, GL_RGB16F, GL_RGB, 0, data);
        stbi_image_free(data);
    }
    else
    {
        Assert(!"??");
    }

    TempArena temp = ScratchStart(0, 0);
    shader         = R_OpenGL_CreateShader(temp.arena, "", "src/shaders/cubemap.vs", "src/shaders/equirectangular.fs", "", "");

    GLuint captureFbo, captureRbo;
    openGL->glGenFramebuffers(1, &captureFbo);
    openGL->glGenRenderbuffers(1, &captureRbo);

    openGL->glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
    openGL->glBindRenderbuffer(GL_RENDERBUFFER, captureRbo);
    openGL->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 2048, 2048);
    openGL->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);

    // Create output cubemap
    envCubemap = R_OpenGL_AllocTexture(ImageType_Cubemap, 2048, 2048, GL_RGB16F, GL_RGB, true);

    openGL->glUseProgram(shader);

    // Bind hdr texture
    GLint textureLoc = openGL->glGetUniformLocation(shader, "equirectangular");
    openGL->glUniform1i(textureLoc, 0);

    openGL->glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex);

    // Upload cube vertices/indices
    openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
    openGL->glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(cubeVertices), cubeVertices);
    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEbo);
    openGL->glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(cubeIndices), cubeIndices);
    openGL->glEnableVertexAttribArray(0);
    openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V3), 0);

    // VP for 6 cube faces

    // NOTE: gl cubemaps are left handed
    Mat4 projection = Perspective4(Radians(90.f), 1.f, .1f, 10.f);
    Mat4 views[]    = {
        // LookAt4({0, 0, 0}, {1, 0, 0}, {0, -1, 0}),  // +x
        // LookAt4({0, 0, 0}, {-1, 0, 0}, {0, -1, 0}), // -x
        // LookAt4({0, 0, 0}, {0, 1, 0}, {0, 0, 1}),   // +y
        // LookAt4({0, 0, 0}, {0, -1, 0}, {0, 0, -1}), // -y
        // LookAt4({0, 0, 0}, {0, 0, 1}, {0, -1, 0}),  // +z
        // LookAt4({0, 0, 0}, {0, 0, -1}, {0, -1, 0}), // +z
        LookAt4({0, 0, 0}, {1, 0, 0}, {0, -1, 0}),  // +x
        LookAt4({0, 0, 0}, {-1, 0, 0}, {0, -1, 0}), // -x
        LookAt4({0, 0, 0}, {0, 1, 0}, {0, 0, 1}),   // +y
        LookAt4({0, 0, 0}, {0, -1, 0}, {0, 0, -1}), // -y
        LookAt4({0, 0, 0}, {0, 0, 1}, {0, -1, 0}),  // +z
        LookAt4({0, 0, 0}, {0, 0, -1}, {0, -1, 0}), // +z
    };

    glViewport(0, 0, 2048, 2048);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    openGL->glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
    GLint matrix = openGL->glGetUniformLocation(shader, "projection");
    openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, projection.elements[0]);

    // Render hdr texture to each cube map face
    for (u32 i = 0; i < 6; i++)
    {
        openGL->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        matrix = openGL->glGetUniformLocation(shader, "view");
        openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, views[i].elements[0]);
        glDrawElements(GL_TRIANGLES, ArrayLength(cubeIndices), GL_UNSIGNED_SHORT, 0);
    }

    // Generates mips for the cubemap
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    openGL->glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glDeleteTextures(1, &hdrTex);

    //////////////////////////////////////////////////////////////////
    // Create irradiance map
    shader = R_OpenGL_CreateShader(temp.arena, "", "src/shaders/cubemap.vs", "src/shaders/convolution.fs", "", "");
    openGL->glUseProgram(shader);

    textureLoc = openGL->glGetUniformLocation(shader, "environmentMap");
    openGL->glUniform1i(textureLoc, 0);
    GLuint irradianceMap = R_OpenGL_AllocTexture(ImageType_Cubemap, 32, 32, GL_RGB16F, GL_RGB);

    openGL->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    openGL->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);

    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glViewport(0, 0, 32, 32);
    openGL->glBindFramebuffer(GL_FRAMEBUFFER, captureFbo);
    matrix = openGL->glGetUniformLocation(shader, "projection");
    openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, projection.elements[0]);

    // Convolute the cubemap (all directions equally for diffuse)
    for (u32 i = 0; i < 6; i++)
    {
        openGL->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        matrix = openGL->glGetUniformLocation(shader, "view");
        openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, views[i].elements[0]);
        glDrawElements(GL_TRIANGLES, ArrayLength(cubeIndices), GL_UNSIGNED_SHORT, 0);
    }

    //////////////////////////////////////////////////////////////////
    // Specular BRDF Split Sum Approximation
    //

    // Prefilter the environment map
    shader = R_OpenGL_CreateShader(temp.arena, "", "src/shaders/cubemap.vs", "src/shaders/prefilter.fs", "", "");
    openGL->glUseProgram(shader);

    textureLoc = openGL->glGetUniformLocation(shader, "envMap");
    openGL->glUniform1i(textureLoc, 0);
    matrix = openGL->glGetUniformLocation(shader, "projection");
    openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, projection.elements[0]);

    i32 baseWidth           = 128;
    i32 baseHeight          = 128;
    GLuint prefilterCubemap = R_OpenGL_AllocTexture(ImageType_Cubemap, baseWidth, baseHeight, GL_RGB16F, GL_RGB, true);
    openGL->glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    const i32 cMipLevels = 5;
    for (u32 mips = 0; mips < cMipLevels; mips++)
    {
        u32 mipWidth  = (u32)(baseWidth * Powf(.5f, (f32)mips));
        u32 mipHeight = (u32)(baseHeight * Powf(.5f, (f32)mips));
        openGL->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        openGL->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);
        glViewport(0, 0, mipWidth, mipHeight);
        f32 roughness = (f32)(mips) / (cMipLevels - 1);

        GLint rLoc = openGL->glGetUniformLocation(shader, "roughness");
        openGL->glUniform1f(rLoc, roughness);
        for (u32 i = 0; i < 6; i++)
        {
            openGL->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterCubemap, mips);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            matrix = openGL->glGetUniformLocation(shader, "view");
            openGL->glUniformMatrix4fv(matrix, 1, GL_FALSE, views[i].elements[0]);

            glDrawElements(GL_TRIANGLES, ArrayLength(cubeIndices), GL_UNSIGNED_SHORT, 0);
        }
    }

    //////////////////////////////////////////////////////////////////
    // Generate LUT for specular BRDF

    GLuint brdfLut = R_OpenGL_AllocTexture(ImageType_2D, 512, 512, GL_RG16F, GL_RG);

    shader = R_OpenGL_CreateShader(temp.arena, "", "src/shaders/quad.vs", "src/shaders/integrate_brdf.fs", "", "");
    openGL->glUseProgram(shader);

    openGL->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    openGL->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRbo);
    openGL->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLut, 0);
    glViewport(0, 0, 512, 512);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    *outCubeMap          = envCubemap;
    *outIrradianceMap    = irradianceMap;
    *outPrefilterCubemap = prefilterCubemap;
    *outBrdfLut          = brdfLut;
    ScratchEnd(temp);

    openGL->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    openGL->glDeleteFramebuffers(1, &captureFbo);
    openGL->glDeleteRenderbuffers(1, &captureRbo);
}

//////////////////////////////
// Tex parameters
//

// enum TexParams
// {
//     TexParams_Nearest,
//     TexParams_Linear,
//     TexParams_Repeat,
//     TexParams_Clamp,
//     TexParams_ClampToBorder
// };
