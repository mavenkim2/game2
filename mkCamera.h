#ifndef KEEPMOVINGFORWARD_CAMERA_H
#define KEEPMOVINGFORWARD_CAMERA_H

#include "mkMath.h"

struct Camera
{
    f32 pitch;
    f32 yaw;

    V3 position;

    V3 forward;
    V3 right;
};

#endif
