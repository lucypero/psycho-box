// based on shadertoy's shader:
//   https://www.shadertoy.com/view/MsdGWn

#include "_inc_fx_cbuffer.hlsl"

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

VSOutput mainVS(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    output.tex = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.tex * 2.0f + float2(-1.0f, -1.0f), 0.0f, 1.0f);

    output.tex.y = 1.0f - output.tex.y;
    return output;
}

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

float3 mod289(float3 x)
{
    return x - (floor(x * 0.00346020772121846675872802734375f) * 289.0f);
}

float4 mod289(float4 x)
{
    return x - (floor(x * 0.00346020772121846675872802734375f) * 289.0f);
}

float4 permute(float4 x)
{
    float4 param = ((x * 34.0f) + 1.0f.xxxx) * x;
    return mod289(param);
}

float4 taylorInvSqrt(float4 r)
{
    return 1.792842864990234375f.xxxx - (r * 0.8537347316741943359375f);
}

float simplex(float3 v)
{
    float3 i = floor(v + dot(v, 0.3333333432674407958984375f.xxx).xxx);
    float3 x0 = (v - i) + dot(i, 0.16666667163372039794921875f.xxx).xxx;
    float3 g = step(x0.yzx, x0);
    float3 l = 1.0f.xxx - g;
    float3 i1 = min(g, l.zxy);
    float3 i2 = max(g, l.zxy);
    float3 x1 = (x0 - i1) + 0.16666667163372039794921875f.xxx;
    float3 x2 = (x0 - i2) + 0.3333333432674407958984375f.xxx;
    float3 x3 = x0 - 0.5f.xxx;
    float3 param = i;
    i = mod289(param);
    float4 param_1 = i.z.xxxx + float4(0.0f, i1.z, i2.z, 1.0f);
    float4 param_2 = (permute(param_1) + i.y.xxxx) + float4(0.0f, i1.y, i2.y, 1.0f);
    float4 param_3 = (permute(param_2) + i.x.xxxx) + float4(0.0f, i1.x, i2.x, 1.0f);
    float4 p = permute(param_3);
    float n_ = 0.14285714924335479736328125f;
    float3 ns = (float3(2.0f, 0.5f, 1.0f) * n_) - float3(0.0f, 1.0f, 0.0f);
    float4 j = p - (floor((p * ns.z) * ns.z) * 49.0f);
    float4 x_ = floor(j * ns.z);
    float4 y_ = floor(j - (x_ * 7.0f));
    float4 x = (x_ * ns.x) + ns.yyyy;
    float4 y = (y_ * ns.x) + ns.yyyy;
    float4 h = (1.0f.xxxx - abs(x)) - abs(y);
    float4 b0 = float4(x.xy, y.xy);
    float4 b1 = float4(x.zw, y.zw);
    float4 s0 = (floor(b0) * 2.0f) + 1.0f.xxxx;
    float4 s1 = (floor(b1) * 2.0f) + 1.0f.xxxx;
    float4 sh = -step(h, 0.0f.xxxx);
    float4 a0 = b0.xzyw + (s0.xzyw * sh.xxyy);
    float4 a1 = b1.xzyw + (s1.xzyw * sh.zzww);
    float3 p0 = float3(a0.xy, h.x);
    float3 p1 = float3(a0.zw, h.y);
    float3 p2 = float3(a1.xy, h.z);
    float3 p3 = float3(a1.zw, h.w);
    float4 param_4 = float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3));
    float4 norm = taylorInvSqrt(param_4);
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    float4 m = max(0.60000002384185791015625f.xxxx - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0f.xxxx);
    m *= m;
    return 42.0f * dot(m * m, float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float fbm3(float3 v)
{
    float3 param = v;
    float result = simplex(param);
    float3 param_1 = v * 2.0f;
    result += (simplex(param_1) / 2.0f);
    float3 param_2 = v * 4.0f;
    result += (simplex(param_2) / 4.0f);
    result /= 1.75f;
    return result;
}

float fbm5(float3 v)
{
    float3 param = v;
    float result = simplex(param);
    float3 param_1 = v * 2.0f;
    result += (simplex(param_1) / 2.0f);
    float3 param_2 = v * 4.0f;
    result += (simplex(param_2) / 4.0f);
    float3 param_3 = v * 8.0f;
    result += (simplex(param_3) / 8.0f);
    float3 param_4 = v * 16.0f;
    result += (simplex(param_4) / 16.0f);
    result /= 1.9375f;
    return result;
}

float getNoise(inout float3 v)
{
    for (int i = 0; i < 2; i++)
    {
        float3 param = v;
        float3 param_1 = float3(v.xy, v.z + 1000.0f);
        float2 _206 = v.xy + (float2(fbm3(param), fbm3(param_1)) * 0.5f);
        v = float3(_206.x, _206.y, v.z);
    }
    float3 param_2 = v;
    return (fbm5(param_2) / 2.0f) + 0.5f;
}

SPIRV_Cross_Output mainPS(VSOutput input)
{
    float2 fragCoord = input.tex;
    float4 color1 = float4(0.4000000059604644775390625f, 0.4000000059604644775390625f, 0.0f, 1.0f);
    float4 color2 = float4(0.0f, 0.4000000059604644775390625f, 0.4000000059604644775390625f, 1.0f);
    float2 uv = fragCoord;
    float3 param = float3(uv * 2.0f, (cbf_time * 0.300000011920928955078125f) * 0.100000001490116119384765625f);
    float _82 = getNoise(param);
    float _noise = _82;
    _noise = (((_noise * _noise) * _noise) * _noise) * 2.0f;
    float4 colorNow = lerp(color1, color2, sin(cbf_time * 0.5f).xxxx);
    float4 fragColor = (float4(_noise, _noise, _noise, 1.0f) * colorNow) * 1.2999999523162841796875f;
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}