#version 460 core
#extension GL_ARB_bindless_texture : require

const uint MAT_FLAG_ALBEDO_MAP = 1u << 0u;
const float FILL_FULL_EPSILON = 0.99;

layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock {
    uvec2 textureHandles[];
};

in vec2 vBaseUV;
flat in vec4 vAlbedo;
flat in vec3 vEmissive;
flat in float vEmissiveIntensity;
flat in uint vTextureFlags;
flat in int vAlbedoMapIndex;
flat in float vFillAmount;
in float vFillCoord;
in vec2 vFillScrollDirUV;

out vec4 FragColor;

uniform float u_Time;
uniform float u_FillScrollSpeed = 0.2;

void main()
{
    float fillAmount = clamp(vFillAmount, 0.0, 1.0);
    if (fillAmount <= FILL_FULL_EPSILON && vFillCoord > fillAmount) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float dirLenSq = dot(vFillScrollDirUV, vFillScrollDirUV);
    vec2 scrollDir = (dirLenSq > 1e-8) ? (vFillScrollDirUV * inversesqrt(dirLenSq)) : vec2(0.0);
    vec4 albedo = vAlbedo;
    if ((vTextureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && vAlbedoMapIndex >= 0) {
        // Dual-phase scrolling removes visible reset pop from a single fract() wrap.
        float t = u_FillScrollSpeed * u_Time;
        float phaseA = fract(t);
        float phaseB = fract(t + 0.5);
        vec2 uvA = vBaseUV - (scrollDir * phaseA);
        vec2 uvB = vBaseUV - (scrollDir * phaseB);
        float blend = abs(phaseA * 2.0 - 1.0);
        vec4 scrolled = mix(
            texture(sampler2D(textureHandles[vAlbedoMapIndex]), uvA),
            texture(sampler2D(textureHandles[vAlbedoMapIndex]), uvB),
            blend
        );
        albedo *= scrolled;
    }

    vec3 emissive = (vEmissive * vEmissiveIntensity) * albedo.rgb;
    FragColor = vec4(albedo.rgb + emissive, albedo.a);
}
