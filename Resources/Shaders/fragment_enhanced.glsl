#version 460
#extension GL_ARB_bindless_texture : require

const int MAX_LIGHTS = 32;
const int NUM_CASCADES = 4;

// Material texture flag bits (must match C++ MaterialTextureFlags enum)
const uint MAT_FLAG_ALBEDO_MAP    = 1u << 0u;  // bit 0
const uint MAT_FLAG_NORMAL_MAP    = 1u << 1u;  // bit 1
const uint MAT_FLAG_ROUGHNESS_MAP = 1u << 2u;  // bit 2
const uint MAT_FLAG_METALLIC_MAP  = 1u << 3u;  // bit 3
const uint MAT_FLAG_AO_MAP        = 1u << 4u;  // bit 4
const uint MAT_FLAG_EMISSIVE_MAP  = 1u << 5u;  // bit 5

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 ViewPos;
in vec3 Tangent;
in vec3 Bitangent;
flat in uint vMaterialIndex;

out vec4 FragColor;

// Material structure (112 bytes, matches C++ MaterialSSBO)
struct MaterialData {
    vec4 albedo;                // 16 bytes (0-15)
    float metallic;             // 4 bytes (16-19)
    float roughness;            // 4 bytes (20-23)
    float ao;                   // 4 bytes (24-27)
    float normalStrength;       // 4 bytes (28-31)

    vec3 emissive;              // 12 bytes (32-43)
    float emissiveIntensity;    // 4 bytes (44-47)

    int shadingModel;           // 4 bytes (48-51) - 0 = PBR, 1 = Blinn-Phong
    uint textureFlags;          // 4 bytes (52-55) - Packed bitfield for all texture flags
    float _pad0;                // 4 bytes (56-59)
    float _pad1;                // 4 bytes (60-63)

    vec2 uvScale;               // 8 bytes (64-71) - UV scale for texture tiling
    vec2 uvOffset;              // 8 bytes (72-79) - UV offset for texture positioning

    // Texture Array Indices
    int albedoMapIndex;         // 4 bytes (80-83)
    int normalMapIndex;         // 4 bytes (84-87)
    int roughnessMapIndex;      // 4 bytes (88-91)
    int metallicMapIndex;       // 4 bytes (92-95)

    int aoMapIndex;             // 4 bytes (96-99)
    int emissiveMapIndex;       // 4 bytes (100-103)
    int _pad2;                  // 4 bytes (104-107)
    int _pad3;                  // 4 bytes (108-111)
    // Total: 112 bytes
};

// Material SSBO block - array of materials
layout(std430, binding = 3) restrict readonly buffer MaterialBlock {
    MaterialData materials[];
};

// Bindless texture array SSBO - stores texture handles as uvec2 (64-bit split into two 32-bit values)
layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock
{
    uvec2 textureHandles[];
};

// Shading mode toggle
uniform bool isBlinnPhong;
uniform mat4 view;

// Material properties for Blinn-Phong
uniform vec3 materialKa = vec3(0.2, 0.2, 0.2);
uniform vec3 materialKd = vec3(0.8, 0.8, 0.8);
uniform vec3 materialKs = vec3(1.0, 1.0, 1.0);
uniform vec3 materialKe = vec3(0.0, 0.0, 0.0);
uniform float materialShininess = 64.0;

// Light structure
struct Light {
    vec4 position_type;    // xyz = position (view space), w = light type
    vec4 color_intensity;  // xyz = color, w = intensity
    vec4 direction_range;  // xyz = direction (view space), w = range
    vec4 spot_angles_castshadows_startOffset; // x = inner angle (cos), y = outer angle (cos), z = flags bitfield (bit 0: castsShadows, bit 1: castsRays), w = shadow base layer or -1 if no shadows
    mat4 lightSpaceMatrix[NUM_CASCADES]; // Light view-projection matrices for cascaded shadow maps
    mat4 pointLightMatrices[6]; // Point light shadow matrices for cubemap faces
    vec4 splitDepths[(NUM_CASCADES + 3) / 4]; // Split depths for cascaded shadow maps
};

layout (std140, binding = 1) uniform LightsUBO {
    vec4 lightCount;
    Light lights[MAX_LIGHTS]; // Fixed size array
};

const float PI = 3.14159265359;
const int POINT_LIGHT = 0;
const int DIRECTIONAL_LIGHT = 1;
const int SPOT_LIGHT = 2;

// Light flag bit positions
const int LIGHT_FLAG_CASTS_SHADOWS = 1;  // bit 0
const int LIGHT_FLAG_CASTS_RAYS = 2;     // bit 1

// Helper functions to extract light flags
bool lightCastsShadows(Light light) {
    return (int(light.spot_angles_castshadows_startOffset.z) & LIGHT_FLAG_CASTS_SHADOWS) != 0;
}

