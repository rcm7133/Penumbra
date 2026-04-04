#define MAX_TOTAL_LIGHTS 16
#define MAX_SHADOW_LIGHTS 3

#define LIGHT_DIRECTIONAL 0
#define LIGHT_SPOT        1
#define LIGHT_POINT       2

struct Light {
    vec3 position;
    vec3 color;
    vec3 direction;
    float intensity;
    float innerCutoff;
    float outerCutoff;
    int type;
    bool castsShadow;
};

uniform int lightCount;
uniform Light lights[MAX_TOTAL_LIGHTS];

uniform int shadowLightCount;
uniform sampler2D shadowMap[MAX_SHADOW_LIGHTS];
uniform mat4 lightSpaceMatrix[MAX_SHADOW_LIGHTS];