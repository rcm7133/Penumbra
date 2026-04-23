#version 430 core
// Input is pos and UV because we are writing to a fullscreen quad, not a
layout (location = 0) in vec2 pos; // clip space coords from -1,1
layout (location = 1) in vec2 uv;  // screen space coords from 0,1

out vec2 fragUV;

void main()
{
    fragUV = uv;
    gl_Position = vec4(pos, 0.0, 1.0);
}