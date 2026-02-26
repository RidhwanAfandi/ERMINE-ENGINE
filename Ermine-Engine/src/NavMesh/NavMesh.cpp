/* Start Header ************************************************************************/
/*!
\file       NavMesh.cpp
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       03/11/2025
\brief      This file defines the NavMeshSystem class, which manages navigation mesh
            data and pathfinding functionality for AI.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "NavMesh.h"
#include "ECS.h"
#include "Components.h"

#include "Recast.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourNavMeshQuery.h"
#include "DebugDraw.h"
#include "DetourDebugDraw.h"
#include "RecastDebugDraw.h"
#include "EditorCamera.h"
#include "Renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cfloat>

#include "RecastAlloc.h"
#include "DetourAlloc.h"

static std::atomic<int> g_rcAllocs{ 0 }, g_rcFrees{ 0 };
static std::atomic<int> g_dtAllocs{ 0 }, g_dtFrees{ 0 };

// Recast alloc hooks
static void* MyRcAlloc(size_t size, rcAllocHint)
{
    g_rcAllocs++;
    return malloc(size);
}
static void MyRcFree(void* p)
{
    if (p) { g_rcFrees++; free(p); }
}

// Detour alloc hooks
static void* MyDtAlloc(size_t size, dtAllocHint)
{
    g_dtAllocs++;
    return malloc(size);
}
static void MyDtFree(void* p)
{
    if (p) { g_dtFrees++; free(p); }
}


class DebugDrawGL : public duDebugDraw
{
public:
    DebugDrawGL() = default;
    void depthMask(bool state) override { glDepthMask(state ? GL_TRUE : GL_FALSE); }

    void begin(duDebugDrawPrimitives prim, float size = 1.0f) override
    {
        switch (prim)
        {
        case DU_DRAW_POINTS: glPointSize(size); glBegin(GL_POINTS); break;
        case DU_DRAW_LINES:  glLineWidth(size); glBegin(GL_LINES);  break;
        case DU_DRAW_TRIS:   glBegin(GL_TRIANGLES); break;
        case DU_DRAW_QUADS:  glBegin(GL_QUADS); break;
        }
    }

    void vertex(const float* pos, unsigned int color) override
    {
        unsigned char r = (color >> 24) & 0xFF;
        unsigned char g = (color >> 16) & 0xFF;
        unsigned char b = (color >> 8) & 0xFF;
        unsigned char a = (color) & 0xFF;
        glColor4ub(r, g, b, a);
        glVertex3f(pos[0], pos[1], pos[2]);
    }

    void vertex(const float x, const float y, const float z, unsigned int color) override
    {
        unsigned char r = (color >> 24) & 0xFF;
        unsigned char g = (color >> 16) & 0xFF;
        unsigned char b = (color >> 8) & 0xFF;
        unsigned char a = (color) & 0xFF;
        glColor4ub(r, g, b, a);
        glVertex3f(x, y, z);
    }

    void vertex(const float* pos, unsigned int color, const float* /*uv*/) override { vertex(pos, color); }
    void vertex(const float x, const float y, const float z, unsigned int color, const float /*u*/, const float /*v*/) override { vertex(x, y, z, color); }
    void end() override { glEnd(); }
    void texture(bool) override {}
};

struct Ermine::NavMeshComponent::BuildData
{
    rcContext* ctx = nullptr;
    rcHeightfield* hf = nullptr;
    rcCompactHeightfield* chf = nullptr;
    rcContourSet* cset = nullptr;
    rcPolyMesh* pmesh = nullptr;
    rcPolyMeshDetail* dmesh = nullptr;
};

/*struct Ermine::NavMeshComponent::Runtime
{
    dtNavMesh* nav = nullptr;
    dtNavMeshQuery* query = nullptr;
    //unsigned char* navData = nullptr;
    dtTileRef tileRef = 0;
};*/

namespace Ermine {
    static bool EnsureRuntimeFromBaked(NavMeshSystem& sys, NavMeshComponent& c)
    {
        if (c.runtime && c.runtime->query) return true;
        if (!c.HasBaked()) return false;

        sys.DestroyRuntime(c);

        c.runtime = new NavMeshComponent::Runtime();
        c.runtime->nav = dtAllocNavMesh();

        dtNavMeshParams p{};
        p.orig[0] = c.bakedOrig[0];
        p.orig[1] = c.bakedOrig[1];
        p.orig[2] = c.bakedOrig[2];
        p.tileWidth = c.bakedTileWidth;
        p.tileHeight = c.bakedTileHeight;
        p.maxTiles = (c.bakedMaxTiles > 0) ? c.bakedMaxTiles : 1;
        p.maxPolys = (c.bakedMaxPolys > 0) ? c.bakedMaxPolys : 0x8000;

        if (dtStatusFailed(c.runtime->nav->init(&p)))
        {
            sys.DestroyRuntime(c);
            return false;
        }

        for (auto const& t : c.bakedTiles)
        {
            std::vector<unsigned char> bytes;
            if (!NavMeshComponent::Base64Decode(t.dataB64.c_str(), bytes)) continue;

            unsigned char* data = (unsigned char*)dtAlloc((int)bytes.size(), DT_ALLOC_PERM);
            if (!data) continue;
            std::memcpy(data, bytes.data(), bytes.size());

            dtTileRef ref{};
            dtStatus st = c.runtime->nav->addTile(data, (int)bytes.size(), DT_TILE_FREE_DATA, 0, &ref);
            if (dtStatusFailed(st))
            {
                dtFree(data);
            }
            else
            {
                c.runtime->tileRef = (unsigned long long)ref;
            }
        }

        c.runtime->query = dtAllocNavMeshQuery();
        if (!c.runtime->query || dtStatusFailed(c.runtime->query->init(c.runtime->nav, 2048)))
        {
            sys.DestroyRuntime(c);
            return false;
        }

        return true;
    }


