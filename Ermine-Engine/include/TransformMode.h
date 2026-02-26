/* Start Header ************************************************************************/
/*!
\file       TransformMode.h
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       Jan 25, 2025
\brief      Transform manipulation modes for editor gizmo (Pivot vs Center)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Entity.h"  // For EntityID typedef
#include "MathVector.h"
#include <vector>

namespace Ermine
{
    /**
     * @brief Transform manipulation mode determines where the gizmo appears
     * 
     * STANDARD UNITY/UNREAL BEHAVIOR:
     * - Pivot: Gizmo at selected entity's Transform.position (pivot point)
     * - Center: Gizmo at selected entity's bounding box center
     * 
     * Parent-child relationships affect world position calculations,
     * but do NOT change gizmo positioning logic (each entity is independent).
     */
    enum class TransformMode
    {
        Pivot,  // Gizmo at object's pivot (Transform.position)
        Center  // Gizmo at geometric center (bounding box center)
    };

    /**
     * @brief Helper class to calculate manipulation positions for transform gizmos
     * 
     * Implements standard Unity/Unreal behavior:
     * - Single selection: Gizmo at selected entity's pivot or center
     * - Multi-selection Pivot: Gizmo at first entity's pivot (Unity uses last selected)
     * - Multi-selection Center: Gizmo at combined bounding box center
     */
    class TransformModeHelper
    {
    public:
        /**
         * @brief Calculate the position where the gizmo should appear
         * @param entity The entity to manipulate
         * @param mode Current transform mode
         * @return Position for gizmo placement
         * 
         * Standard behavior: Only considers the selected entity's own transform/mesh.
         * Parent transforms are respected for world position, but parent/child meshes
         * are NOT combined in the calculation.
         */
        static Vec3 GetManipulationPosition(EntityID entity, TransformMode mode);

        /**
         * @brief Calculate the geometric center of an entity's mesh/model
         * @param entity The entity to calculate center for
         * @return Bounding box center in world space
         * 
         * Only considers the selected entity's own mesh bounds.
         * Parent transforms affect the calculation (rotation/scale),
         * but parent/child meshes are NOT included.
         */
        static Vec3 CalculateGeometricCenter(EntityID entity);

        /**
         * @brief Calculate center position for multiple selected entities
         * @param entities List of selected entities
         * @param mode Current transform mode
         * @return Combined position based on mode
         * 
         * Pivot mode: Returns first entity's pivot (Unity uses last selected).
         * Center mode: Returns combined bounding box center of all selected entities.
         */
        static Vec3 GetMultiSelectionCenter(const std::vector<EntityID>& entities, TransformMode mode);
    };
}
