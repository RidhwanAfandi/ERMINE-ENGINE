#version 460 core
#extension GL_ARB_bindless_texture : require

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 ViewPos;
in vec3 Tangent;
in vec3 Bitangent;
flat in uint vMaterialIndex;
flat in vec3 vCameraPos;
flat in vec3 vModelCenter;

out vec4 FragColor;

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
    int castsShadows;
    float fillAmount;
    vec2 uvScale;
    vec2 uvOffset;
    int albedoMapIndex;
    int normalMapIndex;
    int roughnessMapIndex;
    int metallicMapIndex;
    int aoMapIndex;
    int emissiveMapIndex;
    float fillDirOctX;
    float fillDirOctY;
};

layout(std430, binding = 3) restrict readonly buffer MaterialBlock {
    MaterialData materials[];
};

layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock {
    uvec2 textureHandles[];
};

uniform float u_Time;
uniform float orbRadius = 0.38;
uniform float orbIntensity = 30.0;
uniform float swirlRadius = 0.50;
uniform float swirlIntensity = 2;
uniform float zoomCyclesPerSecond = 0.45;
uniform float zoomOctaves = 2.0;
uniform float shellRadiusOffset = 0.06;
uniform float shellPower = 1.0;
uniform float shellAlphaClip = 0.7;
uniform float ringIntensity = 2.0;

const float ORB_SIZE_SCALE = 0.55;
const float SWIRL_RADIUS_SCALE = 1.15;

const uint MAT_FLAG_ALBEDO_MAP = 1u << 0u;
const uint MAT_FLAG_NORMAL_MAP = 1u << 1u;
const uint MAT_FLAG_ROUGHNESS_MAP = 1u << 2u;

const float PI = 3.14159265359;
const vec3 ORB_GLOW_COLOR = vec3(1.0, 0.87058824, 0.16862746); // #FFDE2B
const vec3 EMISSIVE_RAYS_COLOR = vec3(1.0, 0.87058824, 0.16862746); // #FFDE2B
const vec3 RING_COLOR = vec3(1.0, 0.87058824, 0.16862746); // #FFDE2B
const vec3 FRONT_SHELL_COLOR = vec3(0.43529412, 0.36862746, 0.05490196); // #6F5E0E
const vec3 BACK_SHELL_COLOR = vec3(1.0, 0.87058824, 0.16862746);  // #FFDE2B

bool intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius, out float t0, out float t1) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) {
        return false;
    }
    h = sqrt(h);
    t0 = -b - h;
    t1 = -b + h;
    return true;
}

vec2 getRayPlaneUV(vec3 rayOrigin, vec3 rayDir, vec3 center, out float tClosest) {
    vec3 toCenter = center - rayOrigin;
    tClosest = dot(toCenter, rayDir);
    vec3 closestPoint = rayOrigin + rayDir * tClosest;
    vec3 local = closestPoint - center;

    vec3 up = abs(rayDir.y) > 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tangent = normalize(cross(up, rayDir));
    vec3 bitangent = normalize(cross(rayDir, tangent));
    return vec2(dot(local, tangent), dot(local, bitangent));
}

vec2 sphereUV(vec3 dir) {
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, theta / PI);
}

vec2 safeUVScale(vec2 uvScale) {
    return vec2(
        abs(uvScale.x) > 0.0001 ? uvScale.x : 1.0,
        abs(uvScale.y) > 0.0001 ? uvScale.y : 1.0
    );
}

float sampleMask(int textureIndex, vec2 uv) {
    if (textureIndex < 0) {
        return 0.0;
    }
    vec4 s = texture(sampler2D(textureHandles[textureIndex]), uv);
    return clamp(s.r, 0.0, 1.0);
}

float thresholdMask(float value, float thresholdControl) {
    float threshold = clamp(thresholdControl, 0.0, 1.0);
    float softness = mix(0.35, 0.08, threshold);
    return smoothstep(threshold - softness, threshold + softness, value);
}

void addAdditive(inout vec3 color, inout float alpha, vec3 layerColor, float layerAlpha) {
    layerAlpha = clamp(layerAlpha, 0.0, 1.0);
    color += layerColor * layerAlpha;
    alpha = clamp(alpha + layerAlpha * 0.35, 0.0, 1.0);
}

void compositeOver(inout vec3 color, inout float alpha, vec3 layerColor, float layerAlpha) {
    layerAlpha = clamp(layerAlpha, 0.0, 1.0);
    color = mix(color, layerColor, layerAlpha);
    alpha = alpha + layerAlpha * (1.0 - alpha);
}

