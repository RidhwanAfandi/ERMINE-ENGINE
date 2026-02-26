using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    /// <summary>
    /// Provides access to UI system for skill effects and animations
    /// </summary>
    public static class UISystem
    {
        /// <summary>
        /// Triggers a skill cast which activates the UI flash animation.
        /// This will check if entity has enough health, deduct cost, start cooldown, and play flash effect.
        /// </summary>
        /// <param name="entity">The entity with UIComponent (usually the HUD entity)</param>
        /// <param name="skillIndex">Skill slot index: 0=LMB/ShootOrb, 1=LMB2/Teleport, 2=RMB/BlindBurst, 3=R/Recall</param>
        /// <returns>True if skill was successfully cast, false if on cooldown or insufficient health</returns>
        public static bool CastSkill(GameObject entity, int skillIndex)
        {
            if (entity == null)
                return false;

            return Internal_CastSkill((ulong)entity.GetInstanceID(), skillIndex);
        }

        /// <summary>
        /// Sets the selected state of a skill in UISkillsComponent.
        /// Use this to visually indicate which skill is currently active.
        /// </summary>
        /// <param name="entityName">Name of the entity with UISkillsComponent (e.g., "SkillsHUD")</param>
        /// <param name="skillIndex">Skill slot index (0-based)</param>
        /// <param name="isSelected">True to show selected icon, false to show unselected icon</param>
        public static void SetSkillSelected(string entityName, int skillIndex, bool isSelected)
        {
            Internal_SetSkillSelected(entityName, skillIndex, isSelected);
        }

        /// <summary>
        /// Gets the selected state of a skill in UISkillsComponent.
        /// </summary>
        /// <param name="entityName">Name of the entity with UISkillsComponent</param>
        /// <param name="skillIndex">Skill slot index (0-based)</param>
        /// <returns>True if skill is selected, false otherwise</returns>
        public static bool GetSkillSelected(string entityName, int skillIndex)
        {
            return Internal_GetSkillSelected(entityName, skillIndex);
        }

        /// <summary>
        /// Selects one skill and deselects all others in UISkillsComponent.
        /// Useful for showing only one active skill at a time.
        /// </summary>
        /// <param name="entityName">Name of the entity with UISkillsComponent</param>
        /// <param name="skillIndex">Skill slot index to select (-1 to deselect all)</param>
        public static void SelectOnlySkill(string entityName, int skillIndex)
        {
            Internal_SelectOnlySkill(entityName, skillIndex);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_CastSkill(ulong entityID, int skillIndex);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetSkillSelected(string entityName, int skillIndex, bool isSelected);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetSkillSelected(string entityName, int skillIndex);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SelectOnlySkill(string entityName, int skillIndex);
    }
}
