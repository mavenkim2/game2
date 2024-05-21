flat in unsigned int container;
flat in f32 slice;
in V2 uv;
out V4 fragColor;

uniform sampler2DArray textureMaps[32];

void main()
{
    fragColor = V4(1, 1, 1, texture(textureMaps[container], vec3(uv, slice)).r);
}
