#version 450

#define kAntialiasing 2.0

layout(location = 0) noperspective in vec2 vUv;
layout(location = 1) noperspective in float vSize;
layout(location = 2) smooth in vec4  vColor;

layout(location = 0) out vec4 fResult;

void main()
{
    fResult = vColor;
    float d = length(vUv - vec2(0.5));
    d = smoothstep(0.5, 0.5 - (kAntialiasing / vSize), d);
    fResult.a = 1.0;
}