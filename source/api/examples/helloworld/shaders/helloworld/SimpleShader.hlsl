
struct Transform
{
    float4x4 ModelViewProjection;
};

ConstantBuffer<Transform> TransformData : register(b0);

struct VertexInput
{
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct VertexOutput
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.Position = mul(TransformData.ModelViewProjection, float4(input.Position, 1.0f));
    output.Color = input.Color;

    return output;
}

float4 PSMain(VertexOutput input) : SV_Target
{
    return input.Color;
}
