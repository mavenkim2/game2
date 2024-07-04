#include "../mkCommon.h"
#include "../mkMath.h"
#include "../mkMemory.h"
#include "../mkString.h"
#include "../mkList.h"
#include "../mkPlatformInc.h"
#include "../mkTypes.h"
#include "../mkJobsystem.h"
#include "../render/mkGraphics.h"
#include "../mkThreadContext.h"
#include "../mkAsset.h"
#include "../mkScene.h"
#include "../mkShared.h"
#include "../render/mkRenderGraph.h"

#include "../mkPlatformInc.cpp"
#include "../mkThreadContext.cpp"
#include "../mkMemory.cpp"
#include "../mkString.cpp"
#include "../mkJobsystem.cpp"

struct ShaderResource
{
    string name;
    rendergraph::ResourceType type;
    rendergraph::ResourceUsage usage;
    i32 bindingSlot;
};

struct ShaderPushConstant
{
    string name;
};

struct ShaderResources
{
    string name;
    u32 shaderIndex;
    list<ShaderResource> resources;
    list<u32> includeShaderSid; // integer sid of shaders you include
    list<u32> includeShaderIndex;
};

internal string ConvertPathToStructName(Arena *arena, const string input)
{
    string output;
    output.str = PushArray(arena, u8, input.size);

    b8 passedUnderscore = 1;
    u32 size            = 0;
    for (u32 i = 0; i < input.size; i++)
    {
        u8 ch = input.str[i];
        if (passedUnderscore)
        {
            Assert(ch != '_');
            Assert(CharIsAlpha(ch));
            passedUnderscore = 0;
            ch               = CharToUpper(ch);
        }
        if (ch == '_')
        {
            passedUnderscore = 1;
        }
        else
        {
            output.str[size++] = ch;
        }
    }
    output.size = size;
    if (input.str[input.size - 3] == '_')
    {
        output.size -= 2;
    }
    output = PushStr8F(arena, "RG%S", output);
    return output;
}

internal inline string ConvertStructNameToMemberName(Arena *arena, const string input)
{
    string output = PushStr8Copy(arena, input);
    output        = Substr8(output, 2, input.size);
    output.str[0] = CharToLower(output.str[0]);
    return output;
}

void TopologicalSortUtil(u32 *data, u32 *offsets, u8 *visited, u32 *topologicalSort, u32 *stackCount, u32 index)
{
    if (visited[index]) return;

    visited[index] = 1;
    u32 *begin     = &data[offsets[index]];
    u32 *end       = &data[offsets[index + 1]];
    for (u32 *dependent = begin; dependent != end; dependent++)
    {
        TopologicalSortUtil(data, offsets, visited, topologicalSort, stackCount, *dependent);
    }
    u32 top              = *stackCount;
    topologicalSort[top] = index;
    *stackCount          = top + 1;
}

u32 *TopologicalSort(Arena *arena, u32 *data, u32 *offsets, u32 count)
{
    u32 *result    = PushArray(arena, u32, count);
    TempArena temp = ScratchStart(&arena, 1);
    u8 *visited    = PushArray(temp.arena, u8, count);
    u32 stackCount = 0;
    for (u32 i = 0; i < count; i++)
    {
        TopologicalSortUtil(data, offsets, visited, result, &stackCount, i);
    }
    ScratchEnd(temp);
    return result;
}

