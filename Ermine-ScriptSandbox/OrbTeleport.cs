using ErmineEngine;
using System;
using System.Threading;

public class OrbTeleport : MonoBehaviour
{
    private Transform origin;
    private Transform cam;
    private Animator anim;
    private GameObject orbProjectile;
    private Sphere orbSphere;

    private float health = 0f;
    public float damage = 10f;
    public float recallHealAmt = 10f;

    private float forwardOffset = 2.0f; // Distance in front of the player
    private float rightOffset = -0.3f;  // Slightly to the right
    private float upOffset = 4.2f;      // Above the player

    private float teleportVerticalOffset = -3.5f; // Adjust so player sits on orb

    private bool orbShot = false;       // Tracks if we already shot an orb

    private float timeSinceLastDamage = 0f;
    public float regenDelay = 2.0f; // seconds before regen starts

    private float teleportDashDuration = 0.4f; // total dash travel duration in seconds
    private float teleportDashRadialBlurBaseStrength = 0.015f; // baseline radial blur during dash
    private float teleportDashRadialBlurPeakStrength = 0.3f; // max radial blur near dash end
    private float teleportDashRadialBlurSpikeStart = 0.70f; // normalized time when blur spike starts
    private int teleportDashRadialBlurSamples = 14; // sample count for radial blur pass
    private float dashVignetteIntensity = 0.5f; // vignette intensity during dash
    private float dashVignetteCoverage = 0.5f; // vignette coverage during dash

    public float dashEndRadialBlurRestoreDuration = 0.1f; // time to restore pre-dash radial blur state
    private bool isTeleportDashing = false; // true while teleport dash is in progress
    private float teleportDashElapsed = 0f; // elapsed time for current dash
    private Vector3 teleportDashStart; // world position where dash begins
    private Vector3 teleportDashTarget; // world position where dash ends
    private bool dashEndPrevVignetteEnabled = false; // cached vignette enabled state before dash
    private float dashEndPrevVignetteIntensity = 0f; // cached vignette intensity before dash
    private float dashEndPrevVignetteRadius = 0f; // cached vignette radius before dash
    private float dashEndPrevVignetteCoverage = 0f; // cached vignette coverage before dash
    private float dashEndPrevVignetteFalloff = 0f; // cached vignette falloff before dash
    private Vector3 dashEndPrevVignetteRGBModifier = Vector3.zero; // cached vignette tint before dash
    private bool dashEndRadialBlurRestoreActive = false; // whether radial blur restore is active
    private float dashEndRadialBlurRestoreElapsed = 0f; // elapsed time for radial blur restore
    private bool dashEndPrevRadialBlurEnabled = false; // cached radial blur enabled state before dash
    private float dashEndPrevRadialBlurStrength = 0f; // cached radial blur strength before dash
    private int dashEndPrevRadialBlurSamples = 12; // cached radial blur sample count before dash
    private Vector2 dashEndPrevRadialBlurCenter = new Vector2(0.5f, 0.5f); // cached radial blur center before dash
                                                                           
    private float teleportChromaticAberrationIntensity = 0.02f;// Chromatic aberration (teleport FX)
    private bool dashEndPrevChromaticAberrationEnabled = false;// Cache previous values
    private float dashEndPrevChromaticAberrationIntensity = 0.003f;


    // Name of the entity with UISkillsComponent (must match your scene)
    public string skillsHUDName = "Skills";

    // Name of the entity with UIHealthbarComponent (must match your scene)
    public string healthBarName = "Healthbar";

    // Skill indices - match your UISkillsComponent skill slot order (0-based)
    // Slot 1 = Shoot (index 0)
    // Slot 2 = Return (index 1)
    // Slot 3 = Teleport (index 2)
    // Slot 4 = Disrupt (index 3)
    private const int SKILL_SHOOT = 0;
    private const int SKILL_RETURN = 1;
    private const int SKILL_TELEPORT = 2;
    private const int SKILL_DISRUPT = 3;

    //Health
    private GameObject healthBar;

