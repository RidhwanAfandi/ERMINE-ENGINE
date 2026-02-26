/* Start Header ************************************************************************/
/*!
\file       PhysicDebugRenderer.cpp
\author     Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu
\date       Oct 15, 2025
\brief      This file contains the definition of the PhysicsDebugRenderer structure.

            This class extends JPH::DebugRenderer to implement application-specific
            debug visualization of physics bodies, lines, triangles, and 3D text.
            It is used for rendering wireframes, collision shapes, and other debug
            information in the engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "PhysicDebugRenderer.h"
#include "MathVector.h"
#include "Renderer.h"
#include "ECS.h"

using namespace JPH;
namespace Ermine
{
    /*!*************************************************************************
      \brief
        Draws a line between two points in 3D space with a specified color.
      \param[in] from
        Starting point of the line.
      \param[in] to
        Ending point of the line.
      \param[in] color
        Color of the line.
    ***************************************************************************/
    void MyDebugRenderer::DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color)
    {
        const auto& renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
        glm::vec3 col(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
        renderer->SubmitDebugLine(
            glm::vec3((float)from.GetX(), (float)from.GetY(), (float)from.GetZ()),
            glm::vec3((float)to.GetX(), (float)to.GetY(), (float)to.GetZ()),
            col
        );
    }

    /*!*************************************************************************
      \brief
        Transforms a 3D point using a 4x4 transformation matrix.
      \param[in] M
        The 4x4 transformation matrix to apply.
      \param[in] p
        The point to transform, provided as a JPH::Float3.
      \return
        The transformed point as a JPH::RVec3.
    ***************************************************************************/
    static inline JPH::RVec3 Txf(JPH::RMat44Arg M, const JPH::Float3& p) {
        return M * JPH::RVec3(p.x, p.y, p.z);
    }

    /*!*************************************************************************
      \brief
        Draws a triangle given three vertices, with optional shadow casting.
      \param[in] v1
        First vertex of the triangle.
      \param[in] v2
        Second vertex of the triangle.
      \param[in] v3
        Third vertex of the triangle.
      \param[in] color
        Color of the triangle.
      \param[in] castShadow
        Determines if the triangle should cast a shadow.
    ***************************************************************************/
    void MyDebugRenderer::DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color, ECastShadow castShadow)
    {
        (void)castShadow;
        // Draw edges of triangle as lines
        DrawLine(v1, v2, color);
        DrawLine(v2, v3, color);
        DrawLine(v3, v1, color);
    }

    /*!*************************************************************************
      \brief
        Draws 3D text at a given position in the world.
      \param[in] pos
        Position of the text in 3D space.
      \param[in] string
        The text string to draw.
      \param[in] color
        Color of the text.
      \param[in] height
        Height of the text in world units.
    ***************************************************************************/
    void MyDebugRenderer::DrawText3D(RVec3Arg, const std::string_view&, ColorArg, float)
    {
     
    }

    /*!*************************************************************************
      \brief
        Creates a batch of triangles for rendering. Used to optimize repeated drawing.
      \param[in] inTriangles
        Array of triangles to batch.
      \param[in] inTriangleCount
        Number of triangles in the array.
      \return
        A DebugRenderer::Batch handle for batched rendering.
    ***************************************************************************/
    JPH::DebugRenderer::Batch
        MyDebugRenderer::CreateTriangleBatch(const Triangle*, int)
    {
        return {};
    }

    /*!*************************************************************************
      \brief
        Creates a batch of triangles from a set of vertices and indices.
      \param[in] inVertices
        Array of vertices.
      \param[in] inVertexCount
        Number of vertices.
      \param[in] inIndices
        Array of triangle indices.
      \param[in] inIndexCount
        Number of indices.
      \return
        A DebugRenderer::Batch handle for batched rendering.
    ***************************************************************************/
    JPH::DebugRenderer::Batch
        MyDebugRenderer::CreateTriangleBatch(const Vertex* , int ,
            const JPH::uint32* , int )
    {
        return {};
    }

    /*!*************************************************************************
      \brief
        Draws arbitrary geometry with a given transformation, bounding box, and rendering settings.
      \param[in] inModelMatrix
        Model transformation matrix.
      \param[in] inWorldSpaceBounds
        Axis-aligned bounding box of the geometry in world space.
      \param[in] inLODScaleSq
        Level-of-detail scaling factor.
      \param[in] inModelColor
        Color of the model.
      \param[in] inGeometry
        Geometry reference to draw.
      \param[in] inCullMode
        Specifies backface culling mode.
      \param[in] inCastShadow
        Determines if the geometry should cast a shadow.
      \param[in] inDrawMode
        Drawing mode (solid, wireframe, etc.).
    ***************************************************************************/
    void MyDebugRenderer::DrawGeometry(JPH::RMat44Arg ,
        const JPH::AABox&, float,
        JPH::ColorArg , const GeometryRef& ,
        ECullMode, ECastShadow, EDrawMode)
    {

    }
}