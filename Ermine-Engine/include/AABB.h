/* Start Header ************************************************************************/
/*!
\file       AABB.h
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       Jan 25, 2025
\brief      Axis-Aligned Bounding Box structure for frustum culling optimization

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "MathVector.h"
#include "Matrix4x4.h"
#include <limits>

namespace Ermine
{
	/*!***********************************************************************
	\brief
	 Axis-Aligned Bounding Box structure
	*************************************************************************/
	struct AABB
	{
		Vec3 min{  std::numeric_limits<float>::max(),
				   std::numeric_limits<float>::max(),
				   std::numeric_limits<float>::max() };

		Vec3 max{ -std::numeric_limits<float>::max(),
				  -std::numeric_limits<float>::max(),
				  -std::numeric_limits<float>::max() };

		AABB() = default;
		AABB(const Vec3& minimum, const Vec3& maximum) : min(minimum), max(maximum) {}

		// Expand AABB to include a point
		void Expand(const Vec3& point)
		{
			min.x = (point.x < min.x) ? point.x : min.x;
			min.y = (point.y < min.y) ? point.y : min.y;
			min.z = (point.z < min.z) ? point.z : min.z;

			max.x = (point.x > max.x) ? point.x : max.x;
			max.y = (point.y > max.y) ? point.y : max.y;
			max.z = (point.z > max.z) ? point.z : max.z;
		}

		// Merge with another AABB
		void Merge(const AABB& other)
		{
			Expand(other.min);
			Expand(other.max);
		}

		// Transform AABB by a matrix (world transform)
		AABB Transform(const Mtx44& matrix) const
		{
			// Get 8 corners of the AABB
			Vec3 corners[8] = {
				Vec3(min.x, min.y, min.z),
				Vec3(max.x, min.y, min.z),
				Vec3(min.x, max.y, min.z),
				Vec3(max.x, max.y, min.z),
				Vec3(min.x, min.y, max.z),
				Vec3(max.x, min.y, max.z),
				Vec3(min.x, max.y, max.z),
				Vec3(max.x, max.y, max.z)
			};

			AABB result;
			for (int i = 0; i < 8; ++i)
			{
				// Transform corner using matrix multiplication
				// Convert Vec3 to homogeneous coordinates (w=1) for transformation
				float x = corners[i].x * matrix.m00 + corners[i].y * matrix.m01 + corners[i].z * matrix.m02 + matrix.m03;
				float y = corners[i].x * matrix.m10 + corners[i].y * matrix.m11 + corners[i].z * matrix.m12 + matrix.m13;
				float z = corners[i].x * matrix.m20 + corners[i].y * matrix.m21 + corners[i].z * matrix.m22 + matrix.m23;
				
				result.Expand(Vec3(x, y, z));
			}

			return result;
		}

		// Get center point
		Vec3 GetCenter() const
		{
			return Vec3(
				(min.x + max.x) * 0.5f,
				(min.y + max.y) * 0.5f,
				(min.z + max.z) * 0.5f
			);
		}

		// Get extents (half-sizes)
		Vec3 GetExtents() const
		{
			return Vec3(
				(max.x - min.x) * 0.5f,
				(max.y - min.y) * 0.5f,
				(max.z - min.z) * 0.5f
			);
		}

		// Check if AABB is valid (min < max)
		bool IsValid() const
		{
			return min.x <= max.x && min.y <= max.y && min.z <= max.z;
		}
	};
}
