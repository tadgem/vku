struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float2 uv : UV;
};

struct VertexOutput
{
    [[vk::location(0)]] float4 pos :SV_Position;
    [[vk::location(1)]] float2 uv :UV;
};


[shader("vertex")]
VertexOutput vert(VertexInput input)
{
    VertexOutput output;

    output.pos = float4(input.position, 1.0);
    output.uv = input.uv;

    return output;
}

static const int MAX_NUM_EACH_LIGHTS = 256;
static const float ATTENUATION_CONSTANT = 1.0;
static const float ATTENUATION_LINEAR_CONSTANT = 4.5;
static const float ATTENUATION_QUADRATIC_CONSTANT = 75.0;

struct DirectionalLight
{
    float4 Direction;
    float4 Ambient;
    float4 Colour;
};

struct PointLight
{
    float4 PositionRadius;
    float4 Ambient;
    float4 Colour;
};

struct SpotLight
{
    float4 PositionRadius;
    float4 DirectionAngle;
    float4 Ambient;
    float4 Colour;
};

struct UniformLightData
{
    DirectionalLight    Directional;
    PointLight          PointLights[MAX_NUM_EACH_LIGHTS];
    SpotLight           SpotLights[MAX_NUM_EACH_LIGHTS];

    uint PointLightsActive;
    uint SpotLightsActive;
}

struct UniformCameraData
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
}


[vk::push_constant]
UniformCameraData u_Cam;

[[vk::binding(0, 0)]]
ConstantBuffer<UniformLightData> u_Lights;

float BlinnPhong_Attenuation(float range, float dist)
{
    float l     = ATTENUATION_LINEAR_CONSTANT / range;
    float q     = ATTENUATION_QUADRATIC_CONSTANT / range;

    return 1.0 / (ATTENUATION_CONSTANT + l * dist + q * (dist * dist));
}

float3 BlinnPhong_Directional(
        float3 normal,
        float3 materialAmbient,
        float3 materialDiffuse,
        float3 materialSpecular,
        float shininess)
{
    float3 lightDir = normalize(-u_Lights.Directional.Direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    float s = pow(max(dot(normal, lightDir), 0.0), shininess);

    float3    ambient     = u_Lights.Directional.Ambient.xyz * materialAmbient;
    float3    diffuse     = u_Lights.Directional.Colour.xyz * diff * materialDiffuse;
    float3    spec        = u_Lights.Directional.Colour.xyz * s * materialSpecular;

    return (ambient + diffuse + spec);
}

float3 BlinnPhong_Point(int lightIdx, float3 position, float3 normal, float3 materialAmbient, float3 materialDiffuse, float3 materialSpecular, float shininess)
{
    float3    ambient    = u_Lights.PointLights[lightIdx].Ambient.xyz * materialAmbient;
    float3    plDir      = u_Lights.PointLights[lightIdx].PositionRadius.xyz - position;
    float3    s          = normalize(plDir);
    float   sDotN      = max(dot(s, normal), 0.0);
    float3    diffuse    = materialDiffuse * sDotN;
    float3    spec       = float3(0.0, 0.0, 0.0);

    if (sDotN > 0.0)
    {
        float3 v = normalize(-position);
        float3 h = normalize(v + s);
        spec =  materialSpecular * pow(max(dot(h, normal), 0.0), shininess);
    }
    float range     = u_Lights.PointLights[lightIdx].PositionRadius.w;

    float attenuation = BlinnPhong_Attenuation(range, length(plDir));

    ambient     *= attenuation;
    diffuse     *= attenuation;
    spec        *= attenuation;

    return (ambient + u_Lights.PointLights[lightIdx].Colour.xyz * (diffuse + spec));
}

float3 BlinnPhong_Spot(int lightIdx, float3 position, float3 normal, float3 materialAmbient, float3 materialDiffuse, float3 materialSpecular, float shininess)
{
    float3    slDir      = u_Lights.SpotLights[lightIdx].PositionRadius.xyz - position;
    float3    s          = normalize(slDir);

    float cosAngle     = dot(-s, normalize(u_Lights.SpotLights[lightIdx].DirectionAngle.xyz));
    float angle        = acos(cosAngle);
    float   theta      = dot(slDir, normalize(-u_Lights.SpotLights[lightIdx].DirectionAngle.xyz));

    if (angle < u_Lights.SpotLights[lightIdx].DirectionAngle.w)
    {
        float3 ambient    = u_Lights.SpotLights[lightIdx].Ambient.xyz * materialAmbient;

        float3    norm    = normalize(normal);
        float   diff    = max(dot(norm, slDir), 0.0);
        float3    diffuse = u_Lights.SpotLights[lightIdx].Colour.xyz * diff * materialDiffuse;

        float3    viewPos = float3(u_Cam.viewMatrix[3][0], u_Cam.viewMatrix[3][1], u_Cam.viewMatrix[3][2]);
        float3    viewDir = normalize(viewPos - position);
        float3    reflectDir = reflect(-slDir, norm);
        float   spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        float3    specular = u_Lights.SpotLights[lightIdx].Colour.xyz * spec * materialSpecular;

        float   range     = u_Lights.SpotLights[lightIdx].PositionRadius.w;
        float   attenuation = BlinnPhong_Attenuation(range, length(slDir));

        diffuse     *= attenuation;
        specular    *= attenuation;

        return (ambient + diffuse + specular);

    }
    else
    {
        return u_Lights.SpotLights[lightIdx].Ambient.xyz * materialAmbient;
    }
}



Sampler2D       positionBuffer;
Sampler2D       normalBuffer;
Sampler2D       colourBuffer;


[shader("fragment")]
float4 frag(VertexOutput input) : SV_Target
{
    float3 textureColour = colourBuffer.Sample(input.uv).xyz;
    float3 position = positionBuffer.Sample( input.uv).xyz;
    float3 normal = normalBuffer.Sample(input.uv).xyz;

    float3 lightColour = BlinnPhong_Directional(normal, float3(0.0f), textureColour.xyz, float3(0.0f), 0.0f);

    for (int i = 0; i < 16; i++)
    {
        //lightColour
        lightColour += BlinnPhong_Point(i, position, normal, float3(0.0f), textureColour.xyz, float3(0.0f), 0.0f);
    }

    for (int i = 0; i < 16; i++)
    {
        //lightColour
        lightColour += BlinnPhong_Spot(i, position, normal, float3(0.0f), textureColour.xyz, float3(0.0f), 0.0f);
    }

    return float4(pow(lightColour, 2.2), 1.0);
}
