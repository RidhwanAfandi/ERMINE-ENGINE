/* Start Header ************************************************************************/
/*!
\file       lighting_fragment.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Deferred lighting pass fragment shader.
            Reads the G-buffer (set 1), lights UBO and camera UBO (set 0), evaluates
            PBR or Blinn-Phong lighting with cascaded shadow maps, SSAO, fog, and
            light probes, then writes to the HDR colour target.

            Vulkan depth NDC is [0,1] (GLM_FORCE_DEPTH_ZERO_TO_ONE):
              pos_reconstruct uses  vec4(uv*2-1, depth, 1) — no depth*2-1 remapping.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in  vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

// ─────────────────────────────────────────────────────────────────────────────
// Set 0 – Per-frame
// ─────────────────────────────────────────────────────────────────────────────

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invView;
    mat4 invProjection;
    mat4 prevViewProjection;
    vec4 cameraPosition;
    vec4 cameraDirection;
    vec2 resolution;
    vec2 invResolution;
    float elapsedTime;
    float deltaTime;
    uint frameIndex;
    float _pad;
} camera;

const int MAX_LIGHTS   = 32;
const int NUM_CASCADES = 4;

struct Light {
    vec4 position_type;
    vec4 color_intensity;
    vec4 direction_range;
    vec4 spot_angles_castshadows_startOffset;
    mat4 lightSpaceMatrix[NUM_CASCADES];
    mat4 pointLightMatrices[6];
    vec4 splitDepths[1]; // (NUM_CASCADES+3)/4 = 1
};

layout(set = 0, binding = 1) uniform LightsUBO {
    vec4  lightCount;
    Light lights[MAX_LIGHTS];
} lights_ubo;

const int MAX_PROBES = 128;

struct LightProbe {
    vec4 position_radius;
    vec4 shCoefficients[9];
    vec4 boundsMin;
    vec4 boundsMax;
    vec4 flags;
};

layout(set = 0, binding = 2) uniform LightProbesUBO {
    vec4       probeCount;
    LightProbe probes[MAX_PROBES];
} probes_ubo;

// ─────────────────────────────────────────────────────────────────────────────
// Set 1 – G-buffer read + shadow map
// ─────────────────────────────────────────────────────────────────────────────
layout(set = 1, binding = 0) uniform sampler2D gBuffer0;   // Albedo
layout(set = 1, binding = 1) uniform sampler2D gBuffer1;   // View-space normal
layout(set = 1, binding = 2) uniform sampler2D gBuffer2;   // Emissive
layout(set = 1, binding = 3) uniform sampler2D gBuffer3;   // Material (rough/metal/ao)
layout(set = 1, binding = 4) uniform sampler2D gBufferDepth;
layout(set = 1, binding = 5) uniform sampler2DArray shadowMapArray;

// ─────────────────────────────────────────────────────────────────────────────
// Light constants
// ─────────────────────────────────────────────────────────────────────────────
const float PI           = 3.14159265359;
const int   POINT_LIGHT  = 0;
const int   DIR_LIGHT    = 1;
const int   SPOT_LIGHT   = 2;
const int   FLAG_SHADOWS = 1;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
bool  lightCastsShadows(int i) { return (int(lights_ubo.lights[i].spot_angles_castshadows_startOffset.z) & FLAG_SHADOWS) != 0; }
int   shadowBaseLayer  (int i) { return int(lights_ubo.lights[i].spot_angles_castshadows_startOffset.w); }

// Vulkan depth [0,1] — reconstruction: NDC z = depth (no *2-1)
vec3 reconstructWorld(vec2 uv, float depth)
{
    vec4 ndc      = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos  = camera.invProjection * ndc;
    viewPos      /= viewPos.w;
    return (camera.invView * viewPos).xyz;
}

vec3 reconstructView(vec2 uv, float depth)
{
    vec4 ndc     = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = camera.invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

// Shadow map PCF 3×3
float shadowFactor(mat4 lightVP, int layerIndex, vec3 fragWorld, vec3 normalWorld,
                   int lightIndex, int lightType)
{
    vec4 proj   = lightVP * vec4(fragWorld, 1.0);
    if (proj.w == 0.0) return 1.0;
    vec3 projN  = proj.xyz / proj.w;
    vec2 uv     = projN.xy * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;

    float currentDepth = clamp(projN.z, 0.0, 1.0);  // Vulkan depth in [0,1]

    vec3 lightDir;
    if (lightType == DIR_LIGHT)
        lightDir = normalize(lights_ubo.lights[lightIndex].direction_range.xyz);
    else
        lightDir = normalize(lights_ubo.lights[lightIndex].position_type.xyz - fragWorld);

    float cosA = max(0.0, dot(normalWorld, lightDir));
    float bias = max(0.0002, 0.002 * (1.0 - cosA));

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0).xy);
    float shadow   = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            vec3  sCoord = vec3(uv + vec2(x, y) * texelSize, float(layerIndex));
            float sDepth = texture(shadowMapArray, sCoord).r;
            shadow += (currentDepth - bias > sDepth) ? 1.0 : 0.0;
        }
    return 1.0 - shadow / 9.0;
}

int pointFaceIndex(vec3 dir)
{
    vec3 a = abs(dir);
    if (a.x >= a.y && a.x >= a.z) return dir.x >= 0.0 ? 0 : 1;
    if (a.y >= a.x && a.y >= a.z) return dir.y >= 0.0 ? 2 : 3;
    return dir.z >= 0.0 ? 4 : 5;
}

float computeShadow(int i, vec3 fragWorld, vec3 fragView, vec3 normalWorld)
{
    int baseLayer = shadowBaseLayer(i);
    if (!lightCastsShadows(i) || baseLayer < 0) return 1.0;

    int lightType = int(lights_ubo.lights[i].position_type.w);

    if (lightType == DIR_LIGHT)
    {
        int cascade = NUM_CASCADES - 1;
        float d = length(fragView);
        for (int c = 0; c < NUM_CASCADES; ++c)
            if (d <= lights_ubo.lights[i].splitDepths[c/4][c%4]) { cascade = c; break; }
        return shadowFactor(lights_ubo.lights[i].lightSpaceMatrix[cascade],
                            baseLayer + cascade, fragWorld, normalWorld, i, lightType);
    }
    else if (lightType == SPOT_LIGHT)
    {
        mat4 m = lights_ubo.lights[i].lightSpaceMatrix[0];
        if (m[0] == vec4(0) && m[1] == vec4(0)) return 1.0;
        return shadowFactor(m, baseLayer, fragWorld, normalWorld, i, lightType);
    }
    else // POINT_LIGHT
    {
        vec3 toFrag = fragWorld - lights_ubo.lights[i].position_type.xyz;
        float dist  = length(toFrag);
        if (dist < 0.001) return 1.0;
        int face    = pointFaceIndex(toFrag / dist);
        return shadowFactor(lights_ubo.lights[i].pointLightMatrices[face],
                            baseLayer + face, fragWorld, normalWorld, i, lightType);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Attenuation
// ─────────────────────────────────────────────────────────────────────────────
float lightAttenuation(int i, vec3 fragView, out vec3 lightDir)
{
    int   ltype = int(lights_ubo.lights[i].position_type.w);
    float range = lights_ubo.lights[i].direction_range.w;

    if (ltype == DIR_LIGHT)
    {
        vec3 wd = lights_ubo.lights[i].direction_range.xyz;
        lightDir = normalize(mat3(camera.view) * wd);
        return 1.0;
    }

    vec3 lightPosView = (camera.view * vec4(lights_ubo.lights[i].position_type.xyz, 1.0)).xyz;
    lightDir = normalize(lightPosView - fragView);
    float dist = length(lightPosView - fragView);
    dist       = max(dist, 0.01);

    float att = 1.0 / (1.0 + 0.045 * dist + 0.0075 * dist * dist);
    if (dist > range) return 0.0;
    att *= smoothstep(range, range * 0.6, dist);

    if (ltype == SPOT_LIGHT)
    {
        vec3 spotDir = normalize(mat3(camera.view) * lights_ubo.lights[i].direction_range.xyz);
        float cosA   = dot(-lightDir, spotDir);
        float inner  = lights_ubo.lights[i].spot_angles_castshadows_startOffset.x;
        float outer  = lights_ubo.lights[i].spot_angles_castshadows_startOffset.y;
        att         *= clamp((cosA - outer) / (inner - outer), 0.0, 1.0);
    }
    return att;
}

// ─────────────────────────────────────────────────────────────────────────────
// PBR
// ─────────────────────────────────────────────────────────────────────────────
float distGGX(vec3 N, vec3 H, float r)
{
    float a  = r * r, a2 = a * a;
    float nh = max(dot(N, H), 0.0);
    float d  = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float geomSchlick(float nv, float r) { float k = (r+1.0)*(r+1.0)/8.0; return nv / (nv*(1.0-k)+k); }
float geomSmith(vec3 N, vec3 V, vec3 L, float r) { return geomSchlick(max(dot(N,V),0.0),r)*geomSchlick(max(dot(N,L),0.0),r); }
vec3  fresnel(float cosT, vec3 F0) { return F0 + (1.0 - F0) * pow(clamp(1.0 - cosT, 0.0, 1.0), 5.0); }

vec3 calcPBR(int i, vec3 N, vec3 V, vec3 fragView, vec3 albedo, float metal, float rough, vec3 F0, vec3 fragWorld)
{
    vec3  lightDir;
    float att = lightAttenuation(i, fragView, lightDir);
    if (att <= 0.0) return vec3(0.0);

    vec3  radiance = lights_ubo.lights[i].color_intensity.xyz * lights_ubo.lights[i].color_intensity.w * att;
    vec3  H        = normalize(V + lightDir);
    rough          = clamp(rough, 0.05, 1.0);

    float NDF = distGGX(N, H, rough);
    float G   = geomSmith(N, V, lightDir, rough);
    vec3  F   = fresnel(max(dot(H, V), 0.0), F0);
    vec3  kD  = (vec3(1.0) - F) * (1.0 - metal);

    vec3  spec = NDF * G * F / (4.0 * max(dot(N,V),0.0) * max(dot(N,lightDir),0.0) + 0.0001);
    return (kD * albedo / PI + spec) * radiance * max(dot(N, lightDir), 0.0);
}

vec3 calcBlinnPhong(int i, vec3 N, vec3 V, vec3 fragView, vec3 albedo, float roughness)
{
    vec3  lightDir;
    float att = lightAttenuation(i, fragView, lightDir);
    if (att <= 0.0) return vec3(0.0);

    vec3  lc  = lights_ubo.lights[i].color_intensity.xyz * lights_ubo.lights[i].color_intensity.w;
    float diff = max(dot(N, lightDir), 0.0);
    vec3  H    = normalize(lightDir + V);
    float shin = (1.0 - roughness) * 128.0;
    float spec = (shin > 0.0) ? pow(max(dot(N, H), 0.0), shin) : 0.0;
    return (diff * lc * albedo + spec * lc) * att;
}

// ─────────────────────────────────────────────────────────────────────────────
// Spherical Harmonics
// ─────────────────────────────────────────────────────────────────────────────
vec3 evalSH(vec3 n, vec4 c[9])
{
    const float c0=0.282095, c1=0.488603, c2=1.092548, c3=0.315392, c4=0.546274;
    float sh[9];
    sh[0]=c0; sh[1]=c1*n.y; sh[2]=c1*n.z; sh[3]=c1*n.x;
    sh[4]=c2*n.x*n.y; sh[5]=c2*n.y*n.z; sh[6]=c3*(3.0*n.z*n.z-1.0);
    sh[7]=c2*n.x*n.z; sh[8]=c4*(n.x*n.x-n.y*n.y);
    vec3 r=vec3(0.0);
    for(int i=0;i<9;++i) r+=c[i].xyz*sh[i];
    return max(r,vec3(0.0));
}

vec3 sampleProbes(vec3 worldPos, vec3 worldNormal)
{
    int numProbes = int(probes_ubo.probeCount.x);
    if (numProbes == 0) return vec3(0.0);

    const int MAX_INF = 4;
    float   weights[MAX_INF];
    int     indices[MAX_INF];
    float   totalW = 0.0;
    for (int i = 0; i < MAX_INF; ++i) { weights[i] = 0.0; indices[i] = -1; }

    for (int i = 0; i < numProbes && i < MAX_PROBES; ++i)
    {
        if (probes_ubo.probes[i].flags.x < 0.5) continue;
        vec3 bmin = probes_ubo.probes[i].boundsMin.xyz;
        vec3 bmax = probes_ubo.probes[i].boundsMax.xyz;
        if (!all(greaterThanEqual(worldPos, bmin)) || !all(lessThanEqual(worldPos, bmax))) continue;
        for (int j = 0; j < MAX_INF; ++j)
            if (1.0 > weights[j]) {
                for (int k = MAX_INF-1; k > j; --k) { weights[k]=weights[k-1]; indices[k]=indices[k-1]; }
                weights[j] = 1.0; indices[j] = i; break;
            }
    }
    for (int i = 0; i < MAX_INF; ++i) if (indices[i] >= 0) totalW += weights[i];
    if (totalW < 0.001) return vec3(0.0);

    vec3 indirect = vec3(0.0);
    for (int i = 0; i < MAX_INF; ++i)
        if (indices[i] >= 0)
            indirect += evalSH(worldNormal, probes_ubo.probes[indices[i]].shCoefficients) * (weights[i] / totalW);
    return indirect;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
void main()
{
    float depth = texture(gBufferDepth, vTexCoord).r;
    if (depth >= 1.0)
    {
        outColor = vec4(0.2, 0.3, 0.3, 1.0); // Sky / clear colour
        return;
    }

    // ── Unpack G-buffer ────────────────────────────────────────────────────────
    vec4 gb0 = texture(gBuffer0, vTexCoord);
    vec4 gb1 = texture(gBuffer1, vTexCoord);
    vec4 gb2 = texture(gBuffer2, vTexCoord);
    vec4 gb3 = texture(gBuffer3, vTexCoord);

    vec3  albedo    = gb0.rgb;
    vec3  normalV   = normalize(gb1.rgb * 2.0 - 1.0); // view-space normal
    vec3  emissive  = gb2.rgb;
    float emissiveI = gb2.a * 255.0;
    float roughness = gb3.r;
    float metallic  = gb3.g;
    float ao        = gb3.b;

    // ── Reconstruct positions ──────────────────────────────────────────────────
    vec3 fragWorld = reconstructWorld(vTexCoord, depth);
    vec3 fragView  = reconstructView (vTexCoord, depth);
    vec3 normalW   = normalize(mat3(camera.invView) * normalV);
    vec3 viewDir   = normalize(-fragView);

    // ── Lighting ───────────────────────────────────────────────────────────────
    int  numLights = int(lights_ubo.lightCount.x);
    vec3 result    = vec3(0.0);
    vec3 F0        = mix(vec3(0.04), albedo, metallic);

    // Ambient + probes
    const float AMBIENT_INT = 0.08;
    vec3 ambient   = vec3(1.0) * AMBIENT_INT * albedo * ao;
    ambient       += sampleProbes(fragWorld, normalW) * albedo * ao;
    result        += ambient;

    for (int i = 0; i < numLights; ++i)
    {
        int ltype = int(lights_ubo.lights[i].position_type.w);

        vec3  contrib;
        int   shading = 0; // TODO: expose per-material shading model
        if (shading == 1)
            contrib = calcBlinnPhong(i, normalV, viewDir, fragView, albedo, roughness);
        else
            contrib = calcPBR(i, normalV, viewDir, fragView, albedo, metallic, roughness, F0, fragWorld);

        result += contrib * computeShadow(i, fragWorld, fragView, normalW);
    }

    // PBR energy compensation
    if (roughness > 0.7)
        result *= mix(1.0, 1.4, (roughness - 0.7) / 0.3);

    // Emissive
    if (emissiveI > 0.001) result += emissive * emissiveI;

    outColor = vec4(result, 1.0);
}
