#include "_inc_lighting.hlsl"
#include "_inc_input_basic.hlsl"
#include "_inc_cbuffers.hlsl"

Texture2D text_atlas : register (t0);
SamplerState basic_sampler : register(s0);

struct VSOutput
{
    float4 pos_h : SV_POSITION;
    float2 tex : TEXCOORD;
	float4 color : COLOR;
};

VSOutput mainVS(VSInput input)
{
    VSOutput output;
    
    // Transform vertex position
    output.pos_h = mul(float4(input.pos_l, 1.0f), WorldViewProjection);
    output.tex = input.tex;
    output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    return output;
}

float4 mainPS(VSOutput input) : SV_Target
{
    // return float4(1.0f, 0.0f, 0.0f, 1.0f);
	float4 tex_color = text_atlas.Sample(basic_sampler, input.tex);
    return float4(1.0f, 1.0f, 1.0f, tex_color.r);
}