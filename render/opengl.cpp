#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../asset.h"
#include "../asset_cache.h"
#include "./render.h"
#endif

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

internal OpenGLShader OpenGLCreateProgram(char *defines, char *vertexCode, char *fragmentCode)
{
    GLuint vertexShaderId      = openGL->glCreateShader(GL_VERTEX_SHADER);
    GLchar *vertexShaderCode[] = {
        defines,
        vertexCode,
    };
    openGL->glShaderSource(vertexShaderId, ArrayLength(vertexShaderCode), vertexShaderCode, 0);
    openGL->glCompileShader(vertexShaderId);

    GLuint fragmentShaderId      = openGL->glCreateShader(GL_FRAGMENT_SHADER);
    GLchar *fragmentShaderCode[] = {
        defines,
        fragmentCode,
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

    OpenGLShader result;
    result.id         = shaderProgramId;
    result.positionId = openGL->glGetAttribLocation(shaderProgramId, "pos");
    result.normalId   = openGL->glGetAttribLocation(shaderProgramId, "n");
    return result;
}

internal void LoadModel(Model *model)
{
    // NOTE: must be 0
    if (!model->vbo)
    {
        // TODO: this shouldn't be necessary. render.cpp should just give the right data

        LoadedModel *loadedModel = GetModel(model->loadedModel);
        openGL->glGenBuffers(1, &model->vbo);
        openGL->glGenBuffers(1, &model->ebo);

        openGL->glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
        openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(loadedModel->vertices[0]) * loadedModel->vertexCount,
                             loadedModel->vertices, GL_STREAM_DRAW);

        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ebo);
        openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(loadedModel->indices[0]) * loadedModel->indexCount,
                             loadedModel->indices, GL_STREAM_DRAW);
    }
}

internal void CompileCubeProgram(b32 instanced = false)
{
    TempArena scratch     = ScratchBegin(openGL->arena);
    string globalFilename = Str8Lit("src/shaders/global.glsl");
    string vsFilename     = Str8Lit("src/shaders/basic_3d.vs");
    string fsFilename     = Str8Lit("src/shaders/basic_3d.fs");

    string g  = ReadEntireFile(globalFilename);
    string vs = ReadEntireFile(vsFilename);
    string fs = ReadEntireFile(fsFilename);

    CubeShader *shader = &openGL->cubeShader;

    string globals;
    if (instanced)
    {
        globals = PushStr8F(scratch.arena, "%S#define INSTANCED 1\n", g);
        shader  = &openGL->instancedBasicShader;
    }
    else
    {
        globals = g;
    }

    shader->base    = OpenGLCreateProgram((char *)globals.str, (char *)vs.str, (char *)fs.str);
    shader->colorId = openGL->glGetAttribLocation(shader->base.id, "colorIn");

    shader->base.globalsFile      = globalFilename;
    shader->base.vsFile           = vsFilename;
    shader->base.fsFile           = fsFilename;
    shader->base.globalsWriteTime = OS_GetLastWriteTime(globalFilename);
    shader->base.vsWriteTime      = OS_GetLastWriteTime(vsFilename);
    shader->base.fsWriteTime      = OS_GetLastWriteTime(fsFilename);

    OS_Release(vs.str);
    OS_Release(fs.str);
    OS_Release(g.str);
    ScratchEnd(scratch);
}

internal void CompileModelProgram()
{
    string globalFilename = Str8Lit("src/shaders/global.glsl");
    string vsFilename     = Str8Lit("src/shaders/model.vs");
    string fsFilename     = Str8Lit("src/shaders/model.fs");

    string globals = ReadEntireFile(globalFilename);
    string vs      = ReadEntireFile(vsFilename);
    string fs      = ReadEntireFile(fsFilename);

    ModelShader *modelShader  = &openGL->modelShader;
    modelShader->base         = OpenGLCreateProgram((char *)globals.str, (char *)vs.str, (char *)fs.str);
    modelShader->uvId         = openGL->glGetAttribLocation(modelShader->base.id, "uv");
    modelShader->boneIdId     = openGL->glGetAttribLocation(modelShader->base.id, "boneIds");
    modelShader->boneWeightId = openGL->glGetAttribLocation(modelShader->base.id, "boneWeights");
    modelShader->tangentId    = openGL->glGetAttribLocation(modelShader->base.id, "tangent");

    modelShader->base.globalsFile      = globalFilename;
    modelShader->base.vsFile           = vsFilename;
    modelShader->base.fsFile           = fsFilename;
    modelShader->base.globalsWriteTime = OS_GetLastWriteTime(globalFilename);
    modelShader->base.vsWriteTime      = OS_GetLastWriteTime(vsFilename);
    modelShader->base.fsWriteTime      = OS_GetLastWriteTime(fsFilename);

    OS_Release(globals.str);
    OS_Release(vs.str);
    OS_Release(fs.str);
}

