#version 330 core
in vec2 fragUV;
out vec4 screenColor;
uniform sampler2D litScene;
void main()
{
    screenColor = vec4(texture(litScene, fragUV).rgb, 1.0);
}