using ErmineEngine;

public class GearRotateAnimation : MonoBehaviour
{
    public float rotationSpeed = 30f;

    void Update()
    {
        Vector3 deltaEuler = new Vector3(0, rotationSpeed * Time.deltaTime, 0);
        transform.Rotate(deltaEuler);
    }
}