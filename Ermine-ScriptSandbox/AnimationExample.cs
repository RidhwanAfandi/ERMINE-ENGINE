using ErmineEngine;
using System;

public class AnimationExample : MonoBehaviour
{
    private Animator animator;

    void Start()
    {
        // get/set parameters: bool, float, int, trigger
        // get/set animation state: current state and play state

        animator = GetComponent<Animator>();
        if (animator == null) Debug.LogError("Animator component missing!");

        // Play starting state in the animation editor
        animator.PlayStartState();
    }

    void Update()
    {
        if (animator == null)
        {
            Debug.LogError("Animator component missing!");
            return;
        }

        // Example of setting a float parameter based on input
        if (Input.GetKeyDown(KeyCode.W) || Input.GetKeyDown(KeyCode.A) ||
            Input.GetKeyDown(KeyCode.S) || Input.GetKeyDown(KeyCode.D))
            animator.SetFloat("Speed", 1.0f);
        else
            animator.SetFloat("Speed", 0.0f);

        // Example of setting a boolean parameter based on speed
        if (animator.GetFloat("Speed") > 0.5f)
            animator.SetBool("Running", true);
        else
            animator.SetBool("Running", false);

        // Example of changing to attack state when in idle state
        if (animator.currentState == "Idle" && Input.GetKeyDown(KeyCode.J))
        {
            animator.SetBool("IsAttacking", true);
        }
        
        if(animator.currentState == "Attack" && Input.GetKeyDown(KeyCode.K))
        {
            animator.SetBool("IsAttacking", false);
        }

        // Example of playing death animation if IsDead is true
        bool isDead = animator.GetBool("IsDead");
        if (isDead)
        {
            animator.Play("Death");
            return;
        }
    }
}
