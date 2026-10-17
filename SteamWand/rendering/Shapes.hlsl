
cbuffer ConstantBuffer : register(b0)
{
    column_major float4x4 model;
    column_major float4x4 viewProjection;
};

struct InputElement
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct OutputElement
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

OutputElement VSMesh(InputElement input)
{
    OutputElement output;
    
    float4 worldPos = mul(model, float4(input.pos, 1.0f));

    output.pos = mul(viewProjection, worldPos);
    output.uv = input.uv;
    
    return output;
}

float4 PSMesh(OutputElement input) : SV_Target
{
    return g_texture.Sample(g_sampler, input.uv);
}   
