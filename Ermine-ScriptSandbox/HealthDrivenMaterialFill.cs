using System;
using ErmineEngine;

public class HealthDrivenMaterialFill : MonoBehaviour
{
    public string healthBarName = "Healthbar";
    public float fillWhenEmpty = 0.0f;
    public float fillWhenFull = 0.3f;

    private Material targetMaterial;
    private GameObject healthSource;
    private float targetFill = 1.0f;
    private bool subscribed = false;

    void Start()
    {
        ResolveTargetMaterial();
        ResolveHealthSource();
        SubscribeHealthEvent();
        SyncFromCurrentHealth(true);
    }

    void OnEnable()
    {
        SubscribeHealthEvent();
    }

    void OnDisable()
    {
        UnsubscribeHealthEvent();
    }

    void OnDestroy()
    {
        UnsubscribeHealthEvent();
    }

    private void ResolveTargetMaterial()
    {
        if (gameObject != null)
            targetMaterial = gameObject.GetComponent<Material>();

        if (targetMaterial == null)
            return;
        else
            targetFill = targetMaterial.fill;
    }

    private void ResolveHealthSource()
    {
        if (!string.IsNullOrEmpty(healthBarName))
            healthSource = GameObject.Find(healthBarName);

        if (healthSource == null)
            healthSource = GameplayHUD.GetHealthBar();
    }

    private void SubscribeHealthEvent()
    {
        if (subscribed) return;
        GameplayHUD.HealthChanged += OnHealthChanged;
        subscribed = true;
    }

    private void UnsubscribeHealthEvent()
    {
        if (!subscribed) return;
        GameplayHUD.HealthChanged -= OnHealthChanged;
        subscribed = false;
    }

    private void SyncFromCurrentHealth(bool immediate)
    {
        if (healthSource == null) return;
        float h = GameplayHUD.GetHealth(healthSource);
        float max = GameplayHUD.GetMaxHealth(healthSource);
        ApplyHealth(h, max, immediate);
    }

    private void OnHealthChanged(GameObject source, float health, float maxHealth)
    {
        if (healthSource == null || source == null) return;
        if (source.GetInstanceID() != healthSource.GetInstanceID())
        {
            return;
        }
        ApplyHealth(health, maxHealth, false);
    }

    private void ApplyHealth(float health, float maxHealth, bool immediate)
    {
        if (targetMaterial == null) return;

        float ratio = (maxHealth > 0.0f) ? (health / maxHealth) : 0.0f;
        ratio = Mathf.Clamp(ratio, 0.0f, 1.0f);
        float minFill = Mathf.Clamp(fillWhenEmpty, 0.0f, 1.0f);
        float maxFill = Mathf.Clamp(fillWhenFull, 0.0f, 1.0f);
        targetFill = Mathf.Lerp(minFill, maxFill, ratio);
        targetMaterial.fill = targetFill;
    }
}
