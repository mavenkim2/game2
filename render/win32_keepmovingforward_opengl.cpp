global char *globalHeaderCode = R"(
                                #version 330 core
                                #define f32 float
                                #define V2 vec2
                                #define V3 vec3
                                #define V4 vec4
                                #define Mat4 mat4
                                )";
// TODO: hotload shaders?
internal OpenGLShader OpenGLCreateProgram(OpenGL *openGL, char *defines, char *vertexCode, char *fragmentCode)
{
    GLuint vertexShaderId = openGL->glCreateShader(GL_VERTEX_SHADER);
    GLchar *vertexShaderCode[] = {
        defines,
        vertexCode,
    };
    openGL->glShaderSource(vertexShaderId, ArrayLength(vertexShaderCode), vertexShaderCode, 0);
    openGL->glCompileShader(vertexShaderId);

    GLuint fragmentShaderId = openGL->glCreateShader(GL_FRAGMENT_SHADER);
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

        Assert(!"Shader failed");
    }

    openGL->glDeleteShader(vertexShaderId);
    openGL->glDeleteShader(fragmentShaderId);

    OpenGLShader result;
    result.id = shaderProgramId;
    result.positionId = openGL->glGetAttribLocation(shaderProgramId, "pos");
    result.normalId = openGL->glGetAttribLocation(shaderProgramId, "n");
    return result;
}

internal void CompileCubeProgram(OpenGL *openGL)
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
                                        V3 lightPosition = vec3(-20, 10, 5);
                                        f32 lightCoefficient = 50.f;
                                        V3 toLight = normalize(lightPosition - worldPosition);
                                        f32 lightDistance = distance(lightPosition, worldPosition);
                                        f32 lightStrength = lightCoefficient / (lightDistance * lightDistance); 

                                        //AMBIENT
                                        f32 ambient= 0.1f;
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
    cubeShader->base = OpenGLCreateProgram(openGL, globalHeaderCode, vertexCode, fragmentCode);
    cubeShader->colorId = openGL->glGetAttribLocation(cubeShader->base.id, "colorIn");
}

internal void CompileModelProgram(OpenGL *openGL)
{
    // char *vertexCode = R"(
    //                                 in V3 pos;
    //                                 in V3 n;
    //                                 in V2 uv;
    //
    //                                 out V4 outPos;
    //                                 out V3 outN;
    //                                 out V2 outUv;
    //
    //                                 uniform mat4 transform;
    //
    //                                 uniform mat4
    //
    //                                 void main()
    //                                 {
    //                                     gl_Position = transform * V4(pos, 1.0);
    //                                     outPos = V4(pos, 1.0f);
    //                                     outN = n;
    //                                     outUv = uv;
    //                                 })";

    char *vertexCode = R"(
                                    in V3 pos;
                                    in V3 n;
                                    in V2 uv;
                                    in uvec4 boneIds;
                                    in vec4 boneWeights;

                                    out V4 outPos;
                                    out V3 outN;
                                    out V2 outUv;

                                    const int MAX_BONES = 200;

                                    uniform Mat4 transform; 
                                    uniform Mat4 boneTransforms[MAX_BONES];


                                    void main()
                                    { 
                                        Mat4 boneTransform = boneTransforms[boneIds[0]] * boneWeights[0];
                                        boneTransform += boneTransforms[boneIds[1]] * boneWeights[1];
                                        boneTransform += boneTransforms[boneIds[2]] * boneWeights[2];
                                        boneTransform += boneTransforms[boneIds[3]] * boneWeights[3];

                                        gl_Position = transform * boneTransform * V4(pos, 1.0);
                                        
                                        // TODO: these normals are wrong now
                                        outPos = V4(pos, 1.0f);
                                        outN = n;
                                        outUv = uv;
                                    })";

    char *fragmentCode = R"(
                                    in V4 outPos;
                                    in V3 outN;
                                    in V2 outUv;


                                    out V4 FragColor;

                                    uniform sampler2D diffuseTexture;

                                    void main()
                                    {
                                        FragColor = V4(texture(diffuseTexture, outUv).rgb, 1.f);
                                    })";

    ModelShader *modelShader = &openGL->modelShader;
    modelShader->base = OpenGLCreateProgram(openGL, globalHeaderCode, vertexCode, fragmentCode);
    modelShader->uvId = openGL->glGetAttribLocation(modelShader->base.id, "uv");
    modelShader->boneIdId = openGL->glGetAttribLocation(modelShader->base.id, "boneIds");
    modelShader->boneWeightId = openGL->glGetAttribLocation(modelShader->base.id, "boneWeights");
}