    void Start()
    {
        origin = GameObject.Find("Player").GetComponent<Transform>();
        cam = GameObject.Find("Main Camera").transform;
        anim = GameObject.Find("PlayerAnim").GetComponent<Animator>();

        // Find healthbar by name
        healthBar = GameObject.Find(healthBarName);
        if (healthBar != null)
        {
            health = GameplayHUD.GetHealth(healthBar);
        }

        // Initialize vignette map texture and color modifier for dash effect
        PostEffects.SetVignetteMapTexture("../Resources/Textures/White_Vignette_Alpha.png");
        PostEffects.VignetteMapRGBModifier = new Vector3(1f, 1f, 1f);

        EnsureOrbProjectile();

        // Initialize: Shoot is ready (lit up), others are not
        SetSkillsForReadyToShoot();
    }

    void EnsureOrbProjectile()
    {
        if (orbProjectile != null && orbSphere != null)
            return;

        orbProjectile = Prefab.Instantiate("../Resources/Prefabs/Sphere.prefab");
        if (orbProjectile == null)
            return;

        orbSphere = orbProjectile.GetComponent<Sphere>();
        if (orbSphere == null)
            return;

        orbSphere.Deactivate();
    }

    // Helper: Set skills to "ready to shoot" state (no orb out)
    void SetSkillsForReadyToShoot()
    {
        bool canShoot = CanShootOrb();

        UISystem.SetSkillSelected(skillsHUDName, SKILL_SHOOT, canShoot);     // Shoot ready
        UISystem.SetSkillSelected(skillsHUDName, SKILL_RETURN, false);   // Return not ready
        UISystem.SetSkillSelected(skillsHUDName, SKILL_TELEPORT, false); // Teleport not ready
        UISystem.SetSkillSelected(skillsHUDName, SKILL_DISRUPT, false);  // Disrupt not ready (for now)
    }

    // Helper: Set skills to "orb is out" state (ready to teleport or recall)
    void SetSkillsForOrbOut()
    {
        UISystem.SetSkillSelected(skillsHUDName, SKILL_SHOOT, false);    // Shoot not ready (already shot)
        UISystem.SetSkillSelected(skillsHUDName, SKILL_RETURN, true);    // Return ready
        UISystem.SetSkillSelected(skillsHUDName, SKILL_TELEPORT, true);  // Teleport ready
        UISystem.SetSkillSelected(skillsHUDName, SKILL_DISRUPT, false);  // Disrupt not ready (for now)
    }

    void Update()
    {
        if (dashEndRadialBlurRestoreActive)
        {
            UpdateDashEndRadialBlurRestore();
        }

        if (isTeleportDashing)
        {
            UpdateTeleportDash();
            return;
        }

        timeSinceLastDamage += Time.deltaTime;

        // Check if orb disappeared on its own (hit something, traveled too far, etc.)
        if (orbShot && (orbProjectile == null || !orbProjectile.activeSelf))
        {
            Debug.Log("OrbTeleport: Orb vanished, resetting state");
            orbShot = false;
            SetSkillsForReadyToShoot();
        }

        // Regenerate health when orb is not out
        if (!orbShot && timeSinceLastDamage >= regenDelay)
        {
            RegenerateHealth();
        }


        if (Input.GetMouseButton(0))
        {
            if (!orbShot)
            {
                if (!CanShootOrb())
                {
                    // Optional feedback
                    //GlobalAudio.PlaySFX("Error"); 
                    return;
                }

                ShootOrb();
                orbShot = true;
                return;
            }

            TeleportToOrb();
            orbShot = false;
        }


        // Recall orb on 'R' key press
        if (Input.GetKeyDown(KeyCode.R))
            RecallOrb();
    }

    void RegenerateHealth()
    {
        if (healthBar == null) return;

        float currentHealth = GameplayHUD.GetHealth(healthBar);
        float maxHealth = GameplayHUD.GetMaxHealth(healthBar);
        float regenRate = GameplayHUD.GetRegenRate(healthBar);

        if (currentHealth < maxHealth && regenRate > 0)
        {
            float newHealth = Math.Min(currentHealth + regenRate * Time.deltaTime, maxHealth);
            GameplayHUD.SetHealth(healthBar, newHealth);
        }
    }

    void ShootOrb()
    {
        EnsureOrbProjectile();
        if (orbProjectile == null || orbSphere == null || orbProjectile.activeSelf) return;

        // Deal damage to player
        TakeDamage(damage);
        GlobalAudio.PlaySFX("Shoot");

        // Play shoot animation
        anim.SetTrigger("shoot");

        // Update UI: Orb is out, teleport and return are now ready
        SetSkillsForOrbOut();

        Vector3 spawnPosition = origin.transform.position + cam.forward * forwardOffset + cam.right * rightOffset + Vector3.up * upOffset;
        orbSphere.Launch(spawnPosition, transform.rotation, -cam.forward);
    }

