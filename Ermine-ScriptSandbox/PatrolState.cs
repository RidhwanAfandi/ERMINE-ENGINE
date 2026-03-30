using ErmineEngine;
using System;

public class Patrol : MonoBehaviour
{
    // Global VO lock - prevent multiple guards playing VO simultaneously (combat/death VOs)
    private static float globalVOLockTime = 0f;
    private static float globalVOLockDuration = 2.0f;
    private static ulong lastVOGuardID = 0;
    
    // ScanningArea VO lock - separate lock for patrol VO (longer, less urgent)
    private static float scanVOLockTime = 0f;
    private static float scanVOLockDuration = 4.0f;
    private static ulong lastScanVOGuardID = 0;
    
    // ScanningArea VO - plays when arriving at patrol points
    private float lastScanVOTime = 0f;
    private float scanVOCooldown = 10.0f; // Min seconds between scan lines per guard
    private float scanTimeTracker = 0f; // Track time for ScanningArea VO
    private float scanVOMaxDistance = 30.0f; // Only play if player within this distance
    
    public float radius = 30f;
    public int pointCount = 16;
    public float reachDist = 0.5f;

    // If we’re not getting closer for this long, we give up and pick the next point.
    public float stuckTime = 0.75f;
    public float minProgressEpsilon = 0.02f;

    //public float recenterDelay = 1.0f;

    public string playerName = "Player";

    private GameObject playerGO;

    private Vector3[] patrolPoints;
    private int currentIndex = -1;

    private float stuckTimer = 0f;
    private float lastDist = float.MaxValue;

    private ulong entityID;

    private ulong jumpLinkEntityID = 0;
    private bool insideJumpArea = false;
    public float jumpCooldown = 3.0f;
    private float jumpCooldownTimer = 0.0f;

    // stun guard
    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;
    private bool hasPlayedPowerUpSFX = false; // Track EnemyPowerUp SFX

    public float viewDistance = 15.0f;
    public float rayHeight = 5.5f;
    public float rayForwardOffset = 2.0f;
    public float closeDetectDistance = 2.0f;

    private Vector3 patrolCenter;

    private Animator anim;
    public float stunRecoverDelay = 8.0f;
    private float recoverTimer = 0.0f;

    // STUN FEEDBACK
    private GameObject stunVFX;
    private string stunPrefabPath = "../Resources/Prefabs/EnemyStunSpark.prefab";

    private void CachePlayerIfNeeded()
    {
        if (playerGO == null)
            playerGO = GameObject.Find(playerName);
    }

    private bool HasLineOfSightToPlayer()
    {
        CachePlayerIfNeeded();
        if (playerGO == null) return false;

        Vector3 enemyPos = transform.position;
        Vector3 playerPoint = playerGO.transform.position;

        // fallback for very close targets on tiny platforms
        Vector3 flatToPlayer = playerPoint - enemyPos;
        flatToPlayer.y = 0f;
        if (flatToPlayer.Magnitude <= closeDetectDistance)
            return true;

        Vector3 origin = enemyPos
                       + new Vector3(0f, rayHeight, 0f)
                       + transform.forward * rayForwardOffset;

        Vector3 toPlayer = playerPoint - origin;

        float dist = toPlayer.Magnitude;
        if (dist <= 0.0001f) return true;
        if (dist > viewDistance) return false;

        Vector3 dirToPlayer = toPlayer / dist;

        RaycastHit hit;
        bool didHit = Physics.Raycast(origin, dirToPlayer, out hit, dist);
        if (!didHit) return false;

        var hitGO = hit.transform.gameObject;

        // Ignore self-hit
        ulong hitID = (ulong)hitGO.GetInstanceID();
        if (hitID == entityID) return false;

        string n = hitGO.name;
        if (n.StartsWith("SpawnPoint_")) return false;

        return hit.transform != null &&
               hit.transform.gameObject != null &&
               hit.transform.gameObject.name == playerName;
    }

    private void MoveToNextPoint()
    {
        if (patrolPoints == null || patrolPoints.Length == 0)
            return;

        currentIndex = (currentIndex + 1) % patrolPoints.Length;

        stuckTimer = 0f;
        lastDist = float.MaxValue;

        // Play ScanningArea VO occasionally when arriving at a patrol point
        // Only play if player is within range (so player can actually hear it)
        float distToPlayer = (playerGO.transform.position - transform.position).Magnitude;
        bool isScanVOLocked = (scanTimeTracker - scanVOLockTime < scanVOLockDuration) && (lastScanVOGuardID != entityID);

        if (!isScanVOLocked && scanTimeTracker - lastScanVOTime >= scanVOCooldown && distToPlayer <= scanVOMaxDistance)
        {
            GlobalAudio.PlayVoice("ScanningArea");
            lastScanVOTime = scanTimeTracker;
            scanVOLockTime = scanTimeTracker;
            lastScanVOGuardID = entityID;
        }

        NavAgent.SetDestination(entityID, patrolPoints[currentIndex]);
    }

