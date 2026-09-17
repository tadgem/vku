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
    float4 position : SV_Position;
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
    output.position = mul(modelViewProj, pos);
    output.normal = normalize(mul(normalMatrix, pos)).xyz;
    output.uv = input.uv;

    return output;
}

Sampler2D       diffuse;

struct FragmentOutput
{
    float4 position;
    float4 colour;
    float2 normal;
};


[shader("fragment")]
FragmentOutput frag(VertexOutput input) : SV_Target
{
    FragmentOutput output;
    float3 normalized = normalize(input.normal);
    output.colour = diffuse.Sample( input.uv);
    output.normal = float2(normalized.x, normalized.y);
    output.position = input.position;

    return output;
}
