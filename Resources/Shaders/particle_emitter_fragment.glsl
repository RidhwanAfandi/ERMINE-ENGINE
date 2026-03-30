#version 460

in vec2 vTexCoord;
in vec4 vColor;
in float vLife;
in vec3 vWorldPos;

out vec4 FragColor;

uniform float u_Time;
uniform int u_SparkleShape;  // 0 = soft circle, 1 = star, 2 = diamond
uniform int u_RenderMode;    // 0 = glow, 1 = smoke, 2 = electric
uniform float u_SmokeOpacity;
uniform float u_SmokeSoftness;
uniform float u_SmokeNoiseScale;
uniform float u_SmokeDistortScale;
uniform float u_SmokeDistortStrength;
uniform float u_SmokePuffScale;
uniform float u_SmokePuffStrength;
uniform float u_SmokeDepthFade;
uniform float u_ElectricIntensity;
uniform float u_ElectricFrequency;
uniform float u_ElectricBoltThickness;
uniform float u_ElectricBoltVariation;
uniform float u_ElectricGlow;
uniform int u_ElectricBoltCount;
uniform vec2 u_ScreenSize;
uniform sampler3D u_SmokeNoise;
uniform sampler3D u_SmokeDistort;
uniform sampler3D u_SmokePuff;
uniform sampler2D u_SceneDepth;
uniform sampler2D u_SpriteTexture;  // Swirl01.png, used for renderMode 3 (shockwave)

// Soft circular gradient
float softCircle(vec2 uv) {
    float dist = length(uv - 0.5) * 2.0;
    return 1.0 - smoothstep(0.0, 1.0, dist);
}

// Star/sparkle shape
float starShape(vec2 uv, int points) {
    vec2 centered = uv - 0.5;
    float dist = length(centered);
    float angle = atan(centered.y, centered.x);

    // Create star points
    float star = cos(angle * float(points)) * 0.5 + 0.5;
    star = pow(star, 2.0);

    // Combine with radial falloff
    float radial = 1.0 - smoothstep(0.0, 0.5, dist);
    float core = 1.0 - smoothstep(0.0, 0.15, dist);

    return max(radial * star, core);
}

// Diamond/cross shape
float diamondShape(vec2 uv) {
    vec2 centered = abs(uv - 0.5) * 2.0;

    // Diamond
    float diamond = 1.0 - (centered.x + centered.y);
    diamond = smoothstep(0.0, 0.3, diamond);

    // Cross rays
    float crossH = smoothstep(0.1, 0.0, centered.y) * (1.0 - centered.x);
    float crossV = smoothstep(0.1, 0.0, centered.x) * (1.0 - centered.y);

    return max(diamond, max(crossH, crossV) * 0.5);
}

// Hash functions for electric noise
float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Noise function for electric effect
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Generate electric bolt pattern
float electricBolt(vec2 uv, float seed, float thickness, float variation) {
    vec2 centered = uv - 0.5;
    float angle = atan(centered.y, centered.x);
    float dist = length(centered);
    
    // Create jagged bolt pattern
    float bolt = 0.0;
    float time = u_Time * 2.0 + seed * 10.0;
    
    // Main bolt backbone
    float backbone = abs(centered.y);
    
    // Add zigzag variation
    float zigzag = noise(vec2(centered.x * u_ElectricFrequency, time)) * 2.0 - 1.0;
    zigzag += noise(vec2(centered.x * u_ElectricFrequency * 2.3, time * 1.7)) * 0.5 - 0.25;
    backbone = abs(centered.y - zigzag * variation * 0.1);
    
    // Create bolt with thickness
    bolt = 1.0 - smoothstep(0.0, thickness, backbone);
    
    // Fade at edges
    float edgeFade = 1.0 - smoothstep(0.3, 0.5, abs(centered.x));
    bolt *= edgeFade;
    
    // Add branching
    float branchNoise = noise(vec2(centered.x * u_ElectricFrequency * 0.5, time * 0.8));
    if (branchNoise > 0.7) {
        float branch = abs(centered.y - (branchNoise - 0.7) * 3.0 * sign(zigzag));
        branch = 1.0 - smoothstep(0.0, thickness * 0.5, branch);
        branch *= smoothstep(0.7, 0.9, branchNoise);
        bolt = max(bolt, branch * 0.6);
    }
    
    return bolt;
}