enum ShaderType
{
    OGL_Shader_Basic,
    OGL_Shader_Instanced,
    OGL_Shader_Model,
};

internal void ReloadShader(OpenGLShader *shader)
{
    i32 type = -1;
    if (shader->id == openGL->cubeShader.base.id)
    {
        type = OGL_Shader_Basic;
    }
    else if (shader->id == openGL->modelShader.base.id)
    {
        type = OGL_Shader_Model;
    }
    else if (shader->id == openGL->instancedBasicShader.base.id)
    {
        type = OGL_Shader_Instanced;
    }

    if (shader->id)
    {
        openGL->glDeleteProgram(shader->id);
    }
    switch (type)
    {
        case OGL_Shader_Basic:
            CompileCubeProgram();
            break;
        case OGL_Shader_Instanced:
            CompileCubeProgram(true);
            break;
        case OGL_Shader_Model:
            CompileModelProgram();
            break;
        default:
            break;
    }
}

internal void HotloadShaders(OpenGLShader *shader)
{
    if (OS_GetLastWriteTime(shader->globalsFile) != shader->globalsWriteTime ||
        OS_GetLastWriteTime(shader->vsFile) != shader->vsWriteTime ||
        OS_GetLastWriteTime(shader->fsFile) != shader->fsWriteTime)
    {
        ReloadShader(shader);
    }
}

internal void OpenGLInit()
{
    openGL->arena = ArenaAllocDefault();
    openGL->glGenVertexArrays(1, &openGL->vao);
    openGL->glBindVertexArray(openGL->vao);

    openGL->glGenBuffers(1, &openGL->vertexBufferId);
    openGL->glGenBuffers(1, &openGL->indexBufferId);

    // Pbos
    openGL->glGenBuffers(ArrayLength(openGL->pbos), openGL->pbos);
    openGL->pboIndex          = 0;
    openGL->firstUsedPboIndex = 0;

    for (u32 i = 0; i < ArrayLength(openGL->pbos); i++)
    {
        openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, openGL->pbos[i]);
        openGL->glBufferData(GL_PIXEL_UNPACK_BUFFER, 1024 * 1024 * 4, 0, GL_STREAM_DRAW);
        openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    // Default white texture
    u32 data   = 0xffffffff;
    u8 *buffer = 0;
    u64 pbo    = R_AllocateTexture2D(&buffer);
    MemoryCopy(buffer, &data, sizeof(data));
    R_SubmitTexture2D(&openGL->whiteTextureHandle, pbo, 1, 1, R_TexFormat_RGBA8);

    VSyncToggle(1);

    CompileCubeProgram();
    CompileCubeProgram(true);
    CompileModelProgram();
}

struct OpenGLInfo
{
    char *version;
    char *shaderVersion;
    b32 framebufferArb;
    b32 textureExt;
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
        }
    }
};

internal void Win32GetOpenGLExtensions() {}

internal void Win32InitOpenGL(HWND window)
{
    HDC dc = GetDC(window);

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
    OpenGLInit();
    ReleaseDC(window, dc);
};

internal void OpenGLBeginFrame(i32 width, i32 height)
{
    // openGL->width      = width;
    // openGL->height     = height;
    // RenderGroup *group = &openGL->group;
    // group->indexCount  = 0;
    // group->vertexCount = 0;
    // group->quadCount   = 0;
}

