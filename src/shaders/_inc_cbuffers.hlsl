// mind the layout/padding
cbuffer FrameCBuffer: register(b0)
{
	float4x4 ViewProj;
    float4x4 cbf_view;
    float4x4 cbf_proj;
    float4x4 cbf_view_inv;
    float4x4 cbf_proj_inv;
    float4x4 WorldViewProjection;
    DirectionalLight light;
	float3 eye_pos_w;
    float cbf_near;
    float cbf_far;
};

cbuffer ObjectCBuffer: register(b1)
{
    Material mat;
};