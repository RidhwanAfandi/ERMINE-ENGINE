#version 460 core
#extension GL_ARB_bindless_texture : require

const int MAX_LIGHTS = 32;
const int NUM_CASCADES = 4;
const int VOLUMETRIC_STEPS = 32;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_LightingTexture;
uniform int u_Pass; // 1 = extract bright, 2 = blur horizontal, 3 = blur vertical, 4 = combine

uniform float u_BloomThreshold = 1.0;
uniform float u_BloomRadius = 1.0;

// Bindless texture handles for god rays
uniform uvec2 u_GBufferDepthHandle;
uniform uvec2 u_ShadowMapArrayHandle;
uniform uvec2 u_IGNHandle;
uniform vec2 u_IGNResolution;

// Camera and matrices for god rays
uniform vec3 u_CameraPosition;
uniform mat4 u_InvProjection;
uniform mat4 u_InvView;

// Spotlight ray parameters
uniform int u_SpotlightRays = 0;
uniform float u_SpotlightRayIntensity = 0.3;

// Constants
const int POINT_LIGHT = 0;
const int DIRECTIONAL_LIGHT = 1;
const int SPOT_LIGHT = 2;

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

layout (std140, binding = 1) uniform LightsUBO {
    vec4 lightCount;
    Light lights[MAX_LIGHTS];
};

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

