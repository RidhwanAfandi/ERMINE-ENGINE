/* Start Header ************************************************************************/
/*!
\file       PhysicDebugRenderer.h
\author     Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu
\date       Oct 15, 2025
\brief      This file contains the declaration of the PhysicsDebugRenderer structure.

            This class extends JPH::DebugRenderer to implement application-specific
            debug visualization of physics bodies, lines, triangles, and 3D text.
            It is used for rendering wireframes, collision shapes, and other debug
            information in the engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"
#define JPH_DEBUG_RENDERER
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRenderer.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Core/Reference.h>

namespace Ermine
{
	class MyDebugRenderer : public JPH::DebugRenderer
	{
	public:
        MyDebugRenderer() = default;

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
        void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override;

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
        void DrawTriangle(JPH::RVec3Arg v1, JPH::RVec3Arg v2, JPH::RVec3Arg v3, JPH::ColorArg color, ECastShadow castShadow) override;

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
        void DrawText3D(JPH::RVec3Arg pos, const std::string_view& string, JPH::ColorArg color, float height) override;

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
        DebugRenderer::Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;

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
        DebugRenderer::Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;

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
        void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode = ECullMode::CullBackFace, ECastShadow inCastShadow = ECastShadow::On, EDrawMode inDrawMode = EDrawMode::Solid) override;
        
    };
}