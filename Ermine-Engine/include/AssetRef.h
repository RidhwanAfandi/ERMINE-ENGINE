/* Start Header ************************************************************************/
/*!
\file       Guid.h
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       8/10/2025
\brief      This file contains the declaration of the asset ref struct.
			Used for generating unique identifiers for assets.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
// AssetRef.h
#pragma once
#include "Guid.h"

namespace Ermine
{
    template<typename T>
    struct AssetRef
    {
        Guid guid{};
        bool IsValid() const noexcept { return guid.IsValid(); }
        void Clear() noexcept { guid = {}; }
        auto operator<=>(const AssetRef&) const = default;
    };
}
