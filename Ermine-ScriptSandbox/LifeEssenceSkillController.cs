/* Start Header ************************************************************************/
/*!
\file       LifeEssenceSkillController.cs
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       11/07/2025
\brief      Demonstration script for the Life Essence skill system with steampunk HUD.
            Shows how to configure and use the 4 skills that drain health (life essence).

            SKILL CONFIGURATION (Slot Index → Ability):
            [0] Mouse 1         - Shoot Essence Orb    (Cost: 15, Cooldown: 1.5s)
            [1] Mouse 1 (again) - Teleport to Orb      (Cost: 20, Cooldown: 3.0s)
            [2] Mouse 2         - Blind Burst          (Cost: 25, Cooldown: 5.0s)
            [3] R Key           - Recall Orb           (Cost: 10, Cooldown: 2.0s)

            HEALTH REGENERATION:
            - Rate: 5 HP/second
            - Delay: 3 seconds after last skill use
            - Only regenerates when NO skills are on cooldown (idle state)

            SKILL ACTIVATION ANIMATIONS:
            When you cast a skill, the corresponding skill slot will automatically play
            a 0.4 second animation:
            - Flash bright white (0-0.2s)
            - Scale pulse 1.0x → 1.3x → 1.0x (0-0.4s)
            - Expanding gold ring that fades out (0-0.4s)
            - Border glow enhancement

            See SKILL_ACTIVATION_ANIMATIONS.md for full documentation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using ErmineEngine;

public class LifeEssenceSkillController : MonoBehaviour
{
    // ============================================================================
    // SKILL SYSTEM CONFIGURATION
    // ============================================================================
    // To configure the HUD and skill costs, you need to set these values in the
    // UIComponent (C++ side). These constants are for reference and demonstration.

    private const int SKILL_SHOOT_ORB = 0;      // Slot 0: Shoot Essence Orb
    private const int SKILL_TELEPORT = 1;       // Slot 1: Teleport to Orb
    private const int SKILL_BLIND_BURST = 2;    // Slot 2: Blind Burst
    private const int SKILL_RECALL_ORB = 3;     // Slot 3: Recall Orb

    // ============================================================================
    // RECOMMENDED SKILL CONFIGURATION (Set these in UIComponent):
    // ============================================================================
    //
    // Skill Slot 0 - Shoot Essence Orb:
    //   - manaCost: 15.0f
    //   - maxCooldown: 1.5f
    //   - Description: Shoots an orb of life essence that travels forward
    //
    // Skill Slot 1 - Teleport to Orb:
    //   - manaCost: 20.0f
    //   - maxCooldown: 3.0f
    //   - Description: Teleports player to the orb's location
    //
    // Skill Slot 2 - Blind Burst:
    //   - manaCost: 25.0f
    //   - maxCooldown: 5.0f
    //   - Description: Releases a burst that blinds light sources and enemies
    //
    // Skill Slot 3 - Recall Orb:
    //   - manaCost: 10.0f
    //   - maxCooldown: 2.0f
    //   - Description: Recalls the orb back to the player
    //
    // ============================================================================

    // Game state tracking
    private GameObject essenceOrb = null;
    private bool orbIsActive = false;
    private bool hasShotOrb = false;

    void Start()
    {
        Debug.Log("Life Essence Skill Controller initialized!");
        Debug.Log("=== SKILL BINDINGS ===");
        Debug.Log("[0] Mouse 1 (First Click)  - Shoot Essence Orb (15 HP)");
        Debug.Log("[1] Mouse 1 (Second Click) - Teleport to Orb (20 HP)");
        Debug.Log("[2] Mouse 2                - Blind Burst (25 HP)");
        Debug.Log("[3] R Key                  - Recall Orb (10 HP)");
        Debug.Log("Health regenerates at 5 HP/sec after 3 seconds of no skill usage");
    }

    void Update()
    {
        // ========================================================================
        // MOUSE 1: Shoot Orb / Teleport to Orb (Dual functionality)
        // ========================================================================
        if (Input.GetMouseButtonDown(0))
        {
            if (!orbIsActive && !hasShotOrb)
            {
                // First click: Shoot Essence Orb (Skill Slot 0)
                ShootEssenceOrb();
            }
            else if (orbIsActive && hasShotOrb)
            {
                // Second click: Teleport to Orb (Skill Slot 1)
                TeleportToOrb();
            }
        }

        // ========================================================================
        // MOUSE 2: Blind Burst (Skill Slot 2)
        // ========================================================================
        if (Input.GetMouseButtonDown(1))
        {
            BlindBurst();
        }

        // ========================================================================
        // R KEY: Recall Orb (Skill Slot 3)
        // ========================================================================
        if (Input.GetKeyDown(KeyCode.R))
        {
            RecallOrb();
        }
    }

    // ============================================================================
    // SKILL IMPLEMENTATIONS
    // ============================================================================

    /// <summary>
    /// Skill Slot 0: Shoot Essence Orb
    /// Cost: 15 HP | Cooldown: 1.5s
    /// Shoots a projectile orb forward that the player can teleport to
    ///
    /// ANIMATION: When cast successfully, skill slot 0 will:
    /// - Flash bright white for 0.2 seconds
    /// - Scale pulse from 1.0x → 1.3x → 1.0x over 0.4 seconds
    /// - Show expanding gold ring that fades out
    /// </summary>
    void ShootEssenceOrb()
    {
        // NOTE: In a real implementation, you would call the native C++ function:
        // UIRenderSystem::CastSkill(entityID, SKILL_SHOOT_ORB);
        // This would automatically:
        // 1. Check if you have enough health (15 HP)
        // 2. Deduct the health cost
        // 3. Trigger the cooldown (1.5 seconds)
        // 4. START THE ACTIVATION ANIMATION (automatic!)
        // 5. Reset health regeneration timer

        Debug.Log("[SKILL] Shoot Essence Orb - Draining 15 HP");

        // Instantiate the essence orb projectile
        essenceOrb = Prefab.Instantiate("../Resources/Prefabs/Sphere.prefab");
        if (essenceOrb != null)
        {
            // Position orb in front of player
            Vector3 spawnOffset = new Vector3(0, 1, 2); // Adjust based on your needs
            essenceOrb.transform.position = transform.position + spawnOffset;
            essenceOrb.transform.rotation = transform.rotation;

            // Add forward velocity to orb (you'll need a Rigidbody component)
            // Rigidbody rb = essenceOrb.GetComponent<Rigidbody>();
            // if (rb != null)
            //     rb.AddForce(transform.forward * orbSpeed);

            orbIsActive = true;
            hasShotOrb = true;

            Debug.Log("[SUCCESS] Essence Orb spawned and traveling forward!");
        }
        else
        {
            Debug.Log("[ERROR] Failed to spawn Essence Orb prefab!");
        }
    }

    /// <summary>
    /// Skill Slot 1: Teleport to Orb
    /// Cost: 20 HP | Cooldown: 3.0s
    /// Teleports the player to the orb's current location
    /// </summary>
    void TeleportToOrb()
    {
        if (!orbIsActive || essenceOrb == null)
        {
            Debug.Log("[FAILED] No active orb to teleport to!");
            return;
        }

        // NOTE: Call native function: UIRenderSystem::CastSkill(entityID, SKILL_TELEPORT);

        Debug.Log("[SKILL] Teleport to Orb - Draining 20 HP");

        // Teleport player to orb position
        transform.position = essenceOrb.transform.position;

        // Destroy the orb after teleporting
        GameObject.Destroy(essenceOrb);
        essenceOrb = null;
        orbIsActive = false;
        hasShotOrb = false;

        Debug.Log("[SUCCESS] Teleported to orb location!");
    }

    /// <summary>
    /// Skill Slot 2: Blind Burst
    /// Cost: 25 HP | Cooldown: 5.0s
    /// Releases a burst that blinds nearby light sources and enemies
    /// </summary>
    void BlindBurst()
    {
        // NOTE: Call native function: UIRenderSystem::CastSkill(entityID, SKILL_BLIND_BURST);

        Debug.Log("[SKILL] Blind Burst - Draining 25 HP");

        // Create a temporary burst effect at player position
        // In a real implementation, this would:
        // 1. Spawn a particle effect
        // 2. Disable nearby lights temporarily
        // 3. Apply "blind" status to enemies in radius

        Vector3 burstPosition = transform.position;
        float burstRadius = 10.0f; // Affect 10 unit radius

        // TODO: Implement light blinding logic
        // - Find all lights within burstRadius
        // - Disable them for a duration
        // - Apply blind effect to enemies

        Debug.Log($"[SUCCESS] Blind Burst triggered at {burstPosition} (Radius: {burstRadius})");
    }

    /// <summary>
    /// Skill Slot 3: Recall Orb
    /// Cost: 10 HP | Cooldown: 2.0s
    /// Recalls the essence orb back to the player, destroying it
    /// </summary>
    void RecallOrb()
    {
        if (!orbIsActive || essenceOrb == null)
        {
            Debug.Log("[FAILED] No active orb to recall!");
            return;
        }

        // NOTE: Call native function: UIRenderSystem::CastSkill(entityID, SKILL_RECALL_ORB);

        Debug.Log("[SKILL] Recall Orb - Draining 10 HP");

        // In a real implementation, you could animate the orb flying back
        // For now, we'll just destroy it
        GameObject.Destroy(essenceOrb);
        essenceOrb = null;
        orbIsActive = false;
        hasShotOrb = false;

        Debug.Log("[SUCCESS] Orb recalled and destroyed!");
    }
}
