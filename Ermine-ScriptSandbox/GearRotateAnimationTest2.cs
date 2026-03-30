using ErmineEngine;

public class GearRotateAnimationTEST2 : MonoBehaviour
{
    public float rotationSpeed = 0.8f;

    void Update()
    {
        Vector3 deltaEuler = new Vector3(0, 0, rotationSpeed * Time.deltaTime);
        transform.Rotate(deltaEuler);
    }
}