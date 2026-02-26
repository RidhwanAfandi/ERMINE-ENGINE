#version 460 core
#extension GL_ARB_bindless_texture : require

// Input from vertex shader
in vec2 TexCoord;
in vec3 WorldPos;
in vec3 WorldNormal;
in vec3 ViewPos;
in vec3 ViewNormal;
in vec3 ViewTangent;
in vec3 ViewBitangent;
flat in uint vMaterialIndex; // Material index from vertex shader
flat in uint vDrawFlags;     // DrawInfo flags from vertex shader

// G-Buffer outputs
layout(location = 0) out vec3 gBuffer0; // RT0: Albedo
layout(location = 1) out vec3 gBuffer1; // RT1: Normal
layout(location = 2) out vec4 gBuffer2; // RT2: Emissive
layout(location = 3) out vec4 gBuffer3; // RT3: Material

// Material texture flag bits (must match C++ MaterialTextureFlags enum)
const uint MAT_FLAG_ALBEDO_MAP    = 1u << 0u;  // bit 0
const uint MAT_FLAG_NORMAL_MAP    = 1u << 1u;  // bit 1
const uint MAT_FLAG_ROUGHNESS_MAP = 1u << 2u;  // bit 2
const uint MAT_FLAG_METALLIC_MAP  = 1u << 3u;  // bit 3
const uint MAT_FLAG_AO_MAP        = 1u << 4u;  // bit 4
const uint MAT_FLAG_EMISSIVE_MAP  = 1u << 5u;  // bit 5

// DrawInfo flag bits (must match C++ DrawCommands.h)
const uint FLAG_SKINNING = 1u << 0u;        // bit 0: Skeletal animation
const uint FLAG_CAMERA_ATTACHED = 1u << 1u; // bit 1: Camera-attached (no motion blur)

// NO MATERIAL SSBO IN FRAGMENT SHADER - materials are fetched in vertex shader and passed via varyings!

// Bindless texture array SSBO - stores texture handles as uvec2 (64-bit split into two 32-bit values)
layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock
{
    uvec2 textureHandles[];
};

// Pack albedo RGB into RT0 (RGB16F format)
vec3 packAlbedo(vec3 albedo) {
    // Direct storage - RGB16F provides sufficient precision for albedo
    return clamp(albedo, 0.0, 65504.0); // Clamp to half-float range
}

// Pack normal XYZ into RT1 (RGB16F format)
vec3 packNormal(vec3 normal) {
    // Normalize and convert from [-1,1] to [0,1] range for storage
    vec3 packedNormal = normalize(normal) * 0.5 + 0.5;
    return clamp(packedNormal, 0.0, 1.0);
}

// Pack emissive RGB + intensity into RT2 (RGBA8 format)
vec4 packEmissive(vec3 emissive, float emissiveIntensity) {
    if (emissiveIntensity < 0.001) {
        return vec4(0.0, 0.0, 0.0, 0.0); // Black emissive
    }

    float clampedIntensity = clamp(emissiveIntensity, 0.0, 255.0);

    return vec4(clamp(emissive, 0.0, 1.0), clampedIntensity / 255.0);
}

// Pack material properties into RT3 (RGBA8 format)
vec4 packMaterialProperties(float roughness, float metallic, float ao, float motionBlurFlag) {
    return vec4(
        clamp(roughness, 0.0, 1.0),
        clamp(metallic, 0.0, 1.0),
        clamp(ao, 0.0, 1.0),
        motionBlurFlag  // 0.0 = apply motion blur, 1.0 = skip motion blur
    );
}

// Main G-Buffer output function (call this in geometry fragment shader)
void writeGBuffer(vec3 albedo, vec3 normal, vec3 emissive, float emissiveIntensity,
                  float roughness, float metallic, float ao, float motionBlurFlag) {
    // Output to multiple render targets
    gBuffer0 = packAlbedo(albedo);                                           // RT0: RGB16F
    gBuffer1 = packNormal(normal);                                           // RT1: RGB16F
    gBuffer2 = packEmissive(emissive, emissiveIntensity);                   // RT2: RGBA8
    gBuffer3 = packMaterialProperties(roughness, metallic, ao, motionBlurFlag); // RT3: RGBA8
}

// ========== MATERIAL DATA PASSED FROM VERTEX SHADER ==========
// All material properties are fetched ONCE per vertex (not per fragment!)
// This eliminates millions of SSBO accesses per frame
flat in vec4 vAlbedo;              // rgb=albedo, a=metallic
flat in vec4 vRoughnessAoEmissive; // r=roughness, g=ao, b=emissiveIntensity, a=normalStrength
flat in vec3 vEmissive;
flat in uint vTextureFlags;
flat in ivec4 vTextureIndices;     // albedo, normal, roughness, metallic
flat in ivec2 vTextureIndices2;    // ao, emissive

