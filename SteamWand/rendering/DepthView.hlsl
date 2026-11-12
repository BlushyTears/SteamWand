// F3 view: draws the depth buffer over the whole screen in gray (close = dark, far = white)

Texture2D<float> g_depth : register(t0);

// Has to match the perspective() call in Renderer::Render() to be accurate
static const float nearPlane = 0.1f;
static const float farPlane = 100.0f;

// Anything this far from the camera or further shows as white
static const float maxShownDistance = 10.0f;
static const float2 corners[3] = { float2(-1.0f, 1.0f), float2(3.0f, 1.0f), float2(-1.0f, -3.0f) };

float4 VSFullscreen(uint id : SV_VertexID) : SV_POSITION
{
    return float4(corners[id], 0.0f, 1.0f);
}

float4 PSDepth(float4 pos : SV_POSITION) : SV_Target
{
    float depth = g_depth.Load(int3(pos.xy, 0));
    float viewDistance = nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));

    float gray = saturate(viewDistance / maxShownDistance);
    return float4(gray, gray, gray, 1.0f);
}
