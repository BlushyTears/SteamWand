
cbuffer TriangleData : register(b0)
{
    float2 offset;
    float size;
    float angle;
    column_major float4x4 viewProjection;
};

float4 VSLine(uint vertexID : SV_VertexID) : SV_Position
{
    static const float2 positions[2] =
    {
        float2(0.0f, 0.5f),
        float2(0.0f, -0.5f)
    };
    
    return float4(positions[vertexID] * size + offset, 0.0f, 1.0f);
}

float4 VSQuad(uint vertexID : SV_VertexID) : SV_Position
{
    static const float2 positions[6] =
    {
        float2(0.5f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f),
        float2(-0.5f, 0.5f),
        float2(0.5f, 0.5f),
        float2(-0.5f, -0.5f)
    };
    
    return float4(positions[vertexID] * size + offset, 0.0f, 1.0f);
}

float4 VSCube(uint vertexID : SV_VertexID) : SV_Position
{
    static const float3 positions[8] =
    {
        float3(-0.5f, -0.5f, -0.5f),
        float3(0.5f, -0.5f, -0.5f),
        float3(0.5f, 0.5f, -0.5f),
        float3(-0.5f, 0.5f, -0.5f),
        
        float3(-0.5f, -0.5f, 0.5f),
        float3(0.5f, -0.5f, 0.5f),
        float3(0.5f, 0.5f, 0.5f),
        float3(-0.5f, 0.5f, 0.5f),
    };
    
    static const uint indices[36] =
    {
        0, 1, 2, 0, 2, 3, // z = -0.5 face
        4, 6, 5, 4, 7, 6, // z = +0.5 face
        0, 3, 7, 0, 7, 4, // left
        1, 5, 6, 1, 6, 2, // right
        3, 2, 6, 3, 6, 7, // top
        0, 4, 5, 0, 5, 1 // bottom
    };
    
    float3 p = positions[indices[vertexID]];

    float s = sin(angle);
    float c = cos(angle);

    p = float3(c * p.x + s * p.z,
           p.y,
          -s * p.x + c * p.z);

    p = float3(p.x,
           c * p.y - s * p.z,
           s * p.y + c * p.z);
    
    float3 worldPosition = p * size + float3(offset, 0.0f);
    return mul(viewProjection, float4(worldPosition, 1.0f));
}

float4 VSMain(uint vertexID : SV_VertexID) : SV_Position
{
    static const float2 positions[3] =
    {
        float2(0.0f, 0.5f),
        float2(0.5f, -0.5f),
        float2(-0.5f, -0.5f)
    };

    return float4(positions[vertexID] * size + offset, 0.0f, 1.0f);
}

float4 PSQuad() : Sv_Target
{
    return float4(0.1, 0.1, 0.0f, 1.0f);
}

float4 PSCube() : Sv_Target
{
    return float4(0.1, 0.1, 0.0f, 1.0f);
}

float4 PSLine() : Sv_Target
{
    return float4(0.5, 0.1, 0.0f, 1.0f);
}

float4 PSMain() : Sv_Target
{
    return float4(0.0f, 0.5f, 0.0f, 1.0f);
}

