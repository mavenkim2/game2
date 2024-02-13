layout (location = 0) in V3 pos;
layout (location = 1) in V4 colorIn;
// in V3 n;

out V4 color;
// out V3 worldPosition;
// out V3 worldN;

uniform mat4 transform; 

void main()
{ 
    gl_Position = transform * V4(pos, 1.f);
    color = colorIn;
    // worldPosition = pos.xyz;
    // worldN = n;
}
