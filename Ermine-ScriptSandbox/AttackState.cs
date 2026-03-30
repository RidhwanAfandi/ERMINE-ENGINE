using ErmineEngine;
using System;

public class Attack : MonoBehaviour
{
    // Global VO lock - prevent multiple guards playing VO simultaneously
    private static float globalVOLockTime = 0f;
    private static float globalVOLockDuration = 2.0f;
    private static ulong lastVOGuardID = 0; // Track which guard played last
    
    // Per-guard VO lock - allow this guard to chain VO
    private float myVOLockTime = 0f;
    private float myVOLockDuration = 0.5f; // This guard can't play again for 0.5s
    
    public string playerName = "Player";

    public float attackRange = 5.0f;
    // If player no longer close enough go back to Chase
    public float disengageDistance = 8.0f;

    // LOS
    public float viewDistance = 18.0f;
    public float rayHeight = 5.5f;
    public float rayForwardOffset = 2.0f; // push ray out of own collider
    public float loseSightGraceTime = 0.25f; // prevents flicker behind corners
    public float closeDetectDistance = 2.0f;

    // Damage settings
    public float damagePerTick = 10f;
    public float tickInterval = 1.0f;

    private GameObject playerGO;
    private ulong entityID;

    //private bool collidingWithPlayer = false;
    private float tickTimer = 0f;

    private float loseSightTimer = 0f;
    public float repathInterval = 0.10f;
    private float repathTimer = 0f;

    // stun guard
    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;

    private Animator anim;
    public float stunRecoverDelay = 8.0f;
    private float recoverTimer = 0.0f;

    private GameObject enemyLight;

    public float lightHeight = 5.0f;
    public float lightForwardOffset = 0.5f;
    public Vector3 lightRotationOffset = new Vector3(0f, 0f, 0f); // radians

    bool playSFX = false;

    // replace to this
    // Name of the entity with UIHealthbarComponent (must match your scene)
    //public string playerHealthBarName = "Healthbar";
    //private GameObject playerHealthBar;

    public string healthBarName = "Healthbar";
    private GameObject healthBar;
    private float health = 0f;

    // STUN FEEDBACK
    private GameObject stunVFX;
    private string stunPrefabPath = "../Resources/Prefabs/EnemyStunSpark.prefab";

    // VO flags
    private bool hasPlayedAttackVO = false;
    private float lastVOTime = 0f;
    private float voCooldown = 2.0f;
    private System.Random random = new System.Random();
    private float voTimeTracker = 0f;
    private bool isPlayingVO = false;
    private bool hasPlayedPowerUpSFX = false; // Track EnemyPowerUp SFX

    private void UpdateStunVFX(bool recovering, float recoveryProgress)
    {
        if (stunVFX == null) return;

        if (recovering)
        {
            float ratio = recoveryProgress;
            stunVFX.SetActive(ratio > 0.05f);

            if (stunVFX.activeSelf)
            {
                // Linear scale down from 1.0 to 0.0
                stunVFX.transform.scale = new Vector3(ratio, ratio, ratio);
                stunVFX.transform.position = transform.position + new Vector3(0, 1.0f, 0);
            }
        }
        else
        {
             stunVFX.SetActive(true);
             stunVFX.transform.scale = new Vector3(1.0f, 1.0f, 1.0f);
             stunVFX.transform.position = transform.position + new Vector3(0, 1.0f, 0);
        }
    }

    private void TryStun()
    {
        if (isStunned)
            return;

        isStunned = true;
        stunTimer = stunDuration;
        hasPlayedPowerUpSFX = false; // Reset for next stun

        if (stunVFX != null)
        {
            Debug.Log("AttackState: Activating Stun VFX");
            stunVFX.SetActive(true);
            stunVFX.transform.scale = new Vector3(1.0f, 1.0f, 1.0f);
            stunVFX.transform.position = transform.position + new Vector3(0, 1.0f, 0);
        }

        if (anim != null)
        {
            anim.SetBool("IsMoving", false);
            anim.SetBool("IsHit", true);
        }

        NavAgent.SetDestination(entityID, transform.position);

        HideEnemyLight();
        GlobalAudio.PlaySFX("LightDisable");

        // Play death VO and shutdown SFX
        GlobalAudio.PlayVoice("DieHuman");
        GlobalAudio.PlaySFX("EnemyPowerDown");
    }

    private string GetEnemyLightName()
    {
        return "enemyLight_" + entityID;
    }

    private void EnsureEnemyLight()
    {
        enemyLight = GameObject.Find(GetEnemyLightName());

        if (enemyLight == null)
        {
            enemyLight = Prefab.Instantiate("../Resources/Prefabs/LightCone10.prefab");
            if (enemyLight != null)
                enemyLight.name = GetEnemyLightName();
        }
    }

