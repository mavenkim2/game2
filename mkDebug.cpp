#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "mkDebug.h"
#include "render/mkGraphics.h"
#endif

namespace debug
{

void DebugState::BeginFrame()
{
    if (!initialized)
    {
        initialized = 1;

        arena = ArenaAlloc();
        slots = PushArray(arena, DebugSlot, totalNumSlots);

        // dbg_state.debugFont = AS_GetAsset((Str8Lit("data/liberation_mono.ttf")));
        device->CreateQueryPool(&timestampPool, QueryType_Timestamp, 1024);
        device->CreateQueryPool(&pipelineStatisticsPool, QueryType_PipelineStatistics, 4);

        GPUBufferDesc desc = {};
        desc.mSize         = (u32)(sizeof(u64) * (timestampPool.queryCount + pipelineStatisticsPool.queryCount));
        desc.mUsage        = MemoryUsage::GPU_TO_CPU;

        for (u32 i = 0; i < ArrayLength(queryResultBuffer); i++)
        {
            device->CreateBuffer(&queryResultBuffer[i], desc, 0);
        }
    }

    commandList       = device->BeginCommandList(QueueType_Graphics);
    u32 currentBuffer = device->GetCurrentBuffer();
    u32 numRanges     = currentRangeIndex[currentBuffer].load();

    for (u32 recordIndex = 0; recordIndex < numRecords; recordIndex++)
    {
        Record *record                = &records[recordIndex];
        u32 index                     = record->count++ % (ArrayLength(record->totalTimes));
        record->numInvocations[index] = 0;
        record->totalTimes[index]     = 0;
    }

    u64 *mappedData = (u64 *)queryResultBuffer[currentBuffer].mMappedData;
    for (u32 rangeIndex = 0; rangeIndex < numRanges; rangeIndex++)
    {
        Range *range   = &ranges[currentBuffer][rangeIndex];
        Record *record = debugState.GetRecord(range->function);
        if (range->IsGPURange())
        {
            f64 timestampPeriod = device->GetTimestampPeriod() * 1000;
            range->timeElapsed  = (f64)(mappedData[range->gpuEndIndex] - mappedData[range->gpuBeginIndex]) * timestampPeriod;
        }
        u32 index = (record->count - 1) % (ArrayLength(record->totalTimes));
        record->numInvocations[index]++;
        record->totalTimes[index] += range->timeElapsed;
    }

    u32 lastQuery                        = queryIndex.load();
    pipelineStatistics[currentBuffer][0] = mappedData[lastQuery + 0];
    pipelineStatistics[currentBuffer][1] = mappedData[lastQuery + 1];

    device->ResetQuery(&timestampPool, commandList, 0, timestampPool.queryCount);
    device->ResetQuery(&pipelineStatisticsPool, commandList, 0, pipelineStatisticsPool.queryCount);
    currentRangeIndex[currentBuffer].store(0);
    queryIndex.store(0);
}

void DebugState::EndFrame(CommandList cmd)
{
    u32 currentBuffer = device->GetCurrentBuffer();

    u32 numTimestampQueries = queryIndex.load();
    device->ResolveQuery(&timestampPool, cmd, &queryResultBuffer[currentBuffer], 0, numTimestampQueries, 0);
    device->ResolveQuery(&pipelineStatisticsPool, cmd, &queryResultBuffer[currentBuffer], 0, 1, sizeof(u64) * numTimestampQueries);
}

Record *DebugState::GetRecord(char *name)
{
    string str      = Str8C(name);
    u32 sid         = GetSID(str);
    DebugSlot *slot = &slots[sid % (totalNumSlots - 1)];
    Record *record  = 0;
    for (DebugSlotNode *node = slot->first; node != 0; node = node->next)
    {
        if (node->sid == sid)
        {
            record = &records[node->recordIndex];
            break;
        }
    }
    if (!record)
    {
        AddSID(str);
        u32 recordIndex     = numRecords++;
        DebugSlotNode *node = PushStruct(arena, DebugSlotNode);
        node->recordIndex   = recordIndex;
        node->sid           = sid;
        QueuePush(slot->first, slot->last, node);

        record               = &records[recordIndex];
        record->functionName = name;
        Assert(record->count == 0);
    }
    return record;
}

void DebugState::BeginTriangleCount(CommandList cmdList)
{
    device->BeginQuery(&pipelineStatisticsPool, cmdList, 0);
}
void DebugState::EndTriangleCount(CommandList cmdList)
{
    device->EndQuery(&pipelineStatisticsPool, cmdList, 0);
}

u32 DebugState::BeginRange(char *filename, char *functionName, u32 lineNum, CommandList cmdList)
{
    u32 currentBuffer = device->GetCurrentBuffer();
    u32 rangeIndex    = debugState.currentRangeIndex[currentBuffer].fetch_add(1);
    Assert(rangeIndex < ArrayLength(debugState.ranges[0]));

    Range *range       = &debugState.ranges[currentBuffer][rangeIndex];
    range->filename    = filename;
    range->function    = functionName;
    range->lineNumber  = lineNum;
    range->commandList = cmdList;

    if (range->IsGPURange())
    {
        range->gpuBeginIndex = debugState.queryIndex.fetch_add(1);
        device->EndQuery(&debugState.timestampPool, range->commandList, range->gpuBeginIndex);
    }
    else
    {
        range->counter = platform.StartCounter();
    }
    return rangeIndex;
}

void DebugState::EndRange(u32 rangeIndex)
{
    u32 currentBuffer = device->GetCurrentBuffer();
    Range *range      = &debugState.ranges[currentBuffer][rangeIndex];
    if (range->IsGPURange())
    {
        range->gpuEndIndex = debugState.queryIndex.fetch_add(1);
        device->EndQuery(&debugState.timestampPool, range->commandList, range->gpuEndIndex);
    }
    else
    {
        range->timeElapsed = platform.GetMilliseconds(range->counter);
    }
}

void DebugState::PrintDebugRecords()
{
    for (u32 recordIndex = 0; recordIndex < numRecords; recordIndex++)
    {
        Record *record = &records[recordIndex];
        u32 size       = ArrayLength(record->totalTimes);
        if (record->count > size)
        {
            f64 avg            = 0;
            f32 avgInvocations = 0;
            for (u32 timeIndex = 0; timeIndex < size; timeIndex++)
            {
                avg += record->totalTimes[timeIndex];
                avgInvocations += record->numInvocations[timeIndex];
            }
            avg /= size;
            avgInvocations /= size;
            Printf("%s | Avg Total Time: %f ms | Avg Invocations: %f\n", record->functionName, avg, avgInvocations);
        }
    }

    u64 triangleCount      = 0;
    u64 clippingPrimitives = 0;
    u32 length             = graphics::mkGraphics::cNumBuffers;
    for (u32 i = 0; i < length; i++)
    {
        triangleCount += pipelineStatistics[i][0];
        clippingPrimitives += pipelineStatistics[i][1];
    }
    Printf("Triangle counts: %u\n", triangleCount / length);
    Printf("Clipping primitives: %u\n\n", clippingPrimitives / length);
}

Event::Event(char *filename, char *functionName, u32 lineNum, CommandList cmdList)
{
    rangeIndex = debugState.BeginRange(filename, functionName, lineNum, cmdList);
}

Event::~Event()
{
    debugState.EndRange(rangeIndex);
}

} // namespace debug
