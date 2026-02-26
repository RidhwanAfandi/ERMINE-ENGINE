using System;
using ErmineEngine;

public class Switch : MonoBehaviour
{
    public bool enabled = false;
    public GameObject target;

    void Start()
    {
        target = GameObject.Find("MyObject");
        if (target == null)
        {
            Debug.Log("MyObject NOT found!");
        }
        else
        {
            Debug.Log("MyObject FOUND: " + target.name);
        }
    }

    void Update()
    {
        if (enabled)
        {
            target.GetComponent<RotatingPlatform>().IsActive(enabled);
        }
    }
}