    static void SyncBakeAgentSettingsFromAgents(NavMeshComponent& nm)
    {
        auto& ecs = ECS::GetInstance();

        float maxR = 0.0f;
        float maxH = 0.0f;

        // Use your ECS iteration method; this is a generic idea.
        // If you don't have a global iterator, do it using the system entity lists.
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<NavMeshAgent>(e)) continue;

            const auto& ag = ecs.GetComponent<NavMeshAgent>(e);

            maxR = std::max(maxR, ag.radius);
            maxH = std::max(maxH, ag.height);
        }

        if (maxR > 0.0f) nm.agentRadius = maxR;
        if (maxH > 0.0f) nm.agentHeight = maxH;

        // small safety margin so it doesn’t hug walls
        nm.agentRadius *= 1.05f;
        nm.agentHeight *= 1.02f;
    }

    void NavMeshSystem::Init()
    {
        if (!m_dd) m_dd = new DebugDrawGL();
        EE_CORE_INFO("[NavMeshSystem] Initialized");

        rcAllocSetCustom(MyRcAlloc, MyRcFree);
        dtAllocSetCustom(MyDtAlloc, MyDtFree);
    }

    void NavMeshSystem::FreeAllNavMeshes()
    {
        EE_CORE_INFO("[NavMeshSystem] FreeAllNavMeshes() START");

        auto& ecs = ECS::GetInstance();
        int destroyed = 0;
        int has = 0;

        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.HasComponent<NavMeshComponent>(e))
                continue;

            ++has;
            EE_CORE_INFO("[NavMeshSystem] Found NavMeshComponent on entity {}", (int)e);

            auto& nav = ecs.GetComponent<NavMeshComponent>(e);
            DestroyRuntime(nav);
            DestroyBuild(nav);
            ++destroyed;
        }

        EE_CORE_INFO("[NavMeshSystem] Components found: {}", has);
        EE_CORE_INFO("[NavMeshSystem] FreeAllNavMeshes destroyed: {}", destroyed);
    }

    void NavMeshSystem::Shutdown()
    {
        EE_CORE_INFO("[NavMeshSystem] Freeing all navmesh data");

        FreeAllNavMeshes();

        EE_CORE_INFO("[NavMesh] RC outstanding = {} (allocs={} frees={})",
            g_rcAllocs.load() - g_rcFrees.load(),
            g_rcAllocs.load(), g_rcFrees.load());

        EE_CORE_INFO("[NavMesh] DT outstanding = {} (allocs={} frees={})",
            g_dtAllocs.load() - g_dtFrees.load(),
            g_dtAllocs.load(), g_dtFrees.load());

        delete m_dd;
        m_dd = nullptr;
    }

    void NavMeshSystem::DestroyBuild(NavMeshComponent& c)
    {
        EE_CORE_INFO("[NavMeshSystem] DestroyBuild");
        if (!c.build) return;

        if (c.build->dmesh) { rcFreePolyMeshDetail(c.build->dmesh); c.build->dmesh = nullptr; }
        if (c.build->pmesh) { rcFreePolyMesh(c.build->pmesh); c.build->pmesh = nullptr; }
        if (c.build->cset) { rcFreeContourSet(c.build->cset); c.build->cset = nullptr; }
        if (c.build->chf) { rcFreeCompactHeightfield(c.build->chf); c.build->chf = nullptr; }
        if (c.build->hf) { rcFreeHeightField(c.build->hf); c.build->hf = nullptr; }
        if (c.build->ctx) { delete c.build->ctx; c.build->ctx = nullptr; }

        delete c.build;
        c.build = nullptr;
    }

    void NavMeshSystem::DestroyRuntime(NavMeshComponent& c)
    {
        EE_CORE_INFO("[NavMeshSystem] DestroyRuntime {}", (void*)c.runtime);
        if (!c.runtime) return;

        if (c.runtime->query) {
#ifdef DETOURNAVMESHQUERY_H
            dtFreeNavMeshQuery(c.runtime->query);
#else
            dtFree(c.runtime->query);
#endif
            c.runtime->query = nullptr;
        }

        if (c.runtime->nav) {
            if (c.runtime->tileRef) {
                unsigned char* data = nullptr; int size = 0;
                c.runtime->nav->removeTile(c.runtime->tileRef, &data, &size);
                EE_CORE_INFO("[NavMeshSystem] removeTile ref={} data={} size={}", (unsigned)c.runtime->tileRef, (void*)data, size);
                c.runtime->tileRef = 0;
            }

            const dtNavMesh* navc = c.runtime->nav;
            int removed = 0;
            for (int i = 0; i < navc->getMaxTiles(); ++i) {
                const dtMeshTile* t = navc->getTile(i);
                if (!t || !t->data) continue;
                dtTileRef r = c.runtime->nav->getTileRef(t);
                if (!r) continue;
                unsigned char* d = t->data;
                c.runtime->nav->removeTile(r, &d, nullptr);
                ++removed;
            }
            EE_CORE_INFO("[NavMeshSystem] Removed {} Detour tiles (sweep)", removed);

            dtFreeNavMesh(c.runtime->nav);
            c.runtime->nav = nullptr;
        }

        delete c.runtime;
        c.runtime = nullptr;

        c.bakedAgentRadius = 0.0f;
        c.bakedAgentHeight = 0.0f;
    }

    void NavMeshSystem::WarmStartBakedNavMeshes()
    {
        auto& ecs = ECS::GetInstance();
        int warmed = 0;

        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<NavMeshComponent>(e)) continue;

            auto& nm = ecs.GetComponent<NavMeshComponent>(e);

            if (nm.runtime) continue;           // already live
            if (!nm.HasBaked()) continue;       // nothing to restore

            if (CreateRuntimeFromBaked(nm))
                ++warmed;
        }

        EE_CORE_INFO("[NavMeshSystem] Warm-started {} baked navmeshes", warmed);
    }

    bool NavMeshSystem::CreateRuntimeFromBaked(NavMeshComponent& nm)
    {
        if (!nm.HasBaked()) return false;

        DestroyRuntime(nm);

        nm.runtime = new NavMeshComponent::Runtime();
        nm.runtime->nav = dtAllocNavMesh();
        if (!nm.runtime->nav) return false;

        dtNavMeshParams p{};
        p.orig[0] = nm.bakedOrig[0];
        p.orig[1] = nm.bakedOrig[1];
        p.orig[2] = nm.bakedOrig[2];
        p.tileWidth = nm.bakedTileWidth;
        p.tileHeight = nm.bakedTileHeight;
        p.maxTiles = (nm.bakedMaxTiles > 0) ? nm.bakedMaxTiles : 1;
        p.maxPolys = (nm.bakedMaxPolys > 0) ? nm.bakedMaxPolys : 0x8000;

        if (dtStatusFailed(nm.runtime->nav->init(&p)))
        {
            DestroyRuntime(nm);
            return false;
        }

        for (auto const& t : nm.bakedTiles)
        {
            std::vector<unsigned char> bytes;
            if (!NavMeshComponent::Base64Decode(t.dataB64.c_str(), bytes)) continue;

            unsigned char* data = (unsigned char*)dtAlloc((int)bytes.size(), DT_ALLOC_PERM);
            if (!data) continue;
            std::memcpy(data, bytes.data(), bytes.size());

            dtTileRef ref{};
            dtStatus st = nm.runtime->nav->addTile(data, (int)bytes.size(), DT_TILE_FREE_DATA, 0, &ref);
            if (dtStatusFailed(st))
            {
                dtFree(data);
            }
            else
            {
                nm.runtime->tileRef = (unsigned long long)ref;
            }
        }

        nm.runtime->query = dtAllocNavMeshQuery();
        if (!nm.runtime->query || dtStatusFailed(nm.runtime->query->init(nm.runtime->nav, 2048)))
        {
            DestroyRuntime(nm);
            return false;
        }

        return true;
    }

    bool NavMeshSystem::BuildFromTriangles(NavMeshComponent& c, const float* verts, int nverts, const int* tris, int ntris)
    {
        //EE_CORE_INFO("[NavMeshSystem] Begin BuildFromTriangles (verts=%d tris=%d)", nverts, ntris);
        DestroyBuild(c);
        DestroyRuntime(c);

        if (!verts || !tris || nverts < 3 || ntris < 1)
        {
            EE_CORE_WARN("[NavMeshSystem] Invalid input geometry");
            return false;
        }

        c.build = new NavMeshComponent::BuildData();
        c.build->ctx = new rcContext();

        float bmin[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
        float bmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (int i = 0; i < nverts; ++i)
        {
            const float* v = &verts[i * 3];
            bmin[0] = std::min(bmin[0], v[0]);
            bmin[1] = std::min(bmin[1], v[1]);
            bmin[2] = std::min(bmin[2], v[2]);
            bmax[0] = std::max(bmax[0], v[0]);
            bmax[1] = std::max(bmax[1], v[1]);
            bmax[2] = std::max(bmax[2], v[2]);
        }

        const float pad = c.cellSize * 2.0f;
        for (int i = 0; i < 3; ++i) { bmin[i] -= pad; bmax[i] += pad; }

        rcConfig cfg{};
        cfg.cs = c.cellSize;
        cfg.ch = c.cellHeight;
        cfg.walkableSlopeAngle = c.agentMaxSlope;
        cfg.walkableHeight = (int)std::ceil(c.agentHeight / cfg.ch);
        cfg.walkableClimb = (int)std::floor(c.agentMaxClimb / cfg.ch);
        cfg.walkableRadius = (int)std::ceil(c.agentRadius / cfg.cs);
        rcVcopy(cfg.bmin, bmin);
        rcVcopy(cfg.bmax, bmax);
        rcCalcGridSize(cfg.bmin, cfg.bmax, cfg.cs, &cfg.width, &cfg.height);
        cfg.width = std::max(cfg.width, 2);
        cfg.height = std::max(cfg.height, 2);
        cfg.maxEdgeLen = 12;
        cfg.maxSimplificationError = 1.3f;
        cfg.minRegionArea = (int)rcSqr(8);
        cfg.mergeRegionArea = (int)rcSqr(20);
        cfg.maxVertsPerPoly = 6;
        cfg.detailSampleDist = 6.0f;
        cfg.detailSampleMaxError = 1.0f;

        c.build->hf = rcAllocHeightfield();
        if (!c.build->hf)
        {
            EE_CORE_ERROR("[NavMeshSystem] Failed to allocate heightfield");
            return false;
        }

        if (!rcCreateHeightfield(c.build->ctx, *c.build->hf, cfg.width, cfg.height,
            cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
        {
            EE_CORE_ERROR("[NavMeshSystem] rcCreateHeightfield failed");
            DestroyBuild(c);
            return false;
        }

        std::vector<unsigned char> areas(ntris, RC_WALKABLE_AREA);
        if (!rcRasterizeTriangles(c.build->ctx, verts, nverts, tris, areas.data(), ntris,
            *c.build->hf, cfg.walkableClimb))
        {
            EE_CORE_ERROR("[NavMeshSystem] rcRasterizeTriangles failed");
            DestroyBuild(c);
            return false;
        }

        //// Remove low-hanging obstacles that would be “walkable”
        //rcFilterLowHangingWalkableObstacles(c.build->ctx, cfg.walkableClimb, *c.build->hf);

        //// Remove ledges you can't step up/down safely (prevents fake “step to top” links)
        //rcFilterLedgeSpans(c.build->ctx, cfg.walkableHeight, cfg.walkableClimb, *c.build->hf);

        //// Remove spans where the clearance is too low
        //rcFilterWalkableLowHeightSpans(c.build->ctx, cfg.walkableHeight, *c.build->hf);

        c.build->chf = rcAllocCompactHeightfield();
        rcBuildCompactHeightfield(c.build->ctx, cfg.walkableHeight, cfg.walkableClimb, *c.build->hf, *c.build->chf);

        if (!rcErodeWalkableArea(c.build->ctx, cfg.walkableRadius, *c.build->chf))
        {
            EE_CORE_ERROR("[NavMeshSystem] rcErodeWalkableArea failed (walkableRadius=%d)", cfg.walkableRadius);
            DestroyBuild(c);
            return false;
        }

        rcBuildDistanceField(c.build->ctx, *c.build->chf);
        rcBuildRegions(c.build->ctx, *c.build->chf, 0, cfg.minRegionArea, cfg.mergeRegionArea);

        c.build->cset = rcAllocContourSet();
        rcBuildContours(c.build->ctx, *c.build->chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *c.build->cset);

        c.build->pmesh = rcAllocPolyMesh();
        rcBuildPolyMesh(c.build->ctx, *c.build->cset, cfg.maxVertsPerPoly, *c.build->pmesh);

        c.build->dmesh = rcAllocPolyMeshDetail();
        rcBuildPolyMeshDetail(c.build->ctx, *c.build->pmesh, *c.build->chf,
            cfg.detailSampleDist, cfg.detailSampleMaxError, *c.build->dmesh);

        if (c.build->pmesh->nverts == 0 || c.build->pmesh->npolys == 0)
        {
            EE_CORE_ERROR("[NavMesh] Empty pmesh! Possible over-eroded geometry or agent too tall.");
            DestroyBuild(c);
            return false;
        }

        dtNavMeshCreateParams params{};
        params.verts = c.build->pmesh->verts;
        params.vertCount = c.build->pmesh->nverts;
        params.polys = c.build->pmesh->polys;
        params.polyAreas = c.build->pmesh->areas;

        static const unsigned short WALKABLE = 0x01;
        std::vector<unsigned short> polyFlagsTemp(c.build->pmesh->npolys, WALKABLE);
        params.polyFlags = polyFlagsTemp.data();

        params.polyCount = c.build->pmesh->npolys;
        params.nvp = c.build->pmesh->nvp;
        params.detailMeshes = c.build->dmesh->meshes;
        params.detailVerts = c.build->dmesh->verts;
        params.detailVertsCount = c.build->dmesh->nverts;
        params.detailTris = c.build->dmesh->tris;
        params.detailTriCount = c.build->dmesh->ntris;
        rcVcopy(params.bmin, c.build->pmesh->bmin);
        rcVcopy(params.bmax, c.build->pmesh->bmax);
        params.cs = cfg.cs;
        params.ch = cfg.ch;
        params.walkableHeight = c.agentHeight;
        params.walkableRadius = c.agentRadius;
        params.walkableClimb = c.agentMaxClimb;
        params.buildBvTree = true;

        unsigned char* navData = nullptr;
        int navDataSize = 0;

        if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
        {
            EE_CORE_ERROR("[NavMeshSystem] dtCreateNavMeshData failed");
            DestroyBuild(c);
            return false;
        }

        c.runtime = new NavMeshComponent::Runtime();

        c.bakedAgentRadius = c.agentRadius;
        c.bakedAgentHeight = c.agentHeight;

        c.runtime->nav = dtAllocNavMesh();

        dtNavMeshParams navParams{};
        navParams.orig[0] = bmin[0];
        navParams.orig[1] = bmin[1];
        navParams.orig[2] = bmin[2];
        navParams.tileWidth = bmax[0] - bmin[0];
        navParams.tileHeight = bmax[2] - bmin[2];
        navParams.maxTiles = 1;
        navParams.maxPolys = 0x8000;

        auto st = c.runtime->nav->init(&navParams);
        //EE_CORE_INFO("[NavMeshSystem] nav->init(tiled) status={} maxTiles={}", (int)st, c.runtime->nav->getMaxTiles());
        if (dtStatusFailed(st)) { dtFree(navData); DestroyBuild(c); DestroyRuntime(c); return false; }

        dtTileRef tileRef = 0;
        st = c.runtime->nav->addTile(navData, navDataSize, DT_TILE_FREE_DATA, 0, &tileRef);
        //EE_CORE_INFO("[NavMeshSystem] addTile status={} ref={}", (int)st, (unsigned)tileRef);
        if (dtStatusFailed(st)) { dtFree(navData); DestroyBuild(c); DestroyRuntime(c); return false; }

        //c.runtime->tileRef = tileRef;

        c.runtime->query = dtAllocNavMeshQuery();
        if (dtStatusFailed(c.runtime->query->init(c.runtime->nav, 2048))) { DestroyBuild(c); DestroyRuntime(c); return false; }

        c.runtime->tileRef = (unsigned long long)tileRef;

        // Persist baked navmesh into component (for scene serialization)
        c.bakedOrig[0] = navParams.orig[0];
        c.bakedOrig[1] = navParams.orig[1];
        c.bakedOrig[2] = navParams.orig[2];
        c.bakedTileWidth = navParams.tileWidth;
        c.bakedTileHeight = navParams.tileHeight;
        c.bakedMaxTiles = navParams.maxTiles;
        c.bakedMaxPolys = navParams.maxPolys;

        c.bakedTiles.clear();

        const dtMeshTile* tile = c.runtime->nav->getTileByRef(tileRef);
        if (tile && tile->data && tile->dataSize > 0)
        {
            NavMeshComponent::BakedTile bt;
            bt.dataSize = tile->dataSize;
            bt.dataB64 = NavMeshComponent::Base64Encode((const unsigned char*)tile->data, (size_t)tile->dataSize);
            c.bakedTiles.push_back(std::move(bt));
        }


        EE_CORE_INFO("[NavMeshSystem] Build complete!");
        return true;
    }

    // helper: append a cube (top-only for floor, full cube for obstacles)
    auto AppendCube = [&](EntityID ent, bool topOnly,
        std::vector<float>& outVerts,
        std::vector<int>& outTris)
        {
            auto& ecs = ECS::GetInstance();
            if (!ecs.HasComponent<Transform>(ent) || !ecs.HasComponent<Mesh>(ent))
                return;

            auto& t = ecs.GetComponent<Transform>(ent);
            auto& m = ecs.GetComponent<Mesh>(ent);

            if (m.kind != MeshKind::Primitive || m.primitive.type != "Cube")
                return;

            glm::quat rotQuat = glm::quat(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);
            glm::mat4 model =
                glm::translate(glm::mat4(1.0f), glm::vec3(t.position.x, t.position.y, t.position.z)) *
                glm::mat4_cast(rotQuat) *
                glm::scale(glm::mat4(1.0f),
                    glm::vec3(t.scale.x * m.primitive.size.x,
                        t.scale.y * m.primitive.size.y,
                        t.scale.z * m.primitive.size.z));

            const int base = (int)(outVerts.size() / 3);

            if (topOnly)
            {
                // top face (4 verts, 2 tris)
                const float topVerts[] = {
                    -0.5f, 0.5f, -0.5f,
                     0.5f, 0.5f, -0.5f,
                     0.5f, 0.5f,  0.5f,
                    -0.5f, 0.5f,  0.5f
                };
                const int topTris[] = { 0,1,2, 0,2,3 };

                for (int i = 0; i < 4; ++i)
                {
                    glm::vec4 v = model * glm::vec4(
                        topVerts[i * 3 + 0], topVerts[i * 3 + 1], topVerts[i * 3 + 2], 1.0f);

                    outVerts.push_back(v.x);
                    outVerts.push_back(v.y);
                    outVerts.push_back(v.z);
                }

                for (int i = 0; i < 6; ++i)
                    outTris.push_back(base + topTris[i]);
            }
            else
            {
                // full cube (8 verts, 12 tris)
                const float cubeVerts[] = {
                    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
                    -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
                };

                const int cubeTris[] = {
                    // bottom
                    0,1,2, 0,2,3,
                    // top
                    4,6,5, 4,7,6,
                    // front
                    4,5,1, 4,1,0,
                    // back
                    3,2,6, 3,6,7,
                    // left
                    4,0,3, 4,3,7,
                    // right
                    1,5,6, 1,6,2
                };

                for (int i = 0; i < 8; ++i)
                {
                    glm::vec4 v = model * glm::vec4(
                        cubeVerts[i * 3 + 0], cubeVerts[i * 3 + 1], cubeVerts[i * 3 + 2], 1.0f);

                    outVerts.push_back(v.x);
                    outVerts.push_back(v.y);
                    outVerts.push_back(v.z);
                }

                for (int i = 0; i < (int)(sizeof(cubeTris) / sizeof(int)); ++i)
                    outTris.push_back(base + cubeTris[i]);
            }
        };

    auto AppendStaticCollider = [&](EntityID ent, std::vector<float>& outVerts, std::vector<int>& outTris)
    {
            auto& ecs = ECS::GetInstance();
            auto& t = ecs.GetComponent<Transform>(ent);
            auto& pc = ecs.GetComponent<PhysicComponent>(ent);

            // For now: only Box colliders (fast + most common)
            if (pc.shapeType != ShapeType::Box) return;

            // Build collider local transform (pivot + collider rotation)
            glm::quat entRot = glm::quat(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);

            // If colliderRot is degrees in your editor, convert to radians.
            glm::vec3 colEulerRad(glm::radians(pc.colliderRot.x),
                glm::radians(pc.colliderRot.y),
                glm::radians(pc.colliderRot.z));
            glm::quat colRot = glm::quat(colEulerRad);

            glm::mat4 model =
                glm::translate(glm::mat4(1.0f), glm::vec3(t.position.x, t.position.y, t.position.z)) *
                glm::mat4_cast(entRot) *
                glm::translate(glm::mat4(1.0f), glm::vec3(pc.colliderPivot.x, pc.colliderPivot.y, pc.colliderPivot.z)) *
                glm::mat4_cast(colRot) *
                glm::scale(glm::mat4(1.0f),
                    glm::vec3(t.scale.x * pc.colliderSize.x,
                        t.scale.y * pc.colliderSize.y,
                        t.scale.z * pc.colliderSize.z));

            const int base = (int)(outVerts.size() / 3);

            // Unit cube vertices (centered)
            const float cubeVerts[] = {
                -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
                -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f
            };
            const int cubeTris[] = {
                0,1,2, 0,2,3,
                4,6,5, 4,7,6,
                4,5,1, 4,1,0,
                3,2,6, 3,6,7,
                4,0,3, 4,3,7,
                1,5,6, 1,6,2
            };

            for (int i = 0; i < 8; ++i)
            {
                glm::vec4 v = model * glm::vec4(
                    cubeVerts[i * 3 + 0], cubeVerts[i * 3 + 1], cubeVerts[i * 3 + 2], 1.0f);

                outVerts.push_back(v.x);
                outVerts.push_back(v.y);
                outVerts.push_back(v.z);
            }

            for (int i = 0; i < (int)(sizeof(cubeTris) / sizeof(int)); ++i)
                outTris.push_back(base + cubeTris[i]);
    };


    bool NavMeshSystem::BakeNavMesh(EntityID e)
    {
        auto& ecs = ECS::GetInstance();

        if (!ecs.HasComponent<NavMeshComponent>(e) ||
            !ecs.HasComponent<Transform>(e) ||
            !ecs.HasComponent<Mesh>(e))
            return false;

        auto& nm = ecs.GetComponent<NavMeshComponent>(e);

        SyncBakeAgentSettingsFromAgents(nm);

        auto& navT = ecs.GetComponent<Transform>(e);
        auto& navM = ecs.GetComponent<Mesh>(e);

        // compute bake region from the cube you're baking on
        // (AABB region around the bake cube, plus a bit above it for walls sitting on top)
        if (navM.kind != MeshKind::Primitive || navM.primitive.type != "Cube")
            return false;

        Vec3 bakeCenter = navT.position;

        Vec3 bakeHalf;
        bakeHalf.x = 0.5f * navT.scale.x * navM.primitive.size.x;
        bakeHalf.y = 0.5f * navT.scale.y * navM.primitive.size.y;
        bakeHalf.z = 0.5f * navT.scale.z * navM.primitive.size.z;

        const float extraSide = 0.25f; // small padding so edge-touching walls are included
        const float extraAbove = std::max(2.0f, nm.agentHeight * 2.0f); // include "on top" obstacles

        const float minX = bakeCenter.x - bakeHalf.x - extraSide;
        const float maxX = bakeCenter.x + bakeHalf.x + extraSide;
        const float minZ = bakeCenter.z - bakeHalf.z - extraSide;
        const float maxZ = bakeCenter.z + bakeHalf.z + extraSide;

        const float minY = bakeCenter.y - bakeHalf.y - extraSide;
        const float maxY = bakeCenter.y + bakeHalf.y + extraAbove;

        auto InsideBakeRegionByCenter = [&](const Transform& t) -> bool
            {
                return (t.position.x >= minX && t.position.x <= maxX) &&
                    (t.position.z >= minZ && t.position.z <= maxZ) &&
                    (t.position.y >= minY && t.position.y <= maxY);
            };

        // collect geometry
        std::vector<float> verts;
        std::vector<int> tris;
        verts.reserve(2048);
        tris.reserve(2048);

        // Bake floor's TOP face as walkable
        AppendCube(e, true, verts, tris);

        // Include nearby static colliders as obstacles (skip NavMeshAgents)
        for (EntityID ent = 1; ent < MAX_ENTITIES; ++ent)
        {
            if (!ecs.IsEntityValid(ent)) continue;
            if (ent == e) continue;

            if (!ecs.HasComponent<PhysicComponent>(ent) || !ecs.HasComponent<Transform>(ent)) continue;
            if (ecs.HasComponent<NavMeshAgent>(ent)) continue;

            auto& pc = ecs.GetComponent<PhysicComponent>(ent);

            // Only bake static rigid colliders as obstacles
            if (pc.bodyType != PhysicsBodyType::Rigid) continue;
            if (pc.motionType != JPH::EMotionType::Static) continue;

            auto& ot = ecs.GetComponent<Transform>(ent);
            if (!InsideBakeRegionByCenter(ot)) continue; // you can improve this later to use AABB

            AppendStaticCollider(ent, verts, tris);
        }

        const int nverts = (int)(verts.size() / 3);
        const int ntris = (int)(tris.size() / 3);

        if (nverts < 3 || ntris < 1)
            return false;

        bool ok = BuildFromTriangles(nm, verts.data(), nverts, tris.data(), ntris);
        if (!ok)
        {
            EE_CORE_ERROR("[NavMeshSystem] Bake failed for entity %u", e);
            return false;
        }

        return true;
    }

    void NavMeshSystem::DebugDraw()
    {
        auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
        if (!renderer) { EE_CORE_WARN("[NavMesh] DebugDraw: no Renderer system."); return; }

        //const auto& view = editor::EditorCamera::GetInstance().GetViewMatrix();
        //const auto& proj = editor::EditorCamera::GetInstance().GetProjectionMatrix();

        int tilesVisited = 0;
        int polysVisited = 0;
        int linesSubmitted = 0;

        for (auto e : m_Entities)
        {
            if (!ECS::GetInstance().HasComponent<NavMeshComponent>(e)) continue;
            auto& c = ECS::GetInstance().GetComponent<NavMeshComponent>(e);
            if (!c.drawNavMesh) continue;
            if (!c.runtime || !c.runtime->nav) continue;

            const dtNavMesh* nav = c.runtime->nav;

            for (int ti = 0; ti < nav->getMaxTiles(); ++ti)
            {
                const dtMeshTile* tile = nav->getTile(ti);
                if (!tile || !tile->header) continue;
                ++tilesVisited;

                const dtPoly* polys = tile->polys;
                const float* verts = tile->verts;

                for (int pi = 0; pi < tile->header->polyCount; ++pi)
                {
                    const dtPoly* p = &polys[pi];
                    if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;
                    ++polysVisited;

                    const unsigned nv = p->vertCount;
                    for (unsigned k = 0; k < nv; ++k)
                    {
                        const float* v0 = &verts[p->verts[k] * 3];
                        const float* v1 = &verts[p->verts[(k + 1) % nv] * 3];

                        // lift slightly to avoid z-fighting
                        glm::vec3 a(v0[0], v0[1] + 0.01f, v0[2]);
                        glm::vec3 b(v1[0], v1[1] + 0.01f, v1[2]);

                        renderer->SubmitDebugLine(a, b, glm::vec3(0.f, 1.f, 0.f)); // bright green
                        ++linesSubmitted;
                    }
                }
            }
        }
        //glEnable(GL_DEPTH_TEST);
        //glDepthMask(GL_TRUE);
        //glDisable(GL_BLEND);

        //renderer->RenderDebugLines(view, proj);

        //glDisable(GL_DEPTH_TEST);
    }

    void NavMeshSystem::DebugHighLight()
    {
        auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
        if (!renderer) return;

        //const auto& view = editor::EditorCamera::GetInstance().GetViewMatrix();
        //const auto& proj = editor::EditorCamera::GetInstance().GetProjectionMatrix();

        int trisSubmitted = 0;

        for (auto e : m_Entities)
        {
            if (!ECS::GetInstance().HasComponent<NavMeshComponent>(e)) continue;
            auto& c = ECS::GetInstance().GetComponent<NavMeshComponent>(e);
            if (!c.drawWalkable) continue;
            if (!c.runtime || !c.runtime->nav) continue;

            const dtNavMesh* nav = c.runtime->nav;

            for (int ti = 0; ti < nav->getMaxTiles(); ++ti)
            {
                const dtMeshTile* tile = nav->getTile(ti);
                if (!tile || !tile->header) continue;

                const dtPoly* polys = tile->polys;
                const float* verts = tile->verts;

                for (int pi = 0; pi < tile->header->polyCount; ++pi)
                {
                    const dtPoly* p = &polys[pi];
                    if (p->getType() == DT_POLYTYPE_OFFMESH_CONNECTION) continue;

                    const unsigned nv = p->vertCount;
                    const float* v0 = &verts[p->verts[0] * 3];

                    for (unsigned k = 1; k + 1 < nv; ++k)
                    {
                        const float* v1 = &verts[p->verts[k] * 3];
                        const float* v2 = &verts[p->verts[k + 1] * 3];

                        glm::vec3 a(v0[0], v0[1] + 0.015f, v0[2]);
                        glm::vec3 b(v1[0], v1[1] + 0.015f, v1[2]);
                        glm::vec3 c_(v2[0], v2[1] + 0.015f, v2[2]);

                        renderer->SubmitDebugTriangle(a, b, c_, glm::vec3(0.0f, 1.0f, 0.0f));
                        ++trisSubmitted;
                    }
                }
            }
        }

        //EE_CORE_INFO("[NavMeshSystem] DebugDrawFilled: %d tris submitted", trisSubmitted);
        //renderer->RenderDebugTriangles(view, proj);
    }

    bool NavMeshSystem::ComputeStraightPath(EntityID navEntity,
        const Vec3& start, const Vec3& end,
        const float extents[3],
        std::vector<Vec3>& outPath)
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(navEntity) || !ecs.HasComponent<NavMeshComponent>(navEntity))
            return false;

        auto& navComp = ecs.GetComponent<NavMeshComponent>(navEntity);
        if (!navComp.runtime || !navComp.runtime->query)
        {
            if (!EnsureRuntimeFromBaked(*this, navComp))
                return false;
        }

        dtNavMeshQuery* query = navComp.runtime->query;

        dtQueryFilter filter;
        filter.setIncludeFlags(0xFFFF);
        filter.setExcludeFlags(0);

        dtPolyRef startRef = 0, endRef = 0;

        float spos[3] = { start.x, start.y, start.z };
        float epos[3] = { end.x,   end.y,   end.z };

        float nspos[3]; // nearest start on mesh
        float nepos[3]; // nearest end on mesh

        if (dtStatusFailed(query->findNearestPoly(spos, extents, &filter, &startRef, nspos)) || !startRef)
        {
            EE_CORE_WARN("[NavMeshSystem] findNearestPoly failed for start point");
            return false;
        }

        if (dtStatusFailed(query->findNearestPoly(epos, extents, &filter, &endRef, nepos)) || !endRef)
        {
            //EE_CORE_WARN("[NavMeshSystem] findNearestPoly failed for end point");
            return false;
        }

        // Use clamped points for the rest of the query
        spos[0] = nspos[0]; spos[1] = nspos[1]; spos[2] = nspos[2];
        epos[0] = nepos[0]; epos[1] = nepos[1]; epos[2] = nepos[2];

        dtPolyRef polys[256];
        int nPolys = 0;

        if (dtStatusFailed(query->findPath(startRef, endRef, spos, epos, &filter, polys, &nPolys, 256)))
        {
            EE_CORE_WARN("[NavMeshSystem] findPath failed");
            return false;
        }

        if (nPolys == 0)
        {
            EE_CORE_WARN("[NavMeshSystem] No corridor polys found");
            return false;
        }

        float straightPath[256 * 3];
        unsigned char straightFlags[256];
        dtPolyRef straightPolys[256];
        int nStraight = 0;

        if (dtStatusFailed(query->findStraightPath(spos, epos, polys, nPolys,
            straightPath, straightFlags, straightPolys, &nStraight, 256)))
        {
            EE_CORE_WARN("[NavMeshSystem] findStraightPath failed");
            return false;
        }

        outPath.clear();
        outPath.reserve((size_t)nStraight);

        for (int i = 0; i < nStraight; ++i)
        {
            outPath.push_back({ straightPath[i * 3 + 0], straightPath[i * 3 + 1], straightPath[i * 3 + 2] });
        }

        return !outPath.empty();
    }


    bool NavMeshSystem::ClampToNavMesh(EntityID navEntity,
        const Vec3& inPos,
        const float extents[3],
        Vec3& outPos)
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(navEntity) || !ecs.HasComponent<NavMeshComponent>(navEntity))
            return false;

        auto& navComp = ecs.GetComponent<NavMeshComponent>(navEntity);
        if (!navComp.runtime || !navComp.runtime->query)
        {
            if (!EnsureRuntimeFromBaked(*this, navComp))
                return false;
        }


        dtNavMeshQuery* query = navComp.runtime->query;

        dtQueryFilter filter;
        filter.setIncludeFlags(0xFFFF);
        filter.setExcludeFlags(0);

        float p[3] = { inPos.x, inPos.y, inPos.z };
        dtPolyRef ref = 0;
        float nearest[3];

        if (dtStatusFailed(query->findNearestPoly(p, extents, &filter, &ref, nearest)) || !ref)
            return false;

        outPos = { nearest[0], nearest[1], nearest[2] };
        return true;
    }


    void NavMeshSystem::RemoveEntity(EntityID e)
    {
        auto& ecs = ECS::GetInstance();
        if (ecs.HasComponent<NavMeshComponent>(e))
        {
            auto& c = ecs.GetComponent<NavMeshComponent>(e);
            EE_CORE_INFO("[NavMeshSystem] Cleaning up navmesh for removed entity {}", (uint32_t)e);
            DestroyBuild(c);
            DestroyRuntime(c);
        }

        m_Entities.erase(e);
    }
}
