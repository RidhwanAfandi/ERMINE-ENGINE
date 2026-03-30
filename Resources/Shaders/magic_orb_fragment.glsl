#version 460
#extension GL_ARB_bindless_texture : require

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 ViewPos;
in vec3 Tangent;
in vec3 Bitangent;
flat in uint vMaterialIndex;

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
    int castsShadows; // Kept for CPU/GPU layout parity
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

uniform float u_Time;

// === ORB SETTINGS ===
uniform vec3 u_CoreColor = vec3(0.2, 0.5, 1.0);
uniform vec3 u_EdgeColor = vec3(0.6, 0.8, 1.0);
uniform vec3 u_NoiseColor = vec3(0.1, 0.3, 0.8);

uniform float u_FresnelPower = 3.0;
uniform float u_FresnelIntensity = 2.0;
uniform float u_CoreIntensity = 1.5;
uniform float u_NoiseScale = 2.0;
uniform float u_NoiseSpeed = 0.5;
uniform float u_NoiseIntensity = 0.8;
uniform float u_PulseSpeed = 1.5;
uniform float u_PulseAmount = 0.15;
uniform float u_DistortionStrength = 0.1;

uniform int u_RaymarchSteps = 16;
uniform float u_Density = 3.0;
uniform float u_LightAbsorption = 0.8;

const float PI = 3.14159265359;

// ============ NOISE FUNCTIONS ============

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3D(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
            mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
            mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
        f.z
    );
}

float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 4; i++)
    {
        value += amplitude * noise3D(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// Swirling noise that rotates over time
float swirlNoise(vec3 p, float time)
{
    // Rotate position for swirl effect
    float angle = time * 0.5;
    float s = sin(angle);
    float c = cos(angle);
    p.xz = mat2(c, -s, s, c) * p.xz;

    // Layer multiple noise octaves with different speeds
    float n1 = fbm(p * u_NoiseScale + vec3(0, time * u_NoiseSpeed, 0));
    float n2 = fbm(p * u_NoiseScale * 1.5 - vec3(time * u_NoiseSpeed * 0.7, 0, 0));
    float n3 = fbm(p * u_NoiseScale * 0.5 + vec3(0, 0, time * u_NoiseSpeed * 1.3));

    return (n1 + n2 * 0.5 + n3 * 0.25) / 1.75;
}

// ============ RAYMARCHING ============

// Sphere SDF
float sdSphere(vec3 p, float r)
{
    return length(p) - r;
}

// Density function for the orb volume
float orbDensity(vec3 p, float time)
{
    float dist = length(p);

    // Base sphere falloff
    float sphereFalloff = 1.0 - smoothstep(0.0, 1.0, dist);

    // Add noise for energy effect
    float noise = swirlNoise(p, time);

    // Pulsing
    float pulse = 1.0 + sin(time * u_PulseSpeed) * u_PulseAmount;

    // Combine: denser in center, noise adds variation
    float density = sphereFalloff * pulse;
    density *= 0.5 + noise * u_NoiseIntensity;

    // Add some streaks/tendrils
    float angle = atan(p.z, p.x);
    float streaks = sin(angle * 6.0 + time * 2.0 + dist * 10.0) * 0.5 + 0.5;
    streaks = pow(streaks, 3.0);
    density += streaks * sphereFalloff * 0.3;

    return max(density, 0.0) * u_Density;
}

// Raymarch through the orb volume
vec4 raymarchOrb(vec3 ro, vec3 rd, float time)
{
    vec3 color = vec3(0.0);
    float transmittance = 1.0;

    // Find entry/exit points with sphere
    float t0, t1;
    vec3 sphereCenter = vec3(0.0);
    float sphereRadius = 1.0;

    vec3 oc = ro - sphereCenter;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - c;

    if (discriminant < 0.0)
    {
        return vec4(0.0);
    }

    float sqrtDisc = sqrt(discriminant);
    t0 = max(-b - sqrtDisc, 0.0);
    t1 = -b + sqrtDisc;

    if (t1 < 0.0) return vec4(0.0);

    float stepSize = (t1 - t0) / float(u_RaymarchSteps);

    for (int i = 0; i < u_RaymarchSteps; i++)
    {
        float t = t0 + (float(i) + 0.5) * stepSize;
        vec3 p = ro + rd * t;

        float density = orbDensity(p, time);

        if (density > 0.001)
        {
            // Distance from center for color gradient
            float distFromCenter = length(p);

            // Color gradient: core color in center, edge color outside
            vec3 sampleColor = mix(u_CoreColor, u_EdgeColor, smoothstep(0.0, 0.8, distFromCenter));

            // Add noise color variation
            float noise = swirlNoise(p, time);
            sampleColor = mix(sampleColor, u_NoiseColor, noise * 0.4);

            // Accumulate color with absorption
            float absorption = exp(-density * stepSize * u_LightAbsorption);
            color += sampleColor * density * transmittance * stepSize * u_CoreIntensity;
            transmittance *= absorption;

            if (transmittance < 0.01) break;
        }
    }

    return vec4(color, 1.0 - transmittance);
}

// ============ MAIN ============

void main()
{
    MaterialData material = materials[vMaterialIndex];

    // Get view direction
    vec3 viewDir = normalize(ViewPos - FragPos);
    vec3 normal = normalize(Normal);

    // Transform to object space for raymarching
    // Assuming the mesh is a unit sphere centered at origin
    vec3 localPos = FragPos; // Adjust if needed based on your transform setup

    // Ray origin and direction in local space
    vec3 ro = ViewPos;
    vec3 rd = normalize(FragPos - ViewPos);

    // Scale to unit sphere (adjust based on your mesh scale)
    // This assumes FragPos is already in a reasonable space

    // Fresnel rim effect
    float fresnel = 1.0 - max(dot(viewDir, normal), 0.0);
    fresnel = pow(fresnel, u_FresnelPower);
    vec3 fresnelColor = u_EdgeColor * fresnel * u_FresnelIntensity;

    // Raymarch the volume
    vec4 volumeColor = raymarchOrb(ro, rd, u_Time);

    // Surface noise for extra detail
    vec3 noisePos = FragPos * u_NoiseScale;
    float surfaceNoise = swirlNoise(noisePos, u_Time);

    // Core glow based on view angle
    float coreGlow = max(dot(viewDir, normal), 0.0);
    coreGlow = pow(coreGlow, 2.0);
    vec3 coreColor = u_CoreColor * coreGlow * u_CoreIntensity;

    // Distortion offset for energy look
    vec2 distortUV = TexCoord + vec2(surfaceNoise - 0.5) * u_DistortionStrength;

    // Combine all layers
    vec3 finalColor = vec3(0.0);

    // Add raymarched volume
    finalColor += volumeColor.rgb;

    // Add surface fresnel glow
    finalColor += fresnelColor;

    // Add core glow
    finalColor += coreColor * (0.5 + surfaceNoise * 0.5);

    // Add noise-based energy patterns on surface
    float energyPattern = pow(surfaceNoise, 2.0);
    finalColor += u_NoiseColor * energyPattern * 0.5;

    // Pulsing overall intensity
    float pulse = 1.0 + sin(u_Time * u_PulseSpeed) * u_PulseAmount * 0.5;
    finalColor *= pulse;

    // Apply material emissive multiplier
    finalColor *= material.emissiveIntensity;

    // Alpha: combine fresnel edge and volume
    float alpha = fresnel * 0.5 + volumeColor.a + coreGlow * 0.3;
    alpha = clamp(alpha, 0.0, 1.0);

    // HDR output for bloom
    FragColor = vec4(finalColor, alpha);
}
