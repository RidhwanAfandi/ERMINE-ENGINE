#version 460 core
#extension GL_ARB_bindless_texture : require

// Inputs from vertex shader
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 ViewPos;
in vec3 Tangent;
in vec3 Bitangent;
flat in uint vMaterialIndex;
flat in vec3 vCameraPos;    // Camera position in world space
flat in vec3 vModelCenter;  // Model center in world space

// Output
out vec4 FragColor;

// Material structure
struct MaterialData {
    vec4 albedo;
    float metallic;
    float roughness;
    float ao;
    float normalStrength;
    vec3 emissive;
    float emissiveIntensity;
    int shadingModel;
    uint textureFlags;
    float _pad0;
    float _pad1;
    vec2 uvScale;
    vec2 uvOffset;
    int albedoMapIndex;
    int normalMapIndex;
    int roughnessMapIndex;
    int metallicMapIndex;
    int aoMapIndex;
    int emissiveMapIndex;
    int _pad2;
    int _pad3;
};

layout(std430, binding = 3) restrict readonly buffer MaterialBlock {
    MaterialData materials[];
};

layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock {
    uvec2 textureHandles[];
};

// Uniforms
uniform float u_Time;
uniform uvec2 u_IGNHandle;
uniform vec2 u_IGNResolution;

// Volumetric parameters
uniform float orbRadius = 0.3;          // Central sphere size
uniform float textureRadius = 0.3;       // Texture layer radius
uniform float textureRadius2 = 0.3;     // Second texture layer radius
uniform float volumeRadius = 1.2;        // Outer boundary for sparks
uniform vec3 orbColor = vec3(0.2, 0.6, 1.0);  // Bright blue
uniform float orbIntensity = 80.0;        // HDR brightness

// Spark parameters
uniform int sparkCount = 10;             // Fewer sparks
uniform float sparkSize = 0.1;         // Small but visible
uniform float sparkSpeed = 3.0;          // Slower
uniform float sparkIntensity = 15.0;     // Super bright

const uint MAT_FLAG_ALBEDO_MAP = 1u << 0u;
const uint MAT_FLAG_NORMAL_MAP = 1u << 1u;

// IGN sampling
float getIGN(vec2 fragCoord) {
    sampler2D ignTexture = sampler2D(u_IGNHandle);
    vec2 uv = mod(fragCoord, u_IGNResolution) / u_IGNResolution;
    return texture(ignTexture, uv).r;
}

// Helper function to get IGN for per-spark properties (coherent across all pixels)
float getIGNForSpark(float seed) {
    // Use seed to create a fixed coordinate in IGN texture
    // All pixels sampling the same spark will get the same value
    vec2 fixedCoord = vec2(seed * 127.1, seed * 311.7);
    return getIGN(fixedCoord);
}


// Ray-sphere intersection
bool intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t0, out float t1) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return false;
    h = sqrt(h);
    t0 = -b - h;
    t1 = -b + h;
    return true;
}

vec2 GetRayPlaneUV(vec3 rayOrigin, vec3 rayDir, vec3 center, out float tClosest) {
    vec3 toCenter = center - rayOrigin;
    tClosest = dot(toCenter, rayDir);
    vec3 closestPoint = rayOrigin + rayDir * tClosest;
    vec3 local = closestPoint - center;

    vec3 up = abs(rayDir.y) > 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(up, rayDir));
    vec3 bitangent = normalize(cross(rayDir, tangent));
    return vec2(dot(local, tangent), dot(local, bitangent));
}

