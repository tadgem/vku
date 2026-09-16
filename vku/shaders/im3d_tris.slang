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
    float4x4 uViewProjMatrix;
    float2   uViewport;
};

[[vk::binding(0, 0)]]
ConstantBuffer<VertexDataBlock> uVertexData;

[[vk::binding(1, 0)]]
ConstantBuffer<CameraDataBlock> uCameraData;

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(1)]] noperspective float  size  : TEXCOORD1;
    [[vk::location(2)]] float4               color : COLOR0;
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
    int vid = int(instanceIndex) * 3 + int(vertexIndex);

    VSOutput output;
    output.color = UintToRgba(uVertexData.data[vid].m_color);
    output.size  = 0.0;
    output.position = mul(uCameraData.uViewProjMatrix, float4(uVertexData.data[vid].m_positionSize.xyz, 1.0));

    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    return input.color;
}
