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

Texture2D tex_frame : register (t0);
SamplerState the_sampler : register(s0);

float random(float2 st)
{
    return frac(sin(dot(st, float2(12.98980045318603515625f, 78.233001708984375f))) * 43758.546875f);
}

float2 curve(float2 uv) {
    uv = (uv - 0.5) * 2.0;
    uv *= 1.1;
    uv.x *= 1.0 + pow((abs(uv.y) / 10.0), 2.0);
    uv.y *= 1.0 + pow((abs(uv.x) / 10.0), 2.0);
    uv = (uv / 2.0) + 0.5;
    uv = uv * 0.92 + 0.04;

    return uv;
}

float myPow(float base, float exponent) {
    float the_sign = sign(base);
    float result = pow(abs(base), exponent);
    return result * the_sign;
}

float4 mainPS(VSOutput input): SV_Target
{
    float2 uv = input.tex;
    uv = curve(uv);
    float density = 1.2999999523162841796875f;
    float opacityScanline = 0.300000011920928955078125f;
    float opacityNoise = 0.20000000298023223876953125f;
    float flickering = 0.02999999932944774627685546875f;


    // chromatic aberration
    float2 aberrated = 0.001;

    float3 col = float3(
        tex_frame.Sample(the_sampler, uv - aberrated).x,
        tex_frame.Sample(the_sampler, uv).y,
        tex_frame.Sample(the_sampler, uv + aberrated).z
    );

    // vignette
    float vig = (0.0f + 1.0f*16.0f*uv.x*uv.y*(1.0f-uv.x)*(1.0f-uv.y));

    float the_pow = myPow(vig, 0.2);
    col *= float3(the_pow, the_pow, the_pow);
    // col *= float3(0.98, 1.02, 0.98);
    // col *= 1.8;

    // scanlines

    float count = cbf_res.y * density;
    float2 sl = float2(sin(uv.y * count), cos(uv.y * count));
    float3 scanlines = float3(sl.x, sl.y, sl.x);
    col += ((col * scanlines) * opacityScanline);
    float2 param = uv * cbf_time;
    col += ((col * random(param).xxx) * opacityNoise);
    col += ((col * sin(110.0f * cbf_time)) * flickering);

    if ((uv.x < 0.0004 && uv.y < 0.008) || (uv.x > (1.0 - 0.0004) && uv.y < 0.008)) {
        col = float3(0.0, 0.0, 0.0);
    }

    if ((uv.x < 0.0004 && uv.y > (1.0 - 0.008)) || (uv.x > (1.0 - 0.0004) && uv.y > (1.0 - 0.008))) {
        col = float3(0.0, 0.0, 0.0);
    }

    return float4(col, 1.0f);
}
