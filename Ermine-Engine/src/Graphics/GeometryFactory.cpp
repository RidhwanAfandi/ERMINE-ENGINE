/* Start Header ************************************************************************/
/*!
\file       GeometryFactory.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       15/03/2025
\brief      This reflects the brief

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "GeometryFactory.h"
#include "MathUtils.h"
#include "Renderer.h"
#include "ECS.h"

using namespace Ermine::graphics;

/**
 * @brief Calculate tangents for a mesh using the indices and vertices
 * Uses the Lengyel's Method for computing tangent space basis vectors
 * @param vertices The vertex data (position, normal, texCoord)
 * @param indices The index data forming triangles
 * @return Vector of tangents, one per vertex
 */
std::vector<glm::vec3> GeometryFactory::CalculateTangents(
    const std::vector<Vertex>& vertices,
    const std::vector<unsigned int>& indices)
{
    std::vector<glm::vec3> tangents(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(vertices.size(), glm::vec3(0.0f));

    // Calculate tangent and bitangent for each triangle
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];

        const Vec3& v0 = vertices[i0].pos;
        const Vec3& v1 = vertices[i1].pos;
        const Vec3& v2 = vertices[i2].pos;

        const Vec2& uv0 = vertices[i0].tex;
        const Vec2& uv1 = vertices[i1].tex;
        const Vec2& uv2 = vertices[i2].tex;

        // Calculate edges
        glm::vec3 edge1 = glm::vec3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        glm::vec3 edge2 = glm::vec3(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);

        glm::vec2 deltaUV1 = glm::vec2(uv1.x - uv0.x, uv1.y - uv0.y);
        glm::vec2 deltaUV2 = glm::vec2(uv2.x - uv0.x, uv2.y - uv0.y);

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        glm::vec3 bitangent;
        bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

        // Accumulate tangents and bitangents for each vertex of the triangle
        tangents[i0] += tangent;
        tangents[i1] += tangent;
        tangents[i2] += tangent;

        bitangents[i0] += bitangent;
        bitangents[i1] += bitangent;
        bitangents[i2] += bitangent;
    }

    // Orthogonalize and normalize tangents using Gram-Schmidt process
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        glm::vec3 n = glm::vec3(vertices[i].norms.x, vertices[i].norms.y, vertices[i].norms.z);
        glm::vec3 t = tangents[i];

        // Gram-Schmidt orthogonalize
        t = glm::normalize(t - n * glm::dot(n, t));

        // Calculate handedness (optional, for bitangent calculation)
        // float handedness = (glm::dot(glm::cross(n, t), bitangents[i]) < 0.0f) ? -1.0f : 1.0f;

        tangents[i] = t;
    }

    return tangents;
}

/**
 * @brief Create a cube
 * 
 * @param width The width of the cube
 * @param height The height of the cube
 * @param depth The depth of the cube
 * @return Mesh The cube mesh
 */
