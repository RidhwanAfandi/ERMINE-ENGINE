/* Start Header ************************************************************************/
/*!
\file       postprocess_fragment.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Post-process fragment shader.
            Reads the HDR lighting result from the G-buffer read set (set 1, binding 0),
            applies bloom, ACES tone mapping, gamma correction, and vignette.

            Push-constant layout (offset 0, 32 bytes — VK_SHADER_STAGE_FRAGMENT_BIT):
              float exposure;       // +0
              float contrast;       // +4
              float saturation;     // +8
              float gamma;          // +12
              float bloomStrength;  // +16
              float bloomThreshold; // +20
              uint  flags;          // +24  bit0=FXAA, bit1=bloom, bit2=tonemap, bit3=gamma, bit4=vignette
              float _pad;           // +28

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) in  vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

// ── Set 1: Input textures (HDR buffer read through G-buffer read set) ──────────
// The HDR lighting result is stored in attachment 0 (gBuffer0 slot of set 1 in PP pass).
layout(set = 1, binding = 0) uniform sampler2D hdrLighting;
layout(set = 1, binding = 4) uniform sampler2D depthBuffer;

// ── Push constants ─────────────────────────────────────────────────────────────
layout(push_constant) uniform PPParams {
    float exposure;
    float contrast;
    float saturation;
    float gamma;
    float bloomStrength;
    float bloomThreshold;
    uint  flags;  // bit0=FXAA, bit1=bloom, bit2=tonemap, bit3=gamma, bit4=vignette
    float _pad;
} pp;

// ── Constants ──────────────────────────────────────────────────────────────────
const uint FLAG_FXAA     = 0x01u;
const uint FLAG_BLOOM    = 0x02u;
const uint FLAG_TONEMAP  = 0x04u;
const uint FLAG_GAMMA    = 0x08u;
const uint FLAG_VIGNETTE = 0x10u;

// ── Tone mapping ───────────────────────────────────────────────────────────────
vec3 acesToneMap(vec3 c)
{
    const float a=2.51, b=0.03, cv=2.43, d=0.59, e=0.14;
    return clamp((c*(a*c+b))/(c*(cv*c+d)+e), 0.0, 1.0);
}

// ── FXAA (Nvidia FXAA 3.11 fast approximation) ────────────────────────────────
vec3 applyFXAA(sampler2D tex, vec2 uv)
{
    vec2 res     = vec2(textureSize(tex, 0));
    vec2 invRes  = 1.0 / res;

    vec3 rgbNW = texture(tex, uv + vec2(-1.0,-1.0) * invRes).rgb;
    vec3 rgbNE = texture(tex, uv + vec2( 1.0,-1.0) * invRes).rgb;
    vec3 rgbSW = texture(tex, uv + vec2(-1.0, 1.0) * invRes).rgb;
    vec3 rgbSE = texture(tex, uv + vec2( 1.0, 1.0) * invRes).rgb;
    vec3 rgbM  = texture(tex, uv).rgb;

    const vec3 toLuma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, toLuma);
    float lumaNE = dot(rgbNE, toLuma);
    float lumaSW = dot(rgbSW, toLuma);
    float lumaSE = dot(rgbSE, toLuma);
    float lumaM  = dot(rgbM,  toLuma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    const float FXAA_REDUCE_MIN   = 1.0/128.0;
    const float FXAA_REDUCE_MUL   = 1.0/8.0;
    const float FXAA_SPAN_MAX     = 8.0;
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * FXAA_REDUCE_MUL, FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * invRes;

    vec3 rgbA = 0.5 * (texture(tex, uv + dir * (1.0/3.0 - 0.5)).rgb +
                        texture(tex, uv + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = 0.5 * rgbA + 0.25 * (texture(tex, uv + dir * -0.5).rgb +
                                       texture(tex, uv + dir *  0.5).rgb);

    float lumaB = dot(rgbB, toLuma);
    return (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
}

// ── Bloom (simple brightness threshold + box blur) ─────────────────────────────
vec3 sampleBloom(sampler2D tex, vec2 uv, float threshold)
{
    vec2 invRes = 1.0 / vec2(textureSize(tex, 0));
    vec3 bloom  = vec3(0.0);
    const float RADIUS = 3.0;
    float weight = 0.0;
    for (float x = -RADIUS; x <= RADIUS; x += 1.0)
    for (float y = -RADIUS; y <= RADIUS; y += 1.0)
    {
        vec3 s = texture(tex, uv + vec2(x, y) * invRes).rgb;
        float brightness = max(s.r, max(s.g, s.b));
        float w = max(brightness - threshold, 0.0);
        bloom  += s * w;
        weight += w;
    }
    return (weight > 0.0) ? bloom / weight : vec3(0.0);
}

// ── Vignette ───────────────────────────────────────────────────────────────────
vec3 vignette(vec3 c, vec2 uv)
{
    vec2 d = uv * 2.0 - 1.0;
    float v = smoothstep(1.0, 0.5, dot(d, d) * 0.5);
    return c * v;
}

// ── Contrast / Saturation ──────────────────────────────────────────────────────
vec3 adjustContrast(vec3 c, float con) { return (c - 0.5) * con + 0.5; }

vec3 adjustSaturation(vec3 c, float sat)
{
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), c, sat);
}

// ── main ──────────────────────────────────────────────────────────────────────
void main()
{
    vec3 color;

    if ((pp.flags & FLAG_FXAA) != 0u)
        color = applyFXAA(hdrLighting, vTexCoord);
    else
        color = texture(hdrLighting, vTexCoord).rgb;

    if ((pp.flags & FLAG_BLOOM) != 0u)
        color += sampleBloom(hdrLighting, vTexCoord, pp.bloomThreshold) * pp.bloomStrength;

    float depth  = texture(depthBuffer, vTexCoord).r;
    bool  isSky  = (depth >= 1.0);

    if (!isSky)
    {
        if ((pp.flags & FLAG_TONEMAP) != 0u)
            color = acesToneMap(color * pp.exposure);

        if ((pp.flags & FLAG_GAMMA) != 0u)
            color = pow(max(color, vec3(0.0)), vec3(1.0 / max(pp.gamma, 0.001)));
    }

    color = adjustContrast  (color, pp.contrast);
    color = adjustSaturation(color, pp.saturation);

    if ((pp.flags & FLAG_VIGNETTE) != 0u)
        color = vignette(color, vTexCoord);

    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
