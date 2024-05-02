#include "mkCrack.h"
#ifdef LSP_INCLUDE
#include "keepmovingforward_common.h"
#include "keepmovingforward_math.h"
#include "keepmovingforward_camera.h"
#endif

internal void RotateCamera(Camera *camera, V2 dMouse, f32 rotationSpeed)
{
    camera->pitch -= rotationSpeed * dMouse.y;
    f32 epsilon = 0.01f;
    if (camera->pitch > PI / 2 - epsilon)
    {
        camera->pitch = PI / 2 - epsilon;
    }
    else if (camera->pitch < -PI / 2 + epsilon)
    {
        camera->pitch = -PI / 2 + epsilon;
    }
    camera->yaw -= rotationSpeed * dMouse.x;
    if (camera->yaw > 2 * PI)
    {
        camera->yaw -= 2 * PI;
    }
    if (camera->yaw < -2 * PI)
    {
        camera->yaw += 2 * PI;
    }
}
