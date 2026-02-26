/* Start Header ************************************************************************/
/*!
\file       ScriptInstance.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       07/08/2025
\brief      This file contains the detail of per-entity script

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "ScriptClass.h"

namespace Ermine::scripting
{
	void NativeBindComponentGameObject(MonoObject* componentObj, EntityID id);

	struct ScriptInstance
	{
		std::unique_ptr<ScriptClass> klass;
		MonoObject* object = nullptr;
		MonoGCHandle GCHandle = nullptr; // Mono GC handle for the script instance
		EntityID	entityID; // Entity ID this script instance is attached to

		bool m_enabled = false;

		explicit ScriptInstance(std::unique_ptr<ScriptClass> k, const EntityID eid) : klass(std::move(k)), entityID(eid)
		{
			EE_CORE_TRACE("ScriptInstance: Ctor");
			object = klass ? klass->Instantiate() : nullptr;
			if (!object)
			{
				EE_CORE_ERROR("ScriptInstance: failed to instantiate managed object");
				return;
			}
			GCHandle = mono_gchandle_new_v2(object, false);

			MonoObject* target = mono_gchandle_get_target_v2(GCHandle);
			InjectEntityIfAvailable(target);
			NativeBindComponentGameObject(target, eid);
			Awake(); // called when an enabled script instance is being loaded.
		}

		~ScriptInstance()
		{
			MonoObject* target = GCHandle ? mono_gchandle_get_target_v2(GCHandle) : nullptr;

			if (m_enabled && target) OnDisable();
			Invoke(klass ? klass->MethodOnDestroy : nullptr);

			if (GCHandle)
			{
				mono_gchandle_free_v2(GCHandle);
				GCHandle = nullptr;
			}
			object = nullptr;
		}

		ScriptInstance(const ScriptInstance&) = delete;
		ScriptInstance& operator= (const ScriptInstance&) = delete;

		void Awake() { Invoke(klass ? klass->MethodAwake : nullptr); }
		void Start() { Invoke(klass ? klass->MethodStart : nullptr); }
		void Update() { Invoke(klass ? klass->MethodUpdate : nullptr); }
		void FixedUpdate() { Invoke(klass ? klass->MethodFixedUpdate : nullptr); }
		void OnEnable() { Invoke(klass ? klass->MethodOnEnable : nullptr); }
		void OnDisable() { Invoke(klass ? klass->MethodOnDisable : nullptr); }

		void OnCollisionEnter(EntityID other, bool isTrigger)
		{
			InvokeWithCollision(klass ? klass->MethodOnCollisionEnter : nullptr, other, isTrigger);
		}
		void OnCollisionExit(EntityID other, bool isTrigger)
		{
			InvokeWithCollision(klass ? klass->MethodOnCollisionExit : nullptr, other, isTrigger);
		}
		void OnCollisionStay(EntityID other, bool isTrigger)
		{
			InvokeWithCollision(klass ? klass->MethodOnCollisionStay : nullptr, other, isTrigger);
		}
		void OnTriggerEnter(EntityID other, bool isTrigger)
		{
			InvokeWithCollision(klass ? klass->MethodOnTriggerEnter : nullptr, other, isTrigger);
		}
		void OnTriggerExit(EntityID other, bool isTrigger)
		{
			InvokeWithCollision(klass ? klass->MethodOnTriggerExit : nullptr, other, isTrigger);
		}
		void OnTriggerStay(EntityID other, bool isTrigger)
		{
			InvokeWithCollision(klass ? klass->MethodOnTriggerStay : nullptr, other, isTrigger);
		}

		void SetEnabled(bool enabled)
		{
			if (enabled == m_enabled) return;
			m_enabled = enabled;
			if (m_enabled) OnEnable();
			else OnDisable();
		}

		/**
		 * @brief This function retrieves the managed MonoObject associated with this ScriptInstance.
		 * It gets from the GCHandle if available; otherwise, it returns the direct object pointer (Dangerous, object could have been moved already).
		 * @return The managed MonoObject pointer.
		 */
		MonoObject* GetManaged() const
		{
			return GCHandle ? mono_gchandle_get_target_v2(GCHandle) : object;
		}
	private:

		void Invoke(MonoMethod* method)
		{
			MonoObject* target = GetManaged();
			if (!method || !target) return;

			MonoDomain* objDomain = mono_object_get_domain(target);
			MonoDomain* gameDomain = ECS::GetInstance().GetSystem<ScriptSystem>()->m_ScriptEngine->GetGameDomain();
			if (objDomain != gameDomain)
			{
				EE_CORE_WARN("Skipping invoke: domain mismatch (obj={0}, game={1})",
					static_cast<void*>(objDomain), static_cast<void*>(gameDomain));
				return;
			}

			// Ensure current thread is attached to Mono
			MonoDomain* cur = mono_domain_get();
			MonoDomain* orig = nullptr;
			if (!cur)
				orig = mono_jit_thread_attach(gameDomain); // attaches external thread

			MonoObject* exc = nullptr;
			mono_runtime_invoke(method, target, nullptr, &exc);

			if (orig) {
				// Optional: restore previous domain
				mono_domain_set(orig,true);
			}

			if (exc) ReportManagedException(exc);
		}

		void InvokeArgs(MonoMethod* method, void** args)
		{
			MonoObject* target = GetManaged();
			if (!method || !target) return;
			MonoObject* exc = nullptr;
			mono_runtime_invoke(method, target, args, &exc); // Invoke monobehaviour method and object
			if (exc) ReportManagedException(exc);
		}

		void InvokeWithCollision(MonoMethod* method, EntityID other, bool isTrigger)
		{
			MonoObject* target = GetManaged();
			if (!method || !target) return;
			assert(ECS::GetInstance().IsEntityValid(other) && "ScriptInstance: other entity is not valid!");

			auto scriptSystem = ECS::GetInstance().GetSystem<ScriptSystem>();
			auto* engine = scriptSystem->m_ScriptEngine.get();
			MonoAssembly* apiAsm = engine->GetAPIAsm();
			if (!apiAsm) { EE_CORE_ERROR("ScriptInstance: API assembly not available"); return; }
			
			// Crafting the collision object (collider, gameobject, rigidbody, transform)

			MonoImage* apiImage = mono_assembly_get_image(apiAsm);
			MonoClass* collisionClass = mono_class_from_name(apiImage, "ErmineEngine", "Collision");
			if (!collisionClass) { EE_CORE_ERROR("ScriptInstance: ErmineEngine.Collision not found"); return; }
			MonoClass* colliderClass = mono_class_from_name(apiImage, "ErmineEngine", "Collider");
			if (!colliderClass) { EE_CORE_ERROR("ScriptInstance: ErmineEngine.Collider not found"); return; }
			MonoClass* goClass = mono_class_from_name(apiImage, "ErmineEngine", "GameObject");
			if (!goClass) { EE_CORE_ERROR("ScriptInstance: ErmineEngine.GameObject not found"); return; }
			MonoClass* rbClass = mono_class_from_name(apiImage, "ErmineEngine", "Rigidbody");
			if (!rbClass) { EE_CORE_ERROR("ScriptInstance: ErmineEngine.Rigidbody not found"); return; }
			MonoClass* tfClass = mono_class_from_name(apiImage, "ErmineEngine", "Transform");
			if (!tfClass) { EE_CORE_ERROR("ScriptInstance: ErmineEngine.Transform not found"); return; }

			// Setting up fields
			MonoObject* colObj = mono_object_new(engine->GetGameDomain(), collisionClass);
			if (!colObj) { EE_CORE_ERROR("ScriptInstance: Failed to allocate Collision"); return; }
			mono_runtime_object_init(colObj);

			MonoObject* colliderObj = mono_object_new(engine->GetGameDomain(), colliderClass);
			if (!colliderClass) { EE_CORE_ERROR("ScriptInstance: Failed to allocate Collider"); return; }
			mono_runtime_object_init(colliderObj);

			MonoObject* goObj = nullptr;
			{
				MonoMethod* fromEntity = mono_class_get_method_from_name(goClass, "FromEntityID", 1);
				if (!fromEntity) EE_CORE_ERROR("Method {0} not found on or not used {1}.{2}", "FromEntityID", "ErmineEngine", "GameObject");
				else
				{
					MonoObject* exc = nullptr;
					void* argsGO[1]{ &other };
					goObj = mono_runtime_invoke(fromEntity, nullptr, argsGO, &exc); // Run FromEntityID to populate
					if (exc) ReportManagedException(exc);
				}
			}
			if (!goObj)
			{
				goObj = mono_object_new(engine->GetGameDomain(), goClass);
				if (!goObj) { EE_CORE_ERROR("ScriptInstance: Failed to allocate GameObject"); return; }
				mono_runtime_object_init(goObj);
				if (MonoClassField* f = mono_class_get_field_from_name(goClass, "EntityID"))
					mono_field_set_value(goObj, f, &other);
			}

			MonoObject* rbObj = mono_object_new(engine->GetGameDomain(), rbClass);
			if (!rbObj) { EE_CORE_ERROR("ScriptInstance: Failed to allocate Rigidbody"); return; }
			mono_runtime_object_init(rbObj);
			MonoObject* tfObj = mono_object_new(engine->GetGameDomain(), tfClass);
			if (!tfObj) { EE_CORE_ERROR("ScriptInstance: Failed to allocate Transform"); return; }
			mono_runtime_object_init(tfObj);
			// Collider
			if (MonoClassField* field = mono_class_get_field_from_name(colliderClass, "EntityID")) 
				mono_field_set_value(colliderObj, field, &other);
			//else EE_CORE_WARN("ScriptInstance: Failed to set field ColliderClass");
			// GameObject
			if (MonoClassField* field = mono_class_get_field_from_name(goClass, "EntityID"))
				mono_field_set_value(goObj, field, &other);
			else EE_CORE_WARN("ScriptInstance: Failed to set field goClass");
			// Rigidbody
			if (MonoClassField* field = mono_class_get_field_from_name(rbClass, "EntityID"))
				mono_field_set_value(rbObj, field, &other);
			else EE_CORE_WARN("ScriptInstance: Failed to set field rbClass");
			// Transform
			if (MonoClassField* field = mono_class_get_field_from_name(tfClass, "EntityID"))
				mono_field_set_value(tfObj, field, &other);
			else EE_CORE_WARN("ScriptInstance: Failed to set field tfClass");

			// Set fields of Collision
			if (MonoClassField* f = mono_class_get_field_from_name(colliderClass, "isTrigger")) mono_field_set_value(colliderObj, f, &isTrigger);
			//else EE_CORE_WARN("ScriptInstance: Failed to set field to colliderClass");
			if (MonoClassField* f = mono_class_get_field_from_name(collisionClass, "collider"))   mono_field_set_value(colObj, f, colliderObj);
			else EE_CORE_WARN("ScriptInstance: Failed to set field to collider");
			if (MonoClassField* f = mono_class_get_field_from_name(collisionClass, "gameObject")) mono_field_set_value(colObj, f, goObj);
			else EE_CORE_WARN("ScriptInstance: Failed to set field to Gameobject");
			if (MonoClassField* f = mono_class_get_field_from_name(collisionClass, "rigidbody"))  mono_field_set_value(colObj, f, rbObj);
			else EE_CORE_WARN("ScriptInstance: Failed to set field to Rigidbody");
			if (MonoClassField* f = mono_class_get_field_from_name(collisionClass, "transform"))   mono_field_set_value(colObj, f, tfObj);
			else EE_CORE_WARN("ScriptInstance: Failed to set field to Transform");

			void* args[1]{ colObj };
			InvokeArgs(method , args);
		}

		void ReportManagedException(MonoObject* exc)
		{
			MonoString* s = mono_object_to_string(exc, nullptr);
			char* utf8 = mono_string_to_utf8(s);
			EE_CORE_ERROR("[C# Exception] {}", utf8 ? utf8 : "<null>");
			if (utf8) mono_free(utf8);
		}

		void InjectEntityIfAvailable(MonoObject* target) const
		{
			if (!target || !klass) return;

			if (MonoClassField* field = mono_class_get_field_from_name(klass->Class, "EntityID")) {
				EntityID id = entityID;
				mono_field_set_value(target, field, &id);
			}
		}
	};
}
