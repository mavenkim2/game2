// TODO: is there a way that I can avoid including all of these files that have stuff I don't need in this
// DLL?
#include "mkCommon.h"
#include "mkMath.h"
#include "mkMemory.h"
#include "mkString.h"
#include "mkCamera.h"
#include "mkPlatformInc.h"
#include "mkThreadContext.h"
#include "render/mkRenderCore.h"
#include "mkList.h"
#include "render/mkGraphics.h"

#include "mkShared.h"

#if VULKAN
#include "render/mkGraphicsVulkan.h"
#endif
