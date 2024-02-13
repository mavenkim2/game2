in V4 color;
// in V3 worldPosition;
// in V3 worldN;

out V4 FragColor;

// uniform V3 cameraPosition;

void main()
{
    FragColor = V4(color);
    // V3 lightPosition = vec3(0, 0, 5);
    // f32 lightCoefficient = 50.f;
    // V3 toLight = normalize(lightPosition - worldPosition);
    // f32 lightDistance = distance(lightPosition, worldPosition);
    // f32 lightStrength = lightCoefficient / (lightDistance * lightDistance); 
    //
    // //AMBIENT
    // f32 ambient = 0.1f;
    // //DIFFUSE
    // f32 diffuseCoefficient = 0.1f;
    // f32 diffuseCosAngle = max(dot(worldN, lightPosition), 0.f);
    // f32 diffuse = diffuseCoefficient * diffuseCosAngle * lightStrength;
    // //SPECULAR
    // f32 specularCoefficient = 2.f;
    // V3 toViewPosition = normalize(cameraPosition - worldPosition);
    // // V3 reflectVector = -toLight + 2 * dot(worldN, toLight) * worldN;
    // V3 reflectVector = -toViewPosition + 2 * dot(worldN, toViewPosition) * worldN;
    // f32 specularStrength = pow(max(dot(reflectVector, toLight), 0.f), 64);
    // // f32 specularStrength = pow(max(dot(reflectVector, toViewPosition), 0.f), 64);
    // f32 specular = specularCoefficient * specularStrength * lightStrength;
    //
    // FragColor = (ambient + diffuse + specular) * V4(color, 1.f);
}