Ermine::Mesh GeometryFactory::CreateCube(float width, float height, float depth)
{
    float w = width * 0.5f;
    float h = height * 0.5f;
    float d = depth * 0.5f;

    std::vector<Vertex> vertices = {
        // Front face
        {{-w, -h,  d}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // 0
        {{ w, -h,  d}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // 1
        {{ w,  h,  d}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 2
        {{-w,  h,  d}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 3
    
        // Back face
        {{ w, -h, -d}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // 4
        {{-w, -h, -d}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}}, // 5
        {{-w,  h, -d}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}, // 6
        {{ w,  h, -d}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}}, // 7
    
        // Top face
        {{-w,  h,  d}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // 8
        {{ w,  h,  d}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 9
        {{ w,  h, -d}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 10
        {{-w,  h, -d}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 11
    
        // Bottom face
        {{-w, -h, -d}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // 12
        {{ w, -h, -d}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // 13
        {{ w, -h,  d}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}, // 14
        {{-w, -h,  d}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, // 15
    
        // Right face
        {{ w, -h,  d}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 16
        {{ w, -h, -d}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 17
        {{ w,  h, -d}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 18
        {{ w,  h,  d}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 19
    
        // Left face
        {{-w, -h, -d}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 20
        {{-w, -h,  d}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 21
        {{-w,  h,  d}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 22
        {{-w,  h, -d}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}  // 23
    };

    std::vector<unsigned int> indices = {
        // Front face
        0, 1, 2, 2, 3, 0,
    
        // Back face
        4, 5, 6, 6, 7, 4,
    
        // Top face
        8, 9, 10, 10, 11, 8,
    
        // Bottom face
        12, 13, 14, 14, 15, 12,
    
        // Right face
        16, 17, 18, 18, 19, 16,
    
        // Left face
        20, 21, 22, 22, 23, 20
    };

    auto vao = std::make_shared<VertexArray>();
    vao->SetVertexCount(vertices.size());

    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));

    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, norms));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, tex));
    vbo->Unbind();

    auto ibo = std::make_shared<IndexBuffer>(indices.data(), indices.size() * sizeof(unsigned int));

    vao->Unbind();

	auto mesh = Mesh(vao, vbo, ibo);
	mesh.kind = MeshKind::Primitive;
	mesh.primitive.type = "Cube";
	mesh.primitive.size = Vec3{ width, height, depth };

	// Set AABB for frustum culling (cube extents are half-sizes)
	mesh.aabbMin = Vec3{ -w, -h, -d };
	mesh.aabbMax = Vec3{  w,  h,  d };

    // Register mesh with MeshManager for indirect rendering
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        // Calculate tangents
        std::vector<glm::vec3> tangents = CalculateTangents(vertices, indices);

        // Convert local Vertex to MeshTypes::Vertex
        std::vector<graphics::Vertex> meshVertices;
        meshVertices.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            const auto& v = vertices[i];
            graphics::Vertex meshVert;
			meshVert.position.x = v.pos.x;
			meshVert.position.y = v.pos.y;
			meshVert.position.z = v.pos.z;
			meshVert.normal.x = v.norms.x;
			meshVert.normal.y = v.norms.y;
			meshVert.normal.z = v.norms.z;
			meshVert.texCoord.x = v.tex.x;
			meshVert.texCoord.y = v.tex.y;
            meshVert.tangent = tangents[i];
            meshVertices.push_back(meshVert);
        }

        std::string meshID = "Cube_" + std::to_string(width) + "x" +
                            std::to_string(height) + "x" + std::to_string(depth);
        renderer->m_MeshManager.RegisterMesh(meshVertices, indices, meshID);

        // Store the registered mesh ID in the Mesh component
        mesh.registeredMeshID = meshID;
    }

    return mesh;
}

/**
 * @brief Create a quad
 * 
 * @param width The width of the quad
 * @param height The height of the quad
 * @return Mesh The quad mesh
 */
Ermine::Mesh GeometryFactory::CreateQuad(float width, float height)
{
    float w = width * 0.5f;
    float h = height * 0.5f;

    std::vector<Vertex> vertices = {
        {{-w, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{ w, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{ w,  h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
            {{-w,  h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}
    };

    std::vector<unsigned int> indices = {
        0, 1, 2,
        2, 3, 0
    };

    auto vao = std::make_shared<VertexArray>();
	vao->SetVertexCount(vertices.size());
    
    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));

    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, norms));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, tex));
    vbo->Unbind();

    auto ibo = std::make_shared<IndexBuffer>(indices.data(), indices.size() * sizeof(unsigned int));

    vao->Unbind();

	auto mesh = Mesh(vao, vbo, ibo);
	mesh.kind = MeshKind::Primitive;
	mesh.primitive.type = "Quad";
	mesh.primitive.size = Vec3{ width, height, 0.0f };

	// Set AABB for frustum culling (quad extents are half-sizes)
	mesh.aabbMin = Vec3{ -w, -h, 0.0f };
	mesh.aabbMax = Vec3{  w,  h, 0.0f };

    // Register mesh with MeshManager for indirect rendering
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        // Calculate tangents
        std::vector<glm::vec3> tangents = CalculateTangents(vertices, indices);

        // Convert local Vertex to MeshTypes::Vertex
        std::vector<graphics::Vertex> meshVertices;
        meshVertices.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            const auto& v = vertices[i];
            graphics::Vertex meshVert;
            meshVert.position.x = v.pos.x;
            meshVert.position.y = v.pos.y;
            meshVert.position.z = v.pos.z;
            meshVert.normal.x = v.norms.x;
            meshVert.normal.y = v.norms.y;
            meshVert.normal.z = v.norms.z;
            meshVert.texCoord.x = v.tex.x;
            meshVert.texCoord.y = v.tex.y;
            meshVert.tangent = tangents[i];
            meshVertices.push_back(meshVert);
        }

        std::string meshID = "Quad_" + std::to_string(width) + "x" + std::to_string(height);
        renderer->m_MeshManager.RegisterMesh(meshVertices, indices, meshID);

        // Store the registered mesh ID in the Mesh component
        mesh.registeredMeshID = meshID;
    }

    return mesh;
}

Ermine::Mesh GeometryFactory::CreateSphere(float radius, unsigned int sectors, unsigned int stacks)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float lengthInv = 1.0f / radius;    // vertex normal

    float sectorStep = 2 * PI<float> / sectors;
    float stackStep = PI<float> / stacks;

    // Generate vertices
    for(unsigned int i = 0; i <= stacks; ++i)
    {
        float stackAngle = PI<float> / 2 - i * stackStep;      // starting from pi/2 to -pi/2
        float xy = radius * cosf(stackAngle);             // r * cos(u)
        float z = radius * sinf(stackAngle);              // r * sin(u)

        // Add (sectors+1) vertices per stack
        // The first and last vertices have same position and normal, but different tex coords
        for(unsigned int j = 0; j <= sectors; ++j)
        {
            float sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            float x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            float y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)

            // normalized vertex normal (nx, ny, nz)
            float nx = x * lengthInv;
            float ny = y * lengthInv;
            float nz = z * lengthInv;

            // vertex tex coord (s, t) range between [0, 1]
            float s = (float)j / sectors;
            float t = (float)i / stacks;

            // Add vertex
            vertices.push_back({{x, y, z}, {nx, ny, nz}, {s, t}});
        }
    }

    // Generate indices
    unsigned int k1, k2;
    for(unsigned int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);     // beginning of current stack
        k2 = k1 + sectors + 1;      // beginning of next stack

        for(unsigned int j = 0; j < sectors; ++j, ++k1, ++k2) {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if(i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if(i != stacks-1) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    auto vao = std::make_shared<VertexArray>();
    vao->SetVertexCount(vertices.size());
    
    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));

    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, norms));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, tex));
    vbo->Unbind();

    auto ibo = std::make_shared<IndexBuffer>(indices.data(), indices.size() * sizeof(unsigned int));

    vao->Unbind();

	auto mesh = Mesh(vao, vbo, ibo);
	mesh.kind = MeshKind::Primitive;
	mesh.primitive.type = "Sphere";
	mesh.primitive.size = Vec3{ radius, radius, radius };

	// Set AABB for frustum culling (sphere is contained within a cube of side length 2*radius)
	mesh.aabbMin = Vec3{ -radius, -radius, -radius };
	mesh.aabbMax = Vec3{  radius,  radius,  radius };

    // Register mesh with MeshManager for indirect rendering
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        // Calculate tangents
        std::vector<glm::vec3> tangents = CalculateTangents(vertices, indices);

        // Convert local Vertex to MeshTypes::Vertex
        std::vector<graphics::Vertex> meshVertices;
        meshVertices.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            const auto& v = vertices[i];
            graphics::Vertex meshVert;
            meshVert.position.x = v.pos.x;
            meshVert.position.y = v.pos.y;
            meshVert.position.z = v.pos.z;
            meshVert.normal.x = v.norms.x;
            meshVert.normal.y = v.norms.y;
            meshVert.normal.z = v.norms.z;
            meshVert.texCoord.x = v.tex.x;
            meshVert.texCoord.y = v.tex.y;
            meshVert.tangent = tangents[i];
            meshVertices.push_back(meshVert);
        }

        std::string meshID = "Sphere_" + std::to_string(radius) + "_" +
                            std::to_string(sectors) + "x" + std::to_string(stacks);
        renderer->m_MeshManager.RegisterMesh(meshVertices, indices, meshID);

        // Store the registered mesh ID in the Mesh component
        mesh.registeredMeshID = meshID;
    }

    return mesh;
}

