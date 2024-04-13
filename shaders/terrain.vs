layout (location = 0) in V3 pos;

uniform Mat4 transform;

out f32 height;

void main ()
{
    gl_Position = transform * V4(pos, 1.f);
    height = pos.z;
}
