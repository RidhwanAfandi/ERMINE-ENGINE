using ErmineEngine;
using System;

public class UpwardsPlatform : MonoBehaviour
{
    public string parentObjName = "UpwardsPlatform";
    public float yHeight = 27f;
    private bool isMoving = false;
    private Vector3 startPos;
    private Vector3 endPos;
    private Transform target;

    private float progress = 0f;
    public float speed = 0.05f;

    void Start()
    {
        target = transform.Find(parentObjName);
        if (target == null) return;
        startPos = target.position;
        endPos = new Vector3(startPos.x, yHeight, startPos.z);
    }

    void Update()
    {
        if (target == null) return;
        if (isMoving && progress < 1f)
        {
            progress += speed * Time.deltaTime;

            float newY = Mathf.Lerp(startPos.y, endPos.y, progress);
            target.position = new Vector3(startPos.x, newY, startPos.z);

            if (progress >= 1f)
            {
                target.position = endPos;
                isMoving = false;
            }
        }
    }

    void OnCollisionEnter(Collider collider)
    {
        if (collider.gameObject.name == "Player")
        {
            isMoving = true;
        }
    }
}