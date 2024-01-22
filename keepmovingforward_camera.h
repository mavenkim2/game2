#ifndef KEEPMOVINGFORWARD_CAMERA_H
struct Camera {
    f32 pitch;
    f32 yaw;

    f32 dolly;

    V3 position;

    V3 forward;
    V3 right;
};
#define KEEPMOVINGFORWARD_CAMERA_H
#endif
