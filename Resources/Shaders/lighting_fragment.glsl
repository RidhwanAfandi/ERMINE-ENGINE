#version 460 core
#extension GL_ARB_bindless_texture : require

const int NUM_CASCADES = 4;

in vec2 TexCoord;
out vec4 FragColor;

// Bindless texture handles
uniform uvec2 u_GBuffer0Handle;
uniform uvec2 u_GBuffer1Handle;
uniform uvec2 u_GBuffer2Handle;
uniform uvec2 u_GBuffer3Handle;
uniform uvec2 u_GBufferDepthHandle;
uniform uvec2 u_ShadowMapArrayHandle;
uniform uvec2 u_IGNHandle;
uniform vec2 u_IGNResolution;

// Matrices for position reconstruction
uniform mat4 view;
uniform mat4 invView;
uniform mat4 invProjection;
uniform mat4 projection;

// Shading mode
uniform int u_ShadingMode; // 0 = PBR, 1 = Blinn-Phong

// Ambient lighting parameters
uniform vec3 u_AmbientColor = vec3(1.0, 1.0, 1.0);  // RGB color of ambient light
uniform float u_AmbientIntensity = 0.08;  // Intensity multiplier

// SSAO Parameters
uniform int u_SSAO = 1;
uniform int u_SSAOSamples = 16;
uniform float u_SSAORadius = 10.0;
uniform float u_SSAOBias = 0.01;
uniform float u_SSAOIntensity = 1.0;
uniform float u_SSAOFadeout = 0.1;
uniform float u_SSAOMaxDistance = 1000.0;

// Fog Parameters
uniform int u_FogEnabled = 0;           // 0 = disabled, 1 = enabled
uniform int u_FogMode = 0;              // 0 = linear, 1 = exponential, 2 = exponential squared
uniform vec3 u_FogColor = vec3(0.5, 0.6, 0.7);
uniform float u_FogDensity = 0.02;      // For exponential fog
uniform float u_FogStart = 50.0;        // For linear fog
uniform float u_FogEnd = 200.0;         // For linear fog
uniform float u_FogHeightCoefficient = 0.0;  // Height influence on fog density
uniform float u_FogHeightFalloff = 10.0;     // Rate of fog density falloff with height


// Light structure
struct Light {
    vec4 position_type;    // xyz = position (world space), w = light type
    vec4 color_intensity;  // xyz = color, w = intensity
    vec4 direction_range;  // xyz = direction (world space), w = range
    vec4 spot_angles_castshadows_startOffset; // x = inner angle (cos), y = outer angle (cos), z = flags bitfield (bit 0: castsShadows, bit 1: castsRays), w = shadow base layer or -1 if no shadows
    mat4 lightSpaceMatrix[NUM_CASCADES]; // Light view-projection matrices for cascaded shadow maps
    mat4 pointLightMatrices[6]; // Point light shadow matrices for cubemap faces
    vec4 splitDepths[(NUM_CASCADES + 3) / 4]; // Split depths for cascaded shadow maps
};

layout (std430, binding = 4) readonly buffer LightsSSBO { // Matches LIGHT_SSBO_BINDING in SSBO_Bindings.h
    vec4 lightCount;
    Light lights[];
};

// Light Probe structure
const int MAX_PROBES = 128;

struct LightProbe {
    vec4 position_radius;      // xyz = world position, w = influence radius
    vec4 shCoefficients[9];    // SH L2 coefficients (vec3 stored in xyz, w unused)
    vec4 boundsMin;            // xyz = world bounds min, w = padding
    vec4 boundsMax;            // xyz = world bounds max, w = padding
    vec4 flags;                // x = isActive (1.0 or 0.0), y = priority, zw = padding
};

layout (std140, binding = 5) uniform LightProbesUBO {
    vec4 probeCount;           // x = number of active probes, yzw = unused
    LightProbe probes[MAX_PROBES];
};

// Light probe toggle
uniform int u_LightProbesEnabled = 1;

// Constants
const float PI = 3.14159265359;
const float HALF_PI = PI * 0.5;
const uint SECTOR_COUNT = 32u;
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

int getShadowBaseLayer(Light light) {
    return int(light.spot_angles_castshadows_startOffset.w);
}