// Sample sparks - super small bright particles
float sampleSparksRay(vec3 rayOrigin, vec3 rayDir, vec3 center, float tNear, float tFar) {
    float sparkContribution = 0.0;

    for (int i = 0; i < sparkCount; i++) {
        float sparkSeed = float(i) * 7.123 + float(i * i) * 0.314;

        float emissionCycle = getIGNForSpark(sparkSeed * 8.1);
        float emissionFrequency = 0.2 + emissionCycle * 0.3;
        float emissionPhase = fract(u_Time * emissionFrequency + emissionCycle);

        float activeWindow = getIGNForSpark(sparkSeed * 11.2);
        float windowSize = 0.3 + activeWindow * 0.3;
        if (emissionPhase > windowSize) {
            continue;
        }

        float normalizedPhase = emissionPhase / windowSize;

        vec3 sparkDir = normalize(vec3(
            getIGNForSpark(sparkSeed * 1.0) * 2.0 - 1.0,
            getIGNForSpark(sparkSeed * 2.1) * 2.0 - 1.0,
            getIGNForSpark(sparkSeed * 3.3) * 2.0 - 1.0
        ));

        float sparkDist = mix(orbRadius * 1.05, volumeRadius * 1.5, smoothstep(0.0, 1.0, normalizedPhase));

        vec3 sparkPos = center + sparkDir * sparkDist;

        if (length(sparkPos - center) > volumeRadius + sparkSize) {
            continue;
        }

        vec3 toCenter = sparkPos - rayOrigin;
        float tClosest = dot(toCenter, rayDir);
        if (tClosest < tNear || tClosest > tFar) {
            continue;
        }

        vec3 closestPoint = rayOrigin + rayDir * tClosest;
        vec3 diff = closestPoint - sparkPos;
        float dist2 = dot(diff, diff);
        float sparkSize2 = sparkSize * sparkSize;
        if (dist2 >= sparkSize2) {
            continue;
        }

        float dist = sqrt(dist2);
        float falloff = 1.0 - (dist / sparkSize);
        falloff = falloff * falloff;

        float lifeFade = smoothstep(0.0, 0.1, normalizedPhase) * (1.0 - smoothstep(0.7, 1.0, normalizedPhase));
        sparkContribution += falloff * lifeFade;
    }

    return sparkContribution;
}

