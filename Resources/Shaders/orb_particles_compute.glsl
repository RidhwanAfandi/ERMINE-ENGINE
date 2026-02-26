#version 460

layout(local_size_x = 64) in;

struct Particle {
    vec4 position;      // xyz = position, w = life
    vec4 velocity;      // xyz = velocity, w = age
    vec4 color;         // rgba
    vec4 params;        // x = sizeStart, y = sizeEnd, z = randomSeed, w = unused
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

layout(std430, binding = 1) buffer SpawnCounterBuffer {
    uint spawned;
};

uniform float u_DeltaTime;
uniform float u_Time;
uniform vec3 u_EmitterPosition;
uniform int u_MaxParticles;
uniform vec3 u_EmitterRight;
uniform vec3 u_EmitterUp;
uniform vec3 u_EmitterForward;
uniform vec3 u_EmitterScale;

uniform int u_EmissionShape;     // 0 = point, 1 = sphere, 2 = box, 3 = disc (XZ)
uniform vec3 u_SpawnBoxExtents;
uniform float u_SpawnRadius;
uniform float u_SpawnRadiusInner;
uniform uint u_SpawnCount;       // per-frame spawn budget

uniform int u_DirectionMode;     // 0 = fixed, 1 = cone, 2 = from spawn, 3 = random sphere
uniform vec3 u_Direction;
uniform float u_ConeAngle;       // degrees
uniform float u_ConeInnerAngle;  // degrees

uniform int u_BoundsMode;        // 0 = none, 1 = kill, 2 = clamp, 3 = bounce
uniform int u_BoundsShape;       // 0 = sphere, 1 = box, 2 = disc (XZ)
uniform vec3 u_BoundsBoxExtents;
uniform float u_BoundsRadius;
uniform float u_BoundsRadiusInner;
uniform float u_SpeedMin;
uniform float u_SpeedMax;
uniform vec3 u_Gravity;
uniform float u_Drag;
uniform float u_TurbulenceStrength;
uniform float u_TurbulenceScale;

uniform vec3 u_ColorStart;
uniform vec3 u_ColorEnd;
uniform float u_AlphaStart;
uniform float u_AlphaEnd;
uniform float u_SizeStartMin;
uniform float u_SizeStartMax;
uniform float u_SizeEndMin;
uniform float u_SizeEndMax;
uniform float u_LifetimeMin;
uniform float u_LifetimeMax;

// Random hash functions
uint hash(uint x) {
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

float random(uint seed) {
    return float(hash(seed)) / 4294967295.0;
}

vec3 randomDirection(uint seed) {
    float theta = random(seed) * 6.28318530718;
    float phi = acos(2.0 * random(seed + 1u) - 1.0);
    return vec3(
        sin(phi) * cos(theta),
        sin(phi) * sin(theta),
        cos(phi)
    );
}

float randomRange(uint seed, float minV, float maxV) {
    return mix(minV, maxV, random(seed));
}

vec3 randomVec3(uint seed) {
    return vec3(
        random(seed),
        random(seed + 1u),
        random(seed + 2u)
    );
}

vec3 randomInSphere(uint seed, float innerRadius, float outerRadius) {
    vec3 dir = randomDirection(seed);
    float t = random(seed + 7u);
    float r = mix(innerRadius, outerRadius, pow(t, 1.0 / 3.0));
    return dir * r;
}

vec3 randomInBox(uint seed, vec3 extents) {
    return vec3(
        randomRange(seed, -extents.x, extents.x),
        randomRange(seed + 1u, -extents.y, extents.y),
        randomRange(seed + 2u, -extents.z, extents.z)
    );
}

vec3 randomInDiscXZ(uint seed, float innerRadius, float outerRadius) {
    float angle = random(seed) * 6.28318530718;
    float t = random(seed + 3u);
    float r = mix(innerRadius, outerRadius, sqrt(t));
    return vec3(cos(angle) * r, 0.0, sin(angle) * r);
}

vec3 buildBasisTangent(vec3 n) {
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    return normalize(cross(up, n));
}

vec3 randomDirectionInCone(uint seed, vec3 baseDir, float innerAngleDeg, float outerAngleDeg) {
    float innerRad = radians(clamp(innerAngleDeg, 0.0, outerAngleDeg));
    float outerRad = radians(max(outerAngleDeg, 0.0));
    float cosOuter = cos(outerRad);
    float cosInner = cos(innerRad);

    float u = random(seed);
    float v = random(seed + 1u);
    float cosTheta = mix(cosOuter, cosInner, u);
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = v * 6.28318530718;

    vec3 t = buildBasisTangent(baseDir);
    vec3 b = cross(baseDir, t);
    return normalize(t * (cos(phi) * sinTheta) + b * (sin(phi) * sinTheta) + baseDir * cosTheta);
}

vec3 localToWorld(vec3 local) {
    return u_EmitterPosition
        + u_EmitterRight * local.x
        + u_EmitterUp * local.y
        + u_EmitterForward * local.z;
}

bool applyBounds(inout vec3 pos, inout vec3 vel, vec3 origin) {
    if (u_BoundsMode == 0) {
        return false;
    }

    if (u_BoundsShape == 1) {
        vec3 local = pos - origin;
        vec3 minB = -u_BoundsBoxExtents;
        vec3 maxB = u_BoundsBoxExtents;

        bool outside = any(lessThan(local, minB)) || any(greaterThan(local, maxB));
        if (!outside) return false;

        if (u_BoundsMode == 1) {
            return true;
        }

        if (u_BoundsMode == 2) {
            local = clamp(local, minB, maxB);
            pos = origin + local;
            return false;
        }

        // Bounce
        if (local.x < minB.x || local.x > maxB.x) vel.x = -vel.x;
        if (local.y < minB.y || local.y > maxB.y) vel.y = -vel.y;
        if (local.z < minB.z || local.z > maxB.z) vel.z = -vel.z;
        local = clamp(local, minB, maxB);
        pos = origin + local;
        return false;
    }

    if (u_BoundsShape == 2) {
        vec3 local = pos - origin;
        vec2 xz = vec2(local.x, local.z);
        float r = length(xz);
        float innerR = min(u_BoundsRadiusInner, u_BoundsRadius);
        bool outside = (r > u_BoundsRadius) || (r < innerR && innerR > 0.0);
        if (!outside) return false;

        if (u_BoundsMode == 1) {
            return true;
        }

        if (u_BoundsMode == 2) {
            float clampedR = clamp(r, innerR, u_BoundsRadius);
            if (r > 0.0001) {
                xz = xz / r * clampedR;
                local.x = xz.x;
                local.z = xz.y;
                pos = origin + local;
            }
            return false;
        }

        // Bounce: reflect radial velocity in XZ
        if (r > 0.0001) {
            vec2 n = xz / r;
            vec2 v = vec2(vel.x, vel.z);
            v = v - 2.0 * dot(v, n) * n;
            vel.x = v.x;
            vel.z = v.y;
        }
        float clampedR = clamp(r, innerR, u_BoundsRadius);
        if (r > 0.0001) {
            xz = xz / r * clampedR;
            local.x = xz.x;
            local.z = xz.y;
            pos = origin + local;
        }
        return false;
    }

    // Sphere bounds
    vec3 local = pos - origin;
    float r = length(local);
    float innerR = min(u_BoundsRadiusInner, u_BoundsRadius);
    bool outside = (r > u_BoundsRadius) || (r < innerR && innerR > 0.0);
    if (!outside) return false;

    if (u_BoundsMode == 1) {
        return true;
    }

    if (u_BoundsMode == 2) {
        float clampedR = clamp(r, innerR, u_BoundsRadius);
        if (r > 0.0001) {
            local = local / r * clampedR;
            pos = origin + local;
        }
        return false;
    }

    // Bounce: reflect velocity along normal
    if (r > 0.0001) {
        vec3 n = local / r;
        vel = vel - 2.0 * dot(vel, n) * n;
    }
    float clampedR = clamp(r, innerR, u_BoundsRadius);
    if (r > 0.0001) {
        local = local / r * clampedR;
        pos = origin + local;
    }
    return false;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_MaxParticles) return;

    Particle p = particles[idx];
    float life = p.position.w;
    float age = p.velocity.w;

    // Check if particle is dead
    if (age >= life) {
        if (u_SpawnCount == 0u) {
            p.color.a = 0.0;
            particles[idx] = p;
            return;
        }

        uint spawnIndex = atomicAdd(spawned, 1u);
        if (spawnIndex >= u_SpawnCount) {
            p.color.a = 0.0;
            particles[idx] = p;
            return;
        }

        // Respawn particle
        uint seed = idx + uint(u_Time * 1000.0);

        // Spawn position inside chosen volume
        vec3 localPos = vec3(0.0);
        float innerRadius = min(u_SpawnRadiusInner, u_SpawnRadius);
        if (u_EmissionShape == 2) {
            localPos = randomInBox(seed, u_SpawnBoxExtents);
        } else if (u_EmissionShape == 3) {
            localPos = randomInDiscXZ(seed, innerRadius, u_SpawnRadius);
        } else if (u_EmissionShape == 1) {
            localPos = randomInSphere(seed, innerRadius, u_SpawnRadius);
        } else {
            localPos = vec3(0.0);
        }

        vec3 scaledLocal = localPos * u_EmitterScale;
        vec3 pos = localToWorld(scaledLocal);

        // Random lifetime and size
        float lifetime = mix(u_LifetimeMin, u_LifetimeMax, random(seed + 4u));
        float sizeStart = mix(u_SizeStartMin, u_SizeStartMax, random(seed + 5u));
        float sizeEnd = mix(u_SizeEndMin, u_SizeEndMax, random(seed + 6u));
        float speed = mix(u_SpeedMin, u_SpeedMax, random(seed + 7u));

        // Direction setup (local)
        vec3 baseDir = normalize(u_Direction);
        if (length(baseDir) < 0.0001) {
            baseDir = vec3(0.0, 0.0, 1.0);
        }

        vec3 localDir = baseDir;
        if (u_DirectionMode == 1) {
            localDir = randomDirectionInCone(seed + 12u, baseDir, u_ConeInnerAngle, u_ConeAngle);
        } else if (u_DirectionMode == 2) {
            localDir = normalize(localPos);
            if (length(localPos) < 0.0001) {
                localDir = randomDirection(seed + 12u);
            }
        } else if (u_DirectionMode == 3) {
            localDir = randomDirection(seed + 12u);
        }

        // Transform direction into world space using emitter basis
        vec3 worldDir = normalize(
            u_EmitterRight * localDir.x +
            u_EmitterUp * localDir.y +
            u_EmitterForward * localDir.z
        );

        p.velocity = vec4(worldDir * speed, 0.0);
        p.params = vec4(sizeStart, sizeEnd, random(seed + 9u), 0.0);

        // Store particle
        p.position = vec4(pos, lifetime);
        p.velocity.w = 0.0;  // age = 0
        p.color = vec4(u_ColorStart, u_AlphaStart);
    }
    else {
        // Update existing particle
        age += u_DeltaTime;
        float lifeRatio = age / life;

        // Integrate velocity
        p.velocity.xyz += u_Gravity * u_DeltaTime;
        if (u_Drag > 0.0) {
            float dragFactor = max(0.0, 1.0 - u_Drag * u_DeltaTime);
            p.velocity.xyz *= dragFactor;
        }

        if (u_TurbulenceStrength > 0.0) {
            float t = u_Time * 0.5;
            float seed = p.params.z * 57.0 + float(idx);
            vec3 noise = vec3(
                sin(t + seed + p.position.x * u_TurbulenceScale),
                cos(t * 1.3 + seed + p.position.y * u_TurbulenceScale),
                sin(t * 0.7 + seed + p.position.z * u_TurbulenceScale)
            );
            p.velocity.xyz += noise * u_TurbulenceStrength * u_DeltaTime;
        }

        p.position.xyz += p.velocity.xyz * u_DeltaTime;

        p.velocity.w = age;

        // Bounds handling
        vec3 vel = p.velocity.xyz;
        bool kill = applyBounds(p.position.xyz, vel, u_EmitterPosition);
        p.velocity.xyz = vel;
        if (kill) {
            p.velocity.w = p.position.w;
            p.color.a = 0.0;
            particles[idx] = p;
            return;
        }

        // Color and alpha over lifetime
        p.color.rgb = mix(u_ColorStart, u_ColorEnd, lifeRatio);
        p.color.a = mix(u_AlphaStart, u_AlphaEnd, lifeRatio);
    }

    particles[idx] = p;
}
