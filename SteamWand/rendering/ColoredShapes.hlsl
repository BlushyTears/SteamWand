
// Shader for only color and position
cbuffer ConstantBuffer : register(b0) {

    float4x4 model;
    float4x4 viewProjection;
}

struct VertexInput
{
    float3 pos : POSITION;
    float3 color : COLOR;
};

struct OutputElement
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

OutputElement VSMesh(VertexInput vertexInput)
{
    OutputElement output;
    
    float4 worldPos = mul(model, float4(vertexInput.pos, 1.0f));
    
    output.pos = mul(viewProjection, worldPos);
    output.color = vertexInput.color;
    
    return output;
}

float4 PSMesh(OutputElement vertexOutput) : SV_TARGET
{
    return float4(vertexOutput.color, 1.0f);
}