// Gaussian blur weights for 5-tap kernel
const float weights[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

// IGN sampling
float getIGN(vec2 fragCoord) {
    sampler2D ignTexture = sampler2D(u_IGNHandle);
    vec2 uv = mod(fragCoord, u_IGNResolution) / u_IGNResolution;
    return texture(ignTexture, uv).r;
}

// Reconstruct world position from depth
vec3 reconstructWorldPosition(vec2 texCoord, float depth)
{
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_InvProjection * ndc;
    viewPos /= viewPos.w;
    vec4 worldPos = u_InvView * viewPos;
    return worldPos.xyz;
}

// Calculate volumetric spotlight rays
vec3 calculateSpotlightRays(vec2 texCoord, vec3 fragPosWorld, vec3 cameraPos, float fragmentDistance, int numLights) {
    if (u_SpotlightRays == 0) return vec3(0.0);

    vec3 rayContribution = vec3(0.0);
    vec3 viewDir = normalize(fragPosWorld - cameraPos);
    float noise = getIGN(gl_FragCoord.xy);

    for (int i = 0; i < numLights; ++i) {
        if (int(lights[i].position_type.w) != SPOT_LIGHT) continue;

        // Check if this spotlight casts rays
        if (!lightCastsRays(lights[i])) continue;

        vec3 lightPos = lights[i].position_type.xyz;
        vec3 lightDir = normalize(lights[i].direction_range.xyz);
        vec3 lightColor = lights[i].color_intensity.xyz;
        float intensity = lights[i].color_intensity.w;
        float range = lights[i].direction_range.w;
        float innerCos = lights[i].spot_angles_castshadows_startOffset.x;
        float outerCos = lights[i].spot_angles_castshadows_startOffset.y;

        // --- STEP 1: SPHERE INTERSECTION ---

        vec3 L = lightPos - cameraPos;
        float tca = dot(L, viewDir);
        float d2 = dot(L, L) - tca * tca;
        float radius2 = range * range;

        // Ray misses the Light Sphere entirely
        if (d2 > radius2) continue;

        float thc = sqrt(radius2 - d2);
        float tNear = tca - thc;
        float tFar = tca + thc;

        // --- STEP 2: PLANE CLIPPING ---

        // Calculate height of enter/exit points relative to the light plane
        vec3 pNear = cameraPos + viewDir * tNear;
        vec3 pFar  = cameraPos + viewDir * tFar;

        float hNear = dot(pNear - lightPos, lightDir);
        float hFar  = dot(pFar - lightPos, lightDir);

        // If both are behind the light, skip
        if (hNear < 0.0 && hFar < 0.0) continue;

        // If entry is behind light, slide it forward to the light plane
        if (hNear < 0.0) {
             // Ray-Plane Intersection Logic
             // t = dot(planeOrigin - rayOrigin, planeNormal) / dot(rayDir, planeNormal)
             float num = dot(lightPos - cameraPos, lightDir);
             float den = dot(viewDir, lightDir);
             tNear = num / den;
        }

        // If exit is behind light (rare, but possible with wide spheres), clip it too
        if (hFar < 0.0) {
             float num = dot(lightPos - cameraPos, lightDir);
             float den = dot(viewDir, lightDir);
             tFar = num / den;
        }

        // --- STEP 3: CAMERA & DEPTH CLIPPING ---
        tNear = max(tNear, 0.0);
        tFar = min(tFar, fragmentDistance);

        if (tNear >= tFar) continue;

        // --- STEP 4: RAY MARCHING ---
        float rayLength = tFar - tNear;
        float stepSize = rayLength / float(VOLUMETRIC_STEPS);
        float currentT = tNear + stepSize * noise;

        vec3 accumulatedLight = vec3(0.0);

        for (int s = 0; s < VOLUMETRIC_STEPS; ++s) {
            vec3 samplePos = cameraPos + viewDir * currentT;
            vec3 toLight = samplePos - lightPos;
            float distToLight = length(toLight);

            // We are inside the Sphere and front of Plane.
            // Now we just check the Angle.
            vec3 dirToLight = toLight / max(distToLight, 0.001);
            float currentCos = dot(dirToLight, lightDir);

            // ANGLE CHECK
            // Since we are raymarching, this check is per-pixel and Singularity-Free.
            if (currentCos > outerCos) {

                // Attenuation with proper inner/outer cone angles
                float spotAtten = smoothstep(outerCos, innerCos, currentCos);
                float distAtten = 1.0 - smoothstep(0.0, range, distToLight);

                // Shadows
                float occlusion = 1.0;
                if (lightCastsShadows(lights[i])) {
                    int baseLayer = getShadowBaseLayer(lights[i]);
                    if (baseLayer >= 0 && distToLight > 0.5) { // Prevent near-plane clip
                        mat4 lightSpaceMatrix = lights[i].lightSpaceMatrix[0];
                        int layerIndex = baseLayer;
                        vec4 shadowCoord = lightSpaceMatrix * vec4(samplePos, 1.0);
                        if (shadowCoord.w > 0.0) {
                             vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
                             vec2 shadowUV = projCoords.xy * 0.5 + 0.5;
                             if(shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && shadowUV.y >= 0.0 && shadowUV.y <= 1.0) {
                                 float currentDepth = projCoords.z * 0.5 + 0.5;
                                 sampler2DArray shadowMap = sampler2DArray(u_ShadowMapArrayHandle);
                                 float shadowDepth = texture(shadowMap, vec3(shadowUV, float(layerIndex))).r;
                                 if (currentDepth - 0.002 > shadowDepth) occlusion = 0.0;
                             }
                        }
                    }
                }
                accumulatedLight += lightColor * intensity * spotAtten * distAtten * occlusion;
            }
            currentT += stepSize;
        }

        rayContribution += accumulatedLight * stepSize * u_SpotlightRayIntensity;
    }

    return rayContribution;
}

vec3 extractBrightColor(vec3 color)
{
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float weight = smoothstep(u_BloomThreshold, u_BloomThreshold + 0.1, luminance);
    return color * weight;
}

vec3 gaussianBlur(sampler2D tex, vec2 texCoord, vec2 direction)
{
    vec3 result = texture(tex, texCoord).rgb * weights[0];
    
    for(int i = 1; i < 5; ++i)
    {
        vec2 offset = direction * float(i) * u_BloomRadius;
        result += texture(tex, texCoord + offset).rgb * weights[i];
        result += texture(tex, texCoord - offset).rgb * weights[i];
    }
    
    return result;
}

void main()
{
    vec2 texelSize = 1.0 / textureSize(u_LightingTexture, 0);

    if(u_Pass == 1) // Extract bright colors + volumetric god rays
    {
        // Extract bright pixels
        vec3 color = texture(u_LightingTexture, TexCoord).rgb;
        vec3 brightColor = extractBrightColor(color);

        // Calculate volumetric spotlight rays
        vec3 godRays = vec3(0.0);
        if (u_SpotlightRays > 0) {
            sampler2D depthSampler = sampler2D(u_GBufferDepthHandle);
            float depth = texture(depthSampler, TexCoord).r;

            vec3 fragPosWorld = reconstructWorldPosition(TexCoord, depth);
            float fragmentDistance = length(fragPosWorld - u_CameraPosition);

            int numLights = int(lightCount.x);
            godRays = calculateSpotlightRays(TexCoord, fragPosWorld, u_CameraPosition, fragmentDistance, numLights);
        }

        // Combine bright pixels and god rays
        FragColor = vec4(brightColor + godRays, 1.0);
    }
    else if(u_Pass == 2) // Horizontal blur
    {
        vec3 blurred = gaussianBlur(u_LightingTexture, TexCoord, vec2(texelSize.x, 0.0));
        FragColor = vec4(blurred, 1.0);
    }
    else if(u_Pass == 3) // Vertical blur
    {
        vec3 blurred = gaussianBlur(u_LightingTexture, TexCoord, vec2(0.0, texelSize.y));
        FragColor = vec4(blurred, 1.0);
    }
    else // Pass through
    {
        FragColor = texture(u_LightingTexture, TexCoord);
    }
}