internal void OpenGLBindTexture(OpenGL *openGL, Texture *texture, i32 i)
{
    glGenTextures(1, &openGL->textureIds[i]);
    openGL->glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, openGL->textureIds[i]);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture->width, texture->width, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 texture->contents);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexEnvi(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture->loaded = true;
}

internal void OpenGLInit(OpenGL *openGL)
{
    u32 maxQuadCountPerFrame = 1 << 14;
    openGL->group.maxVertexCount = maxQuadCountPerFrame * 4;
    openGL->group.maxIndexCount = maxQuadCountPerFrame * 6;

    openGL->glGenVertexArrays(1, &openGL->vao);
    openGL->glBindVertexArray(openGL->vao);

    openGL->glGenBuffers(1, &openGL->vertexBufferId);
    openGL->glGenBuffers(1, &openGL->indexBufferId);
    openGL->glGenBuffers(1, &openGL->modelVertexBufferId);
    openGL->glGenBuffers(1, &openGL->modelIndexBufferId);
    // openGL->glGenBuffers(1, &openGL->skeletonBufferId);

    CompileCubeProgram(openGL);
    CompileModelProgram(openGL);
}

internal OpenGL Win32InitOpenGL(HWND window)
{
    OpenGL openGL_ = {};
    OpenGL *openGL = &openGL_;
    HDC dc = GetDC(window);

    PIXELFORMATDESCRIPTOR desiredPixelFormat = {};
    desiredPixelFormat.nSize = sizeof(desiredPixelFormat);
    desiredPixelFormat.nVersion = 1;
    desiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    desiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
    desiredPixelFormat.cColorBits = 32;
    desiredPixelFormat.cAlphaBits = 8;
    desiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

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
    }
    else
    {
        Unreachable;
    }
    ReleaseDC(window, dc);

    OpenGLInit(openGL);

    return openGL_;
};

internal void OpenGLBeginFrame(OpenGL *openGL, i32 width, i32 height)
{
    openGL->width = width;
    openGL->height = height;
    RenderGroup *group = &openGL->group;
    group->indexCount = 0;
    group->vertexCount = 0;
    group->quadCount = 0;
}