    void TeleportToOrb()
    {
        if (orbProjectile == null || orbSphere == null || !orbProjectile.activeSelf)
        {
            orbShot = false;
            SetSkillsForReadyToShoot();
            return;
        }

        TakeDamage(damage);

        GlobalAudio.PlaySFX("Teleport");

        // Snap explosion to ground for better visual accuracy
        Vector3 explosionPos = orbProjectile.transform.position;
        RaycastHit hit;
        if (Physics.Raycast(orbProjectile.transform.position, Vector3.down, out hit, 5.0f))
        {
            explosionPos = hit.point + Vector3.up * 0.1f; // Slightly above ground
        }

        SpawnExplosionLayers(explosionPos);

        teleportDashStart = gameObject.transform.position;
        teleportDashTarget = orbProjectile.transform.position + Vector3.up * teleportVerticalOffset;
        teleportDashElapsed = 0f;
        isTeleportDashing = true;

        dashEndPrevVignetteEnabled = PostEffects.EnableVignette;
        dashEndPrevVignetteIntensity = PostEffects.VignetteIntensity;
        dashEndPrevVignetteRadius = PostEffects.VignetteRadius;
        dashEndPrevVignetteCoverage = PostEffects.VignetteCoverage;
        dashEndPrevVignetteFalloff = PostEffects.VignetteFalloff;
        dashEndPrevVignetteRGBModifier = PostEffects.VignetteMapRGBModifier;

        PostEffects.EnableVignette = true;
        PostEffects.VignetteIntensity = dashVignetteIntensity;
        PostEffects.VignetteCoverage = dashVignetteCoverage;
        PostEffects.VignetteRadius = 0.5f;
        PostEffects.VignetteFalloff = 0.01f;
        PostEffects.VignetteMapRGBModifier = new Vector3(1f, 1f, 1f);

        dashEndPrevRadialBlurEnabled = PostEffects.EnableRadialBlur;
        dashEndPrevRadialBlurStrength = PostEffects.RadialBlurStrength;
        dashEndPrevRadialBlurSamples = PostEffects.RadialBlurSamples;
        dashEndPrevRadialBlurCenter = PostEffects.RadialBlurCenter;
        dashEndRadialBlurRestoreActive = false;
        dashEndRadialBlurRestoreElapsed = 0f;

        PostEffects.EnableRadialBlur = true;
        PostEffects.RadialBlurSamples = teleportDashRadialBlurSamples;
        PostEffects.RadialBlurCenter = new Vector2(0.5f, 0.5f);
        PostEffects.RadialBlurStrength = teleportDashRadialBlurBaseStrength;

        dashEndPrevChromaticAberrationEnabled = PostEffects.EnableChromaticAberration;
        dashEndPrevChromaticAberrationIntensity = 0.003f;

        // Park orb for reuse instead of destroying and recreating it.
        Debug.Log("OrbTeleport: Deactivating orb after teleport: " + orbProjectile.GetInstanceID());
        orbSphere.Deactivate();

        // Back to ready to shoot
        SetSkillsForReadyToShoot();
    }

    void RecallOrb()
    {
        if (orbProjectile != null && orbSphere != null && orbProjectile.activeSelf)
        {
            GlobalAudio.PlaySFX("Teleport"); // Or a custom recall sound

            // Play recall animation
            anim.SetTrigger("recall");

            // Park orb for reuse instead of destroying and recreating it.
            HealDamage(recallHealAmt);
            orbSphere.Deactivate();

            // Back to ready to shoot
            SetSkillsForReadyToShoot();
        }

        // Reset state fully
        orbShot = false;
    }

    void TakeDamage(float dmg)
    {
        if (healthBar == null) return;

        health = GameplayHUD.GetHealth(healthBar);
        health = Math.Max(0, health - dmg);

        timeSinceLastDamage = 0f; // reset regen timer
        GameplayHUD.SetHealth(healthBar, health);

        // Play health drain SFX based on health percentage after damage
        float maxHealth = GameplayHUD.GetMaxHealth(healthBar);
        float healthPercent = maxHealth > 0f ? (health / maxHealth) * 100f : 0f;

        if (healthPercent <= 30f)
            GlobalAudio.PlaySFX("HealthDrain3");      // danger  - loudest
        else if (healthPercent <= 60f)
            GlobalAudio.PlaySFX("HealthDrain2");      // caution - medium
        else
            GlobalAudio.PlaySFX("HealthDrain1");      // safe    - softest
    }