    private void BuildPatrolPoints(Vector3 center)
    {
        if (pointCount < 2) pointCount = 2;

        patrolPoints = new Vector3[pointCount];

        for (int i = 0; i < pointCount; i++)
        {
            float t = (float)i / (float)pointCount;
            float ang = t * 6.28318530718f;

            patrolPoints[i] = new Vector3(
                center.x + (float)Math.Cos(ang) * radius,
                center.y,
                center.z + (float)Math.Sin(ang) * radius
            );
        }

        currentIndex = -1;
        MoveToNextPoint();
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

    private void TryStun()
    {
        if (isStunned)
            return;

        isStunned = true;
        stunTimer = stunDuration;
        hasPlayedPowerUpSFX = false; // Reset for next stun
        lastScanVOTime = 0f; // Reset ScanningArea VO

        if (stunVFX != null)
        {
            Debug.Log("PatrolState: Activating Stun VFX");
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
        
        // Play death VO and shutdown SFX
        GlobalAudio.PlayVoice("DieHuman");
        GlobalAudio.PlaySFX("EnemyPowerDown");
    }

    void Start()
    {
        entityID = (ulong)gameObject.GetInstanceID();
        anim = GetComponent<Animator>();
        CachePlayerIfNeeded();

        // Instantiate Stun VFX
        stunVFX = Prefab.Instantiate(stunPrefabPath);
        if (stunVFX != null)
        {
            stunVFX.SetActive(false);
        }

        // Build patrol points around the spawn position
        patrolCenter = transform.position;
        BuildPatrolPoints(patrolCenter);
    }

    void Update()
    {
        // Update ScanningArea time tracker
        scanTimeTracker += Time.deltaTime;
    
        if (Input.GetMouseButtonDown(1))
            armTimer = 0.3f;

        if (armTimer > 0.0f)
            armTimer -= Time.deltaTime;

        RightClickStunArmed = armTimer > 0f;

        if (jumpCooldownTimer > 0.0f)
            jumpCooldownTimer -= Time.deltaTime;

        if (isStunned)
        {
            if (anim != null)
            {
                anim.SetBool("IsMoving", false);
                anim.SetBool("IsHit", true);
            }

            // Keep VFX attached
            UpdateStunVFX(false, 1.0f);

            //Debug.Log("stunned");
            stunTimer -= Time.deltaTime;
            if (stunTimer <= 0.0f)
            {
                isStunned = false;
                recoverTimer = stunRecoverDelay;

                if (anim != null) anim.SetBool("IsHit", false);

                // Resume the current target after stun ends
                if (patrolPoints != null && patrolPoints.Length > 0 && currentIndex >= 0)
                    NavAgent.SetDestination(entityID, patrolPoints[currentIndex]);
            }
            return; // do NOTHING while stunned
        }
        if (recoverTimer > 0.0f)
        {
            recoverTimer -= Time.deltaTime;

            // Trigger EnemyPowerUp at the halfway point of recovery
            if (!hasPlayedPowerUpSFX && recoverTimer <= stunRecoverDelay * 0.5f)
            {
                GlobalAudio.PlaySFX("EnemyPowerUp");
                hasPlayedPowerUpSFX = true;
            }

            // End 6.0s earlier to avoid lingering effect
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

        if (anim != null)
            anim.SetBool("IsMoving", true);

        if (HasLineOfSightToPlayer())
        {
            StateMachine.RequestNextState(entityID);
            return;
        }

        if (insideJumpArea && jumpCooldownTimer <= 0.0f)
        {
            NavAgent.StartJump(entityID, jumpLinkEntityID);
            jumpCooldownTimer = jumpCooldown;
            return;
        }

        if (patrolPoints == null || patrolPoints.Length == 0 || currentIndex < 0)
        {
            BuildPatrolPoints(patrolCenter);
            return;
        }

        Vector3 pos = transform.position;
        Vector3 target = patrolPoints[currentIndex];

        float dist = (target - pos).Magnitude;

        // reached target, go next
        if (dist <= reachDist)
        {
            MoveToNextPoint();
            return;
        }

        // stuck detection if we’re not getting closer, count time
        if (dist >= lastDist - minProgressEpsilon)
            stuckTimer += Time.deltaTime;
        else
            stuckTimer = 0f;

        lastDist = dist;

        if (stuckTimer >= stuckTime)
        {
            // give up on this point and try the next one
            MoveToNextPoint();
        }
    }

    void OnCollisionEnter(Collision col)
    {
        if (col.gameObject.name == "JumpArea")
        {
            insideJumpArea = true;
            jumpLinkEntityID = (ulong)col.gameObject.GetInstanceID();
        }

        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            Debug.Log("Patrol: Hit by sphere! Attempting stun.");
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }

    void OnCollisionStay(Collision col)
    {
        if (col.gameObject.name == "JumpArea")
        {
            insideJumpArea = true;
            jumpLinkEntityID = (ulong)col.gameObject.GetInstanceID();
        }

        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            Debug.Log("Patrol: Hit by sphere! Attempting stun.");
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }

    void OnCollisionExit(Collision col)
    {
        if (col.gameObject.name == "JumpArea")
        {
            if (jumpLinkEntityID == (ulong)col.gameObject.GetInstanceID())
            {
                insideJumpArea = false;
                jumpLinkEntityID = 0;
            }
        }
    }
}
