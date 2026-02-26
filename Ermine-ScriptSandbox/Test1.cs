using System;
using ErmineEngine;

public class Test1 : MonoBehaviour
{
    public float speed = 2.0f;
    private float stateTimer = 0f;
    private float switchTime = 3f; // seconds before switching to next state

    void Start()
    {
        NavAgent.SetDestination((ulong)gameObject.GetInstanceID(), new Vector3(0, 0, 10));
    }

    void Update()
    {
        transform.Rotate(Vector3.up * (Time.deltaTime * speed));

        // After 3 seconds, trigger next FSM state
        stateTimer += Time.deltaTime;
        if (stateTimer >= switchTime)
        {
            //Debug.Log("Test1 requesting transition to next state");
            StateMachine.RequestNextState((ulong)gameObject.GetInstanceID());
            stateTimer = 0f;
        }
    }
}

