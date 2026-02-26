using System;
using ErmineEngine;

public class RotatingPlatform : MonoBehaviour
{
    public float speed = .8f;
    public bool active = true;

    // Audio activation distance (slightly larger than maxDistance for smooth fade)
    public float audioActivationRange = 120f;

    private AudioComponent audioComp;
    private GameObject player;

    private bool playerOnPlatform = false;

    private Vector3 lastPlatformPos;
    private Quaternion lastPlatformRot;

    void Start()
    {
        audioComp = GetComponent<AudioComponent>();
        player = GameObject.Find("Player");

        if (audioComp == null)
        {
            Console.WriteLine("Warning: No AudioComponent found on rotating platform!");
        }

        if (player == null)
            Console.WriteLine("Warning: Player not found!");

        lastPlatformPos = transform.position;
        lastPlatformRot = transform.rotation;
    }

    void Update()
    {
        if (!active)
        {
            // Stop audio when platform stops
            if (audioComp != null && audioComp.isPlaying)
            {
                audioComp.shouldStop = true;
            }
            return;
        }

        // Cache old transform
        Vector3 oldPos = transform.position;
        Quaternion oldRot = transform.rotation;

        // Rotate platform
        transform.Rotate(new Vector3(0f, speed * Time.deltaTime, 0f));
        Physics.SetRotationQuat(
            (ulong)gameObject.GetInstanceID(),
            transform.rotation
        );

        // Compute platform delta
        Vector3 deltaPos = transform.position - oldPos;
        Quaternion deltaRot = transform.rotation * Quaternion.Inverse(oldRot);

        // Apply platform motion to player
        if (playerOnPlatform && player != null)
        {
            ApplyPlatformMotion(deltaPos, deltaRot);
        }

        lastPlatformPos = transform.position;
        lastPlatformRot = transform.rotation;

        HandleAudioByDistance();
    }

    private void ApplyPlatformMotion(Vector3 deltaPos, Quaternion deltaRot)
    {
        Transform pt = player.transform;

        // Move with platform translation
        pt.position += deltaPos;

        // Rotate around platform pivot (rectangle-safe)
        Vector3 relative = pt.position - transform.position;
        relative = deltaRot * relative;
        pt.position = transform.position + relative;

        // Sync physics
        Physics.SetPosition(
            (ulong)player.GetInstanceID(),
            pt.position
        );
    }

    private void HandleAudioByDistance()
    {
        if (audioComp == null || player == null) return;

        // Calculate distance manually

        Vector3 diff = player.transform.position - transform.position;
        float distance = Mathf.Sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        // Start audio when player is close enough

        if (distance < audioActivationRange && !audioComp.isPlaying)
        {
            audioComp.shouldPlay = true;
        }
        // Stop audio when player is too far
        else if (distance >= audioActivationRange && audioComp.isPlaying)
        {
            audioComp.shouldStop = true;
        }
    }
    public void IsActive(bool state)
    {
        active = state;

        // Control audio based on active state
        if (audioComp != null)
        {
            if (state)
            {
                // Don't immediately play - let HandleAudioByDistance decide based on player proximity
                // Audio will start playing in Update() if player is close enough
            }
            else
            {
                audioComp.shouldStop = true;
            }
        }
    }

    void OnCollisionStay(Collider other)
    {
        if (other.gameObject.name == "Player")
            playerOnPlatform = true;
    }

    void OnCollisionExit(Collider other)
    {
        if (other.gameObject.name == "Player")
            playerOnPlatform = false;
    }
}