bool lightCastsRays(Light light) {
    return (int(light.spot_angles_castshadows_startOffset.z) & LIGHT_FLAG_CASTS_RAYS) != 0;
}

// Normal mapping function
vec3 calculateNormal(MaterialData material, vec2 uv)
{
    vec3 normal = normalize(Normal);

    if ((material.textureFlags & MAT_FLAG_NORMAL_MAP) != 0u && material.normalMapIndex >= 0) {
        vec3 normalMap = texture(sampler2D(textureHandles[material.normalMapIndex]), uv).rgb * 2.0 - 1.0;
        normalMap.xy *= material.normalStrength;

        vec3 T = normalize(Tangent);
        vec3 B = normalize(Bitangent);
        vec3 N = normal;
        mat3 TBN = mat3(T, B, N);

        normal = normalize(TBN * normalMap);
    }

    return normal;
}

// Sample material properties
vec3 getAlbedo(MaterialData material, vec2 uv)
{
    vec3 albedo = material.albedo.rgb;

    if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
        vec4 texColor = texture(sampler2D(textureHandles[material.albedoMapIndex]), uv);
        albedo *= texColor.rgb;
    }

    return albedo;
}

float getRoughness(MaterialData material, vec2 uv)
{
    float roughness = material.roughness;
    if ((material.textureFlags & MAT_FLAG_ROUGHNESS_MAP) != 0u && material.roughnessMapIndex >= 0) {
        roughness *= texture(sampler2D(textureHandles[material.roughnessMapIndex]), uv).r;
    }
    return clamp(roughness, 0.05, 1.0);
}

float getMetallic(MaterialData material, vec2 uv)
{
    float metallic = material.metallic;
    if ((material.textureFlags & MAT_FLAG_METALLIC_MAP) != 0u && material.metallicMapIndex >= 0) {
        metallic *= texture(sampler2D(textureHandles[material.metallicMapIndex]), uv).r;
    }
    return clamp(metallic, 0.0, 1.0);
}

float getAO(MaterialData material, vec2 uv)
{
    float ao = material.ao;
    if ((material.textureFlags & MAT_FLAG_AO_MAP) != 0u && material.aoMapIndex >= 0) {
        ao *= texture(sampler2D(textureHandles[material.aoMapIndex]), uv).r;
    }
    return ao;
}

vec3 getEmissive(MaterialData material, vec2 uv)
{
    vec3 emissive = material.emissive * material.emissiveIntensity;
    if ((material.textureFlags & MAT_FLAG_EMISSIVE_MAP) != 0u && material.emissiveMapIndex >= 0) {
        vec4 emissiveTexel = texture(sampler2D(textureHandles[material.emissiveMapIndex]), uv);
        emissive *= emissiveTexel.rgb;
    }
    return emissive;
}

// PBR Functions
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Calculate light attenuation
float calculateAttenuation(int lightIndex, vec3 fragPosView, out vec3 lightDir)
{
    int lightType = int(lights[lightIndex].position_type.w);
    float range = lights[lightIndex].direction_range.w;
    
    float attenuation = 1.0;
    
    if (lightType == DIRECTIONAL_LIGHT) {
        vec3 dirWorld = lights[lightIndex].direction_range.xyz;
        vec4 dirView4 = view * vec4(dirWorld, 0.0);
        lightDir = normalize(dirView4.xyz);
        attenuation = 1.0;
    } else {
        vec3 lightPosWorld = lights[lightIndex].position_type.xyz;
        vec4 lightPosView4 = view * vec4(lightPosWorld, 1.0);
        vec3 lightPosView = lightPosView4.xyz / lightPosView4.w;
        lightDir = normalize(lightPosView - fragPosView);
        float distance = length(lightPosView - fragPosView);
        
        float linearTerm = 0.045;
        float quadraticTerm = 0.0075;
        attenuation = 1.0 / (1.0 + linearTerm * distance + quadraticTerm * distance * distance);
        
        if (distance > range) {
            float fadeDistance = range * 0.1;
            float fadeStart = range - fadeDistance;
            if (distance > fadeStart) {
                float fadeFactor = 1.0 - (distance - fadeStart) / fadeDistance;
                attenuation *= max(fadeFactor, 0.0);
            } else {
                attenuation = 0.0;
            }
        }
        
        if (lightType == SPOT_LIGHT) {
            vec3 spotDirWorld = lights[lightIndex].direction_range.xyz;
            vec4 spotDirView4 = view * vec4(spotDirWorld, 0.0);
            vec3 spotDir = normalize(spotDirView4.xyz);
            float cosAngle = dot(-lightDir, spotDir);
            float innerCos = lights[lightIndex].spot_angles_castshadows_startOffset.x;
            float outerCos = lights[lightIndex].spot_angles_castshadows_startOffset.y;

            float spotFactor = clamp((cosAngle - outerCos) / (innerCos - outerCos), 0.0, 1.0);
            attenuation *= spotFactor;
        }
    }
    
    return attenuation;
}

