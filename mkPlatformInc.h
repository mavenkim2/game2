#ifndef PLATFORM_INC_H
#define PLATFORM_INC_H

#include "mkPlatform.h"
#if WINDOWS 
#include "mkWin32.h"
#else 
#error OS not supported 
#endif

#endif
