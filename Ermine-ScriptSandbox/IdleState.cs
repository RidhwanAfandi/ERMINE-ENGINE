using ErmineEngine;
using System;

public class Idle : MonoBehaviour
{
    public string playerName = "Player";

    public float viewDistance = 15.0f;
    public float rayHeight = 5.5f;
    public float rayForwardOffset = 2.0f;
    public float closeDetectDistance = 2.0f;

    private GameObject playerGO;
    private ulong entityID;

    private ulong jumpLinkEntityID = 0;
    private bool insideJumpArea = false;

    public float jumpCooldown = 3.0f;
    private float jumpCooldownTimer = 0.0f;

    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;
    private bool hasPlayedPowerUpSFX = false; // Track EnemyPowerUp SFX

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

    private void TryStun()
    {
        if (isStunned)
            return;

        isStunned = true;
        stunTimer = stunDuration;
        hasPlayedPowerUpSFX = false; // Reset for next stun

        if (stunVFX != null)
        {
            Debug.Log("IdleState: Activating Stun VFX");
            stunVFX.SetActive(true);
            stunVFX.transform.scale = new Vector3(1.0f, 1.0f, 1.0f);
            stunVFX.transform.position = transform.position + new Vector3(0, 1.0f, 0);
        }

        if (anim != null)
        {
            anim.SetBool("IsHit", true);
        }

        // stop immediately while stunned
        NavAgent.SetDestination(entityID, transform.position);
        
        // Play death VO and shutdown SFX
        GlobalAudio.PlayVoice("DieHuman");
        GlobalAudio.PlaySFX("EnemyPowerDown");
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
                anim.SetBool("IsHit", true);
            }

            // Keep VFX attached and full power
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
            // RAMP DOWN PHASE
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
                 // Recovery complete, ensure VFX is off
                 if (stunVFX != null) stunVFX.SetActive(false);
            }
            return;
        }
        
        // Normal state - ensure VFX off
        if (stunVFX != null && stunVFX.activeSelf) stunVFX.SetActive(false);

        // If player is visible, switch state

        // If player is visible, switch state
        if (HasLineOfSightToPlayer())
        {
            StateMachine.RequestNextState(entityID);
            return;
        }

        // If standing in jump area, jump
        if (insideJumpArea && jumpCooldownTimer <= 0.0f)
        {
            NavAgent.StartJump(entityID, jumpLinkEntityID);
            jumpCooldownTimer = jumpCooldown;
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
        if (!RightClickStunArmed) return;
        if (col.gameObject.name == "Sphere")
        {
            TryStun();
            armTimer = 0.0f;
            RightClickStunArmed = false;
        }
    }
}
