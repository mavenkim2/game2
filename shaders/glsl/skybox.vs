layout (location = 0) in vec3 pos;
out vec3 localPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    localPos = pos;
    // gl_Position = projection * view * vec4(pos, 1.0);
    mat4 rotView = mat4(mat3(view)); // removes translation
    V4 clipPos = projection * rotView * V4(pos, 1.0);
    gl_Position = clipPos.xyww; // renders sky box at max depth
}
