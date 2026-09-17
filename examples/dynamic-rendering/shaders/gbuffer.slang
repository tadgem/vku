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
    float4 clipPosition : SV_Position;
    [[vk::location(0)]] float3 normal :NORMAL;
    [[vk::location(1)]] float2 uv :UV;
    [[vk::location(2)]] float3 worldPosition :POSITION;
};

float2 SignNotZero(float2 v)
{
    return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

// Packs a unit normal into 2 components (octahedral encoding).
float2 EncodeNormal(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 enc = n.xy;
    if (n.z < 0.0)
    {
        enc = (1.0 - abs(n.yx)) * SignNotZero(enc);
    }
    return enc;
}

[vk::push_constant]
PushConstants pc;

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> ubo;

[shader("vertex")]
VertexOutput vert(VertexInput input)
{
    VertexOutput output;
    float4x4 normalMatrix = transpose(_inverse(ubo.model));
    float4 worldPos = mul(ubo.model, float4(input.position, 1.0));
    output.clipPosition = mul(mul(pc.proj, pc.view), worldPos);
    output.normal = normalize(mul(normalMatrix, float4(input.normal, 0.0)).xyz);
    output.uv = input.uv;
    output.worldPosition = worldPos.xyz;

    return output;
}

Sampler2D       diffuse;

struct FragmentOutput
{
    // locations must match the attachment mapping in main.cpp:
    // 0 -> normalBuffer, 1 -> colourBuffer, 2 -> positionBuffer
    float2 normal;
    float4 colour;
    float4 position;
};


[shader("fragment")]
FragmentOutput frag(VertexOutput input) : SV_Target
{
    FragmentOutput output;
    output.normal = EncodeNormal(normalize(input.normal));
    output.colour = diffuse.Sample( input.uv);
    output.position = float4(input.worldPosition, 1.0);

    return output;
}
