/* Start Header ************************************************************************/
/*!
\file       Guid.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       8/10/2025
\brief      This file contains the declaration of the Guid struct.
			Used for generating unique identifiers for entities and other objects.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"

namespace Ermine
{
	struct Guid
	{
		std::uint64_t hi{};
		std::uint64_t lo{};

		static Guid New();
		bool IsValid() const noexcept { return hi != 0 || lo != 0; }

		std::string ToString() const;
		static Guid FromString(std::string_view str);

		auto operator<=>(const Guid&) const = default;
	};

}

namespace std
{
	template <>
	struct hash<Ermine::Guid>
	{
		size_t operator()(const Ermine::Guid& g) const noexcept
		{
			std::uint64_t x = g.hi ^ g.lo + 0x9e3779b97f4a7c15 + (g.hi << 6) + (g.hi >> 2); // Golden ratio hash | Fibonacci hashing
			return x ^ x >> 33;
		}
	};
}