void main() {
    MaterialData material = materials[vMaterialIndex];
    vec2 uvScale = safeUVScale(material.uvScale);
    float scaledOrbRadius = orbRadius * ORB_SIZE_SCALE;
    float scaledShellRadiusOffset = shellRadiusOffset * ORB_SIZE_SCALE;

    vec3 orbColor = vec3(0.0);
    float orbAlpha = 0.0;
    vec3 ringColor = vec3(0.0);
    float ringAlpha = 0.0;
    float ringMaskValue = 0.0;
    vec3 raysColor = vec3(0.0);
    float raysAlpha = 0.0;
    vec3 rayOrigin = vCameraPos;
    vec3 rayDir = normalize(FragPos - vCameraPos);
    float tPlane;
    vec2 planeUV = getRayPlaneUV(rayOrigin, rayDir, vModelCenter, tPlane);
    float planeDist = length(planeUV);
    vec2 orbPlaneUV = planeUV / ORB_SIZE_SCALE;
    float orbPlaneDist = length(orbPlaneUV);

    if (tPlane <= 0.0) {
        discard;
    }

    float orbMask = 1.0 - smoothstep(orbRadius, orbRadius + 0.02, orbPlaneDist);
    addAdditive(orbColor, orbAlpha, ORB_GLOW_COLOR * orbIntensity, orbMask);

    // if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
    //     float ringScale = mix(1.2, 1.4, 0.5 + 0.5 * sin(u_Time * 1.8));
    //     vec2 ringUV = orbPlaneUV / max(ringScale, 0.0001) * 0.5 + 0.5;
    //     ringUV = ringUV * uvScale + material.uvOffset;
    //     float ringMask = sampleMask(material.albedoMapIndex, ringUV);
    //     ringMask = smoothstep(0.2, 0.9, ringMask);
    //     ringMask *= 0.5;
    //     ringMaskValue = ringMask;
    //     addAdditive(ringColor, ringAlpha, RING_COLOR * ringIntensity, ringMask);
    // }

    if ((material.textureFlags & MAT_FLAG_NORMAL_MAP) != 0u && material.normalMapIndex >= 0) {
        float swirlRadiusScaled = swirlRadius * SWIRL_RADIUS_SCALE;
        vec2 discUV = orbPlaneUV / max(swirlRadiusScaled, 0.0001) * 0.5 + 0.5;
        vec2 centeredUV = discUV - 0.5;

        float zoomPhase = fract(u_Time * zoomCyclesPerSecond);
        float zoomA = exp2(zoomPhase * zoomOctaves);
        float zoomB = exp2((zoomPhase - 1.0) * zoomOctaves);

        vec2 zoomUVA = centeredUV / zoomA + 0.5;
        vec2 zoomUVB = centeredUV / zoomB + 0.5;

        zoomUVA = zoomUVA * uvScale + material.uvOffset;
        zoomUVB = zoomUVB * uvScale + material.uvOffset;

        float sampleA = sampleMask(material.normalMapIndex, zoomUVA);
        float sampleB = sampleMask(material.normalMapIndex, zoomUVB);
        float blend = smoothstep(0.0, 1.0, zoomPhase);
        float grayscaleMask = 1.0 - mix(sampleA, sampleB, blend);
        float swirlMask = thresholdMask(grayscaleMask, material.emissiveIntensity);
        float swirlBand = clamp(1.0 - (orbPlaneDist / swirlRadiusScaled), 0.0, 1.0);
        float orbOcclusion = smoothstep(orbRadius, orbRadius + 0.03, orbPlaneDist);
        float ringOcclusion = 1.0 - ringMaskValue;

        addAdditive(raysColor, raysAlpha, EMISSIVE_RAYS_COLOR * swirlIntensity, swirlMask * swirlBand * orbOcclusion * ringOcclusion);
    }

    vec3 backShellColor = vec3(0.0);
    float backShellAlpha = 0.0;
    vec3 frontShellColor = vec3(0.0);
    float frontShellAlpha = 0.0;

    if ((material.textureFlags & MAT_FLAG_ROUGHNESS_MAP) != 0u && material.roughnessMapIndex >= 0) {
        float shellNear;
        float shellFar;
        float shellRadius = scaledOrbRadius + scaledShellRadiusOffset;

        if (intersectSphere(rayOrigin, rayDir, vModelCenter, shellRadius, shellNear, shellFar)) {
            float shellHits[2] = float[2](shellNear, shellFar);

            for (int i = 0; i < 2; ++i) {
                float tShell = shellHits[i];
                if (tShell <= 0.0) {
                    continue;
                }

                vec3 shellPos = rayOrigin + rayDir * tShell;
                vec3 shellDir = normalize(shellPos - vModelCenter);
                vec2 shellUV = sphereUV(shellDir);
                shellUV = shellUV * (uvScale * 0.5) + material.uvOffset;
                shellUV += vec2(-u_Time * 1.0, 0.0);
                float shellMask = pow(clamp(sampleMask(material.roughnessMapIndex, shellUV), 0.0, 1.0), shellPower);
                if (shellMask <= shellAlphaClip) {
                    continue;
                }

                if (i == 1) {
                    shellMask *= smoothstep(orbRadius, orbRadius + 0.02, orbPlaneDist);
                    if (shellMask <= shellAlphaClip) {
                        continue;
                    }
                    backShellColor = BACK_SHELL_COLOR * shellMask;
                    backShellAlpha = shellMask;
                } else {
                    frontShellColor = FRONT_SHELL_COLOR * shellMask;
                    frontShellAlpha = shellMask;
                }
            }
        }
    }

    vec3 color = vec3(0.0);
    float alpha = 0.0;
    color += raysColor;
    alpha = max(alpha, raysAlpha);
    compositeOver(color, alpha, backShellColor, backShellAlpha);
    compositeOver(color, alpha, orbColor, orbAlpha);
    compositeOver(color, alpha, frontShellColor, frontShellAlpha);
    compositeOver(color, alpha, ringColor, ringAlpha);

    if (alpha <= 0.001) {
        discard;
    }

    FragColor = vec4(color, clamp(alpha, 0.0, 1.0));
}
