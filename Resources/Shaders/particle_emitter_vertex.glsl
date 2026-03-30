#version 460

struct Particle {
    vec4 position;      // xyz = position, w = life
    vec4 velocity;      // xyz = velocity, w = age
    vec4 color;         // rgba
    vec4 params;        // x = sizeStart, y = sizeEnd, z = randomSeed, w = unused
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;
uniform int u_RenderMode;    // 0 = glow, 1 = smoke
uniform float u_SmokeStretch;
uniform float u_SmokeUpBias;

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;
out vec4 vColor;
out float vLife;
out vec3 vWorldPos;

void main() {
    uint particleIdx = gl_InstanceID;

    // Skip dead particles (fully transparent)
    Particle p = particles[particleIdx];

    if (p.color.a <= 0.0) {
        gl_Position = vec4(0.0, 0.0, -1000.0, 1.0);
        vTexCoord = vec2(0.0);
        vColor = vec4(0.0);
        vLife = 0.0;
        return;
    }

    float lifeRatio = clamp(p.velocity.w / max(0.0001, p.position.w), 0.0, 1.0);
    float size = mix(p.params.x, p.params.y, lifeRatio);
    float speed = length(p.velocity.xyz);
    vec2 quadPos = aPosition * size;
    if (u_RenderMode == 1) {
        float stretch = 1.0 + speed * u_SmokeStretch;
        quadPos.y *= stretch;
    }

    // Billboard: face camera (or lay flat for shockwave)
    vec3 worldPos;
    if (u_RenderMode == 3) {
        // Horizontal orientation: expand on XZ plane (ring lies flat on the ground)
        worldPos = p.position.xyz
                 + vec3(1.0, 0.0, 0.0) * quadPos.x
                 + vec3(0.0, 0.0, 1.0) * quadPos.y;
    } else {
        worldPos = p.position.xyz
                 + u_CameraRight * quadPos.x
                 + u_CameraUp * quadPos.y;
        if (u_RenderMode == 1) {
            worldPos += vec3(0.0, 1.0, 0.0) * speed * u_SmokeUpBias;
        }
    }

    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);

    vTexCoord = aTexCoord;
    vColor = p.color;
    vLife = lifeRatio;  // age / life = normalized lifetime
    vWorldPos = p.position.xyz;
}
