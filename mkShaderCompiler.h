#ifndef MK_SHADER_COMPILER_H
#define MK_SHADER_COMPILER_H

#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkShared.h"
#include "mkPlatformInc.h"
#include "mkPlatform.h"
#endif

#include "third_party/dxcapi.h"

#undef Swap

#ifdef WINDOWS
#include <wrl/client.h>
#define CComPtr Microsoft::WRL::ComPtr
#endif

namespace shadercompiler
{

global string shaderDirectory = "src/shaders/\0";
const char *functionTable[]   = {"DxcCreateInstance"};

struct ShaderCompiler
{
    CComPtr<IDxcCompiler3> dxcCompiler;
    CComPtr<IDxcVersionInfo> info;
    CComPtr<IDxcUtils> dxcUtils;
    CComPtr<IDxcIncludeHandler> defaultIncludeHandler;

    OS_DLL dll;

    struct FunctionTable
    {
        DxcCreateInstanceProc DxcCreateInstance;
    } functions;

    void Destroy()
    {
        dxcCompiler->Release();
        info->Release();
        dxcUtils->Release();
        defaultIncludeHandler->Release();
    }

    ~ShaderCompiler() {}
};

struct CompileInput
{
    graphics::ShaderStage stage;
    string shaderName;
};

struct CompileOutput
{
    string shaderData;
    list<string> dependencies;
};

internal void InitShaderCompiler();
internal wchar_t *ConvertToWide(Arena *arena, string input);
internal string ConvertFromWide(Arena *arena, const wchar_t *str);
internal void CompileShader(Arena *arena, CompileInput *input, CompileOutput *output);

#define Swap(type, a, b)            \
    do                              \
    {                               \
        type _swapper_ = a;         \
        a              = b;         \
        b              = _swapper_; \
    } while (0)
} // namespace shadercompiler

#endif
