#version 330 core
in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D gAlbedo;

void main()
{
    screenColor = vec4(texture(gAlbedo,   fragUV).rgb, 1.0);
}