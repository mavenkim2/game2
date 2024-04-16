// TODO: invocations is split + 1, set a cvar
layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

layout (std140, binding = 0) uniform lightVPMatrices
{
    Mat4 lightViewProjectionMatrices[16];
};

void main()
{
    for (int i = 0; i < 3; i++)
    {
        gl_Position = lightViewProjectionMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
