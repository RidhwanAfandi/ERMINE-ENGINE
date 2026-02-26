using System;
using ErmineEngine;

public class Test2 : MonoBehaviour
{
    public float speed = 2.0f;
    private float stateTimer = 0f;
    private float switchTime = 3f;

    void Start()
    {
        NavAgent.SetDestination((ulong)gameObject.GetInstanceID(), new Vector3(0, 0, 0));
    }

    void Update()
    {
        //transform.Rotate(Vector3.down * (Time.deltaTime * speed));
        //transform.Translate(Vector3.forward * speed * Time.deltaTime);
        // After 3 seconds, trigger next FSM state
        stateTimer += Time.deltaTime;
        if (stateTimer >= switchTime)
        {
            //Debug.Log("Test2 requesting transition to previous state");
            StateMachine.RequestPreviousState((ulong)gameObject.GetInstanceID());
            stateTimer = 0f;
        }
    }
}

