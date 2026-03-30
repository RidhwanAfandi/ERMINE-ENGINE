#version 460 core
#extension GL_ARB_bindless_texture : require

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D u_ColorTexture;
uniform uvec2 u_DepthHandle;     // Bindless depth texture handle from G-Buffer
uniform uvec2 u_VelocityHandle;  // Bindless velocity buffer handle (RT4, RG16F)

uniform float u_MotionBlurStrength;
uniform int u_NumSamples;
uniform int u_FirstFrame;  // Skip motion blur on first frame (no previous frame data)
uniform float u_NearClip;
uniform float u_FarClip;

float linearizeDepth(float depth)
{
    float zNDC = depth * 2.0 - 1.0;
    return (2.0 * u_NearClip * u_FarClip) /
           (u_FarClip + u_NearClip - zNDC * (u_FarClip - u_NearClip));
}

void main()
{
    vec3 color = texture(u_ColorTexture, TexCoord).rgb;

    // Skip on first frame
    if (u_FirstFrame == 1)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // Skip for skybox (depth = 1.0)
    sampler2D depthSampler = sampler2D(u_DepthHandle);
    float depth = texture(depthSampler, TexCoord).r;
    if (depth >= 1.0)
    {
        FragColor = vec4(color, 1.0);
        return;
    }
    float centerLinearDepth = linearizeDepth(depth);

    // Sample per-pixel velocity written by the GBuffer vertex shader.
    // Camera-attached objects (weapons, hands) have velocity = (0,0) by construction,
    // so they're never blurred without any explicit masking needed.
    sampler2D velocitySampler = sampler2D(u_VelocityHandle);
    vec2 velocity = texture(velocitySampler, TexCoord).rg * u_MotionBlurStrength;

    // Clamp UV-space velocity to avoid pathological long streaks.
    const float MAX_VEL = 0.15;
    float vLen = length(velocity);
    if (vLen > MAX_VEL) {
        velocity *= (MAX_VEL / vLen);
    }

    // Early-out if essentially stationary
    if (dot(velocity, velocity) < 0.000001)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // Sample along the velocity vector, centered on the current pixel
    int samples = clamp(u_NumSamples, 2, 24);
    vec3 blurredColor = color;
    float totalWeight = 1.0;

    for (int i = 0; i < samples; ++i)
    {
        float t = (float(i) / float(samples - 1)) - 0.5;  // [-0.5, 0.5]
        vec2 sampleUV = clamp(TexCoord + velocity * t, vec2(0.0), vec2(1.0));

        float sampleDepth = texture(depthSampler, sampleUV).r;
        if (sampleDepth >= 1.0)
            continue;

        float sampleLinearDepth = linearizeDepth(sampleDepth);
        float depthDiff = abs(sampleLinearDepth - centerLinearDepth);
        float depthThreshold = max(0.05, centerLinearDepth * 0.02);
        if (depthDiff > depthThreshold)
            continue;

        blurredColor += texture(u_ColorTexture, sampleUV).rgb;
        totalWeight += 1.0;
    }

    FragColor = vec4(blurredColor / totalWeight, 1.0);
}
