struct VSInput
{
    float3 pos_l : POSITION;
    float3 normal_l : NORMAL;
    float2 tex : TEXCOORD;
	float4x4 world  : WORLD;
	float4x4 world_inv_transpose : WORLD_INV_TRANSPOSE;
	float4x4 shadow_transform : SHADOW_TRANSFORM;
	float4 color    : COLOR;
	uint instance_id : SV_InstanceID;
};
