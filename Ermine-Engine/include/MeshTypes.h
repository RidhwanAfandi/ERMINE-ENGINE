/* Start Header ************************************************************************/
/*!
\file       MeshTypes.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       27/09/2025
\brief      This file contains the declaration of Mesh types and structures.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "glm/glm.hpp"
#include "cstdint"
#include "string"

namespace Ermine::graphics {

    // Vertex structure
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;  
        glm::vec2 texCoord;
        glm::vec4 tangent;
    };

    // SkinnedVertex structure
    struct SkinnedVertex {
        glm::vec3 position;     
        glm::vec3 normal;      
        glm::vec2 texCoord;    
        glm::vec4 tangent;
        glm::ivec4 boneIDs;    
        glm::vec4 boneWeights; 
    };

    struct MeshSubset  {
        uint32_t vertexOffset;
		uint32_t vertexCount;
        uint32_t indexOffset;
        uint32_t indexCount;
        uint32_t baseVertex;
        std::string meshID;
    };

    struct MeshHandle {
        uint32_t index = UINT32_MAX;
        bool isValid() const { return index != UINT32_MAX; }
    };

}  //namespace graphics