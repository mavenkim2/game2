#include "../crack.h"
#ifdef LSP_INCLUDE
#include "../asset.h"
#include "../asset_cache.h"
#include "./render.h"
#include "./opengl.h"
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

// internal void ReloadShader(OpenGLShader *shader)
// {
//     i32 type = -1;
//     if (shader->id == openGL->cubeShader.base.id)
//     {
//         type = OGL_Shader_Basic;
//     }
//     else if (shader->id == openGL->modelShader.base.id)
//     {
//         type = OGL_Shader_Model;
//     }
//     else if (shader->id == openGL->instancedBasicShader.base.id)
//     {
//         type = OGL_Shader_Instanced;
//     }
//
//     if (shader->id)
//     {
//         openGL->glDeleteProgram(shader->id);
//     }
//     switch (type)
//     {
//         case OGL_Shader_Basic:
//             CompileCubeProgram();
//             break;
//         case OGL_Shader_Instanced:
//             CompileCubeProgram(true);
//             break;
//         case OGL_Shader_Model:
//             CompileModelProgram();
//             break;
//         default:
//             break;
//     }
// }

// internal void HotloadShaders(OpenGLShader *shader)
// {
//     if (OS_GetLastWriteTime(shader->globalsFile) != shader->globalsWriteTime ||
//         OS_GetLastWriteTime(shader->vsFile) != shader->vsWriteTime ||
//         OS_GetLastWriteTime(shader->fsFile) != shader->fsWriteTime)
//     {
//         ReloadShader(shader);
//     }
// }

global string r_opengl_g_globalsPath = Str8Lit("src/shaders/global.glsl");

global string r_opengl_g_vsPath[] = {Str8Lit("src/shaders/basic_3d.vs"), Str8Lit("src/shaders/basic_3d.vs"),
                                     Str8Lit(""), Str8Lit("src/shaders/model.vs")};
global string r_opengl_g_fsPath[] = {Str8Lit("src/shaders/basic_3d.fs"), Str8Lit("src/shaders/basic_3d.fs"),
                                     Str8Lit(""), Str8Lit("src/shaders/model.fs")};

internal void R_OpenGL_Init()
{
    openGL->arena = ArenaAlloc();

    // Initialize scratch buffers
    {
        openGL->glGenBuffers(1, &openGL->scratchVbo);
        openGL->glGenBuffers(1, &openGL->scratchEbo);
        openGL->glBindBuffer(GL_ARRAY_BUFFER, openGL->scratchVbo);
        openGL->scratchVboSize = kilobytes(64);
        openGL->glBufferData(GL_ARRAY_BUFFER, openGL->scratchVboSize, 0, GL_DYNAMIC_DRAW);

        openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEbo);
        openGL->scratchEboSize = kilobytes(64);
        openGL->glBufferData(GL_ELEMENT_ARRAY_BUFFER, openGL->scratchEboSize, 0, GL_DYNAMIC_DRAW);

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
                case R_ShaderType_Instanced3D: preprocess = Str8Lit("#define INSTANCED 1\n"); break;
                case R_ShaderType_StaticMesh: continue;
                default: break;
            }
            openGL->shaders[type].id = R_OpenGL_CreateShader(r_opengl_g_globalsPath, r_opengl_g_vsPath[type],
                                                             r_opengl_g_fsPath[type], preprocess);
            openGL->shaders[type].globalsLastModified = OS_GetLastWriteTime(r_opengl_g_globalsPath);
            openGL->shaders[type].vsLastModfiied      = OS_GetLastWriteTime(r_opengl_g_vsPath[type]);
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
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &openGL->maxSlices);
        openGL->textureMap.maxSlots  = 16;
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
        }
    }
};

internal void R_Init(HWND window)
{
#if WINDOWS
    R_Win32_OpenGL_Init(window);
#else
#error OS not implemented
#endif
}

internal void R_Win32_OpenGL_Init(HWND window)
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
        Win32GetOpenGLFunction(glMultiDrawElements);
        Win32GetOpenGLFunction(glUniform1iv);
        Win32GetOpenGLFunction(glTexStorage3D);
        Win32GetOpenGLFunction(glTexSubImage3D);

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

