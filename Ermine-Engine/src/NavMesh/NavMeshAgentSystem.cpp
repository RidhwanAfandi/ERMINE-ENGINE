/* Start Header ************************************************************************/
/*!
\file       NavMeshAgentSystem.cpp
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       03/11/2025
\brief      This file defines the NavMeshAgentSystem class, which manages
            navigation agents that move across baked navigation meshes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "NavMeshAgentSystem.h"
#include "Physics.h"

namespace Ermine
{
    static void BuildNavQueryExtents(const NavMeshAgent& a, float outExt[3])
    {
        outExt[0] = a.radius * 2.0f;
        outExt[1] = std::max(2.0f, a.height);
        outExt[2] = a.radius * 2.0f;
    }

    bool RequestPathForAgent(EntityID agentEntity, const Ermine::Vec3& destination)
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.HasComponent<NavMeshAgent>(agentEntity) ||
            !ecs.HasComponent<Transform>(agentEntity))
            return false;

        auto& agent = ecs.GetComponent<NavMeshAgent>(agentEntity);
        auto& trans = ecs.GetComponent<Transform>(agentEntity);

        auto navAgentSys = ecs.GetSystem<NavMeshAgentSystem>();
        if (!navAgentSys)
            return false;

        std::vector<Ermine::Vec3> path;
        if (!navAgentSys->FindPath(agentEntity, trans.position, destination, path))
            return false;

        agent.destination = destination;
        agent.lastDestination = destination;

        agent.path = path;
        //for (size_t i = 0; i < path.size(); ++i)
        //{
        //    auto& p = path[i];
        //}
        agent.currentCorner = 0;
        agent.hasPath = true;
        return true;
    }

    void NavMeshAgentSystem::Init()
    {
    }

    void NavMeshAgentSystem::Update(float dt)
    {
        auto& ecs = ECS::GetInstance();
        auto renderer = ecs.GetSystem<graphics::Renderer>();

        for (auto e : m_Entities)
        {
            if (!ecs.HasComponent<NavMeshAgent>(e) || !ecs.HasComponent<Transform>(e))
                continue;

            if (ECS::GetInstance().HasComponent<ObjectMetaData>(e))
            {
                const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(e);
                if (!meta.selfActive)
                    continue;
            }

            auto& agent = ecs.GetComponent<NavMeshAgent>(e);
            auto& trans = ecs.GetComponent<Transform>(e);

            // jump
            if (agent.isJumping)
            {
                //EE_CORE_INFO("JUMPING");
                agent.jumpTimer += dt;
                float t = (agent.jumpDuration > 1e-5f) ? (agent.jumpTimer / agent.jumpDuration) : 1.0f;
                if (t > 1.0f) t = 1.0f;

                // Linear interpolate start->target
                Vec3 pos = agent.jumpStart + (agent.jumpTarget - agent.jumpStart) * t;

                // Add parabolic arc on Y (visual jump)
                float arc = agent.jumpHeight * 4.0f * t * (1.0f - t);
                pos.y += arc;

                // rotate in jumping direction
                Vec3 face = agent.jumpTarget - agent.jumpStart;
                face.y = 0.0f;

                float lenSq = face.x * face.x + face.z * face.z;
                if (lenSq > 1e-6f)
                {
                    float yawRad = std::atan2(face.x, face.z);
                    float yawDeg = yawRad * 57.2957795f;

                    trans.rotation = FromEulerDegrees(Vec3(0.0f, yawDeg, 0.0f));
                    trans.isDirty = true;
                }

                auto phys = ecs.GetSystem<Physics>();
                if (phys && ecs.HasComponent<PhysicComponent>(e))
                {
                    // move the physics body (so the object actually moves in the world)
                    phys->SetPosition(e, pos);
                    phys->SetRotation(e, trans.rotation);
                    // keep transform in sync (optional, but nice for editor/debug)
                    trans.position = pos;
                }
                else
                {
                    // fallback if no physics body
                    trans.position = pos;
                }

                if (t >= 1.0f)
                {
                    trans.position = agent.jumpTarget;

                    agent.isJumping = false;
                    agent.navPaused = true;
                    agent.postJumpPauseTimer = 2.0f;

                    if (agent.hasPostJumpDestination)
                    {
                        RequestPathForAgent(e, agent.postJumpDestination);
                        agent.hasPostJumpDestination = false;
                    }
                }

                continue; // skip normal nav movement while jumping
            }
            // jump

            //if (agent.navPaused)
            //    continue;

            // Handle post-jump pause
            if (agent.navPaused && agent.postJumpPauseTimer > 0.0f)
            {
                agent.postJumpPauseTimer -= dt;

                if (agent.postJumpPauseTimer <= 0.0f)
                {
                    agent.navPaused = false; // resume movement
                }

                continue; // skip movement while paused
            }

            // skip if no path
            if (!agent.hasPath)
                continue;

            if (agent.path.empty())
            {
                agent.hasPath = false;
                continue;
            }

            // Move toward current path corner
            Vec3 target = agent.path[agent.currentCorner];
            Vec3 pos = trans.position;
            Vec3 dir = target - pos;
            dir.y = 0.0f;

            //float dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            float dist = std::sqrt(dir.x * dir.x + dir.z * dir.z);

            // if very close to the corner, go to next one
            if (dist < agent.stoppingDistance)
            {
                if (++agent.currentCorner >= agent.path.size())
                {
                    agent.hasPath = false;
                    continue;
                }
                else
                {
                    target = agent.path[agent.currentCorner];
                    dir = target - pos;
                    dist = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                }
            }

            if (dist > 1e-3f)
            {
                dir.x /= dist;
                dir.y /= dist;
                dir.z /= dist;

                // Move
                float step = agent.speed * dt;
                if (step > dist) step = dist; // prevents overshoot/corner cutting

                pos += dir * step;

                Vec3 feetQuery = pos;
                feetQuery.y -= agent.centerYOffset; // convert center -> feet for the nav query

                float ext[3];
                BuildNavQueryExtents(agent, ext);

                EntityID navE = FindNearestNavMeshEntity(feetQuery, ext);
                if (navE != 0)
                {
                    Vec3 clampedFeet;
                    auto navSys = ecs.GetSystem<NavMeshSystem>();
                    if (navSys && navSys->ClampToNavMesh(navE, feetQuery, ext, clampedFeet))
                    {
                        // Keep XZ from navmesh, and compute CENTER Y deterministically
                        pos.x = clampedFeet.x;
                        pos.z = clampedFeet.z;
                        
                        if (ecs.HasComponent<PhysicComponent>(e))
                        {
                            auto& p = ecs.GetComponent<PhysicComponent>(e);

                            if (p.shapeType == ShapeType::Capsule)
                            {
                                // For capsule, don't add center offset
                                pos.y = clampedFeet.y;
                            }
                            else
                            {
                                // For box/sphere etc, keep the old behavior
                                pos.y = clampedFeet.y + agent.centerYOffset;
                            }
                        }
                        else
                        {
                            // No physics info: fallback to old behavior
                            pos.y = clampedFeet.y + agent.centerYOffset;
                        }
                    }
                }

                // move using physics so collisions resolve
                auto phys = ecs.GetSystem<Physics>();
                if (phys && ecs.HasComponent<PhysicComponent>(e))
                {
                    if (agent.autoRotate)
                    {
                        Vec3 move = pos - trans.position; // trans.position is old position (before move)
                        move.y = 0.0f;

                        float lenSq = move.x * move.x + move.z * move.z;
                        if (lenSq > 1e-6f) // only rotate if we actually moved
                        {
                            float yawRad = std::atan2(move.x, move.z);
                            float yawDeg = yawRad * 57.2957795f;

                            trans.rotation = FromEulerDegrees(Vec3(0.0f, yawDeg, 0.0f));
                            trans.isDirty = true;
                        }
                    }

                    phys->SetPosition(e, pos);
                    phys->SetRotation(e, trans.rotation);
                }
                else
                {
                    // fallback if no physics body
                    trans.position = pos;
                }

                //if (ecs.HasComponent<GlobalTransform>(e))
                //{
                //    auto& global = ecs.GetComponent<GlobalTransform>(e);
                //    global.worldMatrix = trans.GetLocalMatrix();
                //    global.isDirty = true;
                //}

                //if (ecs.HasComponent<HierarchyComponent>(e))
                //{
                //    auto& h = ecs.GetComponent<HierarchyComponent>(e);
                //    h.worldTransformDirty = true;
                //}
            }

#if defined(EE_EDITOR)

            if (agent.debugDrawPath && !agent.path.empty())
            {
                Vec3 color = { 0.0f, 0.0f, 1.0f };
                Vec3 offset = { 0.0f, 0.05f, 0.0f };

                for (size_t i = 1; i < agent.path.size(); ++i)
                {
                    const Vec3& a = agent.path[i - 1];
                    const Vec3& b = agent.path[i];
                    renderer->SubmitDebugLine(
                        glm::vec3(a.x + offset.x, a.y + offset.y, a.z + offset.z),
                        glm::vec3(b.x + offset.x, b.y + offset.y, b.z + offset.z),
                        glm::vec3(color.x, color.y, color.z)
                    );
                }
            }
#endif
        }
    }

    //bool NavMeshAgentSystem::FindPath(EntityID /*agentEntity*/, const Ermine::Vec3& startPos, const Ermine::Vec3& endPos, std::vector<Ermine::Vec3>& outPath)
    //{
    //    auto& ecs = ECS::GetInstance();

    //    EntityID nearestNav = FindNearestNavMeshEntity(startPos);
    //    if (nearestNav == 0 || !ecs.HasComponent<NavMeshComponent>(nearestNav))
    //        return false;

    //    auto navSystem = ecs.GetSystem<NavMeshSystem>();
    //    if (!navSystem)
    //        return false;

    //    outPath.clear();
    //    return navSystem->ComputeStraightPath(nearestNav, startPos, endPos, outPath);
    //}

    bool NavMeshAgentSystem::FindPath(EntityID agentEntity,
        const Ermine::Vec3& startPos,
        const Ermine::Vec3& endPos,
        std::vector<Ermine::Vec3>& outPath)
    {
        auto& ecs = ECS::GetInstance();

        if (!ecs.HasComponent<NavMeshAgent>(agentEntity))
            return false;

        const auto& agent = ecs.GetComponent<NavMeshAgent>(agentEntity);

        // Query using FEET, not center
        Ermine::Vec3 startFeet = startPos;
        Ermine::Vec3 endFeet = endPos;
        startFeet.y -= agent.centerYOffset;
        endFeet.y -= agent.centerYOffset;

        // Build consistent extents
        float ext[3];
        BuildNavQueryExtents(agent, ext);

        // Find nearest navmesh using FEET position
        EntityID nearestNav = FindNearestNavMeshEntity(startFeet, ext);
        if (nearestNav == 0 || !ecs.HasComponent<NavMeshComponent>(nearestNav))
            return false;

        // (keep your baked radius/height checks as-is)

        auto navSystem = ecs.GetSystem<NavMeshSystem>();
        if (!navSystem)
            return false;

        outPath.clear();
        return navSystem->ComputeStraightPath(nearestNav, startFeet, endFeet, ext, outPath);
    }

    // currently used for finding current nav mesh before jumping
    EntityID NavMeshAgentSystem::FindNearestNavMeshEntity(const Ermine::Vec3& pos)
    {
        auto& ecs = ECS::GetInstance();

        EntityID best = 0;
        float bestDistSq = FLT_MAX;

        for (EntityID e = 1; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<NavMeshComponent>(e)) continue;
            if (!ecs.HasComponent<Transform>(e)) continue;

            const auto& t = ecs.GetComponent<Transform>(e);
            const float dx = t.position.x - pos.x;
            const float dy = t.position.y - pos.y;
            const float dz = t.position.z - pos.z;
            const float d2 = dx * dx + dy * dy + dz * dz;

            if (d2 < bestDistSq)
            {
                bestDistSq = d2;
                best = e;
            }
        }
        return best;
    }
    // for moving
    EntityID NavMeshAgentSystem::FindNearestNavMeshEntity(const Ermine::Vec3& pos, const float ext[3])
    {
        auto& ecs = ECS::GetInstance();
        auto navSys = ecs.GetSystem<NavMeshSystem>();
        if (!navSys)
            return 0;

        EntityID best = 0;
        float bestDistSq = FLT_MAX;

        for (EntityID e = 1; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<NavMeshComponent>(e)) continue;

            Ermine::Vec3 clamped;
            if (!navSys->ClampToNavMesh(e, pos, ext, clamped))
                continue;

            const float dx = clamped.x - pos.x;
            const float dy = clamped.y - pos.y;
            const float dz = clamped.z - pos.z;
            const float d2 = dx * dx + dy * dy + dz * dz;

            if (d2 < bestDistSq)
            {
                bestDistSq = d2;
                best = e;
            }
        }

        return best;
    }
    // currently used for finding next nav mesh during jump
    EntityID NavMeshAgentSystem::FindNearestNavMeshEntityExcluding(const Ermine::Vec3& pos, EntityID exclude, EntityID excludePrev) const
    {
        auto& ecs = ECS::GetInstance();

        EntityID best = 0;
        float bestDist2 = FLT_MAX;

        for (EntityID ent = 1; ent < MAX_ENTITIES; ++ent)
        {
            if (ent == exclude || ent == excludePrev) continue;
            if (!ecs.IsEntityValid(ent)) continue;
            if (!ecs.HasComponent<NavMeshComponent>(ent)) continue;
            if (!ecs.HasComponent<Transform>(ent)) continue;

            const auto& tr = ecs.GetComponent<Transform>(ent);
            Ermine::Vec3 d = tr.position - pos;
            float dist2 = d.x * d.x + d.y * d.y + d.z * d.z;

            if (dist2 < bestDist2)
            {
                bestDist2 = dist2;
                best = ent;
            }
        }

        return best;
    }
}
