using ErmineEngine;

public class GearRotateAnimationTEST : MonoBehaviour
{
    public float rotationSpeed = 0.8f;

    void Update()
    {
        Vector3 deltaEuler = new Vector3(rotationSpeed * Time.deltaTime, 0, 0);
        transform.Rotate(deltaEuler);
    }
}