cbuffer ModelViewProjectionBuffer : register(b0)
{
    float4x4 Model;
    float4x4 View;
    float4x4 Projection;
}

struct VertToPix
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

VertToPix main
(
    float2 pos : POSITION,
    float3 color : COLOR0
)
{    
    VertToPix output = (VertToPix) 0;
    
    output.pos = mul(float4(pos.x, pos.y, 0.0, 1.0), (Model * View * Projection));
    output.color = color;
    
    return output;
}