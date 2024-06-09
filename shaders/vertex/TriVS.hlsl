struct VertToPix
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

VertToPix main(uint id : SV_VertexID)
{
    float2 positions[3] =
    {
        float2(0.0, -0.5),
        float2(0.5, 0.5),
        float2(-0.5, 0.5)
    };

    float3 colors[3] =
    {
        float3(1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, 0.0, 1.0)
    };
    
    VertToPix output = (VertToPix) 0;
    
    float2 pos = positions[id];
    output.pos = float4(pos.x, pos.y, 0.0, 1.0);
    output.color = colors[id];
    
    return output;
}