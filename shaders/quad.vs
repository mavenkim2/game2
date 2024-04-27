#version 330 core
out vec2 uv;

const vec3 positions[4] = vec3[](vec3(-1.0, 1.0, 0.0), vec3(-1.0, -1.0, 0.0), vec3(1.0, 1.0, 0.0), vec3(1.0, -1.0, 0.0));
const vec2 uvs[4] = vec2[](vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(1.0, 0.0));

void main()
{
    uv = uvs[gl_VertexID];
    // not used for anything
    gl_Position = vec4(positions[gl_VertexID], 1.0);
}
