/* Start Header ************************************************************************/
/*!
\file       MultiSelectionmanipulator.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       20/11/2025
\brief      This files contains the declaration for MultiSelectionManipulator
			Provides functionality to manipulate multiple selected entities with ImGuizmo

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "HierarchySystem.h"
#include "MathVector.h"
#include "Selection.h"

namespace Ermine
{
	class HierarchySystem;
}

namespace Ermine::editor
{
	struct GroupTransform
	{
		Vec3	pivotWorld{ 0,0,0 };
		Quaternion rotationWorld{ 0,0,0,1 };
		Vec3	averageScale{ 1,1,1 };
		Vec3	aabbMin{ 0,0,0 };
		Vec3	aabbMax{ 0,0,0 };
		bool	valid = false;
	};

	class MultiSelectionManipulator
	{
		// Quaternion and vector helpers (assuming you already have in MathUtils)
		static Quaternion QuaternionInverse(const Quaternion& q)
		{
			float len2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
			if (len2 <= 0.0f) return Quaternion(0, 0, 0, 1);
			return Quaternion(-q.x / len2, -q.y / len2, -q.z / len2, q.w / len2);
		}

		static Vec3 RotateVector(const Quaternion& q, const Vec3& v)
		{
			// q * v * q^{-1}
			Quaternion qv(v.x, v.y, v.z, 0);
			Quaternion inv = QuaternionInverse(q);
			Quaternion res = q * qv * inv;
			return Vec3(res.x, res.y, res.z);
		}
	public:
        // Build group aggregate (world-space)
        static GroupTransform BuildGroup()
        {
            GroupTransform gt;
            const auto& top = Selection::TopLevel();
            if (top.empty()) return gt;

            auto hs = ECS::GetInstance().GetSystem<HierarchySystem>();
            if (!hs) return gt;

            bool first = true;
            Vec3 sumPos{ 0,0,0 };
            Vec3 sumScale{ 0,0,0 };

            for (auto e : top)
            {
                hs->EnsureGlobalTransform(e);
                Vec3 wpos = hs->GetWorldPosition(e);
                Vec3 wscale = hs->GetWorldScale(e);

                sumPos = sumPos + wpos;
                sumScale = sumScale + wscale;

                if (first)
                {
                    gt.aabbMin = gt.aabbMax = wpos;
                    first = false;
                }
                gt.aabbMin.x = std::min(gt.aabbMin.x, wpos.x);
                gt.aabbMin.y = std::min(gt.aabbMin.y, wpos.y);
                gt.aabbMin.z = std::min(gt.aabbMin.z, wpos.z);
                gt.aabbMax.x = std::max(gt.aabbMax.x, wpos.x);
                gt.aabbMax.y = std::max(gt.aabbMax.y, wpos.y);
                gt.aabbMax.z = std::max(gt.aabbMax.z, wpos.z);
            }

            float invCount = 1.0f / static_cast<float>(top.size());
            gt.pivotWorld = sumPos * invCount;
            gt.averageScale = sumScale * invCount;
            gt.rotationWorld = Quaternion(0, 0, 0, 1); // neutral (we can refine later)
            gt.valid = true;
            return gt;
        }

        // Apply translation delta (world space)
        static void ApplyTranslation(const Vec3& worldDelta)
        {
            auto hs = ECS::GetInstance().GetSystem<HierarchySystem>();
            if (!hs) return;

            const auto& top = Selection::TopLevel();
            for (auto e : top)
            {
                if (!ECS::GetInstance().HasComponent<Transform>(e)) continue;

                hs->EnsureGlobalTransform(e);
                Vec3 wpos = hs->GetWorldPosition(e) + worldDelta;

                // Convert world target to local (parent space)
                EntityID parent = hs->GetParent(e);
                Vec3 parentWorldPos{ 0,0,0 };
                Quaternion parentWorldRot{ 0,0,0,1 };
                Vec3 parentWorldScale{ 1,1,1 };

                if (parent != 0)
                {
                    parentWorldPos = hs->GetWorldPosition(parent);
                    parentWorldRot = hs->GetWorldRotation(parent);
                    parentWorldScale = hs->GetWorldScale(parent);
                }

                // world -> local: localPos = inverse(parentRot) * ((wpos - parentPos) / parentScale)
                Quaternion invParentRot = QuaternionInverse(parentWorldRot);
                Vec3 local = wpos - parentWorldPos;
                local.x /= parentWorldScale.x;
                local.y /= parentWorldScale.y;
                local.z /= parentWorldScale.z;
                local = RotateVector(invParentRot, local);

                auto& tr = ECS::GetInstance().GetComponent<Transform>(e);
                tr.position = local;
                hs->MarkDirty(e);
                hs->OnTransformChanged(e);
            }
        }

        // Rotate group around pivot (world space)
        static void ApplyRotation(const Quaternion& worldDeltaRot, const Vec3& pivotWorld)
        {
            auto hs = ECS::GetInstance().GetSystem<HierarchySystem>();
            if (!hs) return;

            const auto& top = Selection::TopLevel();
            for (auto e : top)
            {
                if (!ECS::GetInstance().HasComponent<Transform>(e)) continue;

                Vec3 origWorldPos = hs->GetWorldPosition(e);
                Quaternion origWorldRot = hs->GetWorldRotation(e);

                // Rotate position around pivot
                Vec3 rel = origWorldPos - pivotWorld;
                rel = RotateVector(worldDeltaRot, rel);
                Vec3 newWorldPos = pivotWorld + rel;

                Quaternion newWorldRot = worldDeltaRot * origWorldRot;

                // Convert to local again
                EntityID parent = hs->GetParent(e);
                Vec3 parentWorldPos{ 0,0,0 };
                Quaternion parentWorldRot{ 0,0,0,1 };
                Vec3 parentWorldScale{ 1,1,1 };
                if (parent != 0)
                {
                    parentWorldPos = hs->GetWorldPosition(parent);
                    parentWorldRot = hs->GetWorldRotation(parent);
                    parentWorldScale = hs->GetWorldScale(parent);
                }

                // Position
                Quaternion invParentRot = QuaternionInverse(parentWorldRot);
                Vec3 local = newWorldPos - parentWorldPos;
                local.x /= parentWorldScale.x;
                local.y /= parentWorldScale.y;
                local.z /= parentWorldScale.z;
                local = RotateVector(invParentRot, local);

                // Rotation local
                Quaternion localRot = invParentRot * newWorldRot;

                auto& tr = ECS::GetInstance().GetComponent<Transform>(e);
                tr.position = local;
                tr.rotation = localRot;
                hs->MarkDirty(e);
                hs->OnTransformChanged(e);
            }
        }

        // Uniform scale about pivot
        static void ApplyUniformScale(float scaleFactor, const Vec3& pivotWorld)
        {
            auto hs = ECS::GetInstance().GetSystem<HierarchySystem>();
            if (!hs) return;

            const auto& top = Selection::TopLevel();
            for (auto e : top)
            {
                if (!ECS::GetInstance().HasComponent<Transform>(e)) continue;

                Vec3 origWorldPos = hs->GetWorldPosition(e);
                Vec3 rel = origWorldPos - pivotWorld;
                rel = rel * scaleFactor;
                Vec3 newWorldPos = pivotWorld + rel;

                Vec3 origWorldScale = hs->GetWorldScale(e);
                Vec3 newWorldScale = origWorldScale * scaleFactor;

                // Convert to local space
                EntityID parent = hs->GetParent(e);
                Vec3 parentWorldPos{ 0,0,0 };
                Quaternion parentWorldRot{ 0,0,0,1 };
                Vec3 parentWorldScale{ 1,1,1 };
                if (parent != 0)
                {
                    parentWorldPos = hs->GetWorldPosition(parent);
                    parentWorldRot = hs->GetWorldRotation(parent);
                    parentWorldScale = hs->GetWorldScale(parent);
                }

                Quaternion invParentRot = QuaternionInverse(parentWorldRot);
                Vec3 localPos = newWorldPos - parentWorldPos;
                localPos.x /= parentWorldScale.x;
                localPos.y /= parentWorldScale.y;
                localPos.z /= parentWorldScale.z;
                localPos = RotateVector(invParentRot, localPos);

                Vec3 localScale{
                    newWorldScale.x / parentWorldScale.x,
                    newWorldScale.y / parentWorldScale.y,
                    newWorldScale.z / parentWorldScale.z
                };

                auto& tr = ECS::GetInstance().GetComponent<Transform>(e);
                tr.position = localPos;
                tr.scale = localScale;
                hs->MarkDirty(e);
                hs->OnTransformChanged(e);
            }
        }
	};
}
