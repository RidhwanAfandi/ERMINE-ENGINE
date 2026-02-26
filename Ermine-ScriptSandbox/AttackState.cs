using ErmineEngine;
using System;

public class Attack : MonoBehaviour
{
    public string playerName = "Player";

    public float attackRange = 5.0f;
    // If player no longer close enough go back to Chase
    public float disengageDistance = 8.0f;

    // Line-of-sight (raycast) settings
    public float viewDistance = 18.0f;
    public float rayHeight = 0.8f;
    public float rayForwardOffset = 2.0f; // push ray out of own collider
    public float loseSightGraceTime = 0.25f; // prevents flicker behind corners

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

    private bool jumping = false;
    private ulong jumpLinkEntityID = 0;

    // stun guard
    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;

    // replace to this
    // Name of the entity with UIHealthbarComponent (must match your scene)
    //public string playerHealthBarName = "Healthbar";
    //private GameObject playerHealthBar;

    private void TryStun()
    {
        if (isStunned)
            return;

        isStunned = true;
        stunTimer = stunDuration;

        // stop immediately while stunned
        NavAgent.SetDestination(entityID, transform.position);
    }

    private void CachePlayerIfNeeded()
    {
        if (playerGO == null)
            playerGO = GameObject.Find(playerName);
    }

    void Start()
    {
        entityID = (ulong)gameObject.GetInstanceID();
        CachePlayerIfNeeded();
        tickTimer = tickInterval;
        loseSightTimer = loseSightGraceTime;

        // Find healthbar by name (replace to this)
        //playerHealthBar = GameObject.Find(playerHealthBarName);
    }

    private bool HasLineOfSightToPlayer()
    {
        if (playerGO == null) return false;

        Vector3 origin = transform.position
                       + new Vector3(0f, rayHeight, 0f)
                       + transform.forward * rayForwardOffset;

        Vector3 playerPoint = playerGO.transform.position + new Vector3(0f, rayHeight, 0f);
        Vector3 toPlayer = playerPoint - origin;

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

        float distToPlayer = (playerGO.transform.position - transform.position).Magnitude;

        // LOS timer logic
        bool hasLOS = HasLineOfSightToPlayer();
        if (hasLOS)
            loseSightTimer = loseSightGraceTime;
        else
            loseSightTimer -= Time.deltaTime;

        // If too far OR lost LOS back to Chase
        if (distToPlayer > disengageDistance || loseSightTimer <= 0f)
        {
            StateMachine.RequestPreviousState(entityID);
            tickTimer = tickInterval;
            return;
        }

        // If NOT in attack range, move towards player but stay in Attack state
        if (distToPlayer > attackRange)
        {
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
        float health = GameplayHUD.GetHealth(GameplayHUD.GetHealthBar());
        health = Math.Max(0, health - dmg);

        GameObject bar = GameplayHUD.GetHealthBar();
        GameplayHUD.SetHealth(bar, health);

        // replace to this
        //if (playerHealthBar == null) return;

        //float health = GameplayHUD.GetHealth(playerHealthBar);
        //health = Math.Max(0, health - dmg);

        //GameplayHUD.SetHealth(playerHealthBar, health);
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
        //{
        //    collidingWithPlayer = false;
        //    tickTimer = tickInterval;
        //}

        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
            jumpLinkEntityID = (ulong)col.gameObject.GetInstanceID();
        }
    }
}
