/* Start Header ************************************************************************/
/*!
\file       Guid.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       8/10/2025
\brief		This file contains the declaration of the Guid struct.
			Used for generating unique identifiers for entities and other objects.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Guid.h"

#include <random>
#ifdef _WIN32
	//#include <objbase.h> // For CoCreateGuid
#endif

namespace
{
	uint8_t hexToNibble(char c)
	{
		if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
		if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
		if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
		return 0;
	}

	char nibbleToHex(uint8_t n)
	{
		static const char* lut = "0123456789abcdef";
		return lut[n & 0xF];
	}
}

Ermine::Guid Ermine::Guid::New()
{
	Guid g{};
#ifdef _WIN32
	GUID win{};
	if (SUCCEEDED(::CoCreateGuid(&win)))
	{
		// Pack bytes into hi/lo
		std::uint8_t b[16]{};
		std::memcpy(b + 0, &win.Data1, 4);
		std::memcpy(b + 4, &win.Data2, 2);
		std::memcpy(b + 6, &win.Data3, 2);
		std::memcpy(b + 8, &win.Data4[0], 8);

		std::memcpy(&g.hi, b + 0, 8);
		std::memcpy(&g.lo, b + 8, 8);
		return g;
	}
#endif

	thread_local std::mt19937_64 rng{ std::random_device{}() };
	g.hi = rng();
	g.lo = rng();
	if (g.hi == 0 && g.lo == 0) g.hi = 1;
	return g;
}

std::string Ermine::Guid::ToString() const
{
	std::string s;
	s.resize(32);
	std::uint8_t bytes[16]{};
	std::memcpy(bytes + 0, &hi, 8);
	std::memcpy(bytes + 8, &lo, 8);

	for (int i = 0; i < 16; ++i)
	{
		uint8_t v = bytes[i];
		s[i * 2] = nibbleToHex(v >> 4);
		s[i * 2 + 1] = nibbleToHex(v & 0xF);
	}
	return s;
}

Ermine::Guid Ermine::Guid::FromString(std::string_view str)
{
	Guid g{};
	if (str.size() < 32) return g;

	std::uint8_t bytes[16]{};

	for (int i = 0; i < 16; ++i)
	{
		uint8_t hiN = hexToNibble(str[i * 2 + 0]);
		uint8_t loN = hexToNibble(str[i * 2 + 1]);
		bytes[i] = static_cast<uint8_t>((hiN << 4) | loN);
	}
	std::memcpy(&g.hi, bytes + 0, 8);
	std::memcpy(&g.lo, bytes + 8, 8);
	return g;
}