internal void OpenGLEndFrame(OpenGL *openGL, HDC deviceContext, int clientWidth, int clientHeight)
{
    // INITIALIZE
    {
        glViewport(0, 0, clientWidth, clientHeight);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glClearColor(0.5f, 0.5f, 0.5f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // RENDER CUBES
    {
        openGL->glUseProgram(openGL->cubeShader.base.id);
        openGL->glBindVertexArray(openGL->vao);
        openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->vertexBufferId);
        openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(RenderVertex) * openGL->group.vertexCount,
                             openGL->group.vertexArray, GL_STREAM_DRAW);

        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->indexBufferId);
        openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * openGL->group.indexCount,
                             openGL->group.indexArray, GL_STREAM_DRAW);

        GLuint positionId = openGL->cubeShader.base.positionId;
        GLuint colorId = openGL->cubeShader.colorId;
        GLuint normalId = openGL->cubeShader.base.normalId;
        openGL->glEnableVertexAttribArray(positionId);
        openGL->glVertexAttribPointer(positionId, 4, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
                                      (void *)Offset(RenderVertex, p));

        openGL->glEnableVertexAttribArray(colorId);
        openGL->glVertexAttribPointer(colorId, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
                                      (void *)Offset(RenderVertex, color));

        openGL->glEnableVertexAttribArray(normalId);
        openGL->glVertexAttribPointer(normalId, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
                                      (void *)Offset(RenderVertex, n));

        GLint transformLocation = openGL->glGetUniformLocation(openGL->cubeShader.base.id, "transform");
        openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, openGL->transform.elements[0]);

        GLint cameraPosition = openGL->glGetUniformLocation(openGL->cubeShader.base.id, "cameraPosition");
        openGL->glUniform3fv(cameraPosition, 1, openGL->camera.position.elements);

        glDrawElements(GL_TRIANGLES, 6 * openGL->group.quadCount, GL_UNSIGNED_SHORT, 0);

        openGL->glUseProgram(0);
        openGL->glDisableVertexAttribArray(positionId);
        openGL->glDisableVertexAttribArray(colorId);
        openGL->glDisableVertexAttribArray(normalId);
    }

    // RENDER MODEL
    {
        openGL->glUseProgram(openGL->modelShader.base.id);
        for (u32 i = 0; i < openGL->group.textureCount; i++) 
        {
            if (!openGL->group.textures[i].loaded)
            {
                OpenGLBindTexture(openGL, &openGL->group.textures[i], i);
            }
        }

        GLuint positionId = openGL->modelShader.base.positionId;
        GLuint normalId = openGL->modelShader.base.normalId;
        GLuint uvId = openGL->modelShader.uvId;
        GLuint boneIdId = openGL->modelShader.boneIdId;
        GLuint boneWeightId = openGL->modelShader.boneWeightId;
        // TODO: I don't think the buffer data should be bound every frame
        u32 baseBone = 0;
        for (u32 i = 0; i < openGL->group.model.meshCount; i++)
        {
            glBindTexture(GL_TEXTURE_2D, openGL->textureIds[i]);
            Model *model = &openGL->group.model;
            Mesh *currentMesh = model->meshes + i;
            Skeleton *skeleton = currentMesh->skeleton;

            openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->modelVertexBufferId);
            openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * model->meshes[i].vertexCount,
                                 currentMesh->vertices, GL_STREAM_DRAW);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->modelIndexBufferId);
            openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * model->meshes[i].indexCount,
                                 currentMesh->indices, GL_STREAM_DRAW);

            openGL->glEnableVertexAttribArray(positionId);
            openGL->glVertexAttribPointer(positionId, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, position));

            openGL->glEnableVertexAttribArray(normalId);
            openGL->glVertexAttribPointer(normalId, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, normal));

            openGL->glEnableVertexAttribArray(uvId);
            openGL->glVertexAttribPointer(uvId, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, uv));

            openGL->glEnableVertexAttribArray(boneIdId);
            openGL->glVertexAttribIPointer(boneIdId, 4, GL_UNSIGNED_INT, sizeof(MeshVertex),
                                           (void *)Offset(MeshVertex, boneIds));

            openGL->glEnableVertexAttribArray(boneWeightId);
            openGL->glVertexAttribPointer(boneWeightId, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                                          (void *)Offset(MeshVertex, boneWeights));

            Mat4 translate = Translate4(V3{0, 0, 5});
            Mat4 scale = Scale4(V3{0.5f, 0.5f, 0.5f});
            Mat4 rotate = Rotate4(V3{1, 0, 0}, PI / 2);
            Mat4 newTransform = openGL->transform * translate * rotate * scale;
            GLint transformLocation = openGL->glGetUniformLocation(openGL->modelShader.base.id, "transform");
            openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, newTransform.elements[0]);

            GLint boneTformLocation = openGL->glGetUniformLocation(openGL->modelShader.base.id, "boneTransforms");
            openGL->glUniformMatrix4fv(boneTformLocation, skeleton->boneCount, GL_FALSE,
                                       openGL->group.finalTransforms[baseBone].elements[0]);
            glDrawElements(GL_TRIANGLES, currentMesh->indexCount, GL_UNSIGNED_INT, 0);

            baseBone += skeleton->boneCount;
        }

        openGL->glUseProgram(0);
        // glBindTexture(GL_TEXTURE_2D, 0);
        openGL->glDisableVertexAttribArray(positionId);
        openGL->glDisableVertexAttribArray(normalId);
        openGL->glDisableVertexAttribArray(uvId);
    }

    // DOUBLE BUFFER SWAP
    SwapBuffers(deviceContext);
}
