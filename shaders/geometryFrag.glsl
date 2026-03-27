#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;

in vec3 fragPos;
in vec2 fragUV;
in vec3 fragNormal;

uniform sampler2D diffuseTex;
uniform float shininess;

void main()
{
    gPosition        = fragPos;
    gNormal          = normalize(fragNormal);
    gAlbedo.rgb      = texture(diffuseTex, fragUV).rgb;
    gAlbedo.a        = shininess / 512.0;
}