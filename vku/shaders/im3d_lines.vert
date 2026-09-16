#version 450

#define kAntialiasing 2.0

struct VertexData
{
    vec4 m_positionSize;
    uint m_color;
};

layout(set =0, binding = 0) uniform VertexDataBlock
{
    VertexData VertexData[(64 * 1024) / 32];// assume a 64kb block size, 32 is the aligned size of VertexData
} uVertexData;

layout(set = 0, binding = 1) uniform CameraDataBlock
{
    mat4 ViewProjMatrix;
    vec2 Viewport;
} uCameraData;


layout(location = 0) in vec4 aPosition;

layout(location = 0) noperspective out float vEdgeDistance;
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
    int vid0  = gl_InstanceIndex * 2;// line start
    int vid1  = vid0 + 1;// line end
    int vid   = (gl_VertexIndex % 2 == 0) ? vid0 : vid1;// data for this vertex

    vColor = UintToRgba(uVertexData.VertexData[vid].m_color);
    vSize = uVertexData.VertexData[vid].m_positionSize.w;
    vColor.a *= smoothstep(0.0, 1.0, vSize / kAntialiasing);
    vSize = max(vSize, kAntialiasing);
    vEdgeDistance = vSize * aPosition.y;

    vec4 pos0  = uCameraData.ViewProjMatrix * vec4(uVertexData.VertexData[vid0].m_positionSize.xyz, 1.0);
    vec4 pos1  = uCameraData.ViewProjMatrix * vec4(uVertexData.VertexData[vid1].m_positionSize.xyz, 1.0);
    vec2 dir = (pos0.xy / pos0.w) - (pos1.xy / pos1.w);
    dir = normalize(vec2(dir.x, dir.y * uCameraData.Viewport.y / uCameraData.Viewport.x));// correct for aspect ratio
    vec2 tng = vec2(-dir.y, dir.x) * vSize / uCameraData.Viewport;

    gl_Position = (gl_VertexIndex % 2 == 0) ? pos0 : pos1;
    gl_Position.xy += tng * aPosition.y * gl_Position.w;
}