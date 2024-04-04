#define TIMED_FUNCTION() DebugRecord(__FILE__, FUNCTION_NAME, __LINE__)

struct DBG_Record
{
    char *filename;
    char *function;
    u32 lineNumber;
    f32 timeElapsed;
};

struct DBG_State
{
    DBG_Record records[32];
    u32 currentEventIndex;
};

global DBG_State dbg_state;

struct DBG_Event
{
    DBG_Record *record;
    f32 startTime;

    DBG_Event(char *filename, char *functionName, u32 lineNum)
    {
        u32 recordIndex    = AtomicAddU32(&dbg_state.currentEventIndex, 1) & (ArrayLength(dbg_state.records) - 1);
        record             = dbg_state.records + recordIndex;
        record->filename   = filename;
        record->function   = functionName;
        record->lineNumber = lineNum;

        startTime = OS_GetWallClock();
    }
    ~DBG_Event()
    {
        record->timeElapsed = OS_GetWallClock() - startTime;
    }
};
