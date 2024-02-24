#include "platform.cpp"
#if WINDOWS 
#include "win32.cpp"
#else 
#error OS not supported 
#endif
