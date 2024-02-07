global char *globalHeaderCode = R"(
#version 330 core
#define f32 float
#define V2 vec2
#define V3 vec3
#define V4 vec4
#define Mat4 mat4)";

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
        Printf("Fragment shader errors: %s\n", vertexShaderErrors);
        Printf("Program errors: %s\n", vertexShaderErrors);
    }

    openGL->glDeleteShader(vertexShaderId);
    openGL->glDeleteShader(fragmentShaderId);

    OpenGLShader result;
    result.id         = shaderProgramId;
    result.positionId = openGL->glGetAttribLocation(shaderProgramId, "pos");
    result.normalId   = openGL->glGetAttribLocation(shaderProgramId, "n");
    return result;
}

internal void CompileCubeProgram()
{
    char *vertexCode = R"(
                                    in V4 pos;
                                    in V3 colorIn;
                                    in V3 n;

                                    out V3 color;
                                    out V3 worldPosition;
                                    out V3 worldN;

                                    uniform mat4 transform; 

                                    void main()
                                    { 
                                        gl_Position = transform * pos;
                                        color = colorIn;
                                        worldPosition = pos.xyz;
                                        worldN = n;
                                    })";

    char *fragmentCode = R"(
                                    in V3 color;
                                    in V3 worldPosition;
                                    in V3 worldN;

                                    out V4 FragColor;

                                    uniform V3 cameraPosition;

                                    void main()
                                    {
                                        V3 lightPosition = vec3(0, 0, 5);
                                        f32 lightCoefficient = 50.f;
                                        V3 toLight = normalize(lightPosition - worldPosition);
                                        f32 lightDistance = distance(lightPosition, worldPosition);
                                        f32 lightStrength = lightCoefficient / (lightDistance * lightDistance); 

                                        //AMBIENT
                                        f32 ambient = 0.1f;
                                        //DIFFUSE
                                        f32 diffuseCoefficient = 0.1f;
                                        f32 diffuseCosAngle = max(dot(worldN, lightPosition), 0.f);
                                        f32 diffuse = diffuseCoefficient * diffuseCosAngle * lightStrength;
                                        //SPECULAR
                                        f32 specularCoefficient = 2.f;
                                        V3 toViewPosition = normalize(cameraPosition - worldPosition);
                                        // V3 reflectVector = -toLight + 2 * dot(worldN, toLight) * worldN;
                                        V3 reflectVector = -toViewPosition + 2 * dot(worldN, toViewPosition) * worldN;
                                        f32 specularStrength = pow(max(dot(reflectVector, toLight), 0.f), 64);
                                        // f32 specularStrength = pow(max(dot(reflectVector, toViewPosition), 0.f), 64);
                                        f32 specular = specularCoefficient * specularStrength * lightStrength;

                                        FragColor = (ambient + diffuse + specular) * V4(color, 1.f);
                                    })";

    CubeShader *cubeShader = &openGL->cubeShader;
    cubeShader->base       = OpenGLCreateProgram(globalHeaderCode, vertexCode, fragmentCode);
    cubeShader->colorId    = openGL->glGetAttribLocation(cubeShader->base.id, "colorIn");
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

internal void ReloadModelProgram()
{
    // HOT RELOAD!
    if (openGL->modelShader.base.id)
    {
        openGL->glDeleteProgram(openGL->modelShader.base.id);
    }
    CompileModelProgram();
}

internal void HotloadShaders(OpenGLShader *shader)
{
    if (GetLastWriteTime(shader->globalsFile) != shader->globalsWriteTime ||
        GetLastWriteTime(shader->vsFile) != shader->vsWriteTime ||
        GetLastWriteTime(shader->fsFile) != shader->fsWriteTime)
    {
        ReloadModelProgram();
    }
}

internal void OpenGLInit()
{
    // u32 maxQuadCountPerFrame     = 1 << 14;
    // openGL->group.maxVertexCount = maxQuadCountPerFrame * 4;
    // openGL->group.maxIndexCount  = maxQuadCountPerFrame * 6;

    openGL->glGenVertexArrays(1, &openGL->vao);
    openGL->glBindVertexArray(openGL->vao);

    openGL->glGenBuffers(1, &openGL->vertexBufferId);
    openGL->glGenBuffers(1, &openGL->indexBufferId);

    CompileCubeProgram();
    CompileModelProgram();
}

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
    }
    else
    {
        Unreachable;
    }
    ReleaseDC(window, dc);

    OpenGLInit();
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

