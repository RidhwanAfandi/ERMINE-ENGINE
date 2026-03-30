using ErmineEngine;

public class EndElevator : MonoBehaviour
{
    private bool playerIn;

    // Elevator movement settings
    public float eSpeed = 2f;
    public float eDuration = 3f;

    // Door movement settings
    public float dSpeed = 1f;
    public float dDuration = 2f;

    private float timer = 0f;
    private Vector3 startPosition;

    // Door variables
    private Transform elevatorDoorLeft;
    private Transform elevatorDoorRight;

    private float leftDoorStartX;
    private float rightDoorStartX;

    private float doorOffset = 3f; // how far doors start open

    private bool doorClosed = false;

    public string sceneName = "m4-LEVEL2_AI.scene";

    void Start()
    {
        startPosition = transform.position;

        elevatorDoorLeft = transform.Find("ElevatorDoorLeft");
        elevatorDoorRight = transform.Find("ElevatorDoorRight");

        // Store original positions
        leftDoorStartX = elevatorDoorLeft.position.x;
        rightDoorStartX = elevatorDoorRight.position.x;

        // Offset doors at start
        elevatorDoorLeft.position = new Vector3(
            leftDoorStartX - doorOffset,
            elevatorDoorLeft.position.y,
            elevatorDoorLeft.position.z
        );

        elevatorDoorRight.position = new Vector3(
            rightDoorStartX + doorOffset,
            elevatorDoorRight.position.y,
            elevatorDoorRight.position.z
        );
    }

    void OnCollisionEnter(Collision collision)
    {
        if (collision.gameObject.name != "Player") return;
        Debug.Log("Player entered the elevator");
        playerIn = true;
    }

    void Update()
    {
        if (!playerIn) return;

        // 1. Close the door
        if (!doorClosed)
        {
            timer += Time.deltaTime * dSpeed;

            if (timer <= dDuration)
            {
                float t = timer / dDuration;

                float leftX = Mathf.Lerp(leftDoorStartX - doorOffset, leftDoorStartX, t);
                float rightX = Mathf.Lerp(rightDoorStartX + doorOffset, rightDoorStartX, t);

                elevatorDoorLeft.position = new Vector3(
                    leftX,
                    elevatorDoorLeft.position.y,
                    elevatorDoorLeft.position.z
                );

                elevatorDoorRight.position = new Vector3(
                    rightX,
                    elevatorDoorRight.position.y,
                    elevatorDoorRight.position.z
                );
            }
            else
            {
                // Snap fully closed
                elevatorDoorLeft.position = new Vector3(
                    leftDoorStartX,
                    elevatorDoorLeft.position.y,
                    elevatorDoorLeft.position.z
                );

                elevatorDoorRight.position = new Vector3(
                    rightDoorStartX,
                    elevatorDoorRight.position.y,
                    elevatorDoorRight.position.z
                );

                doorClosed = true;
                timer = 0f;
            }
        }
        // 2. Move the elevator
        else
        {
            timer += Time.deltaTime;

            if (timer <= eDuration)
            {
                float movement = Mathf.Lerp(
                    0f,
                    eSpeed * eDuration,
                    timer / eDuration
                );

                transform.position = startPosition + new Vector3(0f, movement, 0f);
            }
            else
            {
                float finalOffset = eSpeed * eDuration;
                transform.position = startPosition + new Vector3(0f, finalOffset, 0f);

                LoadNextScene();
            }
        }
    }

    void LoadNextScene()
    {
        SceneManager.LoadScene($"../Resources/Scenes/" + sceneName);
    }
}