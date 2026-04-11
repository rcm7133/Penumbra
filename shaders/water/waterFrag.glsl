#version 430 core

out vec4 FragColor;

in vec3 FragPos;
in vec2 TexCoords;

uniform vec3 cameraPos;
uniform vec3 shallowColor;
uniform vec3 deepColor;
uniform float fresnelPower;
uniform float specularStrength;
uniform sampler2D normalMap;
uniform samplerCube reflectionCubeMap;

void main()
{
    vec3 normal = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;

    FragColor = vec4(normal, 1.0);
}