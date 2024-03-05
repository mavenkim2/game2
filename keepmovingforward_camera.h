#ifndef KEEPMOVINGFORWARD_CAMERA_H
#define KEEPMOVINGFORWARD_CAMERA_H

#ifdef INTERNAL
#include "keepmovingforward_math.h"
#endif

struct Camera
{
    f32 pitch;
    f32 yaw;

    f32 dolly;

    V3 position;

    V3 forward;
    V3 right;
};

#endif
