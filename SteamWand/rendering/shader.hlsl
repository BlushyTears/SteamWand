
cbuffer TriangleData : register(b0)
{
    float2 offset;
    float size;
};

float4 VSLine(uint vertexID : SV_VertexID) : SV_POSITION
{
    static const float2 positions[2] =
    {
        float2(0.0f, 0.5f),
        float2(0.0f, -0.5f)
    };
    
    return float4(positions[vertexID] * size + offset, 0.0f, 1.0f);
}

float4 VSMain(uint vertexID : SV_VertexID) : SV_POSITION
{
    static const float2 positions[3] =
    {
        float2(0.0f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };

    return float4(positions[vertexID] * size + offset, 0.0f, 1.0f);
}

float4 PSLine() : SV_TARGET
{
    return float4(0.5, 0.1, 0, 1.0f);
}

float4 PSMain() : SV_TARGET
{
    return float4(0.0f, 0.5f, 0.0f, 1.0f);
}
