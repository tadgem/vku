import vku;

struct PushConstants
{
    float4x4 view;
    float4x4 proj;
}

struct UniformBufferObject {
    float4x4 model;
};

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : UV;
};

struct VertexOutput
{
    float3 position : SV_Position;
    [[vk::location(0)]] float3 normal :NORMAL;
    [[vk::location(1)]] float2 uv :UV;
};

[vk::push_constant]
PushConstants pc;

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> ubo;

[shader("vertex")]
VertexOutput vert(VertexInput input)
{
    VertexOutput output;
    float4x4 normalMatrix = transpose(_inverse(ubo.model));
    float4x4 modelViewProj = mul(mul(pc.proj, pc.view), ubo.model);
    float4 pos = float4(input.position, 1.0);
    output.position = mul(modelViewProj, pos).xyz;
    output.normal = normalize(mul(normalMatrix, pos)).xyz;
    output.uv = input.uv;

    return output;
}

Sampler2D       diffuse;

[shader("fragment")]
float4 frag(VertexOutput input) : SV_Target
{
    float4 col = diffuse.Sample(input.uv);
    //float4 col = float4( input.uv, 0.0, 1.0 );

    return float4(pow(col.rgb, 2.2), 1.0);
}