internal void R_BeginFrame(i32 width, i32 height)
{
    // openGL->width      = width;
    // openGL->height     = height;
    // RenderGroup *group = &openGL->group;
    // group->indexCount  = 0;
    // group->vertexCount = 0;
    // group->quadCount   = 0;
}

internal void R_EndFrame(RenderState *state, HDC deviceContext, int clientWidth, int clientHeight)
{
    R_OpenGL_LoadBuffers();
    R_OpenGL_LoadTextures();
    TempArena temp = ScratchStart(0, 0);
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

    // Bind texture array
    u32 counter = 0;
    for (u32 i = 0; i < openGL->textureMap.maxSlots; i++)
    {
        R_Texture2DArray *list = &openGL->textureMap.arrayList[i];
        openGL->glActiveTexture(GL_TEXTURE0 + counter++);
        glBindTexture(GL_TEXTURE_2D_ARRAY, list->id);
    }

    // RENDER MODEL

    for (R_PassType type = (R_PassType)0; type < R_PassType_Count; type = (R_PassType)(type + 1))
    {
        R_Pass *pass = &state->passes[type];
        switch (type)
        {
            case R_PassType_UI:
            {
                // R_BatchList *list = pass->passUI->batchList;
                // for (R_BatchNode *node = list->first; node != 0; node = node->next)
                // {
                //
                // }
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
                    R_SkinnedMeshParams *params   = &node->val;
                    LoadedModel *model            = GetModel(params->model);
                    R_OpenGL_Buffer *vertexBuffer = R_OpenGL_BufferFromHandle(model->vertexBuffer);
                    R_OpenGL_Buffer *indexBuffer  = R_OpenGL_BufferFromHandle(model->indexBuffer);

                    openGL->glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->id);
                    openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->id);
                    R_OpenGL_StartShader(state, R_ShaderType_SkinnedMesh, 0);

                    // TODO: uniform blocks so we can just push a struct or something instead of having to do
                    // these all manually

                    // MVP matrix
                    Mat4 newTransform       = state->transform * params->transform;
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
                    openGL->glUniform3fv(viewPosLoc, 1, state->camera.position.elements);

                    // TEXTURE MAP
                    static readonly GLint samplerIds[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
                    GLint textureLoc = openGL->glGetUniformLocation(modelProgramId, "textureMaps");
                    openGL->glUniform1iv(textureLoc, ArrayLength(samplerIds), samplerIds);

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

                    for (u32 i = 0; i < model->materialCount; i++)
                    {
                        Material *material = model->materials + i;
                        counts[i]          = material->onePlusEndIndex - material->startIndex;
                        u64 startIndex     = sizeof(material->startIndex) * material->startIndex;
                        startIndices[i]    = (void *)(startIndex);
                        for (u32 j = 0; j < TextureType_Count; j++)
                        {
                            R_Handle textureHandle = GetTextureRenderHandle(material->textureHandles[j]);
                            if (R_HandleMatch(textureHandle, R_HandleZero()))
                            {
                                textureHandle = openGL->whiteTextureHandle;
                            }
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
                        baseTexture += TEXTURES_PER_MATERIAL;

                        // glDrawElements(GL_TRIANGLES, material->onePlusEndIndex - material->startIndex,
                        //                GL_UNSIGNED_INT, (void *)(sizeof(u32) * material->startIndex));
                    }
                    openGL->glMultiDrawElements(GL_TRIANGLES, counts, GL_UNSIGNED_INT, startIndices,
                                                model->materialCount);
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
                            R_OpenGL_StartShader(state, R_ShaderType_3D, 0);
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
                            R_OpenGL_StartShader(state, R_ShaderType_3D, 0);
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

                            R_OpenGL_StartShader(state, R_ShaderType_Instanced3D, group);
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

    // HOT RELOAD! this probably shouldnt' be here
    // HotloadShaders(&openGL->modelShader.base);
    // HotloadShaders(&openGL->cubeShader.base);
    // HotloadShaders(&openGL->instancedBasicShader.base);

    // DOUBLE BUFFER SWAP
    SwapBuffers(deviceContext);
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
    switch (type)
    {
        case R_ShaderType_3D:
        {
            openGL->glUseProgram(0);
            openGL->glDisableVertexAttribArray(0);
            openGL->glDisableVertexAttribArray(1);
            openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
            break;
        }
        case R_ShaderType_Instanced3D:
        {
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
            openGL->glBindBuffer(GL_ARRAY_BUFFER, 0);
            openGL->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            break;
        }
        case R_ShaderType_SkinnedMesh:
        {
            openGL->glUseProgram(0);
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

// TODO: sparse bindless texture arrays?
R_ALLOCATE_TEXTURE_2D(R_AllocateTexture2D)
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

R_ALLOCATE_TEXTURE_2D(R_AllocateTextureInArray)
{
    R_Texture2DArrayTopology topology;
    topology.levels         = 0;
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
        list->freeList      = PushArray(openGL->arena, GLsizei, openGL->maxSlices);
        list->freeListCount = openGL->maxSlices;
        list->depth         = openGL->maxSlices;
        for (GLsizei i = 0; i < openGL->maxSlices; i++)
        {
            list->freeList[i] = i;
        }
        list->hash = hash;
    }
    Assert(list->freeListCount != 0);
    GLsizei freeIndex = list->freeList[--list->freeListCount];
    EndTicketMutex(&openGL->textureMap.mutex);

    R_Handle result;
    result.u32[0] = hashIndex;
    result.u32[1] = (u32)freeIndex;

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
            op->texSlice.hashIndex = result.u32[0];
            op->texSlice.slice     = result.u32[1];
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
            if (array->id == 0)
            {
                glGenTextures(1, &array->id);
                glBindTexture(GL_TEXTURE_2D_ARRAY, array->id);
                u32 glFormat;
                switch (array->topology.internalFormat)
                {
                    case R_TexFormat_SRGB: glFormat = openGL->defaultTextureFormat; break;
                    case R_TexFormat_RGBA8:
                    default: glFormat = GL_RGBA8; break;
                }
                openGL->glTexStorage3D(GL_TEXTURE_2D_ARRAY, array->topology.levels, glFormat,
                                       array->topology.width, array->topology.height, array->depth);
            }
        }

        u64 availableSlots = ArrayLength(openGL->pbos) - (openGL->pboIndex - openGL->firstUsedPboIndex);
        if (availableSlots >= 1)
        {
            loadPos++;
            op->pboIndex = openGL->pboIndex++;
            GLuint pbo   = GetPbo(op->pboIndex);
            openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            i32 size = op->texture->width * op->texture->height * 4;
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

                u32 glFormat;
                switch (texture->format)
                {
                    case R_TexFormat_SRGB: glFormat = openGL->defaultTextureFormat; break;
                    case R_TexFormat_RGBA8:
                    default: glFormat = GL_RGBA8; break;
                }

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
                u32 arrayIndex          = op->texSlice.slice;
                R_Texture2DArray *array = &openGL->textureMap.arrayList[op->texSlice.hashIndex];

                glBindTexture(GL_TEXTURE_2D_ARRAY, array->id);
                openGL->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GetPbo(op->pboIndex));
                openGL->glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, arrayIndex, array->topology.width,
                                        array->topology.height, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glBindTexture(GL_TEXTURE_2D, 0);
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

// this has to happen on a single threaded context

//////////////////////////////
// Buffer loading
//
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
        openGL->glBufferData(format, op->buffer->size, op->data, GL_DYNAMIC_DRAW);
        openGL->glBindBuffer(format, 0);
    }
    queue->readPos = readPos;
}

//////////////////////////////
// HANDLES
//
internal b8 R_HandleMatch(R_Handle a, R_Handle b)
{
    b8 result = (a.u64[0] == b.u64[0] && a.u64[1] == b.u64[1]);
    return result;
}
internal R_Handle R_HandleZero()
{
    R_Handle handle = {};
    return handle;
}

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
