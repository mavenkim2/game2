#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_memory.h"
#include "keepmovingforward_string.h"
// #include "keepmovingforward_platform.h"
#include "platform.h"

#include "keepmovingforward_string.cpp"
#include "keepmovingforward_memory.cpp"

#if WINDOWS
#include "win32.cpp"
#endif

// GAME GLOBALS

int main(int argCount, char** args) {
    OS_Init();
    return 0; 
}