// TODO: not having to hardcode shaders and vertex attribs would be nice
internal void OpenGLEndFrame(RenderState *state, HDC deviceContext, int clientWidth, int clientHeight)
{
    // INITIALIZE
    {
        as_state = state->as_state;
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
    {
        openGL->glUseProgram(openGL->modelShader.base.id);

        GLuint positionId   = openGL->modelShader.base.positionId;
        GLuint normalId     = openGL->modelShader.base.normalId;
        GLuint uvId         = openGL->modelShader.uvId;
        GLuint boneIdId     = openGL->modelShader.boneIdId;
        GLuint boneWeightId = openGL->modelShader.boneWeightId;
        GLuint tangentId    = openGL->modelShader.tangentId;

        RenderCommand *command;
        foreach (&state->commands, command)
        {
            Model *model             = command->model;
            LoadedSkeleton *skeleton = command->skeleton;
            if (!model->vbo)
            {
                LoadModel(model);
            }
            openGL->glActiveTexture(GL_TEXTURE0);

            openGL->glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ebo);

            openGL->glEnableVertexAttribArray(positionId);
            openGL->glVertexAttribPointer(positionId, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, position));

            openGL->glEnableVertexAttribArray(normalId);
            openGL->glVertexAttribPointer(normalId, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, normal));

            openGL->glEnableVertexAttribArray(uvId);
            openGL->glVertexAttribPointer(uvId, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, uv));

            openGL->glEnableVertexAttribArray(tangentId);
            openGL->glVertexAttribPointer(tangentId, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, tangent));

            if (skeleton)
            {
                openGL->glEnableVertexAttribArray(boneIdId);
                openGL->glVertexAttribIPointer(boneIdId, 4, GL_UNSIGNED_INT, sizeof(MeshVertex),
                                               (void *)Offset(MeshVertex, boneIds));

                openGL->glEnableVertexAttribArray(boneWeightId);
                openGL->glVertexAttribPointer(boneWeightId, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                              (void *)Offset(MeshVertex, boneWeights));
            }

            Mat4 newTransform       = state->transform * model->transform;
            GLint transformLocation = openGL->glGetUniformLocation(openGL->modelShader.base.id, "transform");
            openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, newTransform.elements[0]);

            if (command->finalBoneTransforms)
            {
                GLint boneTformLocation =
                    openGL->glGetUniformLocation(openGL->modelShader.base.id, "boneTransforms");
                openGL->glUniformMatrix4fv(boneTformLocation, skeleton->count, GL_FALSE,
                                           command->finalBoneTransforms->elements[0]);
            }

            GLint modelLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "model");
            openGL->glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model->transform.elements[0]);

            V3 lightPosition  = MakeV3(5, 5, 0);
            GLint lightPosLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "lightPos");
            openGL->glUniform3fv(lightPosLoc, 1, lightPosition.elements);

            GLint viewPosLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "viewPos");
            openGL->glUniform3fv(viewPosLoc, 1, state->camera.position.elements);

            // TEXTURE MAP
            GLint textureLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "diffuseMap");
            openGL->glUniform1i(textureLoc, 0);

            // NORMAL MAP
            GLint nmapLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "normalMap");
            openGL->glUniform1i(nmapLoc, 1);

            openGL->glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, openGL->whiteTextureHandle);
            for (u32 i = 0; i < command->numMaterials; i++)
            {
                Material *material = command->materials + i;
                for (u32 j = 0; j < TextureType_Count; j++)
                {
                    R_Handle textureHandle = GetTextureRenderHandle(material->textureHandles[j]);
                    Texture *texture       = GetTexture(material->textureHandles[j]);
                    if (textureHandle == 0)
                    {
                        textureHandle = openGL->whiteTextureHandle;
                    }
                    switch (j)
                    {
                        case TextureType_Diffuse:
                        {
                            openGL->glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, textureHandle);
                            break;
                        }
                        case TextureType_Normal:
                        {
                            openGL->glActiveTexture(GL_TEXTURE0 + 1);
                            glBindTexture(GL_TEXTURE_2D, textureHandle);
                            break;
                        }
                        default:
                        {
                            continue;
                        }
                    }
                }
                glDrawElements(GL_TRIANGLES, material->onePlusEndIndex - material->startIndex, GL_UNSIGNED_INT,
                               (void *)(sizeof(u32) * material->startIndex));
            }

            // UNBIND
            openGL->glUseProgram(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            openGL->glDisableVertexAttribArray(positionId);
            openGL->glDisableVertexAttribArray(normalId);
            openGL->glDisableVertexAttribArray(uvId);
            openGL->glDisableVertexAttribArray(tangentId);
            openGL->glDisableVertexAttribArray(boneIdId);
            openGL->glDisableVertexAttribArray(boneWeightId);
        }

        // DEBUG PASS
        DebugRenderer *renderer = &state->debugRenderer;
        openGL->glUseProgram(openGL->cubeShader.base.id);
        if (!renderer->vbo)
        {
            openGL->glGenBuffers(1, &renderer->vbo);
            openGL->glGenBuffers(1, &renderer->instanceVbo);
            openGL->glGenBuffers(1, &renderer->instanceVbo2);
            openGL->glGenBuffers(1, &renderer->ebo);
        }

        glLineWidth(4.f);
        openGL->glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
        openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(renderer->lines.items[0]) * renderer->lines.count,
                             renderer->lines.items, GL_DYNAMIC_DRAW);
        openGL->glEnableVertexAttribArray(0);
        openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                                      (void *)Offset(DebugVertex, pos));
        openGL->glEnableVertexAttribArray(1);
        openGL->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                                      (void *)Offset(DebugVertex, color));

        GLint transformLocation = openGL->glGetUniformLocation(openGL->cubeShader.base.id, "transform");
        openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, state->transform.elements[0]);

        // Lines
        glDrawArrays(GL_LINES, 0, renderer->lines.count);

        // Points
        glPointSize(4.f);
        openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(renderer->points.items[0]) * renderer->points.count,
                             renderer->points.items, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS, 0, renderer->points.count);

        // Indexed lines
        openGL->glUseProgram(openGL->instancedBasicShader.base.id);
        transformLocation = openGL->glGetUniformLocation(openGL->instancedBasicShader.base.id, "transform");
        openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, state->transform.elements[0]);
        glLineWidth(4.f);

        openGL->glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
        openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(renderer->indexLines.items[0]) * renderer->indexLines.count,
                             renderer->indexLines.items, GL_DYNAMIC_DRAW);
        openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V3), 0);
        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ebo);

        openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * renderer->indices.count,
                             renderer->indices.items, GL_DYNAMIC_DRAW);

        u32 vertexCount = 0;
        u32 indexCount  = 0;
        Primitive *primitive;
        // TODO: shouldn't have to blit every frame, only need to upload the instance data
        forEach(renderer->primitives, primitive)
        {
            u64 totalSize = sizeof(primitive->colors[0]) * ArrayLen(primitive->colors) +
                            sizeof(primitive->transforms[0]) * ArrayLen(primitive->transforms);

            openGL->glBindBuffer(GL_ARRAY_BUFFER, renderer->instanceVbo);
            openGL->glBufferData(GL_ARRAY_BUFFER, totalSize, 0, GL_DYNAMIC_DRAW);

            u64 totalOffset = 0;
            // Set color of primitive
            openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, sizeof(V4) * ArrayLen(primitive->colors),
                                    primitive->colors);
            totalOffset += sizeof(V4) * ArrayLen(primitive->colors);
            openGL->glBufferSubData(GL_ARRAY_BUFFER, totalOffset, sizeof(Mat4) * ArrayLen(primitive->transforms),
                                    primitive->transforms);
            openGL->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(V4), 0);
            openGL->glVertexAttribDivisor(1, 1);

            // Set transform of primitive
            loopi(0, 4)
            {
                openGL->glEnableVertexAttribArray(2 + i);
                openGL->glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4),
                                              (void *)(totalOffset + sizeof(V4) * i));
                openGL->glVertexAttribDivisor(2 + i, 1);
            }
            // Draw
            openGL->glDrawElementsInstancedBaseVertex(GL_LINES, primitive->indexCount, GL_UNSIGNED_INT,
                                                      (GLvoid *)(indexCount * sizeof(u32)),
                                                      ArrayLen(primitive->transforms), vertexCount);
            // Prep for next primitive draw
            vertexCount += primitive->vertexCount;
            indexCount += primitive->indexCount;
        }
        // TODO: Draw the rest of the indexed lines?
        openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        openGL->glUseProgram(0);
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

        // HOT RELOAD! this probably shouldnt' be here
        HotloadShaders(&openGL->modelShader.base);
        HotloadShaders(&openGL->cubeShader.base);
        HotloadShaders(&openGL->instancedBasicShader.base);
    }

    // DOUBLE BUFFER SWAP
    SwapBuffers(deviceContext);
}

