#include "_inc_lighting.hlsl"
#include "_inc_input_instanced.hlsl"
#include "_inc_cbuffers.hlsl"

struct VSOutput
{
    float4 pos_h : SV_POSITION;
    float3 pos_w : POSITION;
    float3 normal_w : NORMAL;
    float2 tex : TEXCOORD;
	float4 ShadowPosH : TEXCOORD1;
	float4 color    : COLOR;
};

VSOutput mainVS(VSInput input)
{
    VSOutput output;

	float3 posw = mul(float4(input.pos_l, 1.0f), input.world).xyz;
    output.pos_h = mul(float4(posw, 1.0f), ViewProj);
	output.pos_w = posw;
	output.normal_w = mul(input.normal_l, (float3x3)input.world_inv_transpose);
    output.tex = input.tex;
    output.color = input.color;
	// Generate projective tex-coords to project shadow map onto scene.
	output.ShadowPosH = mul(float4(input.pos_l, 1.0f), input.shadow_transform);
    return output;
}

Texture2D tex : register (t0);
Texture2D spec_tex : register (t1);
Texture2D shadow_map : register (t2);

SamplerState tex_sampler : register(s0);
SamplerComparisonState shadow_sampler : register(s1);

float4 mainPS(VSOutput pin) : SV_Target
{
	// Interpolating normal can unnormalize it, so normalize it.
    pin.normal_w = normalize(pin.normal_w);

	// The toEye vector is used in lighting.
	float3 toEye = eye_pos_w - pin.pos_w;

	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye);

	// Normalize.
	toEye /= distToEye;

    float4 texColor = tex.Sample(tex_sampler, pin.tex);
	float4 texSpecular = spec_tex.Sample(tex_sampler, pin.tex) * 10.0;

	//
	// Lighting.
	//

	float4 litColor = texColor;

	float shadow = CalcShadowFactor(shadow_sampler, shadow_map, pin.ShadowPosH);

    // Start with a sum of zero. 
    float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 spec    = float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Sum the light contribution from each light source.  
    float4 A, D, S;


    // taking the color from the instance buffer's color
    Material new_mat = mat;
    new_mat.Diffuse = pin.color;

    compute_directional_light(new_mat, light, pin.normal_w, toEye, 
        A, D, S);

    ambient += A;

    float shadow_factor = saturate(shadow + 0.25);

    diffuse += shadow_factor * D;
    spec    += shadow_factor * S;

    litColor = texColor*(ambient + diffuse) + texSpecular * spec;

	// Common to take alpha from diffuse material and texture.
	litColor.a = pin.color.a;

    return litColor;
    // return texColor;
}
