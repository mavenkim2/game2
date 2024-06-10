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
    f32 timeElapsed;
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
    Record records[totalNumSlots];
    Range ranges[graphics::mkGraphics::cNumBuffers][256];
    u32 numRecords;
    DebugSlot *slots;
    std::atomic<u32> currentRangeIndex[graphics::mkGraphics::cNumBuffers];
    graphics::GPUBuffer queryResultBuffer[graphics::mkGraphics::cNumBuffers];
    std::atomic<u32> queryIndex;
    // AS_Handle debugFont;
    QueryPool timestampPool;
    QueryPool pipelineStatisticsPool;

    b8 initialized = 0;

    void BeginFrame();
    void EndFrame(CommandList cmd);
    u32 BeginRange(char *filename, char *functionName, u32 lineNum, CommandList cmdList);
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
#define TIMED_GPU(cmd) debug::Event(__FILE__, FUNCTION_NAME, __LINE__, cmd);
#define BEGIN_RANGE_CPU()

} // namespace debug

debug::DebugState debugState;