void main()
{
    MaterialData material = materials[vMaterialIndex];

    // Setup ray
    vec3 rayOrigin = vCameraPos;
    vec3 rayDir = normalize(FragPos - vCameraPos);

    // Intersect with bounding volume
    float tNear, tFar;
    if (!intersectSphere(rayOrigin, rayDir, vModelCenter, volumeRadius, tNear, tFar)) {
        discard;
    }

    tNear = max(tNear, 0.0);
    if (tNear >= tFar) discard;

    vec3 accumulatedColor = vec3(0.0);
    float accumulatedAlpha = 0.0;

    // --- 1. ORB SURFACE (FLAT 2D DISC) ---
    float tClosest;
    vec2 orbUV = GetRayPlaneUV(rayOrigin, rayDir, vModelCenter, tClosest);
    if (tClosest > 0.0) {
        float dist = length(orbUV);
        if (dist <= orbRadius) {
            vec3 orbEmission = orbColor * orbIntensity;
            accumulatedColor += orbEmission * (1.0 - accumulatedAlpha);
            accumulatedAlpha = 1.0;
        }
    }

    // --- 2. SUPER SMALL BRIGHT SPARKS (ANALYTIC PER-RAY) ---
    float sparkDensity = sampleSparksRay(rayOrigin, rayDir, vModelCenter, tNear, tFar);
    if (sparkDensity > 0.01) {
        vec3 sparkEmission = vec3(0.5, 0.8, 1.0) * sparkIntensity * sparkDensity;

        float sparkAlpha = sparkDensity * 0.5;

        accumulatedColor += sparkEmission * sparkAlpha * (1.0 - accumulatedAlpha);
        accumulatedAlpha += sparkAlpha * (1.0 - accumulatedAlpha);
    }

    // --- 3. FLAT TEXTURE LAYER (NON-VOLUMETRIC, MOVING AROUND) ---
    // Check if ray intersects texture sphere
    float tTexNear, tTexFar;
    if (intersectSphere(rayOrigin, rayDir, vModelCenter, textureRadius, tTexNear, tTexFar)) {
        // Use the near intersection point (front face of texture sphere)
        float tTex = max(tTexNear, 0.0);

        if (tTex < tFar && tTex >= tNear) {
            vec3 texPos = rayOrigin + rayDir * tTex;
            vec3 texNormal = normalize(texPos - vModelCenter);

            // Add animated rotation/movement to texture UVs
            float rotationAngle = u_Time * 2;
            float cosA = cos(rotationAngle);
            float sinA = sin(rotationAngle);

            // Rotate around Y axis
            vec3 rotatedNormal = vec3(
                texNormal.x * cosA - texNormal.z * sinA,
                texNormal.y,
                texNormal.x * sinA + texNormal.z * cosA
            );

            // Calculate spherical UVs
            float phi = atan(rotatedNormal.z, rotatedNormal.x);
            float theta = acos(clamp(rotatedNormal.y, -1.0, 1.0));
            vec2 sphericalUV = vec2(
                phi / (2.0 * 3.14159265359) + 0.5,
                theta / 3.14159265359
            );

            // Apply material UV transformations
            vec2 transformedUV = sphericalUV * material.uvScale + material.uvOffset;

            // Sample texture
            if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
                vec4 texColor = texture(sampler2D(textureHandles[material.albedoMapIndex]), transformedUV);

                // Only render where texture has alpha
                if (texColor.a > 0.01) {
                    // Blend texture on top
                    vec3 textureLayer = texColor.rgb * texColor.a;
                    accumulatedColor = mix(accumulatedColor, textureLayer, texColor.a * 0.7);
                    accumulatedAlpha = mix(accumulatedAlpha, 1.0, texColor.a * 0.5);
                }
            }
        }
    }

    // --- 4. SECOND TEXTURE LAYER (OPPOSITE ROTATION) ---
    // Check if ray intersects second texture sphere
    float tTex2Near, tTex2Far;
    if (intersectSphere(rayOrigin, rayDir, vModelCenter, textureRadius2, tTex2Near, tTex2Far)) {
        // Use the near intersection point (front face of texture sphere)
        float tTex2 = max(tTex2Near, 0.0);

        if (tTex2 < tFar && tTex2 >= tNear) {
            vec3 texPos2 = rayOrigin + rayDir * tTex2;
            vec3 texNormal2 = normalize(texPos2 - vModelCenter);

            // Add animated rotation in OPPOSITE direction
            float rotationAngle2 = -u_Time * 10;  // Negative for opposite rotation and faster
            float cosA2 = cos(rotationAngle2);
            float sinA2 = sin(rotationAngle2);

            // Rotate around Y axis
            vec3 rotatedNormal2 = vec3(
                texNormal2.x * cosA2 - texNormal2.z * sinA2,
                texNormal2.y,
                texNormal2.x * sinA2 + texNormal2.z * cosA2
            );

            // Calculate spherical UVs
            float phi2 = atan(rotatedNormal2.z, rotatedNormal2.x);
            float theta2 = acos(clamp(rotatedNormal2.y, -1.0, 1.0));
            vec2 sphericalUV2 = vec2(
                phi2 / (2.0 * 3.14159265359) + 0.5,
                theta2 / 3.14159265359
            );

            // Apply material UV transformations with additional offset
            vec2 transformedUV2 = sphericalUV2 * material.uvScale + material.uvOffset;
            transformedUV2 += vec2(0.5, 0.5);  // Add offset to shift the texture

            // Sample same albedo texture as first layer
            if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
                vec4 texColor2 = texture(sampler2D(textureHandles[material.albedoMapIndex]), transformedUV2);

                // Only render where texture has alpha
                if (texColor2.a > 0.01) {
                    // Blend texture on top with slight transparency
                    vec3 textureLayer2 = texColor2.rgb * texColor2.a;
                    accumulatedColor = mix(accumulatedColor, textureLayer2, texColor2.a * 0.6);
                    accumulatedAlpha = mix(accumulatedAlpha, 1.0, texColor2.a * 0.4);
                }
            }
        }
    }

    FragColor = vec4(accumulatedColor, accumulatedAlpha);
}
