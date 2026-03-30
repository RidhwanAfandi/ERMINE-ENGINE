/* Start Header ************************************************************************/
/*!
\file       Physics.cpp
\author     Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu
\date       Sept 02, 2025
\brief      This file contains the definition of the Physics structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Physics.h"
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include "MathVector.h"
#include "PhysicDebugRenderer.h"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Core/Color.h>
#include <iostream>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Renderer/DebugRenderer.h>

#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include "Input.h"

#include "EditorGUI.h"
#include "HierarchySystem.h"

using namespace std;
namespace Ermine
{
	// ------------------ Layer & Filter Implementations ------------------
	namespace Layers
	{
		static constexpr ObjectLayer NON_MOVING = 0;
		static constexpr ObjectLayer MOVING = 1;
		static constexpr ObjectLayer NUM_LAYERS = 2;
	};

	namespace BroadPhaseLayers
	{
		static constexpr BroadPhaseLayer NON_MOVING(0);
		static constexpr BroadPhaseLayer MOVING(1);
		static constexpr uint NUM_LAYERS(2);
	};

	// Defines collision rules between object layers
	class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
		{
			switch (inObject1)
			{
			case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
			case Layers::MOVING:     return true;
			default: JPH_ASSERT(false); return false;
			}
		}
	};

	// Maps object layers to broadphase layers (used for efficient collision detection)
	class BPLayerInterfaceImpl : public BroadPhaseLayerInterface
	{
	public:
		BPLayerInterfaceImpl()
		{
			mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

		// Returns the broadphase layer corresponding to an object layer
		virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
		{
			JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
			return mObjectToBroadPhase[inLayer];
		}

	private:
		BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
	};

	// Defines object vs broadphase layer collision rules
	class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
			case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
			case Layers::MOVING:     return true;
			default: JPH_ASSERT(false); return false;
			}
		}
	};

	// ------------------ Listener Implementations ------------------
	// Handles contact events (collisions)
	class MyContactListener : public ContactListener
	{
	public:
		// Called to validate a potential collision
		virtual ValidateResult OnContactValidate(const Body& inBody1, const Body& inBody2, RVec3Arg, const CollideShapeResult&) override
		{
			(void)inBody1; (void)inBody2;
			return ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		// Called when contact begins
		virtual void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold&, ContactSettings&) override
		{
			EE_CORE_INFO("[Physics] Collision Begin");

			ECS::GetInstance().GetSystem<Physics>()->HandleCollisionEvent(inBody1, inBody2, Physics::CollisionEventType::Begin);
		}

		// Called when contact persists across frames
		virtual void OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold&, ContactSettings&) override
		{
			ECS::GetInstance().GetSystem<Physics>()->HandleCollisionEvent(inBody1, inBody2, Physics::CollisionEventType::Stay);
		}

		// Called when contact ends
		virtual void OnContactRemoved(const SubShapeIDPair& inPair) override
		{
			// We can still get body references using BodyLockRead
			EE_CORE_INFO("[Physics] Collision End");
			ECS::GetInstance().GetSystem<Physics>()->HandleCollisionEvent(inPair.GetBody1ID(), inPair.GetBody2ID(), Physics::CollisionEventType::End);
		}
	};

	// Handles activation/deactivation of bodies (e.g., sleeping/waking up)
	class MyBodyActivationListener : public BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const BodyID&, uint64) override {}
		virtual void OnBodyDeactivated(const BodyID&, uint64) override {}
	};

	// ------------------ Physics Class ------------------
	/*!*************************************************************************
	\brief
	 Constructor for the Physics class. Initializes temporary memory
	 allocators, job system, and sets up filter and listener objects.
	***************************************************************************/
	Physics::Physics()
		: mTempAllocator(10 * 1024 * 1024),  // Allocate 10 MB for temporary physics data
		mJobSystem(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1)
	{
		// Create filter and listener objects for collision handling
		mBroadPhaseLayerInterface = new BPLayerInterfaceImpl();
		mObjectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
		mObjectLayerPairFilter = new ObjectLayerPairFilterImpl();
		mBodyActivationListener = new MyBodyActivationListener();
		mContactListener = new MyContactListener();
		wireframe = false;
	}

	/*!*************************************************************************
	  \brief
		Destructor for the Physics class. Shuts down the physics system and
		deallocates filter/listener objects.
	***************************************************************************/
	Physics::~Physics()
	{
		Shutdown();

		// Clean up allocated filter/listener objects
		delete mBroadPhaseLayerInterface;
		delete mObjectVsBroadPhaseLayerFilter;
		delete mObjectLayerPairFilter;
		delete mBodyActivationListener;
		delete mContactListener;
		mDebugRenderer.reset();
	}

	/*!*************************************************************************
	  \brief
		Initializes the Jolt physics system, allocates core objects, and sets up
		gravity, listeners, and broadphase optimizations.
	***************************************************************************/
	void Physics::Init()
	{
		RegisterDefaultAllocator();

		Factory::sInstance = new Factory();
		RegisterTypes();

		const uint cMaxBodies = 1024;
		const uint cNumBodyMutexes = 0;
		const uint cMaxBodyPairs = 1024;
		const uint cMaxContactConstraints = 1024;

		mPhysicsSystem.Init(
			cMaxBodies,
			cNumBodyMutexes,
			cMaxBodyPairs,
			cMaxContactConstraints,
			*mBroadPhaseLayerInterface,
			*mObjectVsBroadPhaseLayerFilter,
			*mObjectLayerPairFilter
		);

		mPhysicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
		mPhysicsSystem.SetBodyActivationListener(mBodyActivationListener);
		mPhysicsSystem.SetContactListener(mContactListener);
		mPhysicsSystem.OptimizeBroadPhase();

		JPH::DebugRenderer::sInstance = mDebugRenderer.get();
	}

	/*!*************************************************************************
	  \brief
		Shuts down the physics system, clears entity-body mappings, and deletes
		the physics factory.
	***************************************************************************/
	void Physics::Shutdown()
	{
		// unregister types and clear bodies
		UnregisterTypes();
		mEntityToBody.clear();
		mDebugRenderer.reset();

		delete Factory::sInstance;
		Factory::sInstance = nullptr;
	}

	/*!*************************************************************************
	  \brief
		Updates the physics system for the given timestep and synchronizes ECS
		transforms with the latest physics body positions and rotations.
	  \param[in] deltaTime
		Time elapsed since the last frame, in seconds.
	***************************************************************************/
	void Physics::Update(float deltaTime)
	{
		auto& ecs = ECS::GetInstance();
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();
		auto hierarchySystem = ecs.GetSystem<HierarchySystem>();

		if (!hierarchySystem)
			return;

		// Snapshot to avoid iterator invalidation while removing mappings
		std::vector<std::pair<EntityID, JPH::BodyID>> entries;
		entries.reserve(mEntityToBody.size());
		for (const auto& kv : mEntityToBody)
			entries.emplace_back(kv.first, kv.second);

		auto removeMapping = [&](EntityID e)
			{
				auto it = mEntityToBody.find(e);
				if (it != mEntityToBody.end())
					mEntityToBody.erase(it);
			};

		if (editor::EditorGUI::s_state == editor::EditorGUI::SimState::stopped)
		{
			for (const auto& [entity, rigidBody] : entries)
			{
				if (!ecs.IsEntityValid(entity) ||
					!ecs.HasComponent<PhysicComponent>(entity) ||
					!ecs.HasComponent<Transform>(entity))
				{
					removeMapping(entity);
					continue;
				}

				if (const auto* meta = ecs.TryGetComponent<ObjectMetaData>(entity))
				{
					if (!meta->selfActive)
						continue;
				}

				auto& p = ecs.GetComponent<PhysicComponent>(entity);

				Vec3 worldPos = hierarchySystem->GetWorldPosition(entity);
				Quaternion worldRot = hierarchySystem->GetWorldRotation(entity);

				Quaternion colliderOffset = QuaternionNormalize(FromEulerDegrees(p.colliderRot));
				Quaternion combined = QuaternionNormalize(worldRot * colliderOffset);

				JPH::Vec3 pos(
					worldPos.x + p.colliderPivot.x,
					worldPos.y + p.colliderPivot.y,
					worldPos.z + p.colliderPivot.z
				);

				JPH::Quat quat(combined.x, combined.y, combined.z, combined.w);

				bodyInterface.SetPositionAndRotationWhenChanged(
					rigidBody, pos, quat, JPH::EActivation::DontActivate);

				if (p.motionType == JPH::EMotionType::Dynamic)
				{
					bodyInterface.SetLinearVelocity(rigidBody, JPH::Vec3::sZero());
					bodyInterface.SetAngularVelocity(rigidBody, JPH::Vec3::sZero());
				}
			}
			return;
		}


		FlushPendingPairsToEntityEvents();

		while (!mCollisionEvent.empty())
		{
			auto ev = mCollisionEvent.front();
			mCollisionEvent.pop();

			auto const& [type, recipientEntity, otherEntity, sensor] = ev;

			if (!ecs.IsEntityValid(recipientEntity) || !ecs.IsEntityValid(otherEntity))
				continue;

			if (ecs.HasComponent<PhysicComponent>(recipientEntity) &&
				ecs.HasComponent<PhysicComponent>(otherEntity))
			{
				if (ecs.GetComponent<PhysicComponent>(recipientEntity).isDead ||
					ecs.GetComponent<PhysicComponent>(otherEntity).isDead)
				{
					continue;
				}
			}

			if (const auto* meta = ecs.TryGetComponent<ObjectMetaData>(recipientEntity))
			{
				if (!meta->selfActive)
					continue;
			}

			if (const auto* meta = ecs.TryGetComponent<ObjectMetaData>(otherEntity))
			{
				if (!meta->selfActive)
					continue;
			}

			if (ecs.HasComponent<ScriptsComponent>(recipientEntity))
			{
				auto& scs = ecs.GetComponent<ScriptsComponent>(recipientEntity);
				for (auto& scriptComp : scs.scripts)
				{
					if (!scriptComp.m_instance)
						continue;

					switch (type)
					{
					case CollisionEventType::Begin:
						scriptComp.m_instance->OnCollisionEnter(otherEntity, sensor);
						break;
					case CollisionEventType::Stay:
						scriptComp.m_instance->OnCollisionStay(otherEntity, sensor);
						break;
					case CollisionEventType::End:
						scriptComp.m_instance->OnCollisionExit(otherEntity, sensor);
						break;
					}
				}
			}

			if (ecs.HasComponent<StateMachine>(recipientEntity))
			{
				auto& statem = ecs.GetComponent<StateMachine>(recipientEntity);

				if (!statem.m_CurrentScript || !statem.m_CurrentScript->instance)
					continue;

				switch (type)
				{
				case CollisionEventType::Begin:
					statem.m_CurrentScript->instance->OnCollisionEnter(otherEntity, sensor);
					break;
				case CollisionEventType::Stay:
					statem.m_CurrentScript->instance->OnCollisionStay(otherEntity, sensor);
					break;
				case CollisionEventType::End:
					statem.m_CurrentScript->instance->OnCollisionExit(otherEntity, sensor);
					break;
				}
			}
		}


		mPhysicsSystem.Update(deltaTime, 1, &mTempAllocator, &mJobSystem);


		entries.clear();
		entries.reserve(mEntityToBody.size());
		for (const auto& kv : mEntityToBody)
			entries.emplace_back(kv.first, kv.second);

		for (const auto& [entity, rigidBody] : entries)
		{
			if (!ecs.IsEntityValid(entity) ||
				!ecs.HasComponent<PhysicComponent>(entity) ||
				!ecs.HasComponent<Transform>(entity))
			{
				removeMapping(entity);
				continue;
			}

			if (const auto* meta = ecs.TryGetComponent<ObjectMetaData>(entity))
			{
				if (!meta->selfActive)
					continue;
			}

			auto& p = ecs.GetComponent<PhysicComponent>(entity);
			if (p.motionType == JPH::EMotionType::Static || p.isDead)
				continue;

			if (hierarchySystem->GetParent(entity) != 0)
			{

				Vec3 worldPos = hierarchySystem->GetWorldPosition(entity);
				Quaternion worldRot = hierarchySystem->GetWorldRotation(entity);

				Quaternion colliderOffset = QuaternionNormalize(FromEulerDegrees(p.colliderRot));
				Quaternion combined = QuaternionNormalize(worldRot * colliderOffset);

				JPH::Vec3 pos(
					worldPos.x + p.colliderPivot.x,
					worldPos.y + p.colliderPivot.y,
					worldPos.z + p.colliderPivot.z
				);

				JPH::Quat quat(combined.x, combined.y, combined.z, combined.w);

				bodyInterface.SetPositionAndRotationWhenChanged(
					rigidBody, pos, quat, JPH::EActivation::DontActivate
				);

				continue;
			}


			JPH::RMat44 transform = bodyInterface.GetWorldTransform(rigidBody);

			Vec3 worldPos(
				transform.GetTranslation().GetX() - p.colliderPivot.x,
				transform.GetTranslation().GetY() - p.colliderPivot.y,
				transform.GetTranslation().GetZ() - p.colliderPivot.z
			);

			JPH::Quat rot = transform.GetRotation().GetQuaternion().Normalized();
			Quaternion bodyRot(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());

			Quaternion colliderOffset = QuaternionNormalize(FromEulerDegrees(p.colliderRot));
			Quaternion invOffset(
				-colliderOffset.x,
				-colliderOffset.y,
				-colliderOffset.z,
				colliderOffset.w
			);

			Quaternion worldRot = QuaternionNormalize(bodyRot * invOffset);

			hierarchySystem->SetWorldPosition(entity, worldPos);
			hierarchySystem->SetWorldRotation(entity, worldRot);
		}
	}


	/*!*************************************************************************
	  \brief
		Rebuilds the physics body list from ECS entities. Removes old bodies,
		creates shapes based on component data, and registers them with the
		physics system.
	***************************************************************************/
	void Physics::UpdatePhysicList()
	{
		auto& ecs = ECS::GetInstance();
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		// ---------- helper ----------
		auto DestroyBodyForEntity = [&](EntityID entity)
			{
				auto it = mEntityToBody.find(entity);
				if (it == mEntityToBody.end())
					return;

				JPH::BodyID id = it->second;

				if (!id.IsInvalid())
				{
					if (bodyInterface.IsAdded(id))
						bodyInterface.RemoveBody(id);

					bodyInterface.DestroyBody(id);
				}

				mEntityToBody.erase(it);
			};

		auto DestroyComponentBody = [&](PhysicComponent& p)
			{
				if (!p.bodyID.IsInvalid())
				{
					if (bodyInterface.IsAdded(p.bodyID))
						bodyInterface.RemoveBody(p.bodyID);

					bodyInterface.DestroyBody(p.bodyID);
				}

				p.body = nullptr;
				p.bodyID = JPH::BodyID();
				p.shapeRef = nullptr;
			};

		// ---------- nothing tracked ----------
		if (m_Entities.empty())
		{
			for (auto& [entity, bodyID] : mEntityToBody)
			{
				if (!bodyID.IsInvalid())
				{
					if (bodyInterface.IsAdded(bodyID))
						bodyInterface.RemoveBody(bodyID);

					bodyInterface.DestroyBody(bodyID);
				}
			}

			mEntityToBody.clear();
			return;
		}

		// ---------- main loop ----------
		for (EntityID entity : m_Entities)
		{
			// entity deleted
			if (!ecs.IsEntityValid(entity))
			{
				DestroyBodyForEntity(entity);
				continue;
			}

			// no transform = no physics
			if (!ecs.HasComponent<Transform>(entity))
			{
				DestroyBodyForEntity(entity);
				continue;
			}

			// physic component removed
			if (!ecs.HasComponent<PhysicComponent>(entity))
			{
				DestroyBodyForEntity(entity);
				continue;
			}

			auto& t = ecs.GetComponent<Transform>(entity);
			auto& p = ecs.GetComponent<PhysicComponent>(entity);

			// no update requested and body already exists
			if (!p.update && !p.bodyID.IsInvalid())
				continue;

			// rebuild path
			if (!p.bodyID.IsInvalid())
				DestroyComponentBody(p);

			// invalid scale
			if (t.scale.x <= 0 || t.scale.y <= 0 || t.scale.z <= 0)
				continue;

			// ---------- build shape ----------
			JPH::Ref<JPH::Shape> shapeRef;

			switch (p.shapeType)
			{
			case ShapeType::Box:
			{
				Vec3 halfExtent = {
					std::max(0.01f, t.scale.x * 0.5f * p.colliderSize.x),
					std::max(0.01f, t.scale.y * 0.5f * p.colliderSize.y),
					std::max(0.01f, t.scale.z * 0.5f * p.colliderSize.z)
				};

				float convexRadius =
					std::min({ halfExtent.x, halfExtent.y, halfExtent.z }) * 0.5f;

				shapeRef = new JPH::BoxShape(
					JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z),
					convexRadius
				);
				break;
			}

			case ShapeType::Sphere:
			{
				float radius = std::max(0.01f, t.scale.x * p.colliderSize.x);
				shapeRef = new JPH::SphereShape(radius);
				break;
			}

			case ShapeType::Capsule:
			{
				float halfHeight = std::max(0.01f, t.scale.y * 0.5f * p.colliderSize.y);
				float radius = std::max(0.01f, t.scale.x * 0.5f * p.colliderSize.x);
				shapeRef = new JPH::CapsuleShape(halfHeight, radius);
				break;
			}
			case ShapeType::CustomMesh:
			{
				p.customMeshVertices.clear();
				p.customMeshIndices.clear();

				// ---- MODEL ----
				if (ecs.HasComponent<ModelComponent>(entity))
				{
					auto& modelComp = ecs.GetComponent<ModelComponent>(entity);
					if (!modelComp.m_model) continue;

					modelComp.m_model->GetCollisionMesh(
						p.customMeshVertices,
						p.customMeshIndices,
						true
					);
				}
				// ---- MESH ----
				else if (ecs.HasComponent<Mesh>(entity))
				{
					auto& mesh = ecs.GetComponent<Mesh>(entity);
					if (mesh.cpuVertices.empty() || mesh.cpuIndices.empty())
						continue;

					p.customMeshVertices = mesh.cpuVertices;
					p.customMeshIndices = mesh.cpuIndices;
				}
				else
					continue;

				if (p.customMeshVertices.empty() || p.customMeshIndices.empty())
					continue;

				// ---- Dynamic/Kinematic  Convex Hull ----
				if (p.motionType == JPH::EMotionType::Dynamic ||
					p.motionType == JPH::EMotionType::Kinematic)
				{
					bool builtHull = false;

					//ABS scale (handles negative scale)
					glm::vec3 s(std::abs(t.scale.x), std::abs(t.scale.y), std::abs(t.scale.z));
					const float kMinScale = 1e-4f;
					s.x = std::max(s.x, kMinScale);
					s.y = std::max(s.y, kMinScale);
					s.z = std::max(s.z, kMinScale);

					// Collect scaled verts, skip NaN/Inf
					JPH::Array<JPH::Vec3> verts;
					verts.reserve(p.customMeshVertices.size());

					for (const auto& v : p.customMeshVertices)
					{
						if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
							continue;

						verts.push_back(JPH::Vec3(v.x * s.x, v.y * s.y, v.z * s.z));
					}

					if (verts.size() >= 4)
					{
						//Reduce to 64 verts
						constexpr size_t kMaxHullVerts = 64;
						if (verts.size() > kMaxHullVerts)
						{
							JPH::Array<JPH::Vec3> reduced;
							reduced.reserve(kMaxHullVerts);

							size_t step = std::max<size_t>(1, verts.size() / kMaxHullVerts);
							for (size_t i = 0; i < verts.size() && reduced.size() < kMaxHullVerts; i += step)
								reduced.push_back(verts[i]);

							verts = std::move(reduced);
						}

						//Remove near-duplicates via quantization hash
						struct Key { int x, y, z; };
						struct KeyHash {
							size_t operator()(Key const& k) const noexcept {
								size_t h = 1469598103934665603ull;
								auto mix = [&](int v) { h ^= (size_t)v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); };
								mix(k.x); mix(k.y); mix(k.z);
								return h;
							}
						};
						struct KeyEq {
							bool operator()(Key const& a, Key const& b) const noexcept {
								return a.x == b.x && a.y == b.y && a.z == b.z;
							}
						};

						const float kQuant = 1000.0f; // grid size: 0.001 units
						std::unordered_set<Key, KeyHash, KeyEq> seen;
						JPH::Array<JPH::Vec3> unique;
						unique.reserve(verts.size());

						for (const auto& pnt : verts)
						{
							Key k{
								(int)std::round(pnt.GetX() * kQuant),
								(int)std::round(pnt.GetY() * kQuant),
								(int)std::round(pnt.GetZ() * kQuant)
							};
							if (seen.insert(k).second)
								unique.push_back(pnt);
						}

						verts = std::move(unique);

						if (verts.size() >= 4)
						{
							// 5) Build hull
							JPH::ConvexHullShapeSettings hullSettings(verts);
							auto hullRes = hullSettings.Create();
							if (!hullRes.HasError())
							{
								shapeRef = hullRes.Get();
								builtHull = true;
							}
						}
					}

					//Fallback box if hull failed
					if (!builtHull)
					{
						glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
						for (const auto& v : p.customMeshVertices)
						{
							if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
								continue;
							mn = glm::min(mn, v);
							mx = glm::max(mx, v);
						}

						glm::vec3 localCenter = (mn + mx) * 0.5f;
						glm::vec3 localSize = (mx - mn);

						glm::vec3 scaledCenter = localCenter * s;
						glm::vec3 scaledSize = localSize * s;

						const float kMinExtent = 0.02f;
						scaledSize.x = std::max(scaledSize.x, kMinExtent);
						scaledSize.y = std::max(scaledSize.y, kMinExtent);
						scaledSize.z = std::max(scaledSize.z, kMinExtent);

						JPH::Vec3 halfExt(scaledSize.x * 0.5f, scaledSize.y * 0.5f, scaledSize.z * 0.5f);

						float minHalf = std::min({ halfExt.GetX(), halfExt.GetY(), halfExt.GetZ() });
						float convexRadius = std::min(0.05f, minHalf);
						if (minHalf < 0.051f) convexRadius = 0.0f;

						JPH::Ref<JPH::Shape> box = new JPH::BoxShape(halfExt, convexRadius);
						shapeRef = new JPH::RotatedTranslatedShape(
							JPH::Vec3(scaledCenter.x, scaledCenter.y, scaledCenter.z),
							JPH::Quat::sIdentity(),
							box
						);
					}
				}
				// ---- Static ----
				else
				{
					if (p.customMeshVertices.size() < 2000)
					{
						glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
						for (const auto& v : p.customMeshVertices) {
							mn = glm::min(mn, v);
							mx = glm::max(mx, v);
						}

						glm::vec3 localCenter = (mn + mx) * 0.5f;
						glm::vec3 localSize = (mx - mn);

						// use ABS scale (handles negative scale)
						glm::vec3 s(std::abs(t.scale.x), std::abs(t.scale.y), std::abs(t.scale.z));

						glm::vec3 scaledCenter = localCenter * s;
						glm::vec3 scaledSize = localSize * s;

						// clamp tiny assets
						const float kMinExtent = 0.02f;
						scaledSize.x = std::max(scaledSize.x, kMinExtent);
						scaledSize.y = std::max(scaledSize.y, kMinExtent);
						scaledSize.z = std::max(scaledSize.z, kMinExtent);

						JPH::Vec3 halfExt(scaledSize.x * 0.5f, scaledSize.y * 0.5f, scaledSize.z * 0.5f);

						// IMPORTANT: convex radius must be <= smallest half extent
						float minHalf = std::min({ halfExt.GetX(), halfExt.GetY(), halfExt.GetZ() });
						float convexRadius = std::min(0.05f, minHalf);          // <= minHalf
						if (minHalf < 0.051f) convexRadius = 0.0f;              // tiny -> safest

						JPH::Ref<JPH::Shape> box = new JPH::BoxShape(halfExt, convexRadius);

						// Wrap with local translation, so you don't need p.colliderPivot at all:
						shapeRef = new JPH::RotatedTranslatedShape(
							JPH::Vec3(scaledCenter.x, scaledCenter.y, scaledCenter.z),
							JPH::Quat::sIdentity(),
							box
						);

						// Keep pivot at zero now (prevents drifting / weird offsets)
						p.colliderPivot = Vec3(0, 0, 0);

						break;
					}

					if (p.customMeshIndices.size() % 3 != 0)
						continue;

					JPH::Array<JPH::Float3> verts;
					verts.reserve(p.customMeshVertices.size());
					for (const auto& v : p.customMeshVertices)
					{
						glm::vec3 s(std::abs(t.scale.x), std::abs(t.scale.y), std::abs(t.scale.z));
						verts.push_back(JPH::Float3(v.x* s.x, v.y* s.y, v.z* s.z));
					}

					JPH::Array<JPH::IndexedTriangle> tris;
					tris.reserve((p.customMeshIndices.size() / 3) * 2);

					for (size_t i = 0; i < p.customMeshIndices.size(); i += 3)
					{
						uint32_t a = p.customMeshIndices[i];
						uint32_t b = p.customMeshIndices[i + 1];
						uint32_t c = p.customMeshIndices[i + 2];

						tris.push_back(JPH::IndexedTriangle(a, b, c));
						//tris.push_back(JPH::IndexedTriangle(c, b, a)); //no need double face
					}

					JPH::MeshShapeSettings meshSettings(verts, tris);
					meshSettings.mActiveEdgeCosThresholdAngle = 0.999f;

					auto meshRes = meshSettings.Create();
					if (meshRes.HasError())
						continue;

					shapeRef = meshRes.Get();
				}

				break;
			}

			default:
				continue;
			}

			if (!shapeRef)
				continue;

			p.shapeRef = shapeRef;

			// ---------- motion type ----------
			JPH::EMotionType motionType = p.motionType;

			if (p.posX && p.posY && p.posZ &&
				motionType == JPH::EMotionType::Dynamic)
			{
				motionType = JPH::EMotionType::Static;
			}

			ObjectLayer layer =
				(motionType == JPH::EMotionType::Dynamic)
				? Layers::MOVING
				: Layers::NON_MOVING;

			// ---------- world transform ----------
			auto hierarchy = ecs.GetSystem<HierarchySystem>();
			Vec3 worldPos = hierarchy->GetWorldPosition(entity);
			Quaternion worldRot = QuaternionNormalize(
				hierarchy->GetWorldRotation(entity)
			);

			Quaternion colliderOffset =
				QuaternionNormalize(FromEulerDegrees(p.colliderRot));

			Quaternion finalRot =
				QuaternionNormalize(worldRot * colliderOffset);

			// ---------- body settings ----------
			JPH::BodyCreationSettings settings(
				shapeRef,
				JPH::Vec3(
					worldPos.x + p.colliderPivot.x,
					worldPos.y + p.colliderPivot.y,
					worldPos.z + p.colliderPivot.z
				),
				JPH::Quat(finalRot.x, finalRot.y, finalRot.z, finalRot.w),
				motionType,
				layer
			);

			settings.mIsSensor =
				(p.bodyType == PhysicsBodyType::Trigger);

			settings.mEnhancedInternalEdgeRemoval = true;

			if (motionType == JPH::EMotionType::Dynamic)
			{
				settings.mOverrideMassProperties =
					JPH::EOverrideMassProperties::CalculateInertia;
				settings.mMassPropertiesOverride.mMass =
					std::max(0.01f, p.mass);
			}

			JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::All;
			if (p.posX) dofs &= ~JPH::EAllowedDOFs::TranslationX;
			if (p.posY) dofs &= ~JPH::EAllowedDOFs::TranslationY;
			if (p.posZ) dofs &= ~JPH::EAllowedDOFs::TranslationZ;
			if (p.rotX) dofs &= ~JPH::EAllowedDOFs::RotationX;
			if (p.rotY) dofs &= ~JPH::EAllowedDOFs::RotationY;
			if (p.rotZ) dofs &= ~JPH::EAllowedDOFs::RotationZ;
			settings.mAllowedDOFs = dofs;

			// ---------- create ----------
			JPH::Body* body = bodyInterface.CreateBody(settings);
			if (!body)
				continue;

			body->SetUserData(static_cast<uint64_t>(entity));
			bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);

			p.body = body;
			p.bodyID = body->GetID();
			mEntityToBody[entity] = p.bodyID;
			p.update = false;
		}
	}


	/*!*************************************************************************
	  \brief
		Retrieves the physics BodyID corresponding to an ECS entity.
	  \param[in] objectID
		The ECS entity ID to query.
	  \return
		A valid BodyID if found, otherwise an invalid BodyID.
	***************************************************************************/
	JPH::BodyID Physics::GetBodyID(EntityID objectID)
	{
		auto it = mEntityToBody.find(objectID);
		return (it != mEntityToBody.end())
			? it->second
			: JPH::BodyID(JPH::BodyID::cInvalidBodyID);
	}

	/*!*************************************************************************
	  \brief
		Retrieves the ECS entity corresponding to a physics BodyID.
	  \param[in] bodyID
		The BodyID to query.
	  \return
		The entity ID if found, otherwise 0.
	***************************************************************************/
	EntityID Physics::GetEntityID(JPH::BodyID bodyID)
	{
		for (auto& [entity, rigidBody] : mEntityToBody)
		{
			if (rigidBody == bodyID)
			{
				return entity;
			}
		}

		return 0;
	}

	/*!*************************************************************************
	  \brief
		BodyManager DrawSettings setup for draw debug
	***************************************************************************/
	void Physics::DrawDebug()
	{
#ifdef JPH_DEBUG_RENDERER
		if (!mDebugRenderer) // nothing to draw
			return;

		BodyManager::DrawSettings ds{};
		ds.mDrawShape = false;  // solid off
		ds.mDrawShapeWireframe = true;   // wireframe on
		ds.mDrawBoundingBox = true;

		mPhysicsSystem.DrawBodies(ds, mDebugRenderer.get());
#endif
	}

	/*!*************************************************************************
	  \brief
		Draws detailed wireframe visualization for all physics bodies, including
		custom mesh geometry.
	***************************************************************************/
	void Physics::DrawDebugPhysics()
	{
#ifdef JPH_DEBUG_RENDERER
		if (!mDebugRenderer || !wireframe)
			return;

		JPH::BodyIDVector bodies;
		mPhysicsSystem.GetBodies(bodies);

		const JPH::BodyLockInterface& bli = mPhysicsSystem.GetBodyLockInterface();

		constexpr int cMax = 512;
		constexpr int kMaxDebugTrianglesPerShape = 5000;

		for (JPH::BodyID id : bodies)
		{
			JPH::BodyLockRead lock(bli, id);
			if (!lock.SucceededAndIsInBroadPhase())
				continue;

			const JPH::Body& body = lock.GetBody();

			JPH::Color color = JPH::Color::sWhite;
			switch (body.GetMotionType())
			{
			case JPH::EMotionType::Static:    color = JPH::Color::sCyan; break;
			case JPH::EMotionType::Kinematic: color = JPH::Color::sGreen; break;
			case JPH::EMotionType::Dynamic:   color = JPH::Color::sGetDistinctColor(body.GetID().GetIndex()); break;
			default: break;
			}

			JPH::AllHitCollisionCollector<JPH::TransformedShapeCollector> collector;
			body.GetTransformedShape().CollectTransformedShapes(body.GetWorldSpaceBounds(), collector);

			for (const JPH::TransformedShape& ts : collector.mHits)
			{
				// Optional: skip giant static meshes completely
				if (body.GetMotionType() == JPH::EMotionType::Static)
				{
					const JPH::AABox bounds = ts.GetWorldSpaceBounds();
					JPH::Vec3 extent = bounds.GetExtent();

					// Very large object, draw only AABB
					if (extent.GetX() > 20.0f || extent.GetY() > 20.0f || extent.GetZ() > 20.0f)
					{
						JPH::RVec3 mn = bounds.mMin;
						JPH::RVec3 mx = bounds.mMax;

						JPH::RVec3 p000(mn.GetX(), mn.GetY(), mn.GetZ());
						JPH::RVec3 p001(mn.GetX(), mn.GetY(), mx.GetZ());
						JPH::RVec3 p010(mn.GetX(), mx.GetY(), mn.GetZ());
						JPH::RVec3 p011(mn.GetX(), mx.GetY(), mx.GetZ());
						JPH::RVec3 p100(mx.GetX(), mn.GetY(), mn.GetZ());
						JPH::RVec3 p101(mx.GetX(), mn.GetY(), mx.GetZ());
						JPH::RVec3 p110(mx.GetX(), mx.GetY(), mn.GetZ());
						JPH::RVec3 p111(mx.GetX(), mx.GetY(), mx.GetZ());

						mDebugRenderer->DrawLine(p000, p001, color);
						mDebugRenderer->DrawLine(p000, p010, color);
						mDebugRenderer->DrawLine(p000, p100, color);

						mDebugRenderer->DrawLine(p111, p110, color);
						mDebugRenderer->DrawLine(p111, p101, color);
						mDebugRenderer->DrawLine(p111, p011, color);

						mDebugRenderer->DrawLine(p001, p011, color);
						mDebugRenderer->DrawLine(p001, p101, color);

						mDebugRenderer->DrawLine(p010, p011, color);
						mDebugRenderer->DrawLine(p010, p110, color);

						mDebugRenderer->DrawLine(p100, p101, color);
						mDebugRenderer->DrawLine(p100, p110, color);

						continue;
					}
				}

				JPH::Shape::GetTrianglesContext ctx;
				ts.mShape->GetTrianglesStart(
					ctx,
					JPH::AABox::sBiggest(),
					JPH::Vec3::sZero(),
					JPH::Quat::sIdentity(),
					JPH::Vec3::sOne()
				);

				const JPH::RMat44 world = ts.GetCenterOfMassTransform();
				std::vector<JPH::Float3> verts(3 * cMax);

				int totalTriangles = 0;

				for (;;)
				{
					int triCount = ts.mShape->GetTrianglesNext(ctx, cMax, verts.data());
					if (triCount == 0)
						break;

					totalTriangles += triCount;

					// Too expensive, stop drawing this shape
					if (totalTriangles > kMaxDebugTrianglesPerShape)
					{
						break;
					}

					for (int t = 0; t < triCount; ++t)
					{
						const JPH::Float3& p0 = verts[3 * t + 0];
						const JPH::Float3& p1 = verts[3 * t + 1];
						const JPH::Float3& p2 = verts[3 * t + 2];

						if (!std::isfinite(p0.x) || !std::isfinite(p0.y) || !std::isfinite(p0.z) ||
							!std::isfinite(p1.x) || !std::isfinite(p1.y) || !std::isfinite(p1.z) ||
							!std::isfinite(p2.x) || !std::isfinite(p2.y) || !std::isfinite(p2.z))
							continue;

						JPH::RVec3 a = world * JPH::RVec3(p0.x, p0.y, p0.z);
						JPH::RVec3 b = world * JPH::RVec3(p1.x, p1.y, p1.z);
						JPH::RVec3 c = world * JPH::RVec3(p2.x, p2.y, p2.z);

						mDebugRenderer->DrawLine(a, b, color);
						mDebugRenderer->DrawLine(b, c, color);
						mDebugRenderer->DrawLine(c, a, color);
					}
				}
			}
		}
#endif
	}

	/*!*************************************************************************
	  \brief
		Attaches a debug renderer for visualization.
	  \param[in] renderer
		Shared pointer to a MyDebugRenderer instance.
	***************************************************************************/
	void Physics::AttachDebugRenderer(std::shared_ptr<MyDebugRenderer> renderer)
	{
		mDebugRenderer = std::move(renderer);
#ifdef JPH_DEBUG_RENDERER
		JPH::DebugRenderer::sInstance = mDebugRenderer.get();
#endif
	}

	/*!*************************************************************************
	  \brief
		Handles a collision event given physics Body references.
	  \param[in] a
		First body in collision.
	  \param[in] b
		Second body in collision.
	  \param[in] type
		Type of collision event (Begin, Stay, End).
	***************************************************************************/
	void Physics::HandleCollisionEvent(const Body& a, const Body& b, CollisionEventType type)
	{
		std::lock_guard<std::mutex> _l(mPendingMutex);
		mPendingPairs.push_back(PendingPair{ type, a.GetID(), b.GetID() });
	}

	/*!*************************************************************************
	  \brief
		Handles a collision event given BodyIDs.
	  \param[in] a
		First BodyID.
	  \param[in] b
		Second BodyID.
	  \param[in] type
		Type of collision event (Begin, Stay, End).
	***************************************************************************/
	void Physics::HandleCollisionEvent(JPH::BodyID a, JPH::BodyID b, CollisionEventType type)
	{
		std::lock_guard<std::mutex> _l(mPendingMutex);
		mPendingPairs.push_back(PendingPair{ type, a, b });
	}

	/*!*************************************************************************
	  \brief
		Performs a raycast in the physics world.
	  \param[in] origin
		Starting point of the ray.
	  \param[in] direction
		Direction of the ray.
	  \param[in] maxDistance
		Maximum distance to check.
	  \param[out] outResult
		Raycast hit information if a hit occurs.
	  \return
		True if a collision was detected, false otherwise.
	***************************************************************************/
	bool Physics::Raycast(const JPH::Vec3& origin, const JPH::Vec3& direction, float maxDistance, JPH::RayCastResult& outResult)
	{
		JPH::Vec3 dirNormalized = direction.Normalized();

		JPH::RRayCast ray(origin, dirNormalized * maxDistance);

		const JPH::NarrowPhaseQuery& query = mPhysicsSystem.GetNarrowPhaseQuery();

		// Perform the cast
		bool hit = query.CastRay(ray, outResult);

		return hit;
	}

	/*!*************************************************************************
	  \brief
		Performs a raycast and returns all hits sorted by distance.
	  \param[in] origin
		Starting point of the ray.
	  \param[in] direction
		Direction of the ray.
	  \param[in] maxDistance
		Maximum distance to check.
	  \return
		A vector of RayCastResults, sorted nearest to farthest.
	***************************************************************************/
	std::vector<JPH::RayCastResult> Physics::RaycastAll(const JPH::Vec3& origin, const JPH::Vec3& direction, float maxDistance)
	{
		std::vector<JPH::RayCastResult> results;

		// Normalize direction
		JPH::Vec3 dirNormalized = direction.Normalized();

		// Build the ray (RRayCast takes origin and direction *distance)
		JPH::RRayCast ray(origin, dirNormalized * maxDistance);

																			// Ray cast settings WIP to add ignore layer
		JPH::RayCastSettings settings;
		settings.SetBackFaceMode(JPH::EBackFaceMode::IgnoreBackFaces);

		JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;

		// Get the narrow phase query and perform the cast
		const JPH::NarrowPhaseQuery& query = mPhysicsSystem.GetNarrowPhaseQuery();
		query.CastRay(ray, settings, collector);

		for (const auto& hit : collector.mHits)
			results.push_back(hit);

		// Sort nearest to farthest
		std::sort(results.begin(), results.end(),
			[](const JPH::RayCastResult& a, const JPH::RayCastResult& b)
			{
				return a.mFraction < b.mFraction;
			});

		return results;
	}

	/*!*************************************************************************
	 \brief
		Removes all physics bodies from the system and clears the internal mapping.
	***************************************************************************/
	void Physics::ClearPhysicBody()
	{
		auto& ecs = ECS::GetInstance();
		JPH::BodyInterface& bi = mPhysicsSystem.GetBodyInterface();

		// Copy the list first — do NOT iterate live list
		JPH::BodyIDVector bodyIDs;
		mPhysicsSystem.GetBodies(bodyIDs);   // safe snapshot

		// First: Remove all bodies from world
		for (JPH::BodyID id : bodyIDs)
		{
			if (bi.IsAdded(id))
				bi.RemoveBody(id);
		}

		// Second: Destroy all bodies safely
		for (JPH::BodyID id : bodyIDs)
		{
			bi.DestroyBody(id);
		}

		// Clear internal ECS → Physics mappings
		for (auto& [entity, bodyID] : mEntityToBody)
		{
			if (ecs.IsEntityValid(entity) &&
				ecs.HasComponent<PhysicComponent>(entity))
			{
				auto& p = ecs.GetComponent<PhysicComponent>(entity);
				p.body = nullptr;
				p.bodyID = JPH::BodyID(JPH::BodyID::cInvalidBodyID);
			}
		}

		mEntityToBody.clear();
	}


	/*!*************************************************************************
	  \brief
		Sets the world position of a physics body corresponding to the given entity.
	  \param[in] ID
		ECS entity ID.
	  \param[in] position
		New world position.
	***************************************************************************/
	void Physics::SetPosition(EntityID ID, Ermine::Vec3 position)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		auto bodyPos = ECS::GetInstance().GetComponent<PhysicComponent>(ID).colliderPivot;

		bodyInterface.SetPositionAndRotation(
			GetBodyID(ID),
			JPH::Vec3(position.x + bodyPos.x, position.y + bodyPos.y, position.z + bodyPos.z),
			bodyInterface.GetRotation(GetBodyID(ID)),
			JPH::EActivation::Activate);
	}

	/*!*************************************************************************
	  \brief
		Sets the world rotation of a physics body corresponding to the given entity using Euler angles.
	  \param[in] ID
		ECS entity ID.
	  \param[in] rotation
		Rotation in Euler angles.
	***************************************************************************/
	void Physics::SetRotation(EntityID ID, Ermine::Vec3 rotation)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		// Convert input Euler angles to quaternion
		Ermine::Quaternion rot = FromEulerDegrees(rotation);
		rot = QuaternionNormalize(rot);

		// Get collider rotation offset
		auto& p = ECS::GetInstance().GetComponent<PhysicComponent>(ID);
		Ermine::Quaternion offset = QuaternionNormalize(FromEulerDegrees(p.colliderRot));

		// Apply offset
		Ermine::Quaternion finalRot = rot * offset;

		// Convert to JPH::Quat
		JPH::Quat jphQuat(finalRot.x, finalRot.y, finalRot.z, finalRot.w);

		// Set rotation while keeping the current physics position
		bodyInterface.SetPositionAndRotation(
			GetBodyID(ID),
			bodyInterface.GetPosition(GetBodyID(ID)),
			jphQuat,
			JPH::EActivation::Activate
		);
	}

	/*!*************************************************************************
	  \brief
		Sets the world rotation of a physics body corresponding to the given entity using a quaternion.
	  \param[in] ID
		ECS entity ID.
	  \param[in] rotation
		Rotation as a quaternion.
	***************************************************************************/
	void Physics::SetRotation(EntityID ID, Ermine::Quaternion rotation)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		Ermine::Quaternion rot = QuaternionNormalize(rotation);

		Ermine::Quaternion offset = QuaternionNormalize(FromEulerDegrees(ECS::GetInstance().GetComponent<PhysicComponent>(ID).colliderRot));

		Ermine::Quaternion finalRot = rot * offset; // rotate input then apply offset
		JPH::Quat jphQuat(finalRot.x, finalRot.y, finalRot.z, finalRot.w);

		bodyInterface.SetPositionAndRotation(
			GetBodyID(ID),
			bodyInterface.GetPosition(GetBodyID(ID)),
			jphQuat,
			JPH::EActivation::Activate);
	}

	/*!***********************************************************************
	  \brief
		Gets the world position of a physics body by entity ID
	*************************************************************************/
	Ermine::Vec3 Physics::GetPosition(EntityID id)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();
		JPH::BodyID bodyID = GetBodyID(id);
		JPH::Body* body = ECS::GetInstance().GetComponent<PhysicComponent>(id).body;
		if (!body) return Ermine::Vec3{ 0.0f, 0.0f, 0.0f };
		JPH::RVec3 pos = body->GetPosition();
		return Ermine::Vec3{ static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()) };
	}

	/*!***********************************************************************
	  \brief
		Gets the world rotation of a physics body by entity ID
	*************************************************************************/
	Ermine::Quaternion Physics::GetRotation(EntityID id)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();
		JPH::BodyID bodyID = GetBodyID(id);
		JPH::Body* body = ECS::GetInstance().GetComponent<PhysicComponent>(id).body;
		if (!body) return Ermine::Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
		JPH::Quat rot = body->GetRotation();
		return Ermine::Quaternion{ static_cast<float>(rot.GetX()), static_cast<float>(rot.GetY()), static_cast<float>(rot.GetZ()), static_cast<float>(rot.GetW()) };
	}

	/*!*************************************************************************
	  \brief
		Moves a physics body to a new position and rotation (Euler angles).
	  \param[in] ID
		ECS entity ID.
	  \param[in] position
		Target position.
	  \param[in] rotation
		Target rotation in Euler angles.
	***************************************************************************/
	void Physics::Move(EntityID ID, Ermine::Vec3 position, Ermine::Vec3 rotation)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		Ermine::Quaternion rot = FromEulerDegrees(rotation);
		rot = QuaternionNormalize(rot);

		auto& p = ECS::GetInstance().GetComponent<PhysicComponent>(ID);
		Ermine::Quaternion offset = QuaternionNormalize(FromEulerDegrees(p.colliderRot));
		Ermine::Quaternion finalRot = rot * offset;

		JPH::Quat jphQuat(finalRot.x, finalRot.y, finalRot.z, finalRot.w);

		JPH::Vec3 posWithPivot(position.x + p.colliderPivot.x,
			position.y + p.colliderPivot.y,
			position.z + p.colliderPivot.z);

		bodyInterface.SetPositionAndRotation(
			GetBodyID(ID),
			posWithPivot,
			jphQuat,
			JPH::EActivation::Activate
		);
	}

	/*!*************************************************************************
	  \brief
		Moves a physics body to a new position and rotation (quaternion).
	  \param[in] ID
		ECS entity ID.
	  \param[in] position
		Target position.
	  \param[in] rotation
		Target rotation as a quaternion.
	***************************************************************************/
	void Physics::Move(EntityID ID, Ermine::Vec3 position, Ermine::Quaternion rotation)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		Ermine::Quaternion rot = QuaternionNormalize(rotation);

		auto& p = ECS::GetInstance().GetComponent<PhysicComponent>(ID);
		Ermine::Quaternion offset = QuaternionNormalize(FromEulerDegrees(p.colliderRot));
		Ermine::Quaternion finalRot = rot * offset;

		JPH::Quat jphQuat(finalRot.x, finalRot.y, finalRot.z, finalRot.w);

		JPH::Vec3 posWithPivot(position.x + p.colliderPivot.x,
			position.y + p.colliderPivot.y,
			position.z + p.colliderPivot.z);

		bodyInterface.SetPositionAndRotation(
			GetBodyID(ID),
			posWithPivot,
			jphQuat,
			JPH::EActivation::Activate
		);
	}

	void Physics::Jump(EntityID ID, float jumpStrength)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();
		auto bodyID = GetBodyID(ID);

		// Get current velocity so we can preserve horizontal motion
		JPH::BodyID jphBodyID = bodyID;
		JPH::Body* body = ECS::GetInstance().GetComponent<PhysicComponent>(ID).body;
		if (!body) return;

		JPH::Vec3 currentVel = body->GetLinearVelocity();

		// Set new velocity: keep horizontal velocity, add upward jump
		JPH::Vec3 jumpVel = currentVel;
		jumpVel.SetY(jumpStrength);  // assuming Y is up

		bodyInterface.SetLinearVelocity(jphBodyID, jumpVel);
		auto& t = ECS::GetInstance().GetComponent<Transform>(ID).position;
		t = Vec3 (bodyInterface.GetPosition(jphBodyID).GetX(), bodyInterface.GetPosition(jphBodyID).GetY(),bodyInterface.GetPosition(jphBodyID).GetZ());

	}

	void Physics::RemovePhysic(EntityID ID)
	{
		auto& bodyInterface = mPhysicsSystem.GetBodyInterface();

		if (ECS::GetInstance().HasComponent<PhysicComponent>(ID))
		{
			auto& p = ECS::GetInstance().GetComponent<PhysicComponent>(ID);

			if (p.body)
			{
				try
				{
					bodyInterface.RemoveBody(p.bodyID);
				}
				catch (...)
				{
					std::cerr << "[Physics] RemoveBody failed or body not added for entity " << (uint32_t)p.body->GetUserData() << "\n";
				}

				try
				{
					bodyInterface.DestroyBody(p.bodyID);
				}
				catch (...)
				{
					std::cerr << "[Physics] DestroyBody failed for entity " << (uint32_t)p.body->GetUserData() << "\n";
				}

				p.body = nullptr;
				p.bodyID = JPH::BodyID(JPH::BodyID::cInvalidBodyID);
				mEntityToBody.erase(ID);
			}
		}
	}

	bool Physics::HasPhysicComp(EntityID ID)
	{
		if (ECS::GetInstance().HasComponent<PhysicComponent>(ID))
		{
			return true;
		}
		return false;
	}

	void Physics::SetLightValue(EntityID ID, float value)
	{
		if (ECS::GetInstance().HasComponent<Light>(ID))
		{
			auto& lightobj = ECS::GetInstance().GetComponent<Light>(ID);
			lightobj.intensity = value;
		}
	}

	void Physics::ForceUpdate()
	{
		UpdatePhysicList();
	}

	int Physics::GetMotionType(EntityID ID)
	{
		if (ECS::GetInstance().HasComponent<PhysicComponent>(ID))
		{
			return (int)ECS::GetInstance().GetComponent<PhysicComponent>(ID).motionType;
		}
		return 3;
	}

	/*!*************************************************************************
	  \brief
		Converts pending physics collision pairs into ECS collision events.
		This ensures that scripts receive callbacks in the correct order.
	***************************************************************************/
	void Physics::FlushPendingPairsToEntityEvents()
	{
		std::vector<PendingPair> local;
		{
			std::lock_guard<std::mutex> _l(mPendingMutex);
			if (mPendingPairs.empty()) return;
			local.swap(mPendingPairs);
		}

		auto& ecs = ECS::GetInstance();
		const JPH::BodyLockInterface& bli = mPhysicsSystem.GetBodyLockInterface();

		for (const PendingPair& pp : local)
		{
			EntityID entA = 0, entB = 0;
			bool aIsSensor = false, bIsSensor = false;

			{
				JPH::BodyLockRead lockA(bli, pp.a);
				if (lockA.SucceededAndIsInBroadPhase())
				{
					const JPH::Body& ba = lockA.GetBody();
					entA = ba.GetUserData();
					aIsSensor = ba.IsSensor();
				}
			}

			{
				JPH::BodyLockRead lockB(bli, pp.b);
				if (lockB.SucceededAndIsInBroadPhase())
				{
					const JPH::Body& bb = lockB.GetBody();
					entB = bb.GetUserData();
					bIsSensor = bb.IsSensor();
				}
			}

			// Fallback
			if ((!ecs.IsEntityValid(entA) || !ecs.IsEntityValid(entB)))
			{
				for (auto& [entity, bodyID] : mEntityToBody)
				{
					if (entA == 0 && bodyID == pp.a) entA = entity;
					if (entB == 0 && bodyID == pp.b) entB = entity;
					if (entA && entB) break;
				}
			}

			if (!ecs.IsEntityValid(entA) && !ecs.IsEntityValid(entB))
				continue;

			if (ecs.HasComponent<PhysicComponent>(entA) && ecs.HasComponent<PhysicComponent>(entB))
			{
				if (ecs.GetComponent<PhysicComponent>(entA).isDead || ecs.GetComponent<PhysicComponent>(entB).isDead)
					continue;
			}

			if (ecs.IsEntityValid(entA) && (ecs.HasComponent<ScriptsComponent>(entA) || ecs.HasComponent<StateMachine>(entA)))
				mCollisionEvent.emplace(pp.type, entA, entB, bIsSensor);
			if (ecs.IsEntityValid(entB) && (ecs.HasComponent<ScriptsComponent>(entB) || ecs.HasComponent<StateMachine>(entB)))
				mCollisionEvent.emplace(pp.type, entB, entA, aIsSensor);
		}
	}

}
