#include "win32_keepmovingforward.h"
#include "win32_keepmovingforward_opengl.h"

global GLuint TEXTURE_HANDLE;
global OpenGL OPENGL;

internal void Win32InitOpenGl(HWND window)
{
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
        OpenGL *openGL = &OPENGL;
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
        Setup();
    }
    else
    {
        Unreachable;
    }
    ReleaseDC(window, dc);
};

internal void Setup()
{
    OpenGL *openGL = &OPENGL;

    float vertices[] = {
        -1.f, -1.f, 0.f,    0.f, 0.f, 
        1.f, -1.f, 0.f,     1.f, 0.f,  
        1.f, 1.f, 0.f,      1.f, 1.f, 
        -1.f, 1.f, 0.f,     0.f, 1.f
    };

    u32 indices[] = {
        0, 1, 2, 
        0, 2, 3, 
    };
    // SHADERS
    const char *vertexShaderCode = "#version 330 core\n"
                                   "layout (location=0) in vec3 pos;\n"
                                   "layout (location=1) in vec2 aTexCoord;\n"
                                   "out vec2 texCoord;\n"
                                   "void main()\n"
                                   "{\n"
                                   "   gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);\n"
                                   "   texCoord = aTexCoord;\n"
                                   "}\0";

    const char *fragmentShaderCode = "#version 330 core\n"
                                     "out vec4 FragColor;\n"
                                     // "uniform vec4 color;\n"
                                     "in vec2 texCoord;\n"
                                     "uniform sampler2D ourTexture;\n"
                                     "void main()\n"
                                     "{\n"
                                     "  FragColor = texture(ourTexture, texCoord);\n"
                                     "}\0";
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

    GLuint vbo;
    GLuint vao;
    GLuint ebo; 
    // INIT
    openGL->glGenVertexArrays(1, &vao);
    openGL->glBindVertexArray(vao);

    openGL->glGenBuffers(1, &vbo);

    // Bind vertex buffers
    openGL->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    openGL->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    openGL->glGenBuffers(1, &ebo); 
    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);

    // Set vertex attributes
    openGL->vao = vao;
}

// TODO: loading the texture every frame is probably bad 
internal void Draw(Win32OffscreenBuffer *buffer, HDC deviceContext, int clientWidth, int clientHeight, u64 clock)
{
    OpenGL *openGL = &OPENGL;
    glViewport(0, 0, clientWidth, clientHeight);
    
    glGenTextures(1, &TEXTURE_HANDLE);
    openGL->glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TEXTURE_HANDLE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, buffer->width, buffer->height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
            buffer->memory);
    glTexEnvi(GL_TEXTURE_2D, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    openGL->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void *)0);
    openGL->glEnableVertexAttribArray(0);

    openGL->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void *)(3 * sizeof(float)));
    openGL->glEnableVertexAttribArray(1);

    // glEnable(GL_TEXTURE_2D);

    glClearColor(0.f, 1.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    openGL->glUseProgram(openGL->shaderProgram);
    openGL->glBindVertexArray(openGL->vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    // glDrawArrays(GL_TRIANGLES, 0, 3);

    SwapBuffers(deviceContext);
}
