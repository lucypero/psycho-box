#include "_inc_lighting.hlsl"
#include "_inc_cbuffers.hlsl"

struct VSOutput {
    float4 pos_h : SV_POSITION;
    float3 near_point: NEAR_POINT;
    float3 far_point: FAR_POINT;
    float4x4 view : FRAG_VIEW;
    float4x4 proj : FRAG_PROJ;
};

static const float3 grid_plane[6] = {
    float3(1, 1, 0), float3(-1, -1, 0), float3(-1, 1, 0),
    float3(-1, -1, 0), float3(1, 1, 0), float3(1, -1, 0)
};

float3 UnprojectPoint(float x, float y, float z, float4x4 view_inv, float4x4 projection_inv) {
    float4 unprojectedPoint =  mul(mul(float4(x, y, z, 1.0), projection_inv), view_inv);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

// normal vertice projection
VSOutput mainVS(uint vertex_id: SV_VertexID) {
    VSOutput o;
    float3 p = grid_plane[vertex_id];
    o.near_point = UnprojectPoint(p.x, p.y, 0.0, cbf_view_inv, cbf_proj_inv).xyz; // unprojecting on the near plane
    o.far_point = UnprojectPoint(p.x, p.y, 1.0, cbf_view_inv, cbf_proj_inv).xyz; // unprojecting on the far plane
    o.pos_h = float4(p, 1.0); // using directly the clipped coordinates
    o.view = cbf_view;
    o.proj = cbf_proj;
    return o;
}

float4 grid(float3 fragPos3D, float scale, bool drawAxis) {
    float2 coord = fragPos3D.xz * scale;
    float2 derivative = fwidth(coord);
    float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
    float a_line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    float4 color = float4(0.2, 0.2, 0.2, 1.0 - min(a_line, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.z = 1.0;
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.x = 1.0;
    return color;
}

float computeDepth(float3 pos, float4x4 proj, float4x4 view) {
    float4 clip_space_pos = mul(mul(float4(pos.xyz, 1.0), view), proj);
    return (clip_space_pos.z / clip_space_pos.w);
}

float computeLinearDepth(float3 pos, float4x4 proj, float4x4 view) {
    float4 clip_space_pos = mul(mul(float4(pos.xyz, 1.0), view), proj);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * cbf_near * cbf_far) / (cbf_far + cbf_near - clip_space_depth * (cbf_far - cbf_near)); // get linear value between 0.01 and 100
    return linearDepth / cbf_far; // normalize
}

struct PSOutput {
    float4 color: SV_Target0;
    float depth: SV_Depth;
};

PSOutput mainPS(VSOutput i) {
    float t = -i.near_point.y / (i.far_point.y - i.near_point.y);
    float3 fragPos3D = i.near_point + t * (i.far_point - i.near_point);

    float linearDepth = computeLinearDepth(fragPos3D, i.proj, i.view);
    float fading = max(0, (0.3 - linearDepth));

    float scale = 1; 

    PSOutput o;
    o.color = (grid(fragPos3D, scale * 5, true) + grid(fragPos3D, scale, true)) * float(t > 0);
    o.color.a *= fading;
    o.depth = computeDepth(fragPos3D, i.proj, i.view);
    return o;
}