void main() {
    // Shockwave sprite mode - animated dual-layer swirl
    if (u_RenderMode == 3) {
        vec2 centered = vTexCoord - 0.5;
        float dist = length(centered) * 2.0;  // 0 = center, 1 = edge

        // Layer 1: swirl spinning forward
        float a1 = u_Time * 2.5;
        vec2 uv1 = vec2(
            centered.x * cos(a1) - centered.y * sin(a1),
            centered.x * sin(a1) + centered.y * cos(a1)
        ) + 0.5;

        // Layer 2: swirl spinning backward, slightly zoomed in
        float a2 = -u_Time * 1.6;
        vec2 uv2 = vec2(
            centered.x * cos(a2) - centered.y * sin(a2),
            centered.x * sin(a2) + centered.y * cos(a2)
        ) * 1.3 + 0.5;

        vec4 s1 = texture(u_SpriteTexture, uv1);
        vec4 s2 = texture(u_SpriteTexture, uv2);

        // Combine layers additively, clamp alpha
        float swirlAlpha = clamp(s1.a + s2.a * 0.6, 0.0, 1.0);
        vec3  swirlRGB   = s1.rgb + s2.rgb * 0.5;

        // Leading-edge ring: sharp bright ring at the expanding boundary
        float ring = pow(max(0.0, 1.0 - abs(dist - 0.82) / 0.10), 2.5);
        ring *= 1.0 - vLife * 0.8;  // fades as particle ages

        // Hot center core: flares at birth, gone by mid-life
        float core = exp(-dist * 5.0) * max(0.0, 1.0 - vLife * 2.5);

        float alpha = (swirlAlpha + ring * 0.9) * vColor.a;
        if (alpha < 0.01) discard;

        // Swirl body with strong HDR boost for bloom
        vec3 color = vColor.rgb * swirlRGB * 6.0;

        // Ring is white-hot
        color += mix(vec3(1.0), vColor.rgb, 0.35) * ring * 12.0;

        // Center core pulses white
        color += vec3(1.0) * core * 6.0;

        FragColor = vec4(color, alpha);
        return;
    }

    // Electric mode
    if (u_RenderMode == 2) {
        vec2 uv = vTexCoord;
        float electric = 0.0;
        
        // Generate multiple bolts
        for (int i = 0; i < u_ElectricBoltCount; i++) {
            float seed = float(i) * 123.456;
            
            // Rotate UV for each bolt
            float rotAngle = (float(i) / float(u_ElectricBoltCount)) * 6.28318530718;
            vec2 centered = uv - 0.5;
            vec2 rotUV;
            rotUV.x = centered.x * cos(rotAngle) - centered.y * sin(rotAngle);
            rotUV.y = centered.x * sin(rotAngle) + centered.y * cos(rotAngle);
            rotUV += 0.5;
            
            float bolt = electricBolt(rotUV, seed, u_ElectricBoltThickness, u_ElectricBoltVariation);
            electric = max(electric, bolt);
        }
        
        // Add flicker
        float flicker = hash11(u_Time * 30.0 + vWorldPos.x + vWorldPos.y) * 0.3 + 0.7;
        electric *= flicker;
        
        // Add glow
        float dist = length(uv - 0.5) * 2.0;
        float glow = exp(-dist * 3.0) * u_ElectricGlow;
        electric = max(electric, glow * 0.3);
        
        // Fade over lifetime
        float lifeFade = 1.0 - vLife;
        lifeFade = lifeFade * lifeFade; // Square for sharper fade
        
        // Apply color and intensity
        vec3 color = vColor.rgb * u_ElectricIntensity * 2.0; // HDR boost
        float alpha = electric * lifeFade * vColor.a;
        
        if (alpha < 0.01) discard;
        
        FragColor = vec4(color * electric, alpha);
        return;
    }
    
    if (u_RenderMode == 1) {
        vec3 baseUV = vWorldPos * u_SmokeNoiseScale + vec3(0.0, u_Time * 0.05, 0.0);
        vec3 distortUV = vWorldPos * u_SmokeDistortScale + vec3(0.0, u_Time * 0.08, 0.0);
        vec3 distort = texture(u_SmokeDistort, distortUV).rrr * 2.0 - 1.0;
        baseUV += distort * u_SmokeDistortStrength;

        float noise = texture(u_SmokeNoise, baseUV).r;
        float puff = texture(u_SmokePuff, vWorldPos * u_SmokePuffScale).r;
        float density = mix(noise, puff, u_SmokePuffStrength);
        density = smoothstep(0.2, 1.0, density);

        float circle = softCircle(vTexCoord);
        float alpha = circle * density * (1.0 - vLife);
        alpha = smoothstep(0.0, max(0.001, u_SmokeSoftness), alpha) * u_SmokeOpacity;

        // Soft particles: fade when intersecting scene depth
        if (u_ScreenSize.x > 0.0 && u_ScreenSize.y > 0.0) {
            vec2 uv = gl_FragCoord.xy / u_ScreenSize;
            float sceneDepth = texture(u_SceneDepth, uv).r;
            float depthDiff = sceneDepth - gl_FragCoord.z;
            float depthFade = clamp(depthDiff * u_SmokeDepthFade, 0.0, 1.0);
            alpha *= depthFade;
        }

        if (alpha < 0.01) discard;
        vec3 color = mix(vColor.rgb * 0.2, vColor.rgb, 1.0 - density);
        FragColor = vec4(color, alpha);
        return;
    }

    // Choose shape
    float shape;
    if (u_SparkleShape == 1) {
        shape = starShape(vTexCoord, 4);
    } else if (u_SparkleShape == 2) {
        shape = diamondShape(vTexCoord);
    } else {
        shape = softCircle(vTexCoord);
    }

    // Discard fully transparent pixels
    if (shape < 0.01 || vColor.a < 0.01) {
        discard;
    }

    // Apply color with HDR intensity for bloom
    vec3 color = vColor.rgb * 2.0;  // HDR boost

    // Add extra glow in center
    float centerGlow = 1.0 - smoothstep(0.0, 0.3, length(vTexCoord - 0.5));
    color += vColor.rgb * centerGlow * 1.5;

    // Final alpha
    float alpha = shape * vColor.a;

    FragColor = vec4(color, alpha);
}