    private void ShowEnemyLight()
    {
        EnsureEnemyLight();

        if (enemyLight == null)
            return;

        Physics.Internal_SetLightValue((ulong)enemyLight.GetInstanceID(), 6.481f);
        Vector3 lightPos = transform.position
                         + new Vector3(0f, lightHeight, 0f)
                         + transform.forward * lightForwardOffset;

        enemyLight.transform.position = lightPos;

        Quaternion rot = transform.rotation;
        rot = rot * Quaternion.Euler(
            lightRotationOffset.x,
            lightRotationOffset.y,
            lightRotationOffset.z
        );

        enemyLight.transform.rotation = rot;
        enemyLight.SetActive(true);
    }

    private void HideEnemyLight()
    {
        EnsureEnemyLight();
        if (enemyLight != null)
        {
            Physics.Internal_SetLightValue((ulong)enemyLight.GetInstanceID(), 0.0f);
            GlobalAudio.StopSFX("LightDamageLoop");
        }
    }

    private void CachePlayerIfNeeded()
    {
        if (playerGO == null)
            playerGO = GameObject.Find(playerName);
    }

    void Start()
    {
        entityID = (ulong)gameObject.GetInstanceID();
        anim = GetComponent<Animator>();
        CachePlayerIfNeeded();
        tickTimer = tickInterval;
        loseSightTimer = loseSightGraceTime;

        // Instantiate Stun VFX
        stunVFX = Prefab.Instantiate(stunPrefabPath);
        if (stunVFX != null)
        {
            stunVFX.SetActive(false);
        }

        // Find healthbar by name (replace to this)
        //playerHealthBar = GameObject.Find(playerHealthBarName);

        healthBar = GameObject.Find(healthBarName);
        if (healthBar != null)
        {
            health = GameplayHUD.GetHealth(healthBar);
        }

        ShowEnemyLight();
        
        // Reset VO flags
        hasPlayedAttackVO = false;
        lastVOTime = 0f;
    }

    private bool HasLineOfSightToPlayer()
    {
        if (playerGO == null) return false;

        Vector3 enemyPos = transform.position;
        Vector3 playerPos = playerGO.transform.position;
        Vector3 flatToPlayer = playerPos - enemyPos;
        flatToPlayer.y = 0f;
        if (flatToPlayer.Magnitude <= closeDetectDistance)
            return true;

        Vector3 origin = enemyPos
                       + new Vector3(0f, rayHeight, 0f)
                       + transform.forward * rayForwardOffset;

        Vector3 toPlayer = playerPos - origin;

        float dist = toPlayer.Magnitude;
        if (dist <= 0.0001f) return true;
        if (dist > viewDistance) return false;

        Vector3 dir = toPlayer / dist;

        RaycastHit hit;
        bool didHit = Physics.Raycast(origin, dir, out hit, dist);
        if (!didHit) return false;

        var hitGO = hit.transform.gameObject;

        // Ignore self-hit
        ulong hitID = (ulong)hitGO.GetInstanceID();
        if (hitID == entityID) return false;

        string n = hitGO.name;
        if (n == GetEnemyLightName()) return false;
        if (n == "Sphere") return false;
        if (n.StartsWith("SpawnPoint_")) return false;

        return hit.transform != null &&
               hit.transform.gameObject != null &&
               hit.transform.gameObject.name == playerName;
    }

    private void FacePlayer()
    {
        if (playerGO == null) return;

        Vector3 toPlayer = playerGO.transform.position - transform.position;
        toPlayer.y = 0f;

        float dist = toPlayer.Magnitude;
        if (dist <= 0.0001f) return;

        // yaw in degrees
        float yaw = (float)(Math.Atan2(toPlayer.x, toPlayer.z) * 180.0 / Math.PI);

        Physics.SetRotationEuler(entityID, new Vector3(0f, yaw, 0f));
    }

