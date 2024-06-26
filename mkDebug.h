#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "render/mkGraphics.h"
#endif

using namespace graphics;
namespace debug
{

struct Record
{
    f64 totalTimes[20];
    u32 numInvocations[20];
    u64 count;
    char *functionName;
};

struct Range
{
    char *filename;
    char *function;
    PerformanceCounter counter;
    f64 timeElapsed;
    u32 lineNumber;

    // GPU
    CommandList commandList;
    i32 gpuBeginIndex;
    i32 gpuEndIndex;

    b32 IsGPURange() { return commandList.IsValid(); }
};

struct DebugSlotNode
{
    u32 sid;
    u32 recordIndex;
    DebugSlotNode *next;
};

struct DebugSlot
{
    Mutex mutex;
    DebugSlotNode *first;
    DebugSlotNode *last;
};

struct DebugState
{
    static const u32 totalNumSlots = 1024;

    Arena *arena;

    CommandList commandList;
    // Timing
    QueryPool timestampPool;
    Record records[totalNumSlots];
    Range ranges[graphics::mkGraphics::cNumBuffers][1024];
    u32 numRecords;
    DebugSlot *slots;
    std::atomic<u32> currentRangeIndex[graphics::mkGraphics::cNumBuffers];
    graphics::GPUBuffer queryResultBuffer[graphics::mkGraphics::cNumBuffers];
    std::atomic<u32> queryIndex;
    // AS_Handle debugFont;

    // Pipeline statistics
    QueryPool pipelineStatisticsPool;
    u64 pipelineStatistics[graphics::mkGraphics::cNumBuffers][2];

    b8 initialized = 0;

    void BeginFrame();
    void EndFrame(CommandList cmd);
    void BeginTriangleCount(CommandList cmdList);
    void EndTriangleCount(CommandList cmdList);
    u32 BeginRange(char *filename, char *functionName, u32 lineNum, CommandList cmdList = {});
    void EndRange(u32 rangeIndex);
    Record *GetRecord(char *name);
    void PrintDebugRecords();
};

struct Event
{
    u32 rangeIndex;
    Event(char *filename, char *functionName, u32 lineNum, CommandList cmdList = {});
    ~Event();
};

// internal void D_CollateDebugRecords();
#define TIMED_FUNCTION() debug::Event(__FILE__, FUNCTION_NAME, __LINE__);
// TODO: if gpu commands occur cross different function calls, consider
#define TIMED_GPU(cmd)                   debug::Event(__FILE__, FUNCTION_NAME, __LINE__, cmd);
#define TIMED_GPU_RANGE_BEGIN(cmd, name) debugState.BeginRange(__FILE__, name, __LINE__, cmd)
#define TIMED_RANGE_END(index)           debugState.EndRange(index)
#define TIMED_CPU_RANGE_BEGIN()          debugState.BeginRange(__FILE__, FUNCTION_NAME, __LINE__)

#define TIMED_CPU_RANGE_NAME_BEGIN(name) debugState.BeginRange(__FILE__, name, __LINE__)

} // namespace debug

debug::DebugState debugState;
