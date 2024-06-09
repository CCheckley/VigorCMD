struct VertToPix
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

float4 main(in VertToPix input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}