#version 430 core

in vec2 fragUV;
out vec4 FragColor;
// gBuffer
uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gPosition;
// Scene (already lit)
uniform sampler2D sceneColor;

uniform float minStepSize;
uniform float maxStepSize;
uniform int raymarchSteps;

uniform vec3 cameraPos;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 worldPos = texture(gPosition, fragUV).xyz;
    vec3 normal   = normalize(texture(gNormal, fragUV).xyz);
    float roughness = texture(gNormal, fragUV).a;
    // Reflection ray in world space
    vec3 viewDir = normalize(worldPos - cameraPos);
    vec3 reflectionDir = reflect(viewDir, normal);
    // Set up raymarch
    vec3 rayPos = worldPos;
    vec3 hitColor = vec3(0.0f);

    float stepSize = mix(minStepSize, maxStepSize, roughness); // rougher means fewer steps

    bool hit = false;

    // Raymarch
    for (int i = 0; i < raymarchSteps; i++)
    {
        rayPos += reflectionDir * stepSize;

        // Project to clip space
        vec4 clip = projection * view * vec4(rayPos, 1.0);
        // Normalized coordinate
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;
        // Out of screen so its a miss
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            break;

        // Intersection test
        vec3 scenePos = texture(gPosition, uv).xyz;
        float rayDepth  = (view * vec4(rayPos, 1.0)).z;
        float sceneDepth = (view * vec4(scenePos, 1.0)).z;

        if (rayDepth < sceneDepth && sceneDepth - rayDepth < stepSize * 2.0)
        {
            hitColor = texture(sceneColor, uv).rgb;
            hit = true;
            break;
        }
    }

    if (!hit)
        hitColor = vec3(0.0);
    // Roughness fade
    hitColor *= (1.0 - roughness);

    FragColor = vec4(hitColor, hit ? 1.0 : 0.0);
}