using ErmineEngine;

public class TimedDestroy : MonoBehaviour
{
    public float duration = 1.0f;

    private void Update()
    {
        duration -= Time.deltaTime;
        if (duration <= 0)
        {
            Debug.Log("TimedDestroy: Destroying " + gameObject.name + " (" + gameObject.GetInstanceID() + ")");
            GameObject.Destroy(gameObject);
        }
    }
}
