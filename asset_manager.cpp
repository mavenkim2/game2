internal u64 GetFileLastWriteTime(String8 filename);
internal void ReloadShader(String8 file) {
}

// every frame, call check reload assets, which calls a function
// asset manager which has all the currently loaded assets 
// if the asset file write time has changed, then ->
//      reload the asset
