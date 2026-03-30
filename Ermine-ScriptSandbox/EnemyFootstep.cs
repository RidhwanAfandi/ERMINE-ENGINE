using System;
using ErmineEngine;

public class EnemyFootstep : MonoBehaviour
{
    public float footstepInterval = 1.1f; // Adjust per guard type in scene file
    
    private AudioComponent audioComp;
    private Animator anim;
    private float footstepTimer = 0f;
    private bool wasWalking = false;

    void Start()
    {
        audioComp = GetComponent<AudioComponent>();
        anim = GetComponent<Animator>();

        if (audioComp == null)
        {
            Console.WriteLine("Warning: No AudioComponent found on enemy!");
        }
    }

    void Update()
    {
        HandleFootstepTiming();
    }

    private void HandleFootstepTiming()
    {
        if (anim == null)
        {
            anim = GetComponent<Animator>();
            if (anim == null) return;
        }

        if (audioComp == null) return;

        // Check if enemy is walking using the animator bool parameter
        bool isWalking = anim.GetBool("IsMoving");

        // Detect transition from not-walking to walking to reset timer
        if (isWalking && !wasWalking)
        {
            footstepTimer = 0f;
        }

        if (isWalking)
        {
            footstepTimer += Time.deltaTime;

            // Play footstep sound at intervals
            if (footstepTimer >= footstepInterval)
            {
                audioComp.shouldPlay = true;
                footstepTimer = 0f;
            }
        }

        wasWalking = isWalking;
    }
}
