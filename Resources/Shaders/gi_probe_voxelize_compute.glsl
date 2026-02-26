#version 460 core
#extension GL_ARB_bindless_texture : require

// Voxelize scene into a probe-local 3D volume.
// RGBA8: rgb = albedo + emissive, a = occupancy (0/1).
// NOTE: This is a brute-force triangle AABB voxelizer

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(rgba8, binding = 0) uniform image3D u_VoxelAlbedo;
layout(rgba8, binding = 1) uniform image3D u_VoxelEmissive;
layout(rgba8, binding = 2) uniform image3D u_VoxelNormal;

uniform vec3 u_VoxelBoundsMin;
uniform vec3 u_VoxelBoundsMax;
uniform int u_VoxelResolution;
uniform int u_IndexCount;
uniform int u_FirstIndex;
uniform int u_BaseVertex;
uniform int u_VertexStride;         // stride in floats
uniform int u_VertexPositionOffset; // position offset in floats
uniform int u_VertexTexCoordOffset; // texcoord offset in floats
uniform mat4 u_ModelMatrix;
uniform vec3 u_MaterialAlbedo;
uniform vec3 u_MaterialEmissive;
uniform float u_MaterialEmissiveIntensity;
uniform vec2 u_MaterialUVScale;
uniform vec2 u_MaterialUVOffset;
uniform uint u_MaterialTextureFlags;
uniform int u_MaterialAlbedoMapIndex;

// Material texture flag bits (must match C++ MaterialTextureFlags enum)
const uint MAT_FLAG_ALBEDO_MAP = 1u << 0u;  // bit 0

layout(std430, binding = 6) readonly buffer IndexBuffer
{
    uint indices[];
};

// Vertex buffer is tightly packed: 11 floats per vertex
layout(std430, binding = 8) readonly buffer VertexBuffer
{
    float vdata[];
};

vec3 LoadPosition(uint vertexIndex)
{
    uint base = vertexIndex * uint(u_VertexStride) + uint(u_VertexPositionOffset);
    return vec3(vdata[base + 0], vdata[base + 1], vdata[base + 2]);
}

vec2 LoadTexCoord(uint vertexIndex)
{
    uint base = vertexIndex * uint(u_VertexStride) + uint(u_VertexTexCoordOffset);
    return vec2(vdata[base + 0], vdata[base + 1]);
}

// Bindless texture array SSBO - stores texture handles as uvec2 (64-bit split into two 32-bit values)
layout(std430, binding = 5) restrict readonly buffer TextureArrayBlock
{
    uvec2 textureHandles[];
};

void main()
{
    uint triId = gl_GlobalInvocationID.x;
    uint triCount = uint(u_IndexCount / 3);
    if (triId >= triCount)
        return;

    uint i0 = indices[u_FirstIndex + triId * 3 + 0] + uint(u_BaseVertex);
    uint i1 = indices[u_FirstIndex + triId * 3 + 1] + uint(u_BaseVertex);
    uint i2 = indices[u_FirstIndex + triId * 3 + 2] + uint(u_BaseVertex);

    vec3 p0 = LoadPosition(i0);
    vec3 p1 = LoadPosition(i1);
    vec3 p2 = LoadPosition(i2);

    vec3 w0 = vec3(u_ModelMatrix * vec4(p0, 1.0));
    vec3 w1 = vec3(u_ModelMatrix * vec4(p1, 1.0));
    vec3 w2 = vec3(u_ModelMatrix * vec4(p2, 1.0));

    vec3 triMin = min(w0, min(w1, w2));
    vec3 triMax = max(w0, max(w1, w2));

    vec3 boundsSize = max(u_VoxelBoundsMax - u_VoxelBoundsMin, vec3(1e-5));
    vec3 voxelSize = boundsSize / float(u_VoxelResolution);

    ivec3 vmin = ivec3(clamp(floor((triMin - u_VoxelBoundsMin) / voxelSize), vec3(0.0), vec3(u_VoxelResolution - 1)));
    ivec3 vmax = ivec3(clamp(floor((triMax - u_VoxelBoundsMin) / voxelSize), vec3(0.0), vec3(u_VoxelResolution - 1)));

    vec3 emissive = u_MaterialEmissive * u_MaterialEmissiveIntensity;
    vec3 albedo = clamp(u_MaterialAlbedo, 0.0, 1.0);
    if ((u_MaterialTextureFlags & MAT_FLAG_ALBEDO_MAP) != 0u && u_MaterialAlbedoMapIndex >= 0) {
        vec2 uv0 = LoadTexCoord(i0);
        vec2 uv1 = LoadTexCoord(i1);
        vec2 uv2 = LoadTexCoord(i2);
        vec2 uv = (uv0 + uv1 + uv2) * (1.0 / 3.0);
        vec2 transformedUV = uv * u_MaterialUVScale + u_MaterialUVOffset;
        vec3 albedoSample = texture(sampler2D(textureHandles[u_MaterialAlbedoMapIndex]), transformedUV).rgb;
        albedo *= albedoSample;
    }
    vec3 emissiveColor = clamp(emissive, 0.0, 1.0);
    float emissiveOccupied = (dot(emissiveColor, vec3(0.3333)) > 0.0001) ? 1.0 : 0.0;
    vec3 triNormal = normalize(cross(w1 - w0, w2 - w0));
    vec3 encodedNormal = clamp(triNormal * 0.5 + 0.5, 0.0, 1.0);

    for (int z = vmin.z; z <= vmax.z; ++z) {
        for (int y = vmin.y; y <= vmax.y; ++y) {
            for (int x = vmin.x; x <= vmax.x; ++x) {
                imageStore(u_VoxelAlbedo, ivec3(x, y, z), vec4(albedo, 1.0));
                if (emissiveOccupied > 0.5) {
                    imageStore(u_VoxelEmissive, ivec3(x, y, z), vec4(emissiveColor, 1.0));
                }
                imageStore(u_VoxelNormal, ivec3(x, y, z), vec4(encodedNormal, 1.0));
            }
        }
    }
}
