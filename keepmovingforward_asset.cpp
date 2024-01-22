internal void DebugLoadModel(DebugPlatformReadFileFunctionType *PlatformReadFile, const char *filename) 
{
    DebugReadFileOutput output = PlatformReadFile(filename);
    Assert(output.fileSize != 0);
}
