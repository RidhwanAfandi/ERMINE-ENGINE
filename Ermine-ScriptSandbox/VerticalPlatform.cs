using System;
using ErmineEngine;

public class VerticalPlatform : MonoBehaviour
{
    
    public float moveDistance = 2f;  // Total distance it moves up and down
    public float moveSpeed = 2f;     // How fast it moves

    private float timer = 0f;
    private Vector3 startPosition;

    void Awake()
    {
        startPosition = transform.position;
    }
   

    void FixedUpdate()
    {
        // Increase time properly
        timer += Time.deltaTime * moveSpeed;

        // PingPong time
        float t = PingPong(timer, moveDistance);

        // Centered movement
        float offset = t - (moveDistance * 0.5f);

        transform.position = startPosition + new Vector3(0f, offset, 0f);
        if(Physics.HasPhysicComp((ulong)gameObject.GetInstanceID()))
            Physics.SetPosition((ulong)gameObject.GetInstanceID(), startPosition + new Vector3(0f, offset, 0f));
    }
    
    
    

    float PingPong(float value, float length)
    {
        float mod = value % (2 * length);
        if (mod < length)
            return mod;
        else
            return (2 * length) - mod;
    }
}

