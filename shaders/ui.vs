layout (location = 0) in V2 translate;
layout (location = 1) in V2 scale;
layout (location = 2) in V4 handle;

flat out unsigned int container;
flat out f32 slice;
out V2 uv;
uniform Mat4 transform;

V2 pos[4] = V2[](V2(0.0, 1.0), V2(0.0, 0.0), V2(1.0, 1.0), V2(1.0, 0.0));

void main()
{
    V2 instancePos = translate + scale * pos[gl_VertexID];
    gl_Position = transform * V4(instancePos, 0, 1);
    
    container = floatBitsToUint(handle.x);
    slice = handle.y;
    uv = pos[gl_VertexID];
}