//////////////////////////////
// Externally called functions
//
inline GLuint GetPbo(u64 handle)
{
    u64 ringIndex = handle & (ArrayLength(openGL->pbos) - 1);
    GLuint pbo    = openGL->pbos[ringIndex];
    return pbo;
}

R_ALLOC_TEXTURE_2D(R_AllocateTexture2D)
{
    u64 handle         = 0;
    u64 availableSlots = ArrayLength(openGL->pbos) - (openGL->pboIndex - openGL->firstUsedPboIndex);
    if (availableSlots >= 1)
    {
        handle     = openGL->pboIndex++;
        GLuint pbo = GetPbo(handle);
        openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        *out = (u8 *)openGL->glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    return handle;
}

R_TEXTURE_SUBMIT_2D(R_SubmitTexture2D)
{
    b32 newTexture = 0;
    if (*textureHandle == 0)
    {
        glGenTextures(1, textureHandle);
        newTexture = 1;
    }
    glBindTexture(GL_TEXTURE_2D, *textureHandle);

    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GetPbo(pboHandle));
    openGL->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    u32 glFormat = GL_RGB8;
    switch (format)
    {
        case R_TexFormat_RGBA8:
        {
            glFormat = GL_RGBA8;
        }
        case R_TexFormat_SRGB:
        {
            glFormat = openGL->defaultTextureFormat;
        }
        default:
            break;
    }

    if (newTexture)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }
    else
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }

    // TODO: pass these in as params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    openGL->firstUsedPboIndex++;
}

R_DELETE_TEXTURE_2D(R_DeleteTexture2D)
{
    glDeleteTextures(1, &handle);
}
