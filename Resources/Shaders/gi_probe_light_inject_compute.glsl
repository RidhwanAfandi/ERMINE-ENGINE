#version 460 core

// Inject direct lighting from the light SSBO into voxel emissive volume.
layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(rgba8, binding = 0) uniform image3D u_VoxelAlbedo;
layout(rgba8, binding = 1) uniform image3D u_VoxelEmissive;
layout(rgba8, binding = 2) uniform image3D u_VoxelNormal;

uniform vec3 u_VoxelBoundsMin;
uniform vec3 u_VoxelBoundsMax;
uniform int u_VoxelResolution;

const int NUM_CASCADES = 4;
const int POINT_LIGHT = 0;
const int DIRECTIONAL_LIGHT = 1;
const int SPOT_LIGHT = 2;

struct Light {
    vec4 position_type;    // xyz = position (view space), w = light type
    vec4 color_intensity;  // xyz = color, w = intensity
    vec4 direction_range;  // xyz = direction (view space), w = range
    vec4 spot_angles_castshadows_startOffset; // x = inner cos, y = outer cos, z = flags, w = shadow base
    mat4 lightSpaceMatrix[NUM_CASCADES];
    mat4 pointLightMatrices[6];
    vec4 splitDepths[(NUM_CASCADES + 3) / 4];
};

layout (std430, binding = 4) readonly buffer LightsSSBO { // Matches LIGHT_SSBO_BINDING in SSBO_Bindings.h
    vec4 lightCount;
    Light lights[];
};

float calculateAttenuation(int lightIndex, vec3 fragPosWorld, out vec3 lightDir)
{
    int lightType = int(lights[lightIndex].position_type.w);
    vec3 lightPosWorld = lights[lightIndex].position_type.xyz;
    float range = lights[lightIndex].direction_range.w;
    float attenuation = 1.0;

    if (lightType == DIRECTIONAL_LIGHT) {
        vec3 dirWorld = lights[lightIndex].direction_range.xyz;
        lightDir = normalize(dirWorld);
        attenuation = 1.0;
    } else {
        lightDir = normalize(lightPosWorld - fragPosWorld);
        float distance = length(lightPosWorld - fragPosWorld);
        distance = max(distance, 0.01);
        float linearTerm = 0.045;
        float quadraticTerm = 0.0075;
        attenuation = 1.0 / (1.0 + linearTerm * distance + quadraticTerm * distance * distance);

        if (distance > range) {
            attenuation = 0.0;
        } else {
            float fadeDistance = range * 0.1;
            float fadeStart = range - fadeDistance;
            float fadeFactor = smoothstep(range, fadeStart, distance);
            attenuation *= fadeFactor;
        }

        if (lightType == SPOT_LIGHT) {
            vec3 spotDir = normalize(lights[lightIndex].direction_range.xyz);
            float cosAngle = dot(-lightDir, spotDir);
            float innerCos = lights[lightIndex].spot_angles_castshadows_startOffset.x;
            float outerCos = lights[lightIndex].spot_angles_castshadows_startOffset.y;
            float spotFactor = clamp((cosAngle - outerCos) / (innerCos - outerCos), 0.0, 1.0);
            attenuation *= spotFactor;
        }
    }

    return attenuation;
}

void main()
{
    ivec3 gid = ivec3(gl_GlobalInvocationID);
    if (gid.x >= u_VoxelResolution || gid.y >= u_VoxelResolution || gid.z >= u_VoxelResolution)
        return;

    vec4 albedoTex = imageLoad(u_VoxelAlbedo, gid);
    if (albedoTex.a < 0.5)
        return;

    vec3 boundsSize = max(u_VoxelBoundsMax - u_VoxelBoundsMin, vec3(1e-5));
    vec3 voxelSize = boundsSize / float(u_VoxelResolution);
    vec3 worldPos = u_VoxelBoundsMin + (vec3(gid) + 0.5) * voxelSize;

    vec4 normalTex = imageLoad(u_VoxelNormal, gid);
    vec3 normalWorld = normalize(normalTex.rgb * 2.0 - 1.0);
    if (normalTex.a < 0.5 || length(normalWorld) < 0.001) {
        normalWorld = vec3(0.0, 1.0, 0.0);
    }

    int numLights = int(lightCount.x);
    vec3 direct = vec3(0.0);
    for (int i = 0; i < numLights; ++i) {
        vec3 lightDir;
        float attenuation = calculateAttenuation(i, worldPos, lightDir);
        if (attenuation <= 0.0) continue;
        float NdotL = max(dot(normalWorld, lightDir), 0.0);
        vec3 lightColor = lights[i].color_intensity.xyz * lights[i].color_intensity.w;
        direct += lightColor * attenuation * NdotL;
    }

    vec4 emissiveTex = imageLoad(u_VoxelEmissive, gid);
    vec3 emissive = emissiveTex.rgb + direct * albedoTex.rgb;
    imageStore(u_VoxelEmissive, gid, vec4(emissive, 1.0));
}
