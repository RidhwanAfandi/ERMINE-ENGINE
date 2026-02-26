using System;
using ErmineEngine;

public class RotateGears : MonoBehaviour
{
    public float rotationSpeed = 90f;

    void Update()
    {
        Vector3 deltaEuler = new Vector3(0f, 0f, rotationSpeed * Time.deltaTime);
        transform.Rotate(deltaEuler);
    }
}