/**
 * @brief Create a cone mesh
 * 
 * @param radius The radius of the cone base
 * @param height The height of the cone
 * @param sectors The number of sectors around the cone (default 32 for smooth appearance)
 * @return Mesh The cone mesh with smooth normals and proper UV mapping
 */
Ermine::Mesh GeometryFactory::CreateCone(float radius, float height, unsigned int sectors)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Reserve space to avoid reallocations
    vertices.reserve(sectors * 3 + 2);
    indices.reserve(sectors * 6);

    float sectorStep = 2 * PI<float> / sectors;
    
    // Calculate slant length for proper normal calculation
    float slantLength = sqrtf(radius * radius + height * height);

    // --- APEX VERTEX (Tip of the cone) ---
    // The apex will share multiple vertex entries (one per side triangle) for smooth normals
    unsigned int apexStartIndex = 0;
    
    // Create one apex vertex per sector for smooth normals along the sides
    for (unsigned int i = 0; i <= sectors; ++i)
    {
        float sectorAngle = i * sectorStep;
        
        // Calculate smooth surface normal at the apex for this sector
        // The normal points outward from the cone surface
        float normalY = radius / slantLength;      // Vertical component
        float normalXZ = height / slantLength;     // Horizontal component magnitude
        
        float nx = normalXZ * cosf(sectorAngle);
        float nz = normalXZ * sinf(sectorAngle);
        
        // UV for the apex (top center of the unwrapped cone)
        float u = (float)i / sectors;
        float v = 1.0f; // Top of the texture
        
        vertices.push_back({{0.0f, height, 0.0f}, {nx, normalY, nz}, {u, v}});
    }

    // --- BASE CENTER VERTEX (for bottom cap) ---
    unsigned int baseCenterIndex = vertices.size();
    vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}});

    // --- SIDE VERTICES (Base circle for cone sides) ---
    unsigned int sideBaseStartIndex = vertices.size();
    for (unsigned int i = 0; i <= sectors; ++i)
    {
        float sectorAngle = i * sectorStep;
        float x = radius * cosf(sectorAngle);
        float z = radius * sinf(sectorAngle);

        // Calculate smooth surface normal (perpendicular to cone surface)
        float normalY = radius / slantLength;
        float normalXZ = height / slantLength;
        
        float nx = normalXZ * cosf(sectorAngle);
        float nz = normalXZ * sinf(sectorAngle);

        // UV coordinates wrapping around the cone
        float u = (float)i / sectors;
        float v = 0.0f; // Base is at bottom of texture

        vertices.push_back({{x, 0.0f, z}, {nx, normalY, nz}, {u, v}});
    }

    // --- BOTTOM CAP VERTICES ---
    unsigned int bottomCapStartIndex = vertices.size();
    for (unsigned int i = 0; i <= sectors; ++i)
    {
        float sectorAngle = i * sectorStep;
        float x = radius * cosf(sectorAngle);
        float z = radius * sinf(sectorAngle);

        // Bottom face normals point straight down
        // Radial UV mapping for the bottom disc
        float u = 0.5f + 0.5f * cosf(sectorAngle);
        float v = 0.5f + 0.5f * sinf(sectorAngle);

        vertices.push_back({{x, 0.0f, z}, {0.0f, -1.0f, 0.0f}, {u, v}});
    }

    // --- GENERATE INDICES FOR CONE SIDES ---
    // Connect apex vertices to base circle vertices
    for (unsigned int i = 0; i < sectors; ++i)
    {
        unsigned int apex = apexStartIndex + i;
        unsigned int baseVertex = sideBaseStartIndex + i;
        unsigned int nextBaseVertex = sideBaseStartIndex + i + 1;
        
        // Triangle: apex -> current base -> next base (counter-clockwise winding)
        indices.push_back(apex);
        indices.push_back(baseVertex);
        indices.push_back(nextBaseVertex);
    }

    // --- GENERATE INDICES FOR BASE (Bottom cap) ---
    // Connect center to base circle vertices (clockwise for downward normal)
    for (unsigned int i = 0; i < sectors; ++i)
    {
        unsigned int current = bottomCapStartIndex + i;
        unsigned int next = bottomCapStartIndex + i + 1;
        
        // Triangle: center -> next -> current (clockwise winding for bottom face)
        indices.push_back(baseCenterIndex);
        indices.push_back(next);
        indices.push_back(current);
    }

    // Create VAO, VBO, IBO
    auto vao = std::make_shared<VertexArray>();
    vao->SetVertexCount(vertices.size());

    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));

    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, norms));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, tex));
    vbo->Unbind();

    auto ibo = std::make_shared<IndexBuffer>(indices.data(), indices.size() * sizeof(unsigned int));

    auto mesh = Mesh(vao, vbo, ibo);
    mesh.kind = MeshKind::Primitive;
    mesh.primitive.type = "Cone";
    mesh.primitive.size = Vec3{ radius * 2.0f, height, radius * 2.0f };

    // Set AABB for frustum culling
    mesh.aabbMin = Vec3{ -radius, 0.0f, -radius };
    mesh.aabbMax = Vec3{  radius, height,  radius };

    // Register mesh with MeshManager for indirect rendering
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        // Calculate tangents for normal mapping support
        std::vector<glm::vec3> tangents = CalculateTangents(vertices, indices);

        // Convert local Vertex to MeshTypes::Vertex
        std::vector<graphics::Vertex> meshVertices;
        meshVertices.reserve(vertices.size());
        for (size_t i = 0; i < vertices.size(); ++i) {
            const auto& v = vertices[i];
            graphics::Vertex meshVert;
            meshVert.position.x = v.pos.x;
            meshVert.position.y = v.pos.y;
            meshVert.position.z = v.pos.z;
            meshVert.normal.x = v.norms.x;
            meshVert.normal.y = v.norms.y;
            meshVert.normal.z = v.norms.z;
            meshVert.texCoord.x = v.tex.x;
            meshVert.texCoord.y = v.tex.y;
            meshVert.tangent = tangents[i];
            meshVertices.push_back(meshVert);
        }

        //physic
        mesh.cpuVertices.clear();
        mesh.cpuVertices.reserve(indices.size());

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            unsigned int i0 = indices[i + 0];
            unsigned int i1 = indices[i + 1];
            unsigned int i2 = indices[i + 2];

            const auto& v0 = vertices[i0].pos;
            const auto& v1 = vertices[i1].pos;
            const auto& v2 = vertices[i2].pos;

            mesh.cpuVertices.push_back(glm::vec3(v0.x, v0.y, v0.z));
            mesh.cpuVertices.push_back(glm::vec3(v1.x, v1.y, v1.z));
            mesh.cpuVertices.push_back(glm::vec3(v2.x, v2.y, v2.z));
        }

        std::string meshID = "Cone_" + std::to_string(radius) + "_" + std::to_string(height) + "_" + std::to_string(sectors);
        renderer->m_MeshManager.RegisterMesh(meshVertices, indices, meshID);
        //store in comp for physic
        // Store the registered mesh ID in the Mesh component
        mesh.registeredMeshID = meshID;
    }

    return mesh;
}
