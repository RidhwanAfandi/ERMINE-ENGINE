using ErmineEngine;

public class Test3 : MonoBehaviour
{
    private float t = 0.0f;
    private float max, min = 0.0f;

    void Start()
    {
        max = 2.0f;
        min = 0.0f;
    }

    void Update()
    {
        PostEffects.VignetteIntensity = Mathf.Lerp(min, max, t);
        t += 0.5f * Time.deltaTime;

        if (t > 1.0f)
        {
            (max, min) = (min, max);
            t = 0.0f;
        }
    }
}