// Blinn-Phong lighting calculation for one light
vec3 calculateBlinnPhong(int lightIndex, vec3 normal, vec3 viewDir, vec3 fragPosView, vec3 albedo)
{
    vec3 lightDir;
    float attenuation = calculateAttenuation(lightIndex, fragPosView, lightDir);
    
    if (attenuation <= 0.0) return vec3(0.0);
    
    vec3 lightColor = lights[lightIndex].color_intensity.xyz * lights[lightIndex].color_intensity.w;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * materialKd * albedo;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec;
    if (materialShininess <= 0.0) {
        spec = 0.0;
    } else {
        spec = pow(max(dot(normal, halfwayDir), 0.0), materialShininess);
    }
    vec3 specular = spec * lightColor * materialKs;
    
    return (diffuse + specular) * attenuation;
}

// PBR lighting calculation for one light
vec3 calculatePBR(int lightIndex, vec3 normal, vec3 viewDir, vec3 fragPosView, vec3 albedo, vec3 F0, float roughness, float metallic)
{
    vec3 lightDir;
    float attenuation = calculateAttenuation(lightIndex, fragPosView, lightDir);
    
    if (attenuation <= 0.0) return vec3(0.0);
    
    vec3 lightColor = lights[lightIndex].color_intensity.xyz * lights[lightIndex].color_intensity.w;
    vec3 radiance = lightColor * attenuation;
    
    vec3 H = normalize(viewDir + lightDir);
    
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(normal, lightDir), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main()
{
    // Get the material for this draw call from the array
    MaterialData material = materials[vMaterialIndex];

    // Apply UV transform (scale and offset)
    vec2 transformedUV = TexCoord * material.uvScale + material.uvOffset;

    // Early-out optimization for untextured materials
    // If no textures, use base material properties directly (skip function calls and texture sampling)
    vec3 norm;
    vec3 albedo;
    float roughness;
    float metallic;
    float ao;
    vec3 emissive;

    if (material.textureFlags == 0u) {
        // Fast path: no textures, use base material properties
        norm = normalize(Normal);
        albedo = material.albedo.rgb;
        roughness = clamp(material.roughness, 0.05, 1.0);
        metallic = clamp(material.metallic, 0.0, 1.0);
        ao = material.ao;
        emissive = material.emissive * material.emissiveIntensity;
    } else {
        // Standard path: sample textures and calculate properties
        norm = calculateNormal(material, transformedUV);
        albedo = getAlbedo(material, transformedUV);
        roughness = getRoughness(material, transformedUV);
        metallic = getMetallic(material, transformedUV);
        ao = getAO(material, transformedUV);
        emissive = getEmissive(material, transformedUV);
    }

    vec3 fragPosView = ViewPos;
    vec3 normalView = normalize(mat3(view) * norm);
    vec3 viewDir = normalize(-fragPosView);
    
    vec3 result = vec3(0.0);
    int numLights = int(lightCount.x);
    
    // Choose shading model based on material settings or global toggle
    bool useBlinnPhong = isBlinnPhong || (material.shadingModel == 1);
    
    if (useBlinnPhong) {
        // Ambient component
        vec3 ambient = materialKa * 0.1 * albedo * ao;
        result += ambient;

        // Add contribution from each light
        for (int i = 0; i < numLights && i < 16; ++i) {
            result += calculateBlinnPhong(i, normalView, viewDir, fragPosView, albedo);
        }
        
        // Add emissive
        result += emissive;
        result += materialKe;
    } else {
        // PBR Lighting
        vec3 F0 = vec3(0.04); // Default dielectric F0
        F0 = mix(F0, albedo, metallic);
        
        // Simple ambient lighting
        vec3 ambient = vec3(0.03) * albedo * ao;
        result += ambient;

        // Add contribution from each light
        for (int i = 0; i < numLights && i < 16; ++i) {
            result += calculatePBR(i, normalView, viewDir, fragPosView, albedo, F0, roughness, metallic);
        }
        
        // Energy compensation for very rough surfaces
        if (roughness > 0.7) {
            result *= mix(1.0, 1.4, (roughness - 0.7) / 0.3);
        }
        
        result += emissive;
    }
    
    // Tone mapping (ACES approximation)
    vec3 a = 2.51 * result;
    vec3 b = 0.03 + result;
    vec3 c = 2.43 * result + 0.59;
    vec3 d = 0.14 + result;
    result = clamp((a * b) / (c * d), 0.0, 1.0);
    
    // Use alpha directly from albedo
    float alpha = material.albedo.a;
    FragColor = vec4(result, alpha);
}
