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
    
    output.pos = float4(pos.x, pos.y, 0.0, 1.0);
    output.color = color;
    
    return output;
}