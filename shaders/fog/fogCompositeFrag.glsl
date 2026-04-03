#version 430 core
in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D litScene;
uniform sampler2D fogBuffer; // scaled down rgba fog
uniform vec2 resolution; // Full screen resolution
uniform vec2 fogResolution;
uniform int fogBlurKernelSize;

// Simple gaussian blur on the fog buffer before compositing
vec4 BlurFog(vec2 uv)
{
    int halfKernel = fogBlurKernelSize / 2;
    vec2 texel = 1.0 / fogResolution;
    vec4 result = vec4(0.0);
    int sampleCount = 0;

    for (int x = -halfKernel; x <= halfKernel; x++)
    {
        for (int y = -halfKernel; y <= halfKernel; y++)
        {
            result += texture(fogBuffer, uv + vec2(x, y) * texel);
            sampleCount++;
        }
    }
    return result / float(sampleCount);
}

void main()
{
    vec3 scene = texture(litScene, fragUV).rgb;
    vec4 fog   = BlurFog(fragUV);

    vec3  fogLight = fog.rgb;
    float transmit = fog.a;

    screenColor = vec4(scene * transmit + fogLight, 1.0);
}
