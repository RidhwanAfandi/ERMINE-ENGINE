using ErmineEngine;
using System;

public class Chase : MonoBehaviour
{
    // Global VO lock - prevent multiple guards playing VO simultaneously
    private static float globalVOLockTime = 0f;
    private static float globalVOLockDuration = 2.0f; // Lock out OTHER guards for 2 seconds
    private static ulong lastVOGuardID = 0; // Track which guard played last
    
    // Per-guard VO lock - allow this guard to chain VO
    private float myVOLockTime = 0f;
    private float myVOLockDuration = 0.5f; // This guard can't play again for 0.5s
    
    public string playerName = "Player";

    // If player is too far, stop chasing and return to previous state
    public float losePlayerDistance = 18.0f;

    // LOS
    public float viewDistance = 25.0f;
    public float rayHeight = 5.5f;
    public float rayForwardOffset = 2.0f; // push ray out of own collider
    public float loseSightGraceTime = 0.35f; // prevents flicker behind corners
    public float attackEnterDistance = 5.0f;
    public float closeDetectDistance = 2.0f;

    public float repathInterval = 0.10f;

    private GameObject playerGO;
    private ulong entityID;

    private float repathTimer = 0f;

    // counts down while LOS is lost; resets while LOS is true
    private float loseSightTimer = 0f;

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

    // STUN FEEDBACK
    private GameObject stunVFX;
    private string stunPrefabPath = "../Resources/Prefabs/EnemyStunSpark.prefab";

    // VO flags to prevent spamming
    private bool hasPlayedSpotVO = false;
    private bool hasPlayedEngagementVO = false; // Track engagement VO separately
    private float lastVOTime = 0f;
    private float voCooldown = 3.0f; // Minimum time between VO lines
    private System.Random random = new System.Random();
    private float voTimeTracker = 0f; // Track time for VO cooldown
    private bool isPlayingVO = false; // Prevent VO stacking
    private bool hasPlayedPowerUpSFX = false; // Track EnemyPowerUp SFX

    private void CachePlayerIfNeeded()
    {
        if (playerGO == null)
            playerGO = GameObject.Find(playerName);
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
            Debug.Log("ChaseState: Activating Stun VFX");
            stunVFX.SetActive(true);
            stunVFX.transform.scale = new Vector3(1.0f, 1.0f, 1.0f);
            stunVFX.transform.position = transform.position + new Vector3(0, 1.0f, 0);
        }

        if (anim != null)
        {
            anim.SetBool("IsMoving", false);
            anim.SetBool("IsHit", true);
        }

        // stop immediately while stunned
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
            Physics.Internal_SetLightValue((ulong)enemyLight.GetInstanceID() ,0.0f);
            GlobalAudio.StopSFX("LightDamageLoop");
        }
    }
    void Start()
    {
        entityID = (ulong)gameObject.GetInstanceID();
        anim = GetComponent<Animator>();
        CachePlayerIfNeeded();
        loseSightTimer = loseSightGraceTime;

        // Instantiate Stun VFX
        stunVFX = Prefab.Instantiate(stunPrefabPath);
        if (stunVFX != null)
        {
            stunVFX.SetActive(false);
        }

        ShowEnemyLight();
        
        // Reset VO flags
        hasPlayedSpotVO = false;
        lastVOTime = 0f;
    }
    
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

            if (ratio <= 0.01f)
            {
                 if (stunVFX != null) stunVFX.SetActive(false);
            }

            if (anim != null)
                anim.SetBool("IsMoving", false);

            NavAgent.SetDestination(entityID, transform.position);
            
            return;
        }
        
        if (stunVFX != null && stunVFX.activeSelf) stunVFX.SetActive(false);

        if (isStunned || recoverTimer > 0.0f)
            HideEnemyLight();
        else
            ShowEnemyLight();

        if (anim != null)
            anim.SetBool("IsMoving", true);

        CachePlayerIfNeeded();
        if (playerGO == null) return;

        float dist = (playerGO.transform.position - transform.position).Magnitude;

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

        // Play VO when first spotting the player
        if (hasLOS && !hasPlayedSpotVO && !isPlayingVO && !isGlobalVOLocked && !isMyVOLocked && voTimeTracker - lastVOTime >= voCooldown)
        {
            // Randomly choose between intruder alert lines
            double voRoll = random.NextDouble();
            if (voRoll < 0.5)
                GlobalAudio.PlayVoice("IntruderAlert");
            else
                GlobalAudio.PlayVoice("SecurityBreachConfirmed");
            
            hasPlayedSpotVO = true;
            lastVOTime = voTimeTracker;
            globalVOLockTime = voTimeTracker; // Lock out OTHER guards
            lastVOGuardID = entityID; // Remember which guard played
            myVOLockTime = voTimeTracker; // Lock out self briefly
            isPlayingVO = true;
        }
        
        // Play engagement VO when chasing but not yet in attack range (ONCE per chase session)
        if (hasLOS && hasPlayedSpotVO && !hasPlayedEngagementVO && !isPlayingVO && !isGlobalVOLocked && !isMyVOLocked && dist > attackEnterDistance && voTimeTracker - lastVOTime >= voCooldown)
        {
            double voRoll = random.NextDouble();
            if (voRoll < 0.33)
                GlobalAudio.PlayVoice("EnemySighted");
            else if (voRoll < 0.66)
                GlobalAudio.PlayVoice("EngagingTarget");
            else
                GlobalAudio.PlayVoice("ThisIsYourFinalWarning");

            hasPlayedEngagementVO = true; // ← Only play once!
            lastVOTime = voTimeTracker;
            globalVOLockTime = voTimeTracker; // Lock out OTHER guards
            lastVOGuardID = entityID; // Remember which guard played
            myVOLockTime = voTimeTracker; // Lock out self briefly
            isPlayingVO = true;
        }
        
        // Reset VO lock after cooldown
        if (isPlayingVO && voTimeTracker - lastVOTime >= 1.5f)
        {
            isPlayingVO = false;
        }

        // Reset VO flags when LOS is lost
        if (!hasLOS && hasPlayedSpotVO && loseSightTimer <= 0f)
        {
            hasPlayedSpotVO = false;
            hasPlayedEngagementVO = false;
        }

        if (dist > losePlayerDistance || loseSightTimer <= 0f)
        {
            if (anim != null)
                anim.SetBool("IsMoving", false);

            HideEnemyLight();
            
            // Reset all VO flags when giving up chase
            hasPlayedSpotVO = false;
            hasPlayedEngagementVO = false;
            
            StateMachine.RequestPreviousState(entityID);
            return;
        }

        if (dist <= attackEnterDistance && hasLOS)
        {
            if (anim != null)
                anim.SetBool("IsMoving", false);

            StateMachine.RequestNextState(entityID);
            return;
        }

        repathTimer -= Time.deltaTime;
        if (repathTimer <= 0f)
        {
            NavAgent.SetDestination(entityID, playerGO.transform.position);
            repathTimer = repathInterval;
        }
    }

    void OnCollisionEnter(Collision col)
    {
        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }

    void OnCollisionStay(Collision col)
    {
        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }

    void OnCollisionExit(Collision col)
    {
    }
}