// ========== OPTIMIZATIONS: PRE-COMPUTED IN VERTEX SHADER ==========
in vec2 vTransformedUV;            // UV with scale/offset already applied
flat in float vMotionBlurFlag;     // Pre-calculated motion blur flag

void main()
{
    // Unpack material data from varyings (NO SSBO ACCESS!)
    vec3 albedo = vAlbedo.rgb;
    float metallic = vAlbedo.a;
    float roughness = vRoughnessAoEmissive.r;
    float ao = vRoughnessAoEmissive.g;
    float emissiveIntensity = vRoughnessAoEmissive.b;
    float normalStrength = vRoughnessAoEmissive.a;

    // Use pre-calculated motion blur flag from vertex shader (NO per-fragment calculation!)
    float motionBlurFlag = vMotionBlurFlag;

    // Check if material uses any textures (early-out optimization for procedural materials)
    // Fast path: No textures, pure procedural material
    if (vTextureFlags == 0u) {
        writeGBuffer(albedo, ViewNormal, vEmissive,
                     emissiveIntensity, roughness,
                     metallic, ao, motionBlurFlag);
        return;
    }

    // ========== BATCH TEXTURE SAMPLES ==========
    // Use pre-transformed UV from vertex shader (NO per-fragment calculation!)
    // Sample all textures first to hide latency and improve texture cache usage
    vec3 albedoSample = ((vTextureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && vTextureIndices.x >= 0)
        ? texture(sampler2D(textureHandles[vTextureIndices.x]), vTransformedUV).rgb
        : vec3(1.0);

    vec3 normalSample = ((vTextureFlags & MAT_FLAG_NORMAL_MAP) != 0u && vTextureIndices.y >= 0)
        ? texture(sampler2D(textureHandles[vTextureIndices.y]), vTransformedUV).rgb
        : vec3(0.5, 0.5, 1.0);

    float roughnessSample = ((vTextureFlags & MAT_FLAG_ROUGHNESS_MAP) != 0u && vTextureIndices.z >= 0)
        ? texture(sampler2D(textureHandles[vTextureIndices.z]), vTransformedUV).r
        : 1.0;

    float metallicSample = ((vTextureFlags & MAT_FLAG_METALLIC_MAP) != 0u && vTextureIndices.w >= 0)
        ? texture(sampler2D(textureHandles[vTextureIndices.w]), vTransformedUV).r
        : 1.0;

    float aoSample = ((vTextureFlags & MAT_FLAG_AO_MAP) != 0u && vTextureIndices2.x >= 0)
        ? texture(sampler2D(textureHandles[vTextureIndices2.x]), vTransformedUV).r
        : 1.0;

    vec3 emissiveSample = ((vTextureFlags & MAT_FLAG_EMISSIVE_MAP) != 0u && vTextureIndices2.y >= 0)
        ? texture(sampler2D(textureHandles[vTextureIndices2.y]), vTransformedUV).rgb
        : vec3(1.0);

    // ========== PROCESS SAMPLES ==========
    // Albedo
    vec3 finalAlbedo = albedo * albedoSample;

    // Normal - optimized transformation
    vec3 finalNormal = ViewNormal;
    if ((vTextureFlags & MAT_FLAG_NORMAL_MAP) != 0u && vTextureIndices.y >= 0) {
        // Decode normal map using fma for efficiency
        vec3 tangentNormal = fma(normalSample, vec3(2.0), vec3(-1.0));
        tangentNormal.xy *= normalStrength;

        // Normalize only if normal strength modified the vector significantly
        if (abs(normalStrength - 1.0) > 0.01) {
            tangentNormal = normalize(tangentNormal);
        }

        // Transform to view space using explicit vector operations (faster than matrix multiply)
        finalNormal = normalize(ViewTangent * tangentNormal.x +
                               ViewBitangent * tangentNormal.y +
                               ViewNormal * tangentNormal.z);
    }

    // Material properties
    float finalRoughness = roughness * roughnessSample;
    float finalMetallic = metallic * metallicSample;
    float finalAO = ao * aoSample;

    // Emissive
    vec3 finalEmissive = vEmissive * emissiveSample;
    float finalEmissiveIntensity = emissiveIntensity;

    // Write to G-Buffer
    writeGBuffer(finalAlbedo, finalNormal, finalEmissive, finalEmissiveIntensity,
                 finalRoughness, finalMetallic, finalAO, motionBlurFlag);
}
