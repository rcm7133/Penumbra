#version 330 core
out float fragColor;

in vec2 fragUV;

uniform sampler2D ssaoInput;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;

    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            result += texture(ssaoInput, fragUV + vec2(float(x), float(y)) * texelSize).r;
        }
    }

    fragColor = result / 25.0;
}