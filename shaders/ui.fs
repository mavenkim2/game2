flat in unsigned int container;
flat in f32 slice;
in V2 uv;
out V4 fragColor;

uniform sampler2DArray textureMaps[16];

void main()
{
    fragColor = texture(textureMaps[container], vec3(uv, slice)).rrrr;
}
