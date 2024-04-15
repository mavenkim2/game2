layout (location = 0) in f32 altitude;

uniform Mat4 transform;
uniform f32 width;
uniform f32 inHeight;

out f32 height;

/*
The last and first index in each triangle strip is repeated twice.
    1 ---- 3
    | \    |
    |   \  |
    0 ---- 2
*/

void main ()
{
    int vertexId = gl_VertexID - gl_BaseVertex;
    f32 y = inHeight/2 - floor(vertexId / width);
    f32 x = floor(mod(vertexId, width)) - width/2;

    gl_Position = transform * V4(x, y, altitude, 1.f);
    height = altitude;
}
