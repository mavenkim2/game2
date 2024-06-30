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

struct ShaderResources
{
    u32 sid; // integer id representing name of shader
    list<ShaderResource> resources;
    list<u32> includes; // integer id of shaders you include
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
    u32 stringCount = 0;
    AtomicFixedHashTable<1024, 1024> hashTable;

    string directories[1024];
    u32 numDirectories            = 0;
    string cwd                    = StrConcat(temp.arena, OS_GetCurrentWorkingDirectory(), Str8Lit("\\src\\shaders"));
    directories[numDirectories++] = cwd;

    ShaderResources shaders[256];
    u32 numShaders = 0;

    StringBuilder stringBuilder;
    stringBuilder.arena = temp.arena;

    while (numDirectories != 0)
    {
        string directoryPath = directories[--numDirectories];
        OS_FileIter fileIter = OS_DirectoryIterStart(directoryPath, OS_FileIterFlag_SkipHiddenFiles);
        directoryPath        = StrConcat(temp.arena, directoryPath, Str8Lit("\\"));
        for (OS_FileProperties props = {}; OS_DirectoryIterNext(temp.arena, &fileIter, &props);)
        {
            if (!(props.isDirectory))
            {
                if (MatchString(GetFileExtension(props.name), Str8Lit("hlsl"), MatchFlag_RightSideSloppy))
                {
                    string path   = PushStr8F(temp.arena, "%S%S", directoryPath, props.name);
                    string result = OS_ReadEntireFile(temp.arena, path);

                    Tokenizer tokenizer;
                    tokenizer.input  = result;
                    tokenizer.cursor = result.str;

                    string shaderName = ConvertPathToStructName(temp.arena, PathSkipLastSlash(RemoveFileExtension(path)));

                    u32 sid           = Hash(shaderName);
                    b8 alreadyVisited = 0;
                    for (i32 index = hashTable.First(sid); hashTable.IsValid(index); hashTable.Next(index))
                    {
                        if (sids[index] == sid)
                        {
                            alreadyVisited = 1;
                            break;
                        }
                    }
                    if (alreadyVisited) continue;
                    for (u32 i = 0; i < stringCount; i++)
                    {
                        Assert(sids[i] != sid);
                    }
                    sids[stringCount] = sid;
                    hashTable.Add(sid, stringCount);
                    stringCount++;

                    ShaderResources *currentShader = &shaders[numShaders++];
                    while (!EndOfBuffer(&tokenizer))
                    {
                        string line = ReadLine(&tokenizer);
                        if (Contains(line, "include"))
                        {
                            string includedFile;
                            Assert(GetBetweenPair(includedFile, line, '"'));
                            u32 sid = Hash(ConvertPathToStructName(temp.arena, RemoveFileExtension(includedFile)));
                            currentShader->includes.push_back(sid);
                        }
                        else if (Contains(line, "register"))
                        {
                            currentShader->resources.emplace_back();
                            ShaderResource *resource = &currentShader->resources.back();
                            if (StartsWith(line, "RWStructuredBuffer"))
                            {
                                resource->type = rendergraph::ResourceType::RWStructuredBuffer;
                            }
                            else if (StartsWith(line, "StructuredBuffer"))
                            {
                                resource->type = rendergraph::ResourceType::StructuredBuffer;
                            }
                            else if (StartsWith(line, "Texture2D"))
                            {
                                resource->type = rendergraph::ResourceType::Texture2D;
                            }
                            else if (StartsWith(line, "RWTexture2D"))
                            {
                                resource->type = rendergraph::ResourceType::RWTexture2D;
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

                    StringBuilder builder;
                    builder.arena = temp.arena;

                    PutLine(&builder, 0, "namespace rendergraph");
                    PutLine(&builder, 0, "{");
                    PutLine(&builder, 0, "struct %S", shaderName);
                    PutLine(&builder, 0, "{");
                    PutLine(&builder, 1, "const PassResource resources[%u] = {", currentShader->resources.size());
                    for (u32 i = 0; i < currentShader->resources.size(); i++)
                    {
                        ShaderResource *resource = &currentShader->resources[i];
                        // TODO: not checking currently for hash collisions.
                        // TODO: since the names of resources could potentially be the same across shaders,
                        // combine hash of struct w/ hash of name
                        // TODO: obviously generate all of the structs for all of the shaders, multithreaded
                        // TODO: wtf is going on with the engine's performance? on some runs 10-15 seconds into the run 
                        // the performance just tanks??? and sometimes it doesn't. lmfao...
                        PutLine(&builder, 2, "{%u, %S, %u},", Hash(resource->name),
                                rendergraph::ConvertResourceTypeToName(resource->type), resource->bindingSlot);
                    }
                    PutLine(&builder, 1, "};");
                    for (u32 i = 0; i < currentShader->resources.size(); i++)
                    {
                        ShaderResource *resource = &currentShader->resources[i];
                        if (resource->type == rendergraph::ResourceType::Texture2D ||
                            resource->type == rendergraph::ResourceType::RWTexture2D)
                        {
                            PutLine(&builder, 1, "graphics::Texture %S;", resource->name);
                        }
                        else if (resource->type == rendergraph::ResourceType::StructuredBuffer ||
                                 resource->type == rendergraph::ResourceType::RWStructuredBuffer)

                        {
                            PutLine(&builder, 1, "graphics::GPUBuffer %S;", resource->name);
                        }
                    }
                    PutLine(&builder, 0, "};\n");
                    PutLine(&builder, 0, "}");

                    b32 writeResult = WriteEntireFile(&builder, PushStr8F(temp.arena, "src\\generated\\render_graph_resources.h"));
                    if (!writeResult)
                    {
                        Printf("Unable to render_graph_resources.h");
                        Assert(0);
                    }
                    break;
                }
            }
        }
    }
    return 0;
}
