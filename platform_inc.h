#ifndef PLATFORM_INC_H
#define PLATFORM_INC_H

#include "platform.h"
#if WINDOWS 
#include "win32.h"
#else 
#error OS not supported 
#endif

#endif
