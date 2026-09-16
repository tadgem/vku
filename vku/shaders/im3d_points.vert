#version 450

#define kAntialiasing 2.0

struct VertexData
{
    vec4 m_positionSize;
    uint m_color;
};

layout(set =0, binding = 0) uniform VertexDataBlock
{
    VertexData uVertexData[(64 * 1024) / 32];// assume a 64kb block size, 32 is the aligned size of VertexData
};

layout(set = 0, binding = 1) uniform CameraDataBlock
{
    mat4 uViewProjMatrix;
    vec2 uViewport;
};


layout(location = 0) in vec4 aPosition;

layout(location = 0) noperspective out vec2 vUv;
layout(location = 1) noperspective out float vSize;
layout(location = 2) smooth out vec4  vColor;

vec4 UintToRgba(uint _u)
{
    vec4 ret = vec4(0.0);
    ret.r = float((_u & 0xff000000u) >> 24u) / 255.0;
    ret.g = float((_u & 0x00ff0000u) >> 16u) / 255.0;
    ret.b = float((_u & 0x0000ff00u) >> 8u)  / 255.0;
    ret.a = float((_u & 0x000000ffu) >> 0u)  / 255.0;
    return ret;
}

void main()
{
    int vid = gl_InstanceIndex;

    vSize = max(uVertexData[vid].m_positionSize.w, kAntialiasing);
    vColor = UintToRgba(uVertexData[vid].m_color);
    vColor.a *= smoothstep(0.0, 1.0, vSize / kAntialiasing);

    gl_Position = uViewProjMatrix * vec4(uVertexData[vid].m_positionSize.xyz, 1.0);
    vec2 scale = 1.0 / uViewport * vSize;
    gl_Position.xy += aPosition.xy * scale * gl_Position.w;
    vUv = aPosition.xy * 0.5 + 0.5;
}