// TODO: get by tag / other means instead of filenames
internal void DBG_Init()
{
    dbg_state.debugFont = AS_GetAsset((Str8Lit("data/liberation_mono.ttf")));
}

DBG_Event::DBG_Event(char *filename, char *functionName, u32 lineNum)
{
    u32 recordIndex = AtomicIncrementU32(&dbg_state.currentRecordIndex) - 1;
    Assert(recordIndex < ArrayLength(dbg_state.records));
    record             = dbg_state.records + recordIndex;
    record->filename   = filename;
    record->function   = functionName;
    record->lineNumber = lineNum;

    startTime = platform.NowSeconds();
}

DBG_Event::~DBG_Event()
{
    f32 endTime         = platform.NowSeconds();
    record->timeElapsed = endTime - startTime;
}

internal void D_CollateDebugRecords()
{
    u32 length = AtomicExchange(&dbg_state.currentRecordIndex, 0);
    V2 pos     = {0, 10};
    for (u32 i = 0; i < length; i++)
    {
        DBG_Record *record = dbg_state.records + i;
        D_PushTextF(dbg_state.debugFont, pos, 40, (char *)"%s: %f", record->function,
                    record->timeElapsed * 1000.f);
        pos.y += 40;
    }
}
