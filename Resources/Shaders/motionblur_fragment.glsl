#version 460 core
#extension GL_ARB_bindless_texture : require

in vec2 TexCoord;
out vec4 FragColor;

// Input textures
uniform sampler2D u_ColorTexture;
uniform uvec2 u_DepthHandle;   // Bindless depth texture handle from G-Buffer
uniform uvec2 u_GBuffer3Handle; // Bindless GBuffer3 handle for motion blur flag

// Motion blur parameters
uniform mat4 u_CurrentViewProjection;
uniform mat4 u_PreviousViewProjection;
uniform mat4 u_InvViewProjection;  // Inverse of current view-projection for world position reconstruction
uniform float u_MotionBlurStrength;
uniform int u_NumSamples;
uniform int u_FirstFrame;  // Skip motion blur on first frame

void main()
{
    // Sample the color from the input texture
    vec3 color = texture(u_ColorTexture, TexCoord).rgb;

    // Skip motion blur on first frame (no previous frame data)
    if (u_FirstFrame == 1)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // Get depth from G-Buffer using bindless texture
    sampler2D depthSampler = sampler2D(u_DepthHandle);
    float depth = texture(depthSampler, TexCoord).r;

    // Skip motion blur for skybox (depth = 1.0)
    if (depth >= 1.0)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // Sample motion blur flag from GBuffer3.a
    sampler2D gBuffer3Sampler = sampler2D(u_GBuffer3Handle);
    float motionBlurFlag = texture(gBuffer3Sampler, TexCoord).a;

    // Skip motion blur for camera-attached objects (flag = 1.0)
    if (motionBlurFlag > 0.5)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // Reconstruct viewport position H
    // H is in NDC space: x and y in [-1, 1], z is depth value, w = 1
    vec4 H = vec4(TexCoord.x * 2.0 - 1.0,
                  (1.0 - TexCoord.y) * 2.0 - 1.0,  // Flip Y for OpenGL
                  depth,
                  1.0);

    // Transform by inverse view-projection to get world position
    vec4 D = u_InvViewProjection * H;
    vec4 worldPos = D / D.w;  // Perspective divide to get actual world position

    // Current viewport position (already in NDC)
    vec4 currentPos = H;

    // Transform world position by previous frame's view-projection matrix
    vec4 previousPos = u_PreviousViewProjection * worldPos;
    previousPos /= previousPos.w;  // Perspective divide

    // Compute velocity in screen space
    // Velocity is the difference between current and previous positions
    vec2 velocity = (currentPos.xy - previousPos.xy) * 0.5;  // Scale to [0,1] range

    // Apply motion blur strength
    velocity *= u_MotionBlurStrength;

    // Early exit if velocity is too small (optimization)
    float velocityLength = length(velocity);
    if (velocityLength < 0.001)
    {
        FragColor = vec4(color, 1.0);
        return;
    }

    // Sample along the velocity vector to create motion blur
    // Start with the initial color
    vec3 blurredColor = color;
    vec2 texCoordVelocity = velocity / float(u_NumSamples);
    vec2 sampleCoord = TexCoord + texCoordVelocity;

    int validSamples = 1;

    for (int i = 1; i < u_NumSamples; ++i)
    {
        // Check if sample is within texture bounds
        if (sampleCoord.x >= 0.0 && sampleCoord.x <= 1.0 &&
            sampleCoord.y >= 0.0 && sampleCoord.y <= 1.0)
        {
            vec3 currentColor = texture(u_ColorTexture, sampleCoord).rgb;
            blurredColor += currentColor;
            validSamples++;
        }

        sampleCoord += texCoordVelocity;
    }

    // Average all samples
    blurredColor /= float(validSamples);

    FragColor = vec4(blurredColor, 1.0);
}