    void Update()
    {
        if (Input.GetMouseButtonDown(1))
            armTimer = 0.3f;

        if (armTimer > 0.0f)
            armTimer -= Time.deltaTime;

        RightClickStunArmed = armTimer > 0f;

        if (isStunned)
        {
            HideEnemyLight();
            if (anim != null)
            {
                anim.SetBool("IsMoving", false);
                anim.SetBool("IsHit", true);
            }

            // Keep VFX attached
            UpdateStunVFX(false, 1.0f);

            stunTimer -= Time.deltaTime;
            if (stunTimer <= 0.0f)
            {
                isStunned = false;
                recoverTimer = stunRecoverDelay;

                if (anim != null)
                    anim.SetBool("IsHit", false);
            }
            return;
        }

        if (recoverTimer > 0.0f)
        {
            HideEnemyLight();
            recoverTimer -= Time.deltaTime;

            // Trigger EnemyPowerUp at the halfway point of recovery
            if (!hasPlayedPowerUpSFX && recoverTimer <= stunRecoverDelay * 0.5f)
            {
                GlobalAudio.PlaySFX("EnemyPowerUp");
                hasPlayedPowerUpSFX = true;
            }

            // End 6.0s earlier
            float visibleTime = Math.Max(0.0f, stunRecoverDelay - 6.0f);
            float currentVisibleTime = Math.Max(0.0f, recoverTimer - 6.0f);

            float ratio = 0.0f;
            if (visibleTime > 0.001f)
                ratio = currentVisibleTime / visibleTime;

            // Aggressive ramp down (squared)
            ratio = ratio * ratio;

            UpdateStunVFX(true, ratio);

            if (anim != null)
                anim.SetBool("IsMoving", false);

            if (ratio <= 0.01f && stunVFX != null)
                 stunVFX.SetActive(false);

            NavAgent.SetDestination(entityID, transform.position);
            
            return;
        }

        if (isStunned || recoverTimer > 0.0f)
            HideEnemyLight();
        else
            ShowEnemyLight();

        CachePlayerIfNeeded();
        if (playerGO == null) return;

        float distToPlayer = (playerGO.transform.position - transform.position).Magnitude;

        bool hasLOS = HasLineOfSightToPlayer();
        if (hasLOS)
            loseSightTimer = loseSightGraceTime;
        else
            loseSightTimer -= Time.deltaTime;

        // Update VO time tracker
        voTimeTracker += Time.deltaTime;
        
        // Check global VO lock (only blocks if OTHER guard played)
        bool isGlobalVOLocked = (voTimeTracker - globalVOLockTime < globalVOLockDuration) && (lastVOGuardID != entityID);
        
        // Check per-guard VO lock (prevent same guard from spamming)
        bool isMyVOLocked = voTimeTracker - myVOLockTime < myVOLockDuration;

        // Play attack VO when entering attack range (only once per attack session)
        if (hasLOS && distToPlayer <= attackRange && !hasPlayedAttackVO && !isPlayingVO && !isGlobalVOLocked && !isMyVOLocked && voTimeTracker - lastVOTime >= voCooldown)
        {
            double voRoll = random.NextDouble();
            if (voRoll < 0.5)
                GlobalAudio.PlayVoice("ActivateInstantKill");
            else
                GlobalAudio.PlayVoice("EliminationInProgress");
            
            hasPlayedAttackVO = true;
            lastVOTime = voTimeTracker;
            globalVOLockTime = voTimeTracker; // Lock out OTHER guards
            lastVOGuardID = entityID; // Remember which guard played
            myVOLockTime = voTimeTracker; // Lock out self briefly
            isPlayingVO = true;
        }
        
        // Reset VO lock after short delay
        if (isPlayingVO && voTimeTracker - lastVOTime >= 1.0f)
        {
            isPlayingVO = false;
        }
        
        // Don't reset hasPlayedAttackVO here - only reset when leaving attack state
        // This prevents VO spam when player moves in/out of range

        if (distToPlayer > disengageDistance || loseSightTimer <= 0f)
        {
            playSFX = false;
            tickTimer = tickInterval;
            
            // Reset VO flag when leaving AttackState (going back to Chase)
            hasPlayedAttackVO = false;
            
            StateMachine.RequestPreviousState(entityID);
            return;
        }

        if (distToPlayer > attackRange)
        {
            if (anim != null)
                anim.SetBool("IsMoving", true);

            playSFX = false;
            tickTimer = tickInterval; // don’t damage while out of range

            repathTimer -= Time.deltaTime;
            if (repathTimer <= 0f)
            {
                NavAgent.SetDestination(entityID, playerGO.transform.position);
                repathTimer = repathInterval;
            }
            return;
        }

        // IN attack range, stop moving and deal damage
        FacePlayer();
        ShowEnemyLight();

        if (anim != null)
            anim.SetBool("IsMoving", false);

        NavAgent.SetDestination(entityID, transform.position);

        tickTimer -= Time.deltaTime;
        if (tickTimer <= 0f)
        {
            DealDamageToPlayer(damagePerTick);
            tickTimer = tickInterval;
        }
    }

    private void DealDamageToPlayer(float dmg)
    {
        // replace to this
        //if (playerHealthBar == null) return;

        //float health = GameplayHUD.GetHealth(playerHealthBar);
        //health = Math.Max(0, health - dmg);

        //GameplayHUD.SetHealth(playerHealthBar, health);

        if (healthBar == null) return;

        health = GameplayHUD.GetHealth(healthBar);
        health = Math.Max(0f, health - dmg);
        GameplayHUD.SetHealth(healthBar, health);

        if (!playSFX)
        {
            GlobalAudio.PlaySFX("LightDamageLoop");
            playSFX = true;
        }
    }

    void OnCollisionEnter(Collision col)
    {
        //if (col.gameObject.name == playerName)
        //    collidingWithPlayer = true;

        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            Debug.Log("Attack: Hit by sphere! Attempting stun.");
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }

    void OnCollisionStay(Collision col)
    {
        //if (col.gameObject.name == playerName)
        //    collidingWithPlayer = true;

        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            Debug.Log("Attack: Hit by sphere! Attempting stun.");
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }

    void OnCollisionExit(Collision col)
    {
        //if (col.gameObject.name == playerName)
        //{
        //    collidingWithPlayer = false;
        //    tickTimer = tickInterval;
        //}
    }
}
