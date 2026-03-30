using System;
using ErmineEngine;

public class Credits : MonoBehaviour
{
    public float startY = -4.84f;
    public float endY = 5.06f;

    public float speed = 0.5f;            // default scrolling speed
    public float spaceSpeedMultiplier = 3.0f; // speed boost while holding Space

    private UIImage imageComponent;
    private bool finished = false;

    void Start()
    {
        Debug.Log("Start");
        imageComponent = gameObject.GetComponent<UIImage>();

        if (imageComponent == null)
        {
            Debug.LogError("MoveUIImageY: UIImageComponent not found on this GameObject.");
            return;
        }

        Vector3 pos = imageComponent.position;
        pos.y = startY;
        imageComponent.position = pos;
    }

    void Update()
    {
        if (imageComponent == null || finished)
            return;

        float currentSpeed = speed;

        if (Input.GetKeyDown(KeyCode.Space))
        {
            Debug.Log("Space");
            currentSpeed *= spaceSpeedMultiplier;
        }

        Vector3 pos = imageComponent.position;
        pos.y += currentSpeed * Time.deltaTime;

        if (pos.y >= endY)
        {
            pos.y = endY;
            finished = true;
        }

        imageComponent.position = pos;

        if (finished)
        {
            SceneManager.LoadScene($"../Resources/Scenes/mainmenu_video_bg.scene");
        }
    }
}