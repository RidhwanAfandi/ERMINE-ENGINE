#version 460 core

// Indirect-only GI bake for probe cubemap array.
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 0) uniform image2DArray u_Output;
layout(rgba8, binding = 1) uniform image3D u_VoxelAlbedo;
layout(rgba8, binding = 2) uniform image3D u_VoxelEmissive;

uniform int u_ProbeBaseLayer; // base layer index in cubemap array (probeIndex * 6)
uniform vec3 u_ProbePosition;
uniform int u_Resolution;
uniform int u_Bounces;
uniform float u_EnergyLoss;
uniform vec3 u_VoxelBoundsMin;
uniform vec3 u_VoxelBoundsMax;
uniform int u_VoxelResolution;

vec3 FaceDirection(int face, vec2 uv)
{
    // uv in [-1, 1]
    if (face == 0) return normalize(vec3( 1.0, -uv.y, -uv.x)); // +X
    if (face == 1) return normalize(vec3(-1.0, -uv.y,  uv.x)); // -X
    if (face == 2) return normalize(vec3( uv.x,  1.0,  uv.y)); // +Y
    if (face == 3) return normalize(vec3( uv.x, -1.0, -uv.y)); // -Y
    if (face == 4) return normalize(vec3( uv.x, -uv.y,  1.0)); // +Z
    return normalize(vec3(-uv.x, -uv.y, -1.0));                // -Z
}

// Hash for RNG
uint Hash(uvec3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v ^= v >> 16;
    return v.x ^ v.y ^ v.z;
}

float Rand(inout uint state)
{
    state = state * 1664525u + 1013904223u;
    return float(state & 0x00FFFFFFu) / 16777216.0;
}

vec3 RandomDirection(inout uint state)
{
    // Uniform on sphere
    float z = Rand(state) * 2.0 - 1.0;
    float a = Rand(state) * 6.28318530718;
    float r = sqrt(max(0.0, 1.0 - z * z));
    return vec3(r * cos(a), r * sin(a), z);
}

bool RayBoxIntersect(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tmin, out float tmax)
{
    vec3 invD = 1.0 / rd;
    vec3 t0 = (bmin - ro) * invD;
    vec3 t1 = (bmax - ro) * invD;
    vec3 tsmaller = min(t0, t1);
    vec3 tbigger = max(t0, t1);
    tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    tmax = min(min(tbigger.x, tbigger.y), tbigger.z);
    return tmax >= max(tmin, 0.0);
}

bool TraceVoxel(vec3 ro, vec3 rd, out vec3 hitPos, out vec3 albedo, out vec3 emissive)
{
    float tmin, tmax;
    if (!RayBoxIntersect(ro, rd, u_VoxelBoundsMin, u_VoxelBoundsMax, tmin, tmax))
        return false;

    vec3 boundsSize = max(u_VoxelBoundsMax - u_VoxelBoundsMin, vec3(1e-5));
    vec3 voxelSize = boundsSize / float(u_VoxelResolution);

    float t = max(tmin, 0.0);
    vec3 p = ro + rd * t;
    vec3 voxelPos = (p - u_VoxelBoundsMin) / voxelSize;

    ivec3 voxel = ivec3(clamp(floor(voxelPos), vec3(0.0), vec3(u_VoxelResolution - 1)));
    ivec3 step = ivec3(sign(rd));
    vec3 tDelta = abs(voxelSize / rd);

    vec3 nextBoundary = (vec3(voxel) + step * 0.5 + 0.5) * voxelSize + u_VoxelBoundsMin;
    vec3 tMax = (nextBoundary - ro) / rd;

    int maxSteps = u_VoxelResolution * 3;
    for (int i = 0; i < maxSteps; ++i)
    {
        if (voxel.x < 0 || voxel.y < 0 || voxel.z < 0 ||
            voxel.x >= u_VoxelResolution || voxel.y >= u_VoxelResolution || voxel.z >= u_VoxelResolution)
            break;

        vec4 a = imageLoad(u_VoxelAlbedo, voxel);
        vec4 e = imageLoad(u_VoxelEmissive, voxel);
        if (a.a > 0.5 || e.a > 0.5) {
            hitPos = u_VoxelBoundsMin + (vec3(voxel) + 0.5) * voxelSize;
            albedo = a.rgb;
            emissive = e.rgb;
            return true;
        }

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                voxel.x += step.x;
                tMax.x += tDelta.x;
            } else {
                voxel.z += step.z;
                tMax.z += tDelta.z;
            }
        } else {
            if (tMax.y < tMax.z) {
                voxel.y += step.y;
                tMax.y += tDelta.y;
            } else {
                voxel.z += step.z;
                tMax.z += tDelta.z;
            }
        }
    }

    return false;
}

void main()
{
    ivec3 gid = ivec3(gl_GlobalInvocationID);
    if (gid.x >= u_Resolution || gid.y >= u_Resolution || gid.z >= 6)
        return;

    int face = gid.z;
    int layer = u_ProbeBaseLayer + face;

    vec2 uv = (vec2(gid.xy) + 0.5) / float(u_Resolution);
    uv = uv * 2.0 - 1.0;

    vec3 dir = FaceDirection(face, uv);
    vec3 origin = u_ProbePosition;

    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    uint rng = Hash(uvec3(gid.xy, face));

    for (int bounce = 0; bounce < u_Bounces; ++bounce) {
        vec3 hitPos;
        vec3 hitAlbedo;
        vec3 hitEmissive;
        bool hit = TraceVoxel(origin, dir, hitPos, hitAlbedo, hitEmissive);
        if (!hit)
            break;

        // Only emissive contributes as light source
        radiance += throughput * hitEmissive;

        // Bounce: modulate by albedo (reflectance) and energy loss
        throughput *= hitAlbedo * u_EnergyLoss;
        origin = hitPos + dir * 0.001;
        dir = RandomDirection(rng);
    }

    imageStore(u_Output, ivec3(gid.xy, layer), vec4(radiance, 1.0));
}
