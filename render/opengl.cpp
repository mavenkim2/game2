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

internal u32 OpenGLLoadTexture(Texture *texture)
{
    glGenTextures(1, &texture->id);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    switch (texture->type)
    {
        case TextureType_Diffuse:
        {
            // TODO: NOT SAFE!
            glTexImage2D(GL_TEXTURE_2D, 0, openGL->defaultTextureFormat, texture->width, texture->height, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, texture->contents);
        }
        case TextureType_Normal:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         texture->contents);
        }
        default:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         texture->contents);
            break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture->id;
}

internal void LoadModel(Model *model)
{
    // NOTE: must be 0
    if (!model->vbo)
    {
        openGL->glGenBuffers(1, &model->vbo);
        openGL->glGenBuffers(1, &model->ebo);

        openGL->glBindBuffer(GL_ARRAY_BUFFER, model->vbo);
        openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(model->vertices.items[0]) * model->vertices.count,
                             model->vertices.items, GL_STREAM_DRAW);

        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ebo);
        openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(model->indices.items[0]) * model->indices.count,
                             model->indices.items, GL_STREAM_DRAW);
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
    shader->base.globalsWriteTime = GetLastWriteTime(globalFilename);
    shader->base.vsWriteTime      = GetLastWriteTime(vsFilename);
    shader->base.fsWriteTime      = GetLastWriteTime(fsFilename);

    FreeFileMemory(vs.str);
    FreeFileMemory(fs.str);
    FreeFileMemory(g.str);
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
    modelShader->base.globalsWriteTime = GetLastWriteTime(globalFilename);
    modelShader->base.vsWriteTime      = GetLastWriteTime(vsFilename);
    modelShader->base.fsWriteTime      = GetLastWriteTime(fsFilename);

    FreeFileMemory(globals.str);
    FreeFileMemory(vs.str);
    FreeFileMemory(fs.str);
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
    if (GetLastWriteTime(shader->globalsFile) != shader->globalsWriteTime ||
        GetLastWriteTime(shader->vsFile) != shader->vsWriteTime ||
        GetLastWriteTime(shader->fsFile) != shader->fsWriteTime)
    {
        ReloadShader(shader);
    }
}

inline Texture *OpenGLGetTexFromHandle(RenderState *state, u32 handle)
{
    Texture *result = state->assetState->textures + handle;
    return result;
}

internal void OpenGLInit()
{
    openGL->arena = ArenaAllocDefault();
    openGL->glGenVertexArrays(1, &openGL->vao);
    openGL->glBindVertexArray(openGL->vao);

    openGL->glGenBuffers(1, &openGL->vertexBufferId);
    openGL->glGenBuffers(1, &openGL->indexBufferId);

    Texture whiteTexture   = {};
    whiteTexture.width     = 1;
    whiteTexture.height    = 1;
    whiteTexture.type      = TextureType_Nil;
    whiteTexture.loaded    = true;
    u32 data               = 0xffffffff;
    whiteTexture.contents  = (u8 *)&data;
    openGL->whiteTextureId = OpenGLLoadTexture(&whiteTexture);

    openGL->glGenBuffers(1, &openGL->pboId);
    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, openGL->pboId);
    openGL->glBufferData(GL_PIXEL_UNPACK_BUFFER, 1024 * 1024 * 4, 0, GL_STREAM_DRAW);
    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

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

    // RENDER CUBES
    // {
    //     openGL->glUseProgram(openGL->cubeShader.base.id);
    //     openGL->glBindVertexArray(openGL->vao);
    //     openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->vertexBufferId);
    //     openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(RenderVertex) * openGL->group.vertexCount,
    //                          openGL->group.vertexArray, GL_STREAM_DRAW);
    //
    //     openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->indexBufferId);
    //     openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * openGL->group.indexCount,
    //                          openGL->group.indexArray, GL_STREAM_DRAW);
    //
    //     GLuint positionId = openGL->cubeShader.base.positionId_UNIFORMS);;
    //     GLuint colorId    = openGL->cubeShader.colorId;
    //     GLuint normalId   = openGL->cubeShader.base.normalId;
    //     openGL->glEnableVertexAttribArray(positionId);
    //     openGL->glVertexAttribPointer(positionId, 4, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
    //                                   (void *)Offset(RenderVertex, p));
    //
    //     openGL->glEnableVertexAttribArray(colorId);
    //     openGL->glVertexAttribPointer(colorId, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
    //                                   (void *)Offset(RenderVertex, color));
    //
    //     openGL->glEnableVertexAttribArray(normalId);
    //     openGL->glVertexAttribPointer(normalId, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
    //                                   (void *)Offset(RenderVertex, n));
    //
    //     GLint transformLocation = openGL->glGetUniformLocation(openGL->cubeShader.base.id, "transform");
    //     openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, openGL->transform.elements[0]);
    //
    //     GLint cameraPosition = openGL->glGetUniformLocation(openGL->cubeShader.base.id, "cameraPosition");
    //     openGL->glUniform3fv(cameraPosition, 1, openGL->camera.position.elements);
    //
    //     glDrawElements(GL_TRIANGLES, 6 * openGL->group.quadCount, GL_UNSIGNED_SHORT, 0);
    //
    //     openGL->glUseProgram(0);
    //     openGL->glDisableVertexAttribArray(positionId);
    //     openGL->glDisableVertexAttribArray(colorId);
    //     openGL->glDisableVertexAttribArray(normalId);
    // }

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
            Model *model       = command->model;
            Skeleton *skeleton = &model->skeleton;
            if (!model->vbo)
            {
                LoadModel(model);
            }
            loopi(0, model->textureHandles.count)
            {
                Texture *texture = OpenGLGetTexFromHandle(state, model->textureHandles.items[i]);

                // TODO: make the texture loading code more streamlined and less bug prone
                u32 textureId = texture->id;
                if (texture->loaded == false)
                {
                    textureId = openGL->whiteTextureId;
                }
                switch (texture->type)
                {
                    case TextureType_Diffuse:
                    {
                        openGL->glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        break;
                    }
                    case TextureType_Normal:
                    {
                        openGL->glActiveTexture(GL_TEXTURE0 + 1);
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        break;
                    }
                    case TextureType_Nil:
                    {
                        openGL->glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        break;
                    }
                    default:
                    {
                        Assert(!"Texture type not supported in renderer.");
                    }
                }
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

            glDrawElements(GL_TRIANGLES, model->indices.count, GL_UNSIGNED_INT, 0);

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

internal u8 *R_AllocateTexture2D()
{
    u8 *ptr = 0;
    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, openGL->pboId);
    ptr = (u8 *)openGL->glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return ptr;
}

// TODO IMPORTANT: create multiple pbos so multiple textures can be downloaded at once
internal u32 R_SubmitTexture2D(u32 width, u32 height, R_TexFormat format)
{
    u32 id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, openGL->pboId);
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

    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    // glTexSubImage2D(GL_TEXTURE_2D, 0, glFormat, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    // TODO: pass these in as params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return id;
}

// internal void R_DeleteTexture2D
