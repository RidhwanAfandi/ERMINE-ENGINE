using ErmineEngine;
using System;

public class Chase : MonoBehaviour
{
    public string playerName = "Player";

    // If player is too far, stop chasing and return to previous state
    public float losePlayerDistance = 18.0f;

    // Line of sight (raycast) settings
    public float viewDistance = 25.0f;
    public float rayHeight = 0.8f;
    public float rayForwardOffset = 2.0f; // push ray out of own collider
    public float loseSightGraceTime = 0.35f; // prevents flicker behind corners
    public float attackEnterDistance = 5.0f;

    public float repathInterval = 0.10f;

    private GameObject playerGO;
    private ulong entityID;

    //private bool collidingWithPlayer = false;
    private float repathTimer = 0f;

    // counts down while LOS is lost; resets while LOS is true
    private float loseSightTimer = 0f;

    private bool jumping = false;
    private ulong jumpLinkEntityID = 0;

    // stun guard
    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;

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

        // stop immediately while stunned
        NavAgent.SetDestination(entityID, transform.position);
    }

    void Start()
    {
        entityID = (ulong)gameObject.GetInstanceID();
        CachePlayerIfNeeded();
        loseSightTimer = loseSightGraceTime;
    }

    private bool HasLineOfSightToPlayer()
    {
        if (playerGO == null) return false;

        Vector3 origin = transform.position
                       + new Vector3(0f, rayHeight, 0f)
                       + transform.forward * rayForwardOffset;

        Vector3 toPlayer = (playerGO.transform.position + new Vector3(0f, rayHeight, 0f)) - origin;

        float dist = toPlayer.Magnitude;
        if (dist <= 0.0001f) return true;
        if (dist > viewDistance) return false;

        Vector3 dir = toPlayer / dist;

        RaycastHit hit;
        bool didHit = Physics.Raycast(origin, dir, out hit, dist);
        if (!didHit) return false;

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
            //Debug.Log("stunned");
            stunTimer -= Time.deltaTime;
            if (stunTimer <= 0.0f)
            {
                isStunned = false;
            }
            return; // do NOTHING while stunned
        }

        if (jumping)
        {
            //Debug.Log("CALL StartJump: me=" + entityID + " link=" + jumpLinkEntityID);
            NavAgent.StartJump(entityID, jumpLinkEntityID);

            jumping = false;
            jumpLinkEntityID = 0;

            return;
        }

        CachePlayerIfNeeded();
        if (playerGO == null) return;

        float dist = (playerGO.transform.position - transform.position).Magnitude;

        // LOS timer logic
        bool hasLOS = HasLineOfSightToPlayer();
        if (hasLOS)
            loseSightTimer = loseSightGraceTime;
        else
            loseSightTimer -= Time.deltaTime;

        // Too far OR lost sight long enough back to previous
        if (dist > losePlayerDistance || loseSightTimer <= 0f)
        {
            StateMachine.RequestPreviousState(entityID);
            return;
        }

        // If near player go to Attack
        if (dist <= attackEnterDistance && hasLOS)
        {
            StateMachine.RequestNextState(entityID);
            return;
        }

        // Keep chasing, update destination periodically
        repathTimer -= Time.deltaTime;
        if (repathTimer <= 0f)
        {
            NavAgent.SetDestination(entityID, playerGO.transform.position);
            repathTimer = repathInterval;
        }
    }

    void OnCollisionEnter(Collision col)
    {
        //if (col.gameObject.name == playerName)
        //    collidingWithPlayer = true;

        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
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
        //if (col.gameObject.name == playerName)
        //    collidingWithPlayer = true;

        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
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
        //if (col.gameObject.name == playerName)
        //    collidingWithPlayer = false;

        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
            jumpLinkEntityID = (ulong)col.gameObject.GetInstanceID();
        }
    }
}
