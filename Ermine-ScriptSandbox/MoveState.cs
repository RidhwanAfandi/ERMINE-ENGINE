using ErmineEngine;
using System;

public class Move : MonoBehaviour
{
    public float stepDistance = 3.0f;
    public float reachDist = 0.6f;

    public float viewDistance = 15.0f;
    public float rayHeight = 5.5f;
    public float rayForwardOffset = 2.0f;

    public float turnCooldown = 0.5f; // prevents spam turning
    public float repathInterval = 0.25f; // prevents spamming SetDestination

    private ulong entityID;

    private Vector3 dir;
    private Vector3 target;

    private float turnTimer = 0f;
    private float repathTimer = 0f;

    //private bool jumping = false;
    private bool insideJumpArea = false;
    private ulong jumpLinkEntityID = 0;
    public float jumpCooldown = 10.0f;
    private float jumpCooldownTimer = 0.0f;

    public string playerName = "Player";

    private GameObject playerGO;

    // stun guard
    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;
    private bool hasPlayedPowerUpSFX = false; // Track EnemyPowerUp SFX

    public float edgeCheckForward = 1.0f;     // how far ahead to test for ground
    public float edgeCheckUp = 2.0f;          // how high above to start the downward ray
    public float edgeCheckDown = 5.0f;        // how far down to raycast

    public float stuckTimeToTurn = 0.6f;      // seconds stuck before turning
    public float stuckMoveEps = 0.02f;        // how little movement counts as "stuck"
    private Vector3 lastPos;
    private float stuckTimer = 0f;
    private float lastTargetDist = float.MaxValue;

    private GameObject rayDebug;

    private Animator anim;
    public float stunRecoverDelay = 8.0f;
    private float recoverTimer = 0.0f;

    // STUN FEEDBACK
    private GameObject stunVFX;
    private string stunPrefabPath = "../Resources/Prefabs/EnemyStunSpark.prefab";

    private bool isJumpingAnim = false;
    private bool isResting = false;
    private float jumpTimer = 0f;
    public float jumpDuration = 1.2f; // matches your animation
    public float restDuration = 2.0f;
    private float restTimer = 0f;

    private void CachePlayerIfNeeded()
    {
        if (playerGO == null)
            playerGO = GameObject.Find(playerName);
    }

