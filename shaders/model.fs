uniform sampler2D diffuseMap;
uniform sampler2D normalMap;

in V2 outUv;
in V3 tangentLightDir;
in V3 tangentViewPos;
in V3 tangentFragPos;

in V3 outN;

out V4 FragColor;

void main()
{
    V3 normal = normalize(texture(normalMap, outUv).rgb * 2 - 1);

    V3 color = texture(diffuseMap, outUv).rgb;

    // Direction Phong Light
    V3 lightDir = normalize(tangentLightDir);

    //AMBIENT
    V3 ambient = 0.1f * color;
    //DIFFUSE
    f32 diffuseCosAngle = max(dot(normal, lightDir), 0.f);
    V3 diffuse = diffuseCosAngle * color;
    //SPECULAR
    V3 toViewPosition = normalize(tangentViewPos - tangentFragPos);
    V3 reflectVector = -lightDir + 2 * dot(normal, lightDir) * normal;
    f32 specularStrength = pow(max(dot(reflectVector, toViewPosition), 0.f), 64);

    f32 spec = specularStrength;
    V3 specular = spec * color;

    FragColor = V4(ambient + diffuse + specular, 1.0);
    // FragColor = V4(normal, 1.0);
    // FragColor = V4(color, 1.0);
}
