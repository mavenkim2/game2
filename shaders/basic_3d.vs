layout (location = 0) in V3 pos;
layout (location = 1) in V4 colorIn;
#ifdef INSTANCED
layout (location = 2) in Mat4 offset;
#endif
// in V3 n;

out V4 color;
// out V3 worldPosition;
// out V3 worldN;

uniform mat4 transform; 

void main()
{ 

#ifdef INSTANCED
    gl_Position = transform * offset * V4(pos, 1.f);
#else
    gl_Position = transform * V4(pos, 1.f);
#endif

    color = colorIn;
    // worldPosition = pos.xyz;
    // worldN = n;
}
