#version 450

#define kAntialiasing 2.0

layout(location = 0) noperspective in float vEdgeDistance;
layout(location = 1) noperspective in float vSize;
layout(location = 2) smooth in vec4  vColor;

layout(location = 0) out vec4 fResult;

void main()
{
    fResult = vColor;
    float d = abs(vEdgeDistance) / vSize;
    d = smoothstep(1.0, 1.0 - (kAntialiasing / vSize), d);
    fResult.a = 1.0;
}