internal void OpenGLInit(OpenGL *openGL)
{
    u32 maxQuadCountPerFrame = 1 << 14;
    openGL->group.maxVertexCount = maxQuadCountPerFrame * 4;
    openGL->group.maxIndexCount = maxQuadCountPerFrame * 6;

    // TODO: probably shouldn't be hardcoded
    //  openGL->camera.forward = {0, 0, -1};
    //  openGL->camera.right = {1, 0, 0};
    openGL->camera.position = {0, 0, 5};
    openGL->camera.yaw = -PI / 2;

    const char *vertexShaderCode = R"(
                                    #version 330 core
                                    #define f32 float
                                    #define V3 vec3
                                    #define V4 vec4

                                    layout (location=0) in vec4 pos;
                                    layout (location=1) in vec3 colorIn;
                                    layout (location=2) in vec3 n;

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

    const char *fragmentShaderCode = R"(
                                    #version 330 core
                                    #define f32 float
                                    #define V3 vec3
                                    #define V4 vec4

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
    GLuint vertexShader = 0;
    vertexShader = openGL->glCreateShader(GL_VERTEX_SHADER);
    openGL->glShaderSource(vertexShader, 1, &vertexShaderCode, 0);
    openGL->glCompileShader(vertexShader);

    GLuint fragmentShader = 0;
    fragmentShader = openGL->glCreateShader(GL_FRAGMENT_SHADER);
    openGL->glShaderSource(fragmentShader, 1, &fragmentShaderCode, 0);
    openGL->glCompileShader(fragmentShader);

    GLuint shaderProgram = openGL->glCreateProgram();
    openGL->glAttachShader(shaderProgram, vertexShader);
    openGL->glAttachShader(shaderProgram, fragmentShader);
    openGL->glLinkProgram(shaderProgram);

    GLint success = false;
    openGL->glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        char vertexShaderErrors[4096];
        char fragmentShaderErrors[4096];
        char programErrors[4096];
        openGL->glGetShaderInfoLog(vertexShader, 4096, 0, vertexShaderErrors);
        openGL->glGetShaderInfoLog(fragmentShader, 4096, 0, fragmentShaderErrors);
        openGL->glGetProgramInfoLog(shaderProgram, 4096, 0, programErrors);

        Assert(!"Shader failed");
    }

    openGL->glDeleteShader(vertexShader);
    openGL->glDeleteShader(fragmentShader);
    openGL->shaderProgram = shaderProgram;

    openGL->glGenVertexArrays(1, &openGL->vao);
    openGL->glBindVertexArray(openGL->vao);

    openGL->glGenBuffers(1, &openGL->vertexBufferId);
    openGL->glGenBuffers(1, &openGL->indexBufferId);
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
        Win32GetOpenGLFunction(glGenVertexArrays);
        Win32GetOpenGLFunction(glBindVertexArray);
        Win32GetOpenGLFunction(glGetUniformLocation);
        Win32GetOpenGLFunction(glUniform4f);
        Win32GetOpenGLFunction(glUniform1f);
        Win32GetOpenGLFunction(glActiveTexture);
        Win32GetOpenGLFunction(glUniformMatrix4fv);
        Win32GetOpenGLFunction(glUniform3fv);
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
    glViewport(0, 0, clientWidth, clientHeight);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.5f, 0.5f, 0.5f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glGenTextures(1, &TEXTURE_HANDLE);
    // openGL->glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, TEXTURE_HANDLE);
    //
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, buffer->width, buffer->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
    //              buffer->memory);
    // glTexEnvi(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    //
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    openGL->glBindVertexArray(openGL->vao);
    openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->vertexBufferId);
    openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(RenderVertex) * openGL->group.vertexCount, openGL->group.vertexArray,
                         GL_STREAM_DRAW);

    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->indexBufferId);
    openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * openGL->group.indexCount, openGL->group.indexArray,
                         GL_STREAM_DRAW);

    openGL->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void *)Offset(RenderVertex, p));
    openGL->glEnableVertexAttribArray(0);

    openGL->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void *)Offset(RenderVertex, color));
    openGL->glEnableVertexAttribArray(1);

    openGL->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void *)Offset(RenderVertex, n));
    openGL->glEnableVertexAttribArray(2);

    GLint transformLocation = openGL->glGetUniformLocation(openGL->shaderProgram, "transform");
    openGL->glUniformMatrix4fv(transformLocation, 1, GL_FALSE, openGL->transform.elements[0]);

    GLint cameraPosition = openGL->glGetUniformLocation(openGL->shaderProgram, "cameraPosition");
    openGL->glUniform3fv(cameraPosition, 1, openGL->camera.position.e);

    openGL->glUseProgram(openGL->shaderProgram);
    glDrawElements(GL_TRIANGLES, 6 * openGL->group.quadCount, GL_UNSIGNED_SHORT, 0);

    SwapBuffers(deviceContext);
}
