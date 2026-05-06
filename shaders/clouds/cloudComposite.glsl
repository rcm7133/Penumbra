#version 430 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D litScene;
uniform sampler2D cloudBuffer;

void main() {
    vec3 scene = texture(litScene, TexCoords).rgb;
    vec4 cloud = texture(cloudBuffer, TexCoords);

    // cloud.rgb = scattered light, cloud.a = opacity (1 - transmittance)
    vec3 result = scene * (1.0 - cloud.a) + cloud.rgb;
    FragColor = vec4(result, 1.0);
}