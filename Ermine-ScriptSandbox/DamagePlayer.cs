using ErmineEngine;
using System;

public class DamagePlayer : MonoBehaviour
{
    private float health = 0f;
    public float timer = 1f;
    public float damage = 10f;
    private bool playerInside = false;

    // Name of the entity with UIHealthbarComponent (must match your scene)
    public string healthBarName = "Healthbar";

    //Health
    private GameObject healthBar;

    private void Start()
    {
        // Find healthbar by name
        healthBar = GameObject.Find(healthBarName);
        if (healthBar != null)
        {
            health = GameplayHUD.GetHealth(healthBar);
        }
    }

    private void Update()
    {
        if (playerInside)
        {
            timer -= Time.deltaTime;

            if (timer <= 0f)
            {
                TakeDamage(damage);
                timer = 1f;
            }
        }
    }

    void TakeDamage(float dmg)
    {
        if (healthBar == null) return;

        health = GameplayHUD.GetHealth(healthBar);
        health = Math.Max(0, health - dmg);

        GameplayHUD.SetHealth(healthBar, health);
    }

    void OnCollisionEnter(Collision col)
    {
        if (col.gameObject.name == "Player")
            playerInside = true;
    }
    void OnCollisionStay(Collision col)
    {
        if (col.gameObject.name == "Player")
            playerInside = true;
    }

    void OnCollisionExit(Collision col)
    {
        if (col.gameObject.name == "Player")
        {
            playerInside = false;
            timer = 1f; // Reset timer when leaving
        }
    }
}