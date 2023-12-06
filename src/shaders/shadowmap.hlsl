#include "_inc_lighting.hlsl"
#include "_inc_input_instanced.hlsl"
#include "_inc_cbuffers.hlsl"

struct VSOutput
{
    float4 pos_h : SV_POSITION;
};

VSOutput mainVS(VSInput input)
{
    VSOutput output;
	float3 posw = mul(float4(input.pos_l, 1.0f), input.world).xyz;
    output.pos_h = mul(float4(posw, 1.0f), ViewProj);
    return output;
}

// this will not be used
float4 mainPS(VSOutput input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0);
}