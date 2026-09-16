static const float kAntialiasing = 2.0;

struct VertexData
{
    float4 m_positionSize;
    uint   m_color;
};

struct VertexDataBlock
{
    VertexData data[(64 * 1024) / 32]; // assume a 64kb block size, 32 is the aligned size of VertexData
};

struct CameraDataBlock
{
    float4x4 ViewProjMatrix;
    float2   Viewport;
};

[[vk::binding(0, 0)]]
ConstantBuffer<VertexDataBlock> uVertexData;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraDataBlock> uCameraData;

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] noperspective float  edgeDistance : TEXCOORD0;
    [[vk::location(1)]] noperspective float  size         : TEXCOORD1;
    [[vk::location(2)]] float4               color        : COLOR0;
};

float4 UintToRgba(uint u)
{
    float4 ret = float4(0.0);
    ret.r = float((u & 0xff000000u) >> 24u) / 255.0;
    ret.g = float((u & 0x00ff0000u) >> 16u) / 255.0;
    ret.b = float((u & 0x0000ff00u) >> 8u)  / 255.0;
    ret.a = float((u & 0x000000ffu) >> 0u)  / 255.0;
    return ret;
}

[shader("vertex")]
VSOutput vertexMain([[vk::location(0)]] float4 aPosition : POSITION,
                    uint vertexIndex   : SV_VertexID,
                    uint instanceIndex : SV_InstanceID)
{
    int vid0 = int(instanceIndex) * 2; // line start
    int vid1 = vid0 + 1;               // line end
    int vid  = (vertexIndex % 2 == 0) ? vid0 : vid1; // data for this vertex

    VSOutput output;
    output.color = UintToRgba(uVertexData.data[vid].m_color);
    output.size  = uVertexData.data[vid].m_positionSize.w;
    output.color.a *= smoothstep(0.0, 1.0, output.size / kAntialiasing);
    output.size = max(output.size, kAntialiasing);
    output.edgeDistance = output.size * aPosition.y;

    float4 pos0 = mul(uCameraData.ViewProjMatrix, float4(uVertexData.data[vid0].m_positionSize.xyz, 1.0));
    float4 pos1 = mul(uCameraData.ViewProjMatrix, float4(uVertexData.data[vid1].m_positionSize.xyz, 1.0));
    float2 dir  = (pos0.xy / pos0.w) - (pos1.xy / pos1.w);
    dir = normalize(float2(dir.x, dir.y * uCameraData.Viewport.y / uCameraData.Viewport.x)); // correct for aspect ratio
    float2 tng = float2(-dir.y, dir.x) * output.size / uCameraData.Viewport;

    output.position = (vertexIndex % 2 == 0) ? pos0 : pos1;
    output.position.xy += tng * aPosition.y * output.position.w;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float4 result = input.color;
    float  d = abs(input.edgeDistance) / input.size;
    d = smoothstep(1.0, 1.0 - (kAntialiasing / input.size), d);
    result.a = 1.0;
    return result;
}
