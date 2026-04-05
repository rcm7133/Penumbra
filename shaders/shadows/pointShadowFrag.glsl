#version 430 core

in vec4 FragPos;

uniform vec3 lightPos;
uniform float farPlane;

out float FragColor;

void main()
{
    float dist = length(FragPos.xyz - lightPos);
    FragColor = dist / farPlane;
}