struct DBG_Record
{
    char *filename;
    char *function;
    f32 timeElapsed;
    u32 lineNumber;
};

struct DBG_State
{
    DBG_Record records[32];
    u32 currentRecordIndex;
    AS_Handle debugFont;
};

global DBG_State dbg_state;

struct DBG_Event
{
    DBG_Record *record;
    f32 startTime;

    DBG_Event(char *filename, char *functionName, u32 lineNum);
    ~DBG_Event();
};

internal void DBG_Init();
internal void D_CollateDebugRecords();
#define TIMED_FUNCTION() DBG_Event(__FILE__, FUNCTION_NAME, __LINE__);
