#version 460
#extension GL_ARB_bindless_texture : require

// Material texture flag bits
const uint MAT_FLAG_ALBEDO_MAP    = 1u << 0u;
const uint MAT_FLAG_NORMAL_MAP    = 1u << 1u;
const uint MAT_FLAG_ROUGHNESS_MAP = 1u << 2u;
const uint MAT_FLAG_METALLIC_MAP  = 1u << 3u;
const uint MAT_FLAG_AO_MAP        = 1u << 4u;
const uint MAT_FLAG_EMISSIVE_MAP  = 1u << 5u;

// Inputs from vertex shader
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 ViewPos;
in vec3 Tangent;
in vec3 Bitangent;
flat in uint vMaterialIndex;

out vec4 FragColor;

// Material structure (112 bytes, matches C++ MaterialSSBO exactly)
struct MaterialData {
    vec4 albedo;                // 16 bytes (0-15)
    float metallic;             // 4 bytes (16-19)
    float roughness;            // 4 bytes (20-23)
    float ao;                   // 4 bytes (24-27)
    float normalStrength;       // 4 bytes (28-31)

    vec3 emissive;              // 12 bytes (32-43)
    float emissiveIntensity;    // 4 bytes (44-47)

    int shadingModel;           // 4 bytes (48-51)
    uint textureFlags;          // 4 bytes (52-55)
    int castsShadows;           // 4 bytes (56-59) - Kept for CPU/GPU layout parity
    float fillAmount;           // 4 bytes (60-63)

    vec2 uvScale;               // 8 bytes (64-71)
    vec2 uvOffset;              // 8 bytes (72-79)

    // Texture Array Indices
    int albedoMapIndex;         // 4 bytes (80-83)
    int normalMapIndex;         // 4 bytes (84-87)
    int roughnessMapIndex;      // 4 bytes (88-91)
    int metallicMapIndex;       // 4 bytes (92-95)

    int aoMapIndex;             // 4 bytes (96-99)
    int emissiveMapIndex;       // 4 bytes (100-103)
    float fillDirOctX;          // 4 bytes (104-107)
    float fillDirOctY;          // 4 bytes (108-111)
};

// Material SSBO
layout(std430, binding = 3) restrict readonly buffer MaterialBlock {
    MaterialData materials[];
};

// Bindless texture array SSBO
layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock {
    uvec2 textureHandles[];
};

// Uniforms
uniform float u_Time;
uniform vec2 u_ScrollSpeed = vec2(0.5, 0.0);  // UV scroll speed (units per second)

void main()
{
    MaterialData material = materials[vMaterialIndex];

    // Apply material UV scale and offset, then scroll
    vec2 uv = TexCoord * material.uvScale + material.uvOffset;
    uv += u_ScrollSpeed * u_Time;

    // Sample albedo
    vec4 albedo = material.albedo;
    if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
        uvec2 handle = textureHandles[material.albedoMapIndex];
        sampler2D albedoMap = sampler2D(handle);
        albedo *= texture(albedoMap, uv);
    }

    // Simple lighting
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normal, lightDir), 0.0);

    vec3 ambient = albedo.rgb * 0.3;
    vec3 diffuse = albedo.rgb * diff * 0.7;

    // Emissive (also scrolls)
    vec3 emissive = vec3(0.0);
    if ((material.textureFlags & MAT_FLAG_EMISSIVE_MAP) != 0u && material.emissiveMapIndex >= 0) {
        uvec2 handle = textureHandles[material.emissiveMapIndex];
        sampler2D emissiveMap = sampler2D(handle);
        emissive = texture(emissiveMap, uv).rgb * material.emissiveIntensity;
    } else {
        emissive = material.emissive * material.emissiveIntensity;
    }

    FragColor = vec4(ambient + diffuse + emissive, albedo.a);
}
