/* Start Header ************************************************************************/
/*!
\file       Systems.tpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      To maintain a record of registered systems and their signatures.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "Systems.h"

namespace Ermine
{
	/**
	 * @brief Register a system
	 * @tparam T The system to register
	 * @return shared pointer the registered system
	 */
	template <typename T>
	void SystemManager::RegisterSystem()
	{
		const char* typeName = typeid(T).name();

		assert(m_Systems.find(typeName) == m_Systems.end() && "Registering system more than once.");

		auto system = std::make_shared<T>();
		m_Systems.insert({ typeName, system });
	}

	/**
	 * @brief Get a system
	 * @tparam T The system to get
	 * @return shared pointer to the system
	 */
	template <typename T>
	std::shared_ptr<T> SystemManager::GetSystem()
	{
		const char* typeName = typeid(T).name();
		
		auto it = m_Systems.find(typeName);
		assert(it != m_Systems.end() && "System used before registered.");

		return std::static_pointer_cast<T>(it->second);
	}

	/**
	 * @brief Set the signature of a system
	 * @tparam T The system to set the signature of
	 * @param signature The signature to set
	 */
	template <typename T>
	void SystemManager::SetSystemSignature(SignatureID signature)
	{
		const char* typeName = typeid(T).name();

		assert(m_Systems.find(typeName) != m_Systems.end() && "System used before registered.");

		m_Signatures.insert({ typeName, signature });
	}
}