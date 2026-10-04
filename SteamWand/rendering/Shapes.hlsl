
cbuffer ObjectData : register(b0)
{
    float2 offset;
    float size;
    float angle;
    column_major float4x4 viewProjection;
};

float4 VSMesh(float3 position : POSITION) : SV_Position
{
    float3 p = position;
    
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

float4 PSMesh() : Sv_Target
{
    return float4(0.1, 0.1, 0.0f, 1.0f);
}