    private bool HasLineOfSightToPlayer()
    {
        CachePlayerIfNeeded();
        if (playerGO == null) return false;

        Vector3 origin = transform.position
                       + new Vector3(0f, rayHeight, 0f)
                       + transform.forward * rayForwardOffset;

        Vector3 playerPoint = playerGO.transform.position;
        Vector3 toPlayer = playerPoint - origin;

        float dist = toPlayer.Magnitude;
        if (dist <= 0.0001f) return true;
        if (dist > viewDistance) return false;

        Vector3 dirToPlayer = toPlayer / dist;

        RaycastHit hit;
        
        bool didHit = Physics.Raycast(origin, dirToPlayer, out hit, dist);
        if (!didHit) return false;

        return hit.transform != null &&
               hit.transform.gameObject != null &&
               hit.transform.gameObject.name == playerName;
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

        if (stunVFX != null)
        {
            Debug.Log("MoveState: Activating Stun VFX");
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
        lastPos = transform.position;
        lastTargetDist = float.MaxValue;

        // Instantiate Stun VFX
        stunVFX = Prefab.Instantiate(stunPrefabPath);
        if (stunVFX != null)
        {
            stunVFX.SetActive(false);
        }

        // this can be used for the enemy lightcone to damage player
        //rayDebug = Prefab.Instantiate("../Resources/Prefabs/Sphere.prefab");
        //rayDebug.name = "RayDebug";
        //rayDebug.transform.scale = new Vector3(0.2f, 0.2f, 0.2f);

        Vector3 f = transform.forward;
        if (Math.Abs(f.x) >= Math.Abs(f.z))
            dir = new Vector3(f.x >= 0 ? 1f : -1f, 0f, 0f);
        else
            dir = new Vector3(0f, 0f, f.z >= 0 ? 1f : -1f);

        FaceDir();
        PushTargetForward(true);
    }

    void Update()
    {
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

                if (anim != null)
                    anim.SetBool("IsHit", false);
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

        if (anim != null)
            anim.SetBool("IsMoving", true);

        if (!isJumpingAnim && !isResting && HasLineOfSightToPlayer())
        {
            StateMachine.RequestNextState(entityID);
            return;
        }

        if (insideJumpArea && jumpCooldownTimer <= 0.0f && turnTimer <= 0f)
        {
            if (anim != null)
                anim.SetBool("IsGrounded", false);

            isJumpingAnim = true;
            jumpTimer = jumpDuration;
            NavAgent.StartJump(entityID, jumpLinkEntityID);
            jumpCooldownTimer = jumpCooldown;
            return;
        }

        if (isJumpingAnim)
        {
            jumpTimer -= Time.deltaTime;

            if (jumpTimer <= 0f)
            {
                if (anim != null)
                    anim.SetBool("IsGrounded", true); // LAND

                isJumpingAnim = false;
                restTimer = restDuration;
                isResting = true;
            }
        }

        if (isResting)
        {
            if (anim != null)
                anim.SetBool("IsMoving", false);

            restTimer -= Time.deltaTime;

            if (restTimer <= 0f)
            {
                isResting = false;
            }
            NavAgent.SetDestination(entityID, transform.position);
            return;
        }

        if (turnTimer > 0f) turnTimer -= Time.deltaTime;
        if (repathTimer > 0f) repathTimer -= Time.deltaTime;

        // only check turning if cooldown is over
        if (turnTimer <= 0f)
        {
            bool obstacle = HitsObstacleInFront();
            bool edge = !obstacle && IsEdgeAhead(); // only treat "no hit" as bad if it's an edge
            bool stuck = IsStuck();

            if (obstacle || edge || stuck)
            {
                TurnAround();
                FaceDir();
                PushTargetForward(true);

                stuckTimer = 0f;      // reset stuck state after turning
                lastTargetDist = float.MaxValue;
                turnTimer = turnCooldown;
                return;
            }
        }

        float dist = (target - transform.position).Magnitude;

        if (dist <= reachDist)
        {
            PushTargetForward(true);
        }
        else if (repathTimer <= 0f)
        {
            PushTargetForward(false);
        }
    }

    private bool HitsObstacleInFront()
    {
        Vector3 origin = transform.position
                       + new Vector3(0f, rayHeight, 0f)
                       + dir * rayForwardOffset;

        if (rayDebug != null)
            rayDebug.transform.position = origin;

        RaycastHit hit;
        bool didHit = Physics.Raycast(origin, dir, out hit, 0.8f);

        if (!didHit)
            return false;

        var hitGO = hit.transform.gameObject;

        // Ignore self-hit
        ulong hitID = (ulong)hitGO.GetInstanceID();
        if (hitID == entityID) return false;

        string n = hitGO.name;
        if (n == playerName) return false;
        if (n == "Sphere") return false;
        if (n.StartsWith("SpawnPoint_")) return false;

        return true;
    }

    private bool IsEdgeAhead()
    {
        // Probe a point forward (roughly where we're about to step)
        Vector3 probePoint = transform.position + dir * edgeCheckForward;

        // Raycast downward to see if there is ground/navmesh collider below
        Vector3 origin = probePoint + Vector3.up * edgeCheckUp;

        RaycastHit hit;
        bool hasGround = Physics.Raycast(origin, Vector3.down, out hit, edgeCheckDown);

        // If your world has "RayDebug"/enemy colliders, you can add ignores here if needed.
        return !hasGround;
    }

    private bool IsStuck()
    {
        float moved = (transform.position - lastPos).Magnitude;
        float targetDist = (target - transform.position).Magnitude;

        bool barelyMoved = moved <= stuckMoveEps;
        bool notGettingCloser = targetDist >= lastTargetDist - 0.01f;

        if (barelyMoved || notGettingCloser)
            stuckTimer += Time.deltaTime;
        else
            stuckTimer = 0f;

        lastPos = transform.position;
        lastTargetDist = targetDist;

        return stuckTimer >= stuckTimeToTurn;
    }

    private void PushTargetForward(bool force)
    {
        if (!force && repathTimer > 0f) return;

        Vector3 pos = transform.position;
        target = new Vector3(
            pos.x + dir.x * stepDistance,
            pos.y,
            pos.z + dir.z * stepDistance
        );

        NavAgent.SetDestination(entityID, target);
        repathTimer = repathInterval;
    }

    private void TurnAround()
    {
        dir = new Vector3(-dir.x, 0f, -dir.z);
    }

    private void FaceDir()
    {
        float yaw = (dir.z > 0) ? 0f :
                    (dir.z < 0) ? 180f :
                    (dir.x > 0) ? 90f : 270f;

        Vector3 rot = transform.rotation.eulerAngles;
        rot.y = yaw;
        transform.rotation = Quaternion.Euler(rot.x, rot.y, rot.z);
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
            Debug.Log("Move: Hit by sphere! Attempting stun.");
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
            Debug.Log("Move: Hit by sphere! Attempting stun.");
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