int getPointLightFaceIndex(vec3 dir) {
    vec3 absDir = abs(dir);
    if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
        return dir.x >= 0.0 ? 0 : 1;
    }
    if (absDir.y >= absDir.x && absDir.y >= absDir.z) {
        return dir.y >= 0.0 ? 2 : 3;
    }
    return dir.z >= 0.0 ? 4 : 5;
}

vec3 getViewPosition(vec2 texCoord, float depth) {
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

// Reconstruct world position from depth
vec3 reconstructWorldPosition(vec2 texCoord, float depth)
{
    // Convert to NDC
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);

    // Transform to view space
    vec4 viewPos = invProjection * ndc;
    viewPos /= viewPos.w;

    // Transform to world space
    vec4 worldPos = invView * viewPos;

    return worldPos.xyz;
}

vec3 reconstructViewPosition(vec2 texCoord, float depth) {
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

// IGN sampling
float getIGN(vec2 fragCoord) {
    sampler2D ignTexture = sampler2D(u_IGNHandle);
    vec2 uv = mod(fragCoord, u_IGNResolution) / u_IGNResolution;
    return texture(ignTexture, uv).r;
}

float calculateSSAO(vec2 texCoord, vec3 fragPosView, vec3 normalView, float depth) {
    // Early exit if SSAO is disabled
    if (u_SSAO == 0) {
        return 1.0;
    }

    // Early out for invalid depths
    if (depth <= 0.0) {
        return 1.0;
    }
    
    // Distance-based fadeout
    float viewDistance = length(fragPosView);
    float fadeoutFactor = smoothstep(u_SSAOMaxDistance * u_SSAOFadeout, u_SSAOMaxDistance, viewDistance);
    if (fadeoutFactor >= 1.0) {
        return 1.0;
    }

    // Generate random rotation using IGN
    float noise = getIGN(gl_FragCoord.xy);
    float randomAngle = noise * 2.0 * PI;
    
    // Create tangent space basis
    vec3 randomVec = vec3(cos(randomAngle), sin(randomAngle), 0.0);
    vec3 tangent = normalize(randomVec - normalView * dot(randomVec, normalView));
    vec3 bitangent = cross(normalView, tangent);
    mat3 TBN = mat3(tangent, bitangent, normalView);
    
    // Sample kernel - hemisphere distribution
    float occlusion = 0.0;
    int validSamples = 0;
    
    sampler2D depthSampler = sampler2D(u_GBufferDepthHandle);
    
    for (int i = 0; i < u_SSAOSamples; ++i) {
        // Generate sample point in hemisphere
        // Using Hammersley sequence for better distribution
        float phi = 2.0 * PI * fract(float(i) * 0.618034);
        float cosTheta = 1.0 - (float(i) + 0.5) / float(u_SSAOSamples);
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        
        vec3 sampleDir = vec3(
            cos(phi) * sinTheta,
            sin(phi) * sinTheta,
            cosTheta
        );
        
        // Transform sample to view space
        vec3 sampleVec = TBN * sampleDir;
        
        // Scale sample by radius with bias towards surface
        float scale = float(i) / float(u_SSAOSamples);
        scale = mix(0.1, 1.0, scale * scale); // More samples closer to surface
        
        vec3 samplePos = fragPosView + sampleVec * u_SSAORadius * scale;
        
        // Project sample position to screen space
        vec4 offset = projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        // Check if sample is within screen bounds
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }
        
        // Sample depth at offset position
        float sampleDepth = texture(depthSampler, offset.xy).r;
        
        // Reconstruct sample view position
        vec3 samplePosActual = reconstructViewPosition(offset.xy, sampleDepth);
        
        // Range check to reduce artifacts at edges
        float rangeCheck = smoothstep(0.0, 1.0, u_SSAORadius / abs(fragPosView.z - samplePosActual.z));
        
        // Check if sample is occluded
        float occluded = (samplePosActual.z >= samplePos.z + u_SSAOBias) ? 1.0 : 0.0;
        occlusion += occluded * rangeCheck;
        validSamples++;
    }
    
    // Average occlusion
    if (validSamples > 0) {
        occlusion = occlusion / float(validSamples);
    }
    
    // Convert occlusion to ambient visibility
    float aoFactor = 1.0 - (occlusion * u_SSAOIntensity);
    aoFactor = clamp(aoFactor, 0.0, 1.0);
    
    // Apply distance fadeout
    aoFactor = mix(aoFactor, 1.0, fadeoutFactor);
    
    return aoFactor;
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

// Calculate light attenuation and spot effect
float calculateAttenuation(int lightIndex, vec3 fragPosView, out vec3 lightDir)
{
    int lightType = int(lights[lightIndex].position_type.w);
    
    // Transform light position from world space to view space
    vec3 lightPosWorld = lights[lightIndex].position_type.xyz;
    vec4 lightPosView4 = view * vec4(lightPosWorld, 1.0);
    vec3 lightPosView = lightPosView4.xyz / lightPosView4.w;
    
    float range = lights[lightIndex].direction_range.w;
    float attenuation = 1.0;

    if (lightType == DIRECTIONAL_LIGHT) {
        // Transform direction from world space to view space
        vec3 dirWorld = lights[lightIndex].direction_range.xyz;
        vec4 dirView4 = view * vec4(dirWorld, 0.0);
        lightDir = normalize(dirView4.xyz);
        attenuation = 1.0;
    } else {
        // Point or spot: reject fragments outside the light radius before the expensive work.
        vec3 lightToFrag = lightPosView - fragPosView;
        float distanceSq = dot(lightToFrag, lightToFrag);
        if (distanceSq > range * range) {
            lightDir = vec3(0.0);
            return 0.0;
        }

        float distance = sqrt(max(distanceSq, 0.0001));
        lightDir = lightToFrag / distance;
        distance = max(distance, 0.01); // Prevent division issues

        // Attenuation
        float linearTerm = 0.045;
        float quadraticTerm = 0.0075;
        attenuation = 1.0 / (1.0 + linearTerm * distance + quadraticTerm * distance * distance);

        // Range fade
        if (distance > range) {
            attenuation = 0.0;
        } else {
            float fadeDistance = range * 0.4;
            float fadeStart = range - fadeDistance;
            float fadeFactor = smoothstep(range, fadeStart, distance);
            attenuation *= fadeFactor;
        }

        // Spot cone
        if (lightType == SPOT_LIGHT) {
            // Transform spotlight direction from world space to view space
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

// Blinn-Phong shading
vec3 calculateBlinnPhong(int lightIndex, vec3 normal, vec3 viewDir, vec3 fragPosView, 
                        vec3 albedo, float ksIntensity, float shininess)
{
    vec3 lightDir;
    float attenuation = calculateAttenuation(lightIndex, fragPosView, lightDir);

    if (attenuation <= 0.0) return vec3(0.0);

    vec3 lightColor = lights[lightIndex].color_intensity.xyz * lights[lightIndex].color_intensity.w;

    vec3 materialKd = albedo;
    vec3 materialKs = vec3(ksIntensity);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * materialKd;

    // Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = (shininess <= 0.0) ? 0.0 : pow(max(dot(normal, halfwayDir), 0.0), shininess * 256.0);
    vec3 specular = spec * lightColor * materialKs;

    return (diffuse + specular) * attenuation;
}

// PBR shading
vec3 calculatePBR(int lightIndex, vec3 normal, vec3 viewDir, vec3 fragPosView, 
                 vec3 albedo, float metallic, float roughness, vec3 F0, vec3 fragPosWorld)
{
    vec3 lightDir;
    float attenuation = calculateAttenuation(lightIndex, fragPosView, lightDir);

    if (attenuation <= 0.0) return vec3(0.0);

    vec3 lightColor = lights[lightIndex].color_intensity.xyz * lights[lightIndex].color_intensity.w;
    vec3 radiance = lightColor * attenuation;

    vec3 H = normalize(viewDir + lightDir);
    roughness = clamp(roughness, 0.05, 1.0);

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

// Unpack albedo from RT0 (RGB16F format)
vec3 unpackAlbedo(vec3 packedAlbedo) {
    // Direct read - RGBA8 stores albedo in [0,1]
    return packedAlbedo;
}

// Octahedral decode: reconstruct unit normal from two [0,1] values (RT1 is RG16F)
vec2 signNotZero(vec2 v) {
    return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec3 octDecode(vec2 e) {
    e = e * 2.0 - 1.0;  // [0,1] -> [-1,1]
    vec3 n = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * signNotZero(n.xy);
    return normalize(n);
}

// Unpack normal from RT1 (RG16F format, oct-encoded)
vec3 unpackNormal(vec2 packedNormal) {
    return octDecode(packedNormal);
}

// Unpack emissive from RT2 (RGBA8 format)
void unpackEmissive(vec4 packedEmissive, out vec3 emissive, out float emissiveIntensity) {
    emissive = packedEmissive.rgb;
    emissiveIntensity = packedEmissive.a * 255.0;

    // Check for no emissive contribution
    if (emissiveIntensity < 0.001 || length(emissive) < 0.001) {
        emissive = vec3(0.0);
        emissiveIntensity = 0.0;
    }
}

// Unpack material properties from RT3 (RGBA8 format)
void unpackMaterialProperties(vec4 packedMaterial, out float roughness,
                             out float metallic, out float ao) {
    roughness = packedMaterial.r;
    metallic = packedMaterial.g;
    ao = packedMaterial.b;
}

// Main G-Buffer reading function (call this in lighting fragment shader)
void readGBuffer(sampler2D gBuffer0, sampler2D gBuffer1, sampler2D gBuffer2, sampler2D gBuffer3,
                 vec2 texCoords, out vec3 albedo, out vec3 normal, out vec3 emissive,
                 out float emissiveIntensity, out float roughness, out float metallic, out float ao) {

    // Sample all G-Buffer textures
    vec3 packedAlbedo   = texture(gBuffer0, texCoords).rgb;  // RGBA8, take .rgb
    vec2 packedNormal   = texture(gBuffer1, texCoords).rg;   // RG16F oct-encoded
    vec4 packedEmissive = texture(gBuffer2, texCoords);
    vec4 packedMaterial = texture(gBuffer3, texCoords);

    // Unpack all components
    albedo = unpackAlbedo(packedAlbedo);
    normal = unpackNormal(packedNormal);
    unpackEmissive(packedEmissive, emissive, emissiveIntensity);
    unpackMaterialProperties(packedMaterial, roughness, metallic, ao);
}

void readGBufferBindless(uvec2 gBuffer0Handle, uvec2 gBuffer1Handle, uvec2 gBuffer2Handle, uvec2 gBuffer3Handle,
                        vec2 texCoords, out vec3 albedo, out vec3 normal, out vec3 emissive,
                        out float emissiveIntensity, out float roughness, out float metallic, out float ao) {

    // Convert handles to samplers
    sampler2D gBuffer0 = sampler2D(gBuffer0Handle);
    sampler2D gBuffer1 = sampler2D(gBuffer1Handle);
    sampler2D gBuffer2 = sampler2D(gBuffer2Handle);
    sampler2D gBuffer3 = sampler2D(gBuffer3Handle);

    readGBuffer(gBuffer0, gBuffer1, gBuffer2, gBuffer3, texCoords, 
                albedo, normal, emissive, emissiveIntensity, roughness, metallic, ao);
}

float calculateShadowFactor(mat4 lightSpaceMatrix, int lightIndex, vec3 fragPosWorld, vec3 normalWorld, int layerIndex)
{
    sampler2DArray shadowArraySampler = sampler2DArray(u_ShadowMapArrayHandle);

    // Transform world position into light clip space
    vec4 proj = lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    if (proj.w == 0.0) return 1.0;
    vec3 projN = proj.xyz / proj.w;

    // NDC -> UV
    vec2 uv = projN.xy * 0.5 + 0.5;

    // If outside the shadow map, consider it lit
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    // Convert to [0,1] depth
    float currentDepth = projN.z * 0.5 + 0.5;

    // Clamp depth to valid range
    currentDepth = clamp(currentDepth, 0.0, 1.0);

    // Compute light direction in world space for bias calculation
    int lightType = int(lights[lightIndex].position_type.w);
    vec3 lightDirWorld;
    if (lightType == DIRECTIONAL_LIGHT) {
        // Light direction is already in world space
        lightDirWorld = normalize(lights[lightIndex].direction_range.xyz);
    } else {
        // Light position is already in world space
        vec3 lightPosWorld = lights[lightIndex].position_type.xyz;
        lightDirWorld = normalize(lightPosWorld - fragPosWorld);
    }

    // Dynamic bias based on surface angle to light
    float cosAngle = max(0.0, dot(normalWorld, lightDirWorld));
    float slope = 1.0 - cosAngle;
    float bias = max(0.0002, 0.002 * slope);
    vec2 texelSize = 1.0 / vec2(textureSize(shadowArraySampler, 0).xy);
    float shadow = 0.0;

    // PCF 3x3
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec3 sampleCoord = vec3(uv + offset, float(layerIndex));
            float sampledDepth = texture(shadowArraySampler, sampleCoord).r;

            // Apply bias and compare
            if (currentDepth - bias > sampledDepth) {
                shadow += 1.0;
            }
        }
    }
    shadow /= 9.0;

    // Return lighting factor
    return 1.0 - shadow;
}
float calculateFogFactor(float distance, float height) {
    if (u_FogEnabled == 0) {
        return 1.0; // No fog
    }

    float fogFactor = 1.0;

    // Calculate base distance fog
    if (u_FogMode == 0) {
        // Linear fog
        fogFactor = (u_FogEnd - distance) / (u_FogEnd - u_FogStart);
    }
    else if (u_FogMode == 1) {
        // Exponential fog
        fogFactor = exp(-u_FogDensity * distance);
    }
    else if (u_FogMode == 2) {
        // Exponential squared fog (more realistic)
        float exponent = u_FogDensity * distance;
        fogFactor = exp(-exponent * exponent);
    }

    // Apply height-based fog modifier
    // Lower heights = more fog (lower fogFactor)
    // Higher heights = less fog (higher fogFactor)
    if (u_FogHeightCoefficient > 0.001) {
        // Calculate how much height affects fog
        // Negative exponent means fog decreases with height
        float heightFactor = exp(-max(0.0, height) / u_FogHeightFalloff);
        
        // Blend between full fog (0) and current fog factor based on height
        // heightFactor = 1.0 at ground level (max fog influence)
        // heightFactor approaches 0.0 at high altitudes (min fog influence)
        fogFactor = mix(fogFactor, 1.0, (1.0 - heightFactor) * u_FogHeightCoefficient);
    }

    return clamp(fogFactor, 0.0, 1.0);
}

/**
 * @brief Evaluates spherical harmonics at a given direction
 * @param normal Direction vector to evaluate SH (normalized)
 * @param shCoefficients SH coefficients (L2, 9 coefficients)
 * @return Reconstructed color from SH
 */
vec3 evaluateSH(vec3 normal, vec4 shCoefficients[9])
{
    // SH basis function constants
    const float c0 = 0.282095;  // 1 / (2 * sqrt(pi))
    const float c1 = 0.488603;  // sqrt(3 / (4 * pi))
    const float c2 = 1.092548;  // sqrt(15 / (4 * pi))
    const float c3 = 0.315392;  // sqrt(5 / (16 * pi))
    const float c4 = 0.546274;  // sqrt(15 / (16 * pi))

    // L2 SH basis functions
    float sh[9];
    sh[0] = c0;                                    // Y(0,0)
    sh[1] = c1 * normal.y;                         // Y(1,-1)
    sh[2] = c1 * normal.z;                         // Y(1,0)
    sh[3] = c1 * normal.x;                         // Y(1,1)
    sh[4] = c2 * normal.x * normal.y;              // Y(2,-2)
    sh[5] = c2 * normal.y * normal.z;              // Y(2,-1)
    sh[6] = c3 * (3.0 * normal.z * normal.z - 1.0);// Y(2,0)
    sh[7] = c2 * normal.x * normal.z;              // Y(2,1)
    sh[8] = c4 * (normal.x * normal.x - normal.y * normal.y); // Y(2,2)

    // Reconstruct color by summing weighted basis functions
    vec3 result = vec3(0.0);
    for (int i = 0; i < 9; ++i) {
        result += shCoefficients[i].xyz * sh[i];
    }

    return max(result, vec3(0.0)); // Clamp to prevent negative values
}

/**
 * @brief Samples light probes and returns interpolated indirect lighting
 * @param worldPos World position to sample at
 * @param worldNormal Surface normal in world space
 * @return Indirect lighting contribution from probes
 */
vec3 sampleLightProbes(vec3 worldPos, vec3 worldNormal)
{
    if (u_LightProbesEnabled == 0) {
        return vec3(0.0);
    }

    int numProbes = int(probeCount.x);
    if (numProbes == 0) {
        return vec3(0.0);
    }

    // Find probes that contain the point; blend among same-priority probes
    const int MAX_INFLUENCES = 4;
    float weights[MAX_INFLUENCES];
    int probeIndices[MAX_INFLUENCES];
    int probePriorities[MAX_INFLUENCES];
    float totalWeight = 0.0;

    // Initialize with invalid values
    for (int i = 0; i < MAX_INFLUENCES; ++i) {
        weights[i] = 0.0;
        probeIndices[i] = -1;
        probePriorities[i] = -2147483647;
    }

    // Find nearest probes
    for (int i = 0; i < numProbes && i < MAX_PROBES; ++i) {
        float isActive = probes[i].flags.x;
        int priority = int(probes[i].flags.y + 0.5);

        if (isActive < 0.5) continue; // Skip inactive probes

        vec3 bmin = probes[i].boundsMin.xyz;
        vec3 bmax = probes[i].boundsMax.xyz;
        bool inside = all(greaterThanEqual(worldPos, bmin)) && all(lessThanEqual(worldPos, bmax));
        float weight = inside ? 1.0 : 0.0;

        if (weight > 0.0) {
            // Insert into sorted list (keep top MAX_INFLUENCES)
            for (int j = 0; j < MAX_INFLUENCES; ++j) {
                if (weight > weights[j]) {
                    // Shift down
                    for (int k = MAX_INFLUENCES - 1; k > j; --k) {
                        weights[k] = weights[k - 1];
                        probeIndices[k] = probeIndices[k - 1];
                        probePriorities[k] = probePriorities[k - 1];
                    }
                    // Insert
                    weights[j] = weight;
                    probeIndices[j] = i;
                    probePriorities[j] = priority;
                    break;
                }
            }
        }
    }

    // Determine highest priority among influences
    int maxPriority = -2147483647;
    for (int i = 0; i < MAX_INFLUENCES; ++i) {
        if (probeIndices[i] >= 0 && probePriorities[i] > maxPriority) {
            maxPriority = probePriorities[i];
        }
    }

    // Calculate total weight for normalization
    for (int i = 0; i < MAX_INFLUENCES; ++i) {
        if (probeIndices[i] >= 0 && probePriorities[i] == maxPriority) {
            totalWeight += weights[i];
        }
    }

    if (totalWeight < 0.001) {
        return vec3(0.0); // No probe influence
    }

    // Blend probe contributions
    vec3 indirectLighting = vec3(0.0);
    for (int i = 0; i < MAX_INFLUENCES; ++i) {
        if (probeIndices[i] >= 0 && probePriorities[i] == maxPriority) {
            float normalizedWeight = weights[i] / totalWeight;
            vec3 probeContribution = evaluateSH(worldNormal, probes[probeIndices[i]].shCoefficients);
            indirectLighting += probeContribution * normalizedWeight;
        }
    }

    return indirectLighting;
}

void main()
{    
    // Sample depth
    sampler2D gBufferDepthSampler = sampler2D(u_GBufferDepthHandle);
    float depth = texture(gBufferDepthSampler, TexCoord).r;

    // Early exit for background pixels
    if (depth >= 1.0) {
        FragColor = vec4(0.2, 0.3, 0.3, 1.0);
        return;
    }

    // Unpack G-Buffer data
    vec3 albedo, normalView, emissive;
    float emissiveIntensity, metallic, roughness, ao;

    readGBufferBindless(u_GBuffer0Handle, u_GBuffer1Handle, u_GBuffer2Handle, u_GBuffer3Handle,
                       TexCoord, albedo, normalView, emissive, emissiveIntensity, 
                       roughness, metallic, ao);

    // Reconstruct world position
    vec3 worldPos = reconstructWorldPosition(TexCoord, depth);

    // Convert to view space for lighting calculations
    vec4 viewPos4 = view * vec4(worldPos, 1.0);
    vec3 fragPosView = viewPos4.xyz / viewPos4.w;

    // Normalize
    normalView = normalize(normalView);

    // Derive world-space normal for shadow bias calculations
    vec3 normalWorld = normalize(mat3(invView) * normalView);

    // View direction in view space (towards camera)
    vec3 viewDir = normalize(-fragPosView);

    int numLights = int(lightCount.x);

    // Shading model selection
    bool useBlinnPhong = (u_ShadingMode == 1);

    // Calculate lighting using your existing system
    vec3 result = vec3(0.0);

    // Calculate SSAO factor
    float ssaoFactor = calculateSSAO(TexCoord, fragPosView, normalView, depth);

    if (useBlinnPhong) {
        // Ambient with global ambient lighting + light probes
        vec3 ambient = u_AmbientColor * u_AmbientIntensity * albedo * ao * ssaoFactor;
        
        // Add light probe indirect lighting
        vec3 probeContribution = sampleLightProbes(worldPos, normalWorld) * albedo * ao * ssaoFactor;
        ambient += probeContribution;
        
        result += ambient;

        // Blinn-Phong lighting
        for (int i = 0; i < numLights; ++i) {
            float shininess = (1.0 - roughness) * 128.0;
            // Compute light contribution
            vec3 lightContrib = calculateBlinnPhong(i, normalView, viewDir, fragPosView, albedo, 1.0, shininess);
            if (lightContrib == vec3(0.0)) {
                continue;
            }

            // Shadow factor (1.0 = lit, 0.0 = shadowed)
            float shadowFactor = 1.0;
            int lightType = int(lights[i].position_type.w);

            int baseLayer = getShadowBaseLayer(lights[i]);
            if (lightCastsShadows(lights[i]) && baseLayer >= 0 && lightType == DIRECTIONAL_LIGHT) {
                int cascadeIndex = NUM_CASCADES - 1; // Default to last cascade

                // Select cascade based on view-space distance
                float viewDistance = length(fragPosView);
                for (int c = 0; c < NUM_CASCADES; ++c) {
                    if (viewDistance <= lights[i].splitDepths[c/4][c%4]) {
                        cascadeIndex = c;
                        break;
                    }
                }

                // Calculate shadow with selected cascade
                int layerIndex = baseLayer + cascadeIndex;

                shadowFactor = calculateShadowFactor(
                    lights[i].lightSpaceMatrix[cascadeIndex], 
                    i, 
                    worldPos, 
                    normalWorld, 
                    layerIndex
                );

            }
            else if (lightCastsShadows(lights[i]) && baseLayer >= 0 && (lightType == SPOT_LIGHT)) {
                // Check if this cascade has a valid matrix (non-zero)
                mat4 cascadeMatrix = lights[i].lightSpaceMatrix[0];
                bool hasValidMatrix = (cascadeMatrix[0][0] != 0.0 || cascadeMatrix[0][1] != 0.0 || 
                                      cascadeMatrix[0][2] != 0.0 || cascadeMatrix[0][3] != 0.0 ||
                                      cascadeMatrix[1][0] != 0.0 || cascadeMatrix[1][1] != 0.0 || 
                                      cascadeMatrix[1][2] != 0.0 || cascadeMatrix[1][3] != 0.0);

                if (hasValidMatrix) {
                    int layerIndex = baseLayer;            
                    shadowFactor = calculateShadowFactor(
                        cascadeMatrix, 
                        i, 
                        worldPos, 
                        normalWorld, 
                        layerIndex
                    );
                }
            }
            else if (lightCastsShadows(lights[i]) && baseLayer >= 0 && (lightType == POINT_LIGHT)) {
                vec3 lightPosWorld = lights[i].position_type.xyz;
                vec3 toFrag = worldPos - lightPosWorld;
                float dist = length(toFrag);
                if (dist > 0.001) {
                    vec3 dir = toFrag / dist;
                    int faceIndex = getPointLightFaceIndex(dir);
                    int layerIndex = baseLayer + faceIndex;
                    shadowFactor = calculateShadowFactor(
                        lights[i].pointLightMatrices[faceIndex],
                        i,
                        worldPos,
                        normalWorld,
                        layerIndex
                    );
                }
            }
            else {
                    // No valid shadow matrix for this cascade, assume fully lit
                    shadowFactor = 1.0;
            }

            result += lightContrib * shadowFactor;
        }

        // Emissive
        result += emissive * emissiveIntensity;
    } else {
        // PBR ambient with global ambient lighting + light probes
        vec3 ambient = u_AmbientColor * u_AmbientIntensity * albedo * ao * ssaoFactor;
        
        // Add light probe indirect lighting
        vec3 probeContribution = sampleLightProbes(worldPos, normalWorld) * albedo * ao * ssaoFactor;
        ambient += probeContribution;
        
        result += ambient;

        // PBR lighting
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        for (int i = 0; i < numLights; ++i) {
            vec3 lightContrib = calculatePBR(i, normalView, viewDir, fragPosView, albedo, metallic, roughness, F0, worldPos);
            if (lightContrib == vec3(0.0)) {
                continue;
            }

            float shadowFactor = 1.0;
            int lightType = int(lights[i].position_type.w);

            int baseLayer = getShadowBaseLayer(lights[i]);
            if (lightCastsShadows(lights[i]) && baseLayer >= 0 && lightType == DIRECTIONAL_LIGHT) {
                int cascadeIndex = NUM_CASCADES - 1;

                // Select cascade based on view-space distance
                float viewDistance = length(fragPosView);
                for (int c = 0; c < NUM_CASCADES; ++c) {
                    if (viewDistance <= lights[i].splitDepths[c/4][c%4]) {
                        cascadeIndex = c;
                        break;
                    }
                }

                int layerIndex = baseLayer + cascadeIndex;

                shadowFactor = calculateShadowFactor(
                    lights[i].lightSpaceMatrix[cascadeIndex],
                    i,
                    worldPos,
                    normalWorld,
                    layerIndex
                );

            }
            else if (lightCastsShadows(lights[i]) && baseLayer >= 0 && (lightType == SPOT_LIGHT)) {
                // Check if this cascade has a valid matrix (non-zero)
                mat4 cascadeMatrix = lights[i].lightSpaceMatrix[0];
                bool hasValidMatrix = (cascadeMatrix[0][0] != 0.0 || cascadeMatrix[0][1] != 0.0 || 
                                      cascadeMatrix[0][2] != 0.0 || cascadeMatrix[0][3] != 0.0 ||
                                      cascadeMatrix[1][0] != 0.0 || cascadeMatrix[1][1] != 0.0 || 
                                      cascadeMatrix[1][2] != 0.0 || cascadeMatrix[1][3] != 0.0);

                if (hasValidMatrix) {
                    int layerIndex = baseLayer;

                    shadowFactor = calculateShadowFactor(
                        cascadeMatrix, 
                        i, 
                        worldPos, 
                        normalWorld, 
                        layerIndex
                    );
                } 
            }
            else if (lightCastsShadows(lights[i]) && baseLayer >= 0 && (lightType == POINT_LIGHT)) {
                vec3 lightPosWorld = lights[i].position_type.xyz;
                vec3 toFrag = worldPos - lightPosWorld;
                float dist = length(toFrag);
                if (dist > 0.001) {
                    vec3 dir = toFrag / dist;
                    int faceIndex = getPointLightFaceIndex(dir);
                    int layerIndex = baseLayer + faceIndex;
                    shadowFactor = calculateShadowFactor(
                        lights[i].pointLightMatrices[faceIndex],
                        i,
                        worldPos,
                        normalWorld,
                        layerIndex
                    );
                }
            }
            else {
                    // No valid shadow matrix for this cascade, assume fully lit
                    shadowFactor = 1.0;
            }

            result += lightContrib * shadowFactor; 
        }

        // Energy compensation for rough surfaces
        if (roughness > 0.7) {
            result *= mix(1.0, 1.4, (roughness - 0.7) / 0.3);
        }

        // Emissive
        result += emissive * emissiveIntensity;
    }

    // Apply distance-based fog
    if (u_FogEnabled != 0) {
        float fogDistance = length(fragPosView);
        float fogHeight = worldPos.y; // Assuming Y is up axis
        float fogFactor = calculateFogFactor(fogDistance, fogHeight);
        result = mix(u_FogColor, result, fogFactor);
    }


    FragColor = vec4(result, 1.0);
}
