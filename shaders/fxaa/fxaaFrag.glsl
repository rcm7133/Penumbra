#version 330 core

in vec2 fragUV;
out vec4 screenColor;

uniform sampler2D screenTexture;
uniform vec2 resolution;

uniform float edgeThreshold;
uniform float edgeThresholdMin;

void main()
{
    vec2 texel = 1.0 / resolution;

    // Sample neighbors
    vec3 rgbNW = texture(screenTexture, fragUV + vec2(-1, -1) * texel).rgb;
    vec3 rgbNE = texture(screenTexture, fragUV + vec2(1, -1) * texel).rgb;
    vec3 rgbSW = texture(screenTexture, fragUV + vec2(-1, 1) * texel).rgb;
    vec3 rgbSE = texture(screenTexture, fragUV + vec2(1, 1) * texel).rgb;
    vec3 rgbM = texture(screenTexture, fragUV).rgb;

    // Lumiance
    // https://stackoverflow.com/questions/596216/formula-to-determine-perceived-brightness-of-rgb-color
    vec3 luma = vec3(0.299, 0.587, 0.114);

    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    // Skip over flat regions
    // https://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
    //
    if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThreshold))
    {
        screenColor = vec4(rgbM, 1.0);
        return;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    // Magic NVidia constants
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texel;

    // First blur, small sample size
    vec3 rgbA = 0.5 * (
    texture(screenTexture, fragUV + dir * (1.0/3.0 - 0.5)).rgb +
    texture(screenTexture, fragUV + dir * (2.0/3.0 - 0.5)).rgb
    );
    // Second blur, larger sample size
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
    texture(screenTexture, fragUV + dir * -0.5).rgb +
    texture(screenTexture, fragUV + dir *  0.5).rgb
    );

    float lumaB = dot(rgbB, luma);
    screenColor = vec4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, 1.0);
}