PlatformApi platform;
int main(int argc, char *argv[])
{
    platform = GetPlatform();

    ThreadContext tctx = {};
    ThreadContextInitialize(&tctx, 1);
    SetThreadName(Str8Lit("[Main Thread]"));

    OS_Init();
    jobsystem::InitializeJobsystem();

    TempArena temp = ScratchStart(0, 0);

    u32 sids[1024];
    AtomicFixedHashTable<1024, 1024> hashTable;

    string directories[1024];
    u32 numDirectories            = 0;
    string cwd                    = StrConcat(temp.arena, OS_GetCurrentWorkingDirectory(), Str8Lit("\\src\\shaders"));
    directories[numDirectories++] = cwd;

    ShaderResources shaders[256];
    std::atomic<u32> numShaders = 0;

    StringBuilder stringBuilders[256];

    jobsystem::Counter counter = {};

    while (numDirectories != 0)
    {
        string directoryPath = directories[--numDirectories];
        OS_FileIter fileIter = OS_DirectoryIterStart(directoryPath, OS_FileIterFlag_SkipHiddenFiles);
        directoryPath        = StrConcat(temp.arena, directoryPath, Str8Lit("\\"));
        for (OS_FileProperties props = {}; OS_DirectoryIterNext(temp.arena, &fileIter, &props);)
        {
            if (!(props.isDirectory))
            {
                string fileExtension = GetFileExtension(props.name);
                if (!(props.name == "globals.hlsli") && (fileExtension == "hlsl" || fileExtension == "hlsli"))
                {
                    string path = PushStr8F(temp.arena, "%S%S", directoryPath, props.name);
                    jobsystem::KickJob(&counter, [&numShaders, &hashTable,
                                                  &sids, &shaders, &stringBuilders, path](jobsystem::JobArgs jobArgs) {
                        TempArena temp = ScratchStart(0, 0);
                        string result  = OS_ReadEntireFile(temp.arena, path);

                        Tokenizer tokenizer;
                        tokenizer.input  = result;
                        tokenizer.cursor = result.str;

                        rendergraph::ShaderParamFlags flags = rendergraph::ShaderParamFlags::None;
                        string filename                     = PathSkipLastSlash(RemoveFileExtension(path));
                        string shaderType                   = Substr8(filename, filename.size - 2, filename.size);
                        {
                            if (GetFileExtension(path) == "hlsli")
                            {
                                flags = rendergraph::ShaderParamFlags::Header;
                            }
                            else if (shaderType == "cs")
                            {
                                flags = rendergraph::ShaderParamFlags::Compute;
                            }
                            else if (shaderType == "vs" || shaderType == "fs")
                            {
                                flags = rendergraph::ShaderParamFlags::Graphics;
                            }
                        }

                        string shaderName = ConvertPathToStructName(temp.arena, filename);

                        u32 sid           = Hash(shaderName);
                        b8 alreadyVisited = 0;
                        for (i32 index = hashTable.FirstConcurrent(sid); hashTable.IsValidConcurrent(sid, index); hashTable.Next(index))
                        {
                            if (sids[index] == sid)
                            {
                                alreadyVisited = 1;
                                break;
                            }
                        }
                        if (alreadyVisited) return;
                        for (u32 i = 0; i < numShaders.load(); i++)
                        {
                            Assert(sids[i] != sid);
                        }
                        u32 shaderIndex   = numShaders.fetch_add(1);
                        sids[shaderIndex] = sid;
                        hashTable.AddConcurrent(sid, shaderIndex);
                        ShaderResources *currentShader = &shaders[shaderIndex];
                        currentShader->name            = PushStr8Copy(temp.arena, shaderName);
                        currentShader->shaderIndex     = shaderIndex;

                        u32 numBuffers  = 0;
                        u32 numTextures = 0;
                        b8 hasPush      = 0;
                        ShaderPushConstant pc;
                        while (!EndOfBuffer(&tokenizer))
                        {
                            string line = ReadLine(&tokenizer);
                            if (Contains(line, "include") && line.str[0] != '/')
                            {
                                string includedFile;
                                Assert(GetBetweenPair(includedFile, line, '"'));
                                if (!(includedFile == "globals.hlsli"))
                                {
                                    u32 sid = Hash(ConvertPathToStructName(temp.arena, RemoveFileExtension(includedFile)));
                                    currentShader->includeShaderSid.push_back(sid);
                                }
                            }
                            else if (Contains(line, "push_constant"))
                            {
                                hasPush = 1;
                                pc.name = GetFirstWord(SkipToNextWord(line));
                            }
                            else if (Contains(line, "register") && line.str[0] != '/' && line.str[0] != '#')
                            {
                                currentShader->resources.emplace_back();
                                ShaderResource *resource = &currentShader->resources.back();
                                if (StartsWith(line, "RWStructuredBuffer"))
                                {
                                    resource->type = rendergraph::ResourceType::RWStructuredBuffer;
                                    numBuffers++;
                                }
                                else if (StartsWith(line, "StructuredBuffer"))
                                {
                                    resource->type = rendergraph::ResourceType::StructuredBuffer;
                                    numBuffers++;
                                }
                                else if (StartsWith(line, "Texture2DArray"))
                                {
                                    resource->type = rendergraph::ResourceType::Texture2DArray;
                                    numTextures++;
                                }
                                else if (StartsWith(line, "Texture2D"))
                                {
                                    resource->type = rendergraph::ResourceType::Texture2D;
                                    numTextures++;
                                }
                                else if (StartsWith(line, "RWTexture2D"))
                                {
                                    resource->type = rendergraph::ResourceType::RWTexture2D;
                                    numTextures++;
                                }
                                else if (StartsWith(line, "SamplerState"))
                                {
                                }
                                else if (StartsWith(line, "SamplerComparisonState"))
                                {
                                }
                                else
                                {
                                    Assert(!"Not defined resource type");
                                }

                                string reg;
                                Assert(GetBetweenPair(reg, line, '('));
                                if (reg.str[0] == 'u')
                                {
                                    resource->usage = rendergraph::ResourceUsage::UAV;
                                }
                                else if (reg.str[0] == 't')
                                {
                                    resource->usage = rendergraph::ResourceUsage::SRV;
                                }
                                else if (reg.str[0] == 'b')
                                {
                                    resource->usage = rendergraph::ResourceUsage::CBV;
                                }
                                else if (reg.str[0] == 's')
                                {
                                    resource->usage = rendergraph::ResourceUsage::SAM;
                                }
                                else
                                {
                                    Assert(0);
                                }

                                resource->bindingSlot = ConvertToUint(Substr8(reg, 1, reg.size));
                                resource->name        = GetFirstWord(SkipToNextWord(line));
                            }
                            else if (Contains(line, "main"))
                            {
                                break;
                            }
                        }

                        u32 builderIndex       = shaderIndex;
                        StringBuilder *builder = &stringBuilders[builderIndex];
                        builder->arena         = temp.arena;

                        PutLine(builder, 0, "struct %S", shaderName);
                        PutLine(builder, 0, "{");
                        PutLine(builder, 1, "const string name = Str8Lit(\"%S\");", currentShader->name);
                        PutLine(builder, 1, "const u32 numResources = %u;", currentShader->resources.size());
                        PutLine(builder, 1, "const u32 numBuffers = %u;", numBuffers);
                        PutLine(builder, 1, "const u32 numTextures = %u;", numTextures);
                        PutLine(builder, 1, "const ShaderParamFlags flags = %S;", ConvertShaderParamFlagsToString(flags));
                        if (numTextures != 0)
                        {
                            PutLine(builder, 1, "const PassResource textureResources[%u] = {", numTextures);
                            for (u32 i = 0; i < currentShader->resources.size(); i++)
                            {
                                ShaderResource *resource = &currentShader->resources[i];
                                if (IsTexture(resource->type))
                                {
                                    PutLine(builder, 2, "{%S, %S, %u},", rendergraph::ConvertResourceUsageToName(resource->usage),
                                            rendergraph::ConvertResourceTypeToName(resource->type), resource->bindingSlot);
                                }
                            }
                            PutLine(builder, 1, "};");
                        }
                        if (numBuffers != 0)
                        {
                            PutLine(builder, 1, "const PassResource bufferResources[%u] = {", numBuffers);
                            for (u32 i = 0; i < currentShader->resources.size(); i++)
                            {
                                ShaderResource *resource = &currentShader->resources[i];
                                if (IsBuffer(resource->type))
                                {
                                    PutLine(builder, 2, "{%S, %S, %u},", rendergraph::ConvertResourceUsageToName(resource->usage),
                                            rendergraph::ConvertResourceTypeToName(resource->type), resource->bindingSlot);
                                }
                            }
                            PutLine(builder, 1, "};");
                        }
                        for (u32 i = 0; i < currentShader->resources.size(); i++)
                        {
                            ShaderResource *resource = &currentShader->resources[i];
                            if (IsTexture(resource->type))
                            {
                                PutLine(builder, 1, "TextureView %S;", resource->name);
                            }
                        }
                        for (u32 i = 0; i < currentShader->resources.size(); i++)
                        {
                            ShaderResource *resource = &currentShader->resources[i];
                            if (IsBuffer(resource->type))

                            {
                                PutLine(builder, 1, "BufferView %S;", resource->name);
                            }
                        }
                        if (hasPush)
                        {
                            PutLine(builder, 1, "%S push;", pc.name);
                        }
                    });
                }
            }
        }
    }
    jobsystem::WaitJobs(&counter);

    // Join
    StringBuilder startBuilder = {};
    startBuilder.arena         = temp.arena;
    PutLine(&startBuilder, 0, "namespace rendergraph");
    PutLine(&startBuilder, 0, "{");
    u32 finalShaderCount = numShaders.load();

    u32 size              = 0;
    u32 *numChildren      = PushArray(temp.arena, u32, finalShaderCount + 1);
    u32 *numChildrenCount = &numChildren[1];

    // Create adjacency list while also writing struct dependencies
    for (u32 shaderIndex = 0; shaderIndex < finalShaderCount; shaderIndex++)
    {
        ShaderResources *shader = &shaders[shaderIndex];
        StringBuilder *builder  = &stringBuilders[shaderIndex];

        u32 numIncludes = shader->includeShaderSid.size();
        for (u32 includeIndex = 0; includeIndex < numIncludes; includeIndex++)
        {
            u32 includeSid = shader->includeShaderSid[includeIndex];
            Assert(includeSid != 0xffffffff);
            Assert(includeSid > finalShaderCount);
            u32 includeShaderIndex = 0xffffffff;
            for (u32 index = hashTable.First(includeSid); hashTable.IsValid(index); hashTable.Next(index))
            {
                if (sids[index] == includeSid)
                {
                    includeShaderIndex              = index;
                    ShaderResources *includedShader = &shaders[index];
                    PutLine(builder, 1, "%S %S;", includedShader->name, ConvertStructNameToMemberName(temp.arena, includedShader->name));
                    break;
                }
            }
            if (includeShaderIndex != 0xffffffff)
            {
                size++;
                numChildrenCount[shaderIndex]++;
                shader->includeShaderIndex.push_back(includeShaderIndex);
            }
        }
        PutLine(builder, 0, "};\n");
    }

    u32 *data         = PushArray(temp.arena, u32, size);
    u32 currentOffset = 0;
    for (u32 index = 0; index < finalShaderCount; index++)
    {
        u32 count               = numChildrenCount[index];
        numChildrenCount[index] = currentOffset;
        currentOffset += count;
    }

    for (u32 i = 0; i < finalShaderCount; i++)
    {
        ShaderResources *shader = &shaders[i];
        for (u32 j = 0; j < shader->includeShaderIndex.size(); j++)
        {
            data[numChildrenCount[i]] = shader->includeShaderIndex[j];
            numChildrenCount[i]++;
        }
    }

    u32 *topologicalSortResult = TopologicalSort(temp.arena, data, numChildren, finalShaderCount);

    OS_Handle handle = OS_OpenFile(OS_AccessFlag_Write | OS_AccessFlag_ShareWrite, "src\\generated\\render_graph_resources.h");
    string result    = CombineBuilderNodes(&startBuilder);
    Assert(OS_WriteFileIncremental(handle, result.str, result.size));
    for (u32 i = 0; i < finalShaderCount; i++)
    {
        StringBuilder *builder = &stringBuilders[topologicalSortResult[i]];
        result                 = CombineBuilderNodes(builder);
        Assert(OS_WriteFileIncremental(handle, result.str, result.size));
    }
    StringBuilder endBuilder;
    endBuilder.arena = temp.arena;
    PutLine(&endBuilder, 0, "}");
    result = CombineBuilderNodes(&endBuilder);
    Assert(OS_WriteFileIncremental(handle, result.str, result.size));
    OS_CloseFile(handle);
    return 0;
}
