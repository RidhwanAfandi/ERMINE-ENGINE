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

// Delta time uniform for animation
uniform float u_Time;

const float PI = 3.14159265359;

// ========== UV EFFECT FUNCTIONS ==========

// Scrolling UV offset
vec2 uvScroll(vec2 uv, vec2 speed) {
    return uv + speed * u_Time;
}

// Sine wave distortion
vec2 uvWave(vec2 uv, float frequency, float amplitude) {
    float wave = sin(uv.y * frequency + u_Time * 2.0) * amplitude;
    return vec2(uv.x + wave, uv.y);
}

// Circular wave ripple from center
vec2 uvRipple(vec2 uv, float frequency, float amplitude) {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(uv, center);
    float ripple = sin(dist * frequency - u_Time * 3.0) * amplitude;
    vec2 dir = normalize(uv - center);
    return uv + dir * ripple;
}

// Rotating UV coordinates
vec2 uvRotate(vec2 uv, float speed) {
    vec2 center = vec2(0.5, 0.5);
    float angle = u_Time * speed;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rotation = mat2(c, -s, s, c);
    return center + rotation * (uv - center);
}

// Pulsating scale effect
vec2 uvPulse(vec2 uv, float speed, float amount) {
    vec2 center = vec2(0.5, 0.5);
    float scale = 1.0 + sin(u_Time * speed) * amount;
    return center + (uv - center) * scale;
}

// Kaleidoscope effect
vec2 uvKaleidoscope(vec2 uv, int segments) {
    vec2 center = vec2(0.5, 0.5);
    vec2 dir = uv - center;
    float angle = atan(dir.y, dir.x);
    float segmentAngle = 2.0 * PI / float(segments);
    angle = mod(angle, segmentAngle);
    if (mod(floor(atan(dir.y, dir.x) / segmentAngle), 2.0) > 0.5) {
        angle = segmentAngle - angle;
    }
    float dist = length(dir);
    return center + vec2(cos(angle), sin(angle)) * dist;
}

// Pixelate effect
vec2 uvPixelate(vec2 uv, float pixelSize) {
    return floor(uv / pixelSize) * pixelSize;
}

// ========== MAIN SHADER ==========

void main()
{
    // Get material data
    MaterialData material = materials[vMaterialIndex];

    // Apply material UV scale and offset
    vec2 uv = TexCoord * material.uvScale + material.uvOffset;

    // ========== APPLY UV EFFECTS ==========
    // Comment/uncomment different effects to try them out!

    // Example 1: Scrolling texture
    // uv = uvScroll(uv, vec2(0.1, 0.05));

    // Example 2: Sine wave distortion
    uv = uvWave(uv, 10.0, 0.05);

    // Example 3: Ripple effect from center
    // uv = uvRipple(uv, 20.0, 0.03);

    // Example 4: Rotating UVs
    // uv = uvRotate(uv, 0.5);

    // Example 5: Pulsating scale
    // uv = uvPulse(uv, 2.0, 0.1);

    // Example 6: Kaleidoscope
    // uv = uvKaleidoscope(uv, 6);

    // Example 7: Pixelate
    // uv = uvPixelate(uv, 0.02);

    // Example 8: Combined effects (wave + scroll)
    // uv = uvWave(uv, 8.0, 0.04);
    // uv = uvScroll(uv, vec2(0.05, 0.0));

    // ========== SAMPLE TEXTURE ==========
    vec4 albedo = material.albedo;

    // Sample albedo texture if present
    if ((material.textureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && material.albedoMapIndex >= 0) {
        uvec2 handle = textureHandles[material.albedoMapIndex];
        sampler2D albedoMap = sampler2D(handle);
        albedo *= texture(albedoMap, uv);
    }

    // ========== SIMPLE LIGHTING ==========
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(ViewPos - FragPos);

    // Simple directional light
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normal, lightDir), 0.0);

    // Ambient + diffuse
    vec3 ambient = albedo.rgb * 0.3;
    vec3 diffuse = albedo.rgb * diff * 0.7;

    // Add emissive with time-based pulsing effect
    float emissivePulse = 0.5 + 0.5 * sin(u_Time * 2.0);
    vec3 emissive = material.emissive * material.emissiveIntensity * emissivePulse;

    // Combine lighting
    vec3 finalColor = ambient + diffuse + emissive;

    // Optional: Color shift effect based on time
    float colorShift = sin(u_Time) * 0.1;
    finalColor.r += colorShift;
    finalColor.b -= colorShift;

    FragColor = vec4(finalColor, albedo.a);
}