    void HealDamage(float heal)
    {
        if (healthBar == null) return;

        float current = GameplayHUD.GetHealth(healthBar);
        float max = GameplayHUD.GetMaxHealth(healthBar);

        GameplayHUD.SetHealth(healthBar, Math.Min(current + heal, max));
    }

    bool CanShootOrb()
    {
        if (healthBar == null) return true;

        float currentHealth = GameplayHUD.GetHealth(healthBar);
        return currentHealth >= damage;
    }

    void UpdateTeleportDash()
    {
        float duration = Math.Max(0.001f, teleportDashDuration);
        teleportDashElapsed += Time.deltaTime;

        float t = Math.Min(teleportDashElapsed / duration, 1.0f);
        float easedT = t * t; // Ease-in only: accelerate into dash, then stop instantly at the end.
        Vector3 dashPos = teleportDashStart + (teleportDashTarget - teleportDashStart) * easedT;

        float blurStrength = teleportDashRadialBlurBaseStrength;
        float spikeStart = Math.Max(0.0f, Math.Min(teleportDashRadialBlurSpikeStart, 0.99f));
        if (t > spikeStart)
        {
            float spikeT = (t - spikeStart) / (1.0f - spikeStart);
            spikeT = Math.Max(0.0f, Math.Min(spikeT, 1.0f));
            float smoothSpike = spikeT * spikeT * (3.0f - 2.0f * spikeT);
            blurStrength = teleportDashRadialBlurBaseStrength +
                (teleportDashRadialBlurPeakStrength - teleportDashRadialBlurBaseStrength) * smoothSpike;
        }
        PostEffects.RadialBlurStrength = blurStrength;

        gameObject.transform.position = dashPos;
        Physics.SetPosition((ulong)gameObject.GetInstanceID(), dashPos);

        if (t >= 1.0f)
        {
            gameObject.transform.position = teleportDashTarget;
            Physics.SetPosition((ulong)gameObject.GetInstanceID(), teleportDashTarget);
            isTeleportDashing = false;
            PostEffects.EnableVignette = dashEndPrevVignetteEnabled;
            PostEffects.VignetteIntensity = dashEndPrevVignetteIntensity;
            PostEffects.VignetteRadius = dashEndPrevVignetteRadius;
            PostEffects.VignetteCoverage = dashEndPrevVignetteCoverage;
            PostEffects.VignetteFalloff = dashEndPrevVignetteFalloff;
            PostEffects.VignetteMapRGBModifier = dashEndPrevVignetteRGBModifier;
            dashEndRadialBlurRestoreElapsed = 0f;
            dashEndRadialBlurRestoreActive = true;
        }
    }

    void UpdateDashEndRadialBlurRestore()
    {
        float duration = Math.Max(0.0001f, dashEndRadialBlurRestoreDuration);
        dashEndRadialBlurRestoreElapsed += Time.deltaTime;
        float t = Math.Min(dashEndRadialBlurRestoreElapsed / duration, 1.0f);

        float currentStrength = PostEffects.RadialBlurStrength;
        PostEffects.RadialBlurStrength = currentStrength + (dashEndPrevRadialBlurStrength - currentStrength) * t;

        if (t >= 1.0f)
        {
            PostEffects.EnableRadialBlur = dashEndPrevRadialBlurEnabled;
            PostEffects.RadialBlurStrength = dashEndPrevRadialBlurStrength;
            PostEffects.RadialBlurSamples = dashEndPrevRadialBlurSamples;
            PostEffects.RadialBlurCenter = dashEndPrevRadialBlurCenter;
            dashEndRadialBlurRestoreActive = false;
        }
    }

    void SpawnExplosionLayers(Vector3 position)
    {
        GameObject coreExplosion = Prefab.Instantiate("../Resources/Prefabs/ParticleExplosion.prefab");
        if (coreExplosion != null)
        {
            coreExplosion.transform.position = position;
        }

        GameObject smokeExplosion = Prefab.Instantiate("../Resources/Prefabs/ParticleExplosionSmoke.prefab");
        if (smokeExplosion != null)
        {
            smokeExplosion.transform.position = position;
        }
    }

}
