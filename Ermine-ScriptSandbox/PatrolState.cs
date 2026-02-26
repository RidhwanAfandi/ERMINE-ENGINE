using ErmineEngine;
using System;

public class Patrol : MonoBehaviour
{
    public float radius = 5f;
    public int pointCount = 16;
    public float reachDist = 0.5f;

    // If we’re not getting closer for this long, we give up and pick the next point.
    public float stuckTime = 0.75f;
    public float minProgressEpsilon = 0.02f;

    public float recenterDelay = 1.0f;

    public string playerName = "Player";

    private GameObject playerGO;

    private Vector3[] patrolPoints;
    private int currentIndex = -1;

    private float stuckTimer = 0f;
    private float lastDist = float.MaxValue;

    private ulong entityID;

    private bool jumping = false;
    private ulong jumpLinkEntityID = 0;

    private bool pendingRecenter = false;
    private float recenterTimer = 0f;

    // stun guard
    public float stunDuration = 5.0f;
    private bool isStunned = false;
    private float stunTimer = 0.0f;
    public static bool RightClickStunArmed = false;
    private float armTimer = 0.0f;

    public float viewDistance = 15.0f;
    public float rayHeight = 0.8f;
    public float rayForwardOffset = 2.0f;

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

        Vector3 playerPoint = playerGO.transform.position + new Vector3(0f, rayHeight, 0f);
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

    private void MoveToNextPoint()
    {
        if (patrolPoints == null || patrolPoints.Length == 0)
            return;

        currentIndex = (currentIndex + 1) % patrolPoints.Length;

        stuckTimer = 0f;
        lastDist = float.MaxValue;

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

        // Build patrol points around the spawn position
        BuildPatrolPoints(transform.position);
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

                // Resume the current target after stun ends
                if (patrolPoints != null && patrolPoints.Length > 0 && currentIndex >= 0)
                    NavAgent.SetDestination(entityID, patrolPoints[currentIndex]);
            }
            return; // do NOTHING while stunned
        }

        if (HasLineOfSightToPlayer())
        {
            StateMachine.RequestNextState(entityID);
            return;
        }

        if (jumping)
        {
            //Debug.Log("CALL StartJump: me=" + entityID + " link=" + jumpLinkEntityID);
            NavAgent.StartJump(entityID, jumpLinkEntityID);

            jumping = false;
            jumpLinkEntityID = 0;

            // Schedule a patrol recenter after the jump likely finishes
            pendingRecenter = true;
            recenterTimer = recenterDelay;

            return;
        }

        // after landing, rebuild patrol points around current position
        if (pendingRecenter)
        {
            recenterTimer -= Time.deltaTime;
            if (recenterTimer <= 0f)
            {
                pendingRecenter = false;
                BuildPatrolPoints(transform.position);
                return;
            }
        }

        if (patrolPoints == null || patrolPoints.Length == 0 || currentIndex < 0)
        {
            BuildPatrolPoints(transform.position);
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
        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
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
        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
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
        if (jumping) return;
        if (col.gameObject.name == "JumpArea")
        {
            jumping = true;
            jumpLinkEntityID = (ulong)col.gameObject.GetInstanceID();
        }
    }
}

