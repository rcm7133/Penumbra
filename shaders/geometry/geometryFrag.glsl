#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;

in vec3 fragPos;
in vec2 fragUV;
in vec3 fragNormal;
in mat3 TBN;

uniform sampler2D diffuseTex;
uniform sampler2D normalMap;
uniform float shininess;
uniform bool hasNormalMap;

void main()
{
    gPosition   = fragPos;
    gAlbedo.rgb = texture(diffuseTex, fragUV).rgb;
    gAlbedo.a   = shininess / 512.0;

    if (hasNormalMap)
    {
        // Sample normal map and remap from 0,1 to -1,1
        vec3 n = texture(normalMap, fragUV).rgb;
        n = n * 2.0 - 1.0;
        gNormal = normalize(TBN * n);
    }
    else
    {
        gNormal = normalize(fragNormal);
    }
}