internal void OpenGLEndFrame(RenderState *renderState, HDC deviceContext, int clientWidth, int clientHeight)
{
    // INITIALIZE
    {
        glViewport(0, 0, clientWidth, clientHeight);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
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
        // for (u32 i = 0; i < openGL->group.textureCount; i++)
        // {
        //     if (!openGL->group.textures[i].loaded)
        //     {
        //         OpenGLBindTexture(openGL, &openGL->group.textures[i], i);
        //     }
        // }

        GLuint positionId   = openGL->modelShader.base.positionId;
        GLuint normalId     = openGL->modelShader.base.normalId;
        GLuint uvId         = openGL->modelShader.uvId;
        GLuint boneIdId     = openGL->modelShader.boneIdId;
        GLuint boneWeightId = openGL->modelShader.boneWeightId;
        GLuint tangentId    = openGL->modelShader.tangentId;
        // TODO: I don't think the buffer data should be bound every frame
        u32 baseBone = 0;
        RenderCommand *command;
        foreach(&renderState->commands, command)
        {
            openGL->glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, command->mesh->textures[0]->id);

            openGL->glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, command->mesh->textures[1]->id);

            openGL->glActiveTexture(GL_TEXTURE0);

            Mesh *currentMesh  = command->mesh;
            Skeleton *skeleton = currentMesh->skeleton;

            openGL->glBindBuffer(GL_ARRAY_BUFFER, currentMesh->vbo);
            openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(currentMesh->vertices[0]) * currentMesh->vertexCount,
                                 currentMesh->vertices, GL_STREAM_DRAW);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, currentMesh->ebo);
            openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                 sizeof(currentMesh->indices[0]) * currentMesh->indexCount, currentMesh->indices,
                                 GL_STREAM_DRAW);

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

            Mat4 translate = Translate4(V3{0, 0, 5});
            Mat4 scale     = Scale4(V3{0.5f, 0.5f, 0.5f});
            Mat4 rotate    = Rotate4(MakeV3(1, 0, 0), PI / 2);

            Mat4 modelMat           = translate * rotate * scale;
            Mat4 newTransform       = renderState->transform * modelMat;
            GLint transformLocation = openGL->glGetUniformLocation(openGL->modelShader.base.id, "transform");
            openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, newTransform.elements[0]);

            if (command->finalBoneTransforms)
            {
                GLint boneTformLocation =
                    openGL->glGetUniformLocation(openGL->modelShader.base.id, "boneTransforms");
                openGL->glUniformMatrix4fv(boneTformLocation, skeleton->boneCount, GL_FALSE,
                                           command->finalBoneTransforms[baseBone].elements[0]);
            }

            GLint modelLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "model");
            openGL->glUniformMatrix4fv(modelLoc, 1, GL_FALSE, modelMat.elements[0]);

            V3 lightPosition  = MakeV3(5, 5, 0);
            GLint lightPosLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "lightPos");
            openGL->glUniform3fv(lightPosLoc, 1, lightPosition.elements);

            GLint viewPosLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "viewPos");
            openGL->glUniform3fv(viewPosLoc, 1, renderState->camera.position.elements);

            // TEXTURE MAP
            GLint textureLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "diffuseMap");
            openGL->glUniform1i(textureLoc, 0);

            // NORMAL MAP
            GLint nmapLoc = openGL->glGetUniformLocation(openGL->modelShader.base.id, "normalMap");
            openGL->glUniform1i(nmapLoc, 1);

            glDrawElements(GL_TRIANGLES, currentMesh->indexCount, GL_UNSIGNED_INT, 0);

            baseBone += skeleton->boneCount;
        }

        openGL->glUseProgram(0);
        // glBindTexture(GL_TEXTURE_2D, 0);
        openGL->glDisableVertexAttribArray(positionId);
        openGL->glDisableVertexAttribArray(normalId);
        openGL->glDisableVertexAttribArray(uvId);
        openGL->glDisableVertexAttribArray(tangentId);
        openGL->glDisableVertexAttribArray(boneIdId);
        openGL->glDisableVertexAttribArray(boneWeightId);

        // HOT RELOAD! this probably shouldnt' be here
        HotloadShaders(&openGL->modelShader.base);
    }

    // DOUBLE BUFFER SWAP
    SwapBuffers(deviceContext);
}
