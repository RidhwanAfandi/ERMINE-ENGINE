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

// Material structure (112 bytes, matches C++ MaterialSSBO)
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
    float _pad0;                // 4 bytes (56-59)
    float _pad1;                // 4 bytes (60-63)

    vec2 uvScale;               // 8 bytes (64-71)
    vec2 uvOffset;              // 8 bytes (72-79)

    // Texture Array Indices
    int albedoMapIndex;         // 4 bytes (80-83)
    int normalMapIndex;         // 4 bytes (84-87)
    int roughnessMapIndex;      // 4 bytes (88-91)
    int metallicMapIndex;       // 4 bytes (92-95)

    int aoMapIndex;             // 4 bytes (96-99)
    int emissiveMapIndex;       // 4 bytes (100-103)
    int _pad2;                  // 4 bytes (104-107)
    int _pad3;                  // 4 bytes (108-111)
};

// Material SSBO
layout(std430, binding = 3) restrict readonly buffer MaterialBlock {
    MaterialData materials[];
};

// Bindless texture array SSBO
layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock {
    uvec2 textureHandles[];
};

// Delta time uniform for animation
uniform float u_Time;

const float PI = 3.14159265359;

// ========== NOISE FUNCTIONS ==========

// Hash function for pseudo-random numbers
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// 2D Noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal Brownian Motion
float fbm(vec2 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for(int i = 0; i < 5; i++) {
        value += amplitude * noise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Voronoi pattern for crystalline effects
float voronoi(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);

    float minDist = 1.0;
    for(int y = -1; y <= 1; y++) {
        for(int x = -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = hash(i + neighbor) * vec2(1.0);
            point = 0.5 + 0.5 * sin(u_Time + 6.2831 * point);

            float dist = length(neighbor + point - f);
            minDist = min(minDist, dist);
        }
    }
    return minDist;
}

// ========== PARTICLE EFFECTS ==========

// Floating sparkle particles
float sparkles(vec2 uv, float density, float speed) {
    float sparkle = 0.0;

    for(int i = 0; i < 20; i++) {
        float fi = float(i);
        vec2 offset = vec2(hash(vec2(fi, fi * 2.0)), hash(vec2(fi * 3.0, fi)));

        // Particle movement
        float t = u_Time * speed + fi * 0.5;
        vec2 particlePos = offset + vec2(sin(t) * 0.1, cos(t * 0.7) * 0.1);

        // Distance to particle
        float dist = length(uv - particlePos);

        // Flickering effect
        float flicker = 0.5 + 0.5 * sin(u_Time * 5.0 + fi * 2.0);

        // Add sparkle
        sparkle += (1.0 / (1.0 + dist * density)) * flicker;
    }

    return sparkle;
}

// Energy wisps
float energyWisps(vec2 uv, float frequency) {
    vec2 p = uv * frequency;
    float n1 = fbm(p + u_Time * 0.3);
    float n2 = fbm(p - u_Time * 0.2 + vec2(5.2, 1.3));

    float wisps = sin(p.x + n1 * 3.0 + u_Time) * sin(p.y + n2 * 3.0 - u_Time * 0.5);
    return pow(abs(wisps), 0.3);
}

// Swirling vortex
float vortex(vec2 uv, vec2 center, float strength) {
    vec2 dir = uv - center;
    float dist = length(dir);
    float angle = atan(dir.y, dir.x);

    // Rotate based on distance and time
    angle += dist * strength + u_Time;

    // Create spiral pattern
    float spiral = sin(angle * 5.0 - dist * 10.0 + u_Time * 2.0);
    float fade = 1.0 / (1.0 + dist * 5.0);

    return spiral * fade;
}

// Magic circle/rune pattern
float magicCircle(vec2 uv, vec2 center, int segments) {
    vec2 dir = uv - center;
    float dist = length(dir);
    float angle = atan(dir.y, dir.x);

    // Rotating segments
    float segmentAngle = 2.0 * PI / float(segments);
    float seg = mod(angle + u_Time, segmentAngle) / segmentAngle;

    // Rings
    float rings = sin(dist * 20.0 - u_Time * 3.0);

    // Symbols rotating around
    float symbols = step(0.95, sin(seg * float(segments) * PI));

    float circle = rings * (1.0 - smoothstep(0.2, 0.5, dist));
    circle += symbols * (1.0 - smoothstep(0.3, 0.4, abs(dist - 0.35)));

    return circle;
}

// Energy field distortion
vec2 energyField(vec2 uv, float strength) {
    float n1 = fbm(uv * 4.0 + u_Time * 0.2);
    float n2 = fbm(uv * 4.0 - u_Time * 0.15 + vec2(3.2, 1.1));

    return uv + vec2(n1, n2) * strength;
}

// Plasma effect
float plasma(vec2 uv) {
    float v = 0.0;
    v += sin((uv.x + u_Time));
    v += sin((uv.y + u_Time) * 0.5);
    v += sin((uv.x + uv.y + u_Time) * 0.5);

    vec2 center = vec2(0.5, 0.5) + 0.3 * vec2(sin(u_Time * 0.2), cos(u_Time * 0.3));
    v += sin(length(uv - center) * 4.0);

    return v * 0.5;
}

// ========== COLOR PALETTES ==========

// Magical color gradient
vec3 magicGradient(float t) {
    vec3 a = vec3(0.5, 0.2, 0.8);  // Purple
    vec3 b = vec3(0.2, 0.6, 1.0);  // Cyan
    vec3 c = vec3(1.0, 0.3, 0.7);  // Pink

    t = fract(t);
    return mix(mix(a, b, smoothstep(0.0, 0.5, t)), c, smoothstep(0.5, 1.0, t));
}

// Fire-like magic colors
vec3 fireGradient(float t) {
    vec3 color1 = vec3(1.0, 0.1, 0.0);   // Red
    vec3 color2 = vec3(1.0, 0.5, 0.0);   // Orange
    vec3 color3 = vec3(1.0, 1.0, 0.3);   // Yellow
    vec3 color4 = vec3(1.0, 1.0, 1.0);   // White

    if(t < 0.33) return mix(color1, color2, t * 3.0);
    if(t < 0.66) return mix(color2, color3, (t - 0.33) * 3.0);
    return mix(color3, color4, (t - 0.66) * 3.0);
}

// Electric/lightning colors
vec3 electricGradient(float t) {
    vec3 dark = vec3(0.1, 0.1, 0.3);
    vec3 bright = vec3(0.5, 0.8, 1.0);
    vec3 white = vec3(1.0, 1.0, 1.0);

    return mix(mix(dark, bright, t), white, pow(t, 3.0));
}

// ========== MAIN SHADER ==========

void main()
{
    // Get material data
    MaterialData material = materials[vMaterialIndex];
    vec2 uv = TexCoord * material.uvScale + material.uvOffset;

    // ========== CHOOSE YOUR MAGIC EFFECT ==========

    // Effect 1: Sparkle Particles
    float effect = sparkles(uv, 80.0, 0.5);

    // Effect 2: Energy Wisps (comment above, uncomment below)
    // float effect = energyWisps(uv, 3.0);

    // Effect 3: Vortex
    // float effect = vortex(uv, vec2(0.5, 0.5), 5.0);

    // Effect 4: Magic Circle
    // float effect = magicCircle(uv, vec2(0.5, 0.5), 8);

    // Effect 5: Plasma
    // float effect = plasma(uv * 3.0);

    // Effect 6: Crystalline (Voronoi)
    // vec2 distortedUV = energyField(uv, 0.05);
    // float effect = 1.0 - voronoi(distortedUV * 5.0);

    // ========== COMBINED EFFECTS ==========
    // Uncomment to layer multiple effects

    // Add energy wisps to sparkles
    effect += energyWisps(uv, 4.0) * 0.3;

    // Add some plasma glow
    effect += plasma(uv * 2.0) * 0.2;

    // ========== COLOR MAPPING ==========

    // Choose color gradient
    vec3 magicColor = magicGradient(effect + u_Time * 0.2);

    // Alternative: Fire colors
    // vec3 magicColor = fireGradient(effect);

    // Alternative: Electric colors
    // vec3 magicColor = electricGradient(effect);

    // ========== SAMPLE BASE TEXTURE ==========
    vec4 albedo = material.albedo;

    if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
        uvec2 handle = textureHandles[material.albedoMapIndex];
        sampler2D albedoMap = sampler2D(handle);
        albedo *= texture(albedoMap, uv);
    }

    // ========== LIGHTING ==========
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(ViewPos - FragPos);

    // Simple rim lighting for magical glow
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = pow(rim, 3.0);

    // ========== FINAL COLOR ==========

    // Mix base color with magic effect
    vec3 baseColor = albedo.rgb * 0.3;  // Darken base
    vec3 glowColor = magicColor * effect * 2.0;  // Bright magic
    vec3 rimColor = magicColor * rim * 0.5;  // Edge glow

    // Add emissive with pulsing
    float pulse = 0.7 + 0.3 * sin(u_Time * 3.0);
    vec3 emissive = material.emissive * material.emissiveIntensity * pulse;

    // Combine all elements
    vec3 finalColor = baseColor + glowColor + rimColor + emissive;

    // Optional: Add bloom-ready brightness
    finalColor *= 1.5;

    // Alpha: Fade based on effect intensity
    float alpha = albedo.a * (0.5 + effect * 0.5);

    FragColor = vec4(finalColor, alpha);
}
