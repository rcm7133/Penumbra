#version 430 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D tex;
uniform int channel; // 0=R 1=G 2=B 3=A

void main()
{
    vec4 s = texture(tex, TexCoords);
    float v = s[channel];
    FragColor = vec4(v, v, v, 1.0);
}