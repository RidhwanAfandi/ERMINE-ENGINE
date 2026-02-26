/* Start Header ************************************************************************/
/*!
\file       ScriptClass.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       07/08/2025
\brief      This file contains the metadata for one C# type, Caching of MonoMethod for faster access here

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"
#include "ScriptSystem.h"
#include "ECS.h"

namespace Ermine::scripting
{
	struct ScriptClass
	{
		MonoClass* Class = nullptr;

		// Lifecycle methods (for caching)
		MonoMethod* Ctor = nullptr; // Constructor method pointer
		MonoMethod* MethodAwake = nullptr; // Method to awake the script
		MonoMethod* MethodOnEnable = nullptr; // Method to enable the script
		MonoMethod* MethodStart = nullptr; // Method to start the script
		MonoMethod* MethodUpdate = nullptr; // Method to update the script
		MonoMethod* MethodFixedUpdate = nullptr; // Method to fixed update the script
		MonoMethod* MethodOnDisable = nullptr; // Method to disable the script
		MonoMethod* MethodOnDestroy = nullptr; // Method to destroy the script

		MonoMethod* MethodOnCollisionEnter = nullptr; // Method to handle collision enter
		MonoMethod* MethodOnCollisionExit = nullptr; // Method to handle collision exit
		MonoMethod* MethodOnCollisionStay = nullptr; // Method to handle collision stay

		MonoMethod* MethodOnTriggerEnter = nullptr; // Method to handle trigger enter
		MonoMethod* MethodOnTriggerExit = nullptr; // Method to handle trigger exit
		MonoMethod* MethodOnTriggerStay = nullptr; // Method to handle trigger stay

		ScriptClass(const std::string& namespaceName, const std::string& className)
		{
			EE_CORE_TRACE("ScriptClass: resolving {0}.{1}", namespaceName, className);
			
			MonoImage* gameImage = mono_assembly_get_image(ECS::GetInstance().GetSystem<ScriptSystem>()->m_ScriptEngine->GetGameAsm());
	
			Class = mono_class_from_name(
				gameImage,
				namespaceName.c_str(),
				className.c_str()
			);
			if (!Class)
			{
				EE_CORE_ERROR("ScriptClass: type {0}.{1} not found!", namespaceName, className);
				return;
			}

			auto get = [&](const char* n, int argc = 0) -> MonoMethod*
				{
					MonoMethod* m = mono_class_get_method_from_name(Class, n, argc);
					if (!m) EE_CORE_TRACE("Method {0} not found on or not used {1}.{2}", n, namespaceName, className);
					return m;
				};

			MethodAwake = get("Awake", 0);
			MethodOnEnable = get("OnEnable", 0);
			MethodStart = get("Start", 0);
			MethodUpdate = get("Update", 0);
			MethodFixedUpdate = get("FixedUpdate", 0);
			MethodOnDisable = get("OnDisable", 0);
			MethodOnDestroy = get("OnDestroy", 0);
			MethodOnCollisionEnter = get("OnCollisionEnter", 1);
			MethodOnCollisionExit = get("OnCollisionExit", 1);
			MethodOnCollisionStay = get("OnCollisionStay", 1);
			MethodOnTriggerEnter = get("OnTriggerEnter", 1);
			MethodOnTriggerExit = get("OnTriggerExit", 1);
			MethodOnTriggerStay = get("OnTriggerStay", 1);
		}

		MonoObject* Instantiate() const
		{
			if (!Class) return nullptr;
			MonoObject* obj = mono_object_new(ECS::GetInstance().GetSystem<ScriptSystem>()->m_ScriptEngine->GetGameDomain(), Class);
			if (!obj)
			{
				EE_CORE_ERROR("ScriptClass: failed to create instance of {0}", mono_class_get_name(Class));
				return nullptr;
			}
			mono_runtime_object_init(obj); // Default .ctor is invoked here
			return obj;
		}
	};
}
