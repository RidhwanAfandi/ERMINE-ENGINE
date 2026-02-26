using ErmineEngine;

public class EndElevator : MonoBehaviour
{
    private bool playerIn;

    // Elevator movement settings
    public float eSpeed = 2f;        // POSITIVE = up, NEGATIVE = down
    public float eDuration = 3f;

    // Door movement settings
    public float dSpeed = 1f;
    public float dDuration = 2f;

    private float timer = 0f;
    private Vector3 startPosition;

    // Door variables
    private GameObject elevatorDoor;
    private float doorOpenPos = -6f;
    private float doorClosePos = 8f;
    private bool doorClosed = false;

    public string sceneName = "m4-LEVEL2_AI.scene";

    void Start()
    {
        startPosition = transform.position;
        elevatorDoor = GameObject.Find("ElevatorDoor");
    }

    void OnCollisionEnter(Collision collision)
    {
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
                float doorMovement = Mathf.Lerp(
                    doorOpenPos,
                    doorClosePos,
                    timer / dDuration
                );

                elevatorDoor.transform.position = new Vector3(
                    doorMovement,
                    elevatorDoor.transform.position.y,
                    elevatorDoor.transform.position.z
                );
            }
            else
            {
                elevatorDoor.transform.position = new Vector3(
                    doorClosePos,
                    elevatorDoor.transform.position.y,
                    elevatorDoor.transform.position.z
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
        SceneManager.LoadScene($"../Resources/Scenes/{sceneName}");
    }
}