#version 430 core

in vec4 vColor;
in float vLifeRatio;

out vec4 FragColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);

    if (dist > 0.5)
    discard;

    float alpha = smoothstep(0.5, 0.0, dist);
    alpha *= vLifeRatio;

    FragColor = vec4(vColor.rgb, vColor.a * alpha);
}