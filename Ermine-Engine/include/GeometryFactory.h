/* Start Header ************************************************************************/
/*!
\file       GeometryFactory.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       15/03/2025
\brief      This reflects the brief

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Components.h"
#include "Math.h"

namespace Ermine::graphics
{
    /**
     * @biref a static class that generate geometry
     */
    class GeometryFactory
    {
        struct Vertex
        {
            Vec3 pos;
            Vec3 norms;
            Vec2 tex;
        };
        GeometryFactory() = default;
        ~GeometryFactory() = default;

        /**
         * @brief Calculate tangents for a mesh using the indices and vertices
         * @param vertices The vertex data (position, normal, texCoord)
         * @param indices The index data forming triangles
         * @return Vector of tangents, one per vertex
         */
        static std::vector<glm::vec3> CalculateTangents(
            const std::vector<Vertex>& vertices,
            const std::vector<unsigned int>& indices);

    public:
        static GeometryFactory& GetInstance()
        {
            static GeometryFactory instance;
            return instance;
        }

        /**
         * @brief Create a cube
         * 
         * @param width The width of the cube
         * @param height The height of the cube
         * @param depth The depth of the cube
         * @return Mesh The cube mesh
         */
        static Mesh CreateCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);

        /**
         * @brief Create a quad
         * 
         * @param width The width of the quad
         * @param height The height of the quad
         * @return Mesh The quad mesh
         */
        static Mesh CreateQuad(float width = 1.0f, float height = 1.0f);

        /**
         * @brief Create a sphere
         * 
         * @param radius The radius of the sphere
         * @param sectors The number of sectors
         * @param stacks The number of stacks
         * @return Mesh The sphere mesh
         */
        static Mesh CreateSphere(float radius = 1.0f, unsigned int sectors = 36, unsigned int stacks = 18);

        /**
         * @brief Create a cone
         * 
         * @param radius The radius of the cone base
         * @param height The height of the cone
         * @param sectors The number of sectors around the cone
         * @return Mesh The cone mesh
         */
        static Mesh CreateCone(float radius = 1.0f, float height = 2.0f, unsigned int sectors = 32);
    };
}
