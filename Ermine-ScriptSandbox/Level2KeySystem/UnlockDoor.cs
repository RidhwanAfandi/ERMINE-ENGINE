using ErmineEngine;

public class UnlockDoor : MonoBehaviour
{
    public static UnlockDoor I;

    public float doorOpenSpeed = 2f;
    public float doorOpenDistance = 3f;

    private int numOfKeys = 0;
    private bool computer1Unlocked = false;
    private bool computer2Unlocked = false;

    private Transform doorLeft;
    private Transform doorRight;

    private float leftClosedX;
    private float rightClosedX;

    private float timer = 0f;
    public float openDuration = 2f;

    private bool doorUnlocked = false;


    void Awake()
    {
        I = this;
    }

    void Start()
    {
        doorLeft = transform.GetChild(1);
        doorRight = transform.GetChild(0);

        if (doorLeft != null)
        {
            leftClosedX = doorLeft.position.x;
        }
        else
        {
            Debug.Log("FinalGateL not found in the scene.");
        }

        if (doorRight != null)
        {
            rightClosedX = doorRight.position.x;
        }
        else
        {
            Debug.Log("FinalGateR not found in the scene.");
        }
    }

    void Update()
    {
        if (!doorUnlocked) return;

        // Extra safety checks
        if (doorLeft == null || doorRight == null)
        {
            Debug.Log("Door transforms missing. Cannot open door.");
            return;
        }

        timer += Time.deltaTime * doorOpenSpeed;

        if (timer <= openDuration)
        {
            float t = timer / openDuration;

            float leftX = Mathf.Lerp(leftClosedX, leftClosedX - doorOpenDistance, t);
            float rightX = Mathf.Lerp(rightClosedX, rightClosedX + doorOpenDistance, t);

            doorLeft.position = new Vector3(
                leftX,
                doorLeft.position.y,
                doorLeft.position.z
            );

            doorRight.position = new Vector3(
                rightX,
                doorRight.position.y,
                doorRight.position.z
            );
        }
        else
        {
            if (doorLeft != null)
            {
                doorLeft.position = new Vector3(
                    leftClosedX - doorOpenDistance,
                    doorLeft.position.y,
                    doorLeft.position.z
                );
            }

            if (doorRight != null)
            {
                doorRight.position = new Vector3(
                    rightClosedX + doorOpenDistance,
                    doorRight.position.y,
                    doorRight.position.z
                );
            }

            doorUnlocked = false;
        }
    }

    public void IncrementKeys()
    {
        numOfKeys++;
        Debug.Log("Key collected! Total keys: " + numOfKeys);
    }

    private void UnlockDoorBool()
    {
        if (doorLeft == null || doorRight == null)
        {
            Debug.Log("Cannot unlock door because door objects are missing.");
            return;
        }

        doorUnlocked = true;
        GlobalAudio.PlaySFX("OpenGate");
    }

    // Called when player interacts with ComputerDoorUnlock1
    public void UnlockComputer1()
    {
        // Check if player has at least 1 key
        if (numOfKeys <= 0)
        {
            Debug.Log("No keys available! Collect a key first.");
            return;
        }

        // Check if this computer is already unlocked
        if (computer1Unlocked)
        {
            Debug.Log("Computer 1 is already unlocked!");
            return;
        }

        // Use a key and unlock computer 1
        numOfKeys--;
        computer1Unlocked = true;
        Debug.Log("Computer 1 unlocked! Keys remaining: " + numOfKeys);

        GlobalAudio.PlaySFX("InsertKey");
        // Check if door should open
        CheckDoorUnlock();
    }

    // Called when player interacts with ComputerDoorUnlock2
    public void UnlockComputer2()
    {
        // Check if player has at least 1 key
        if (numOfKeys <= 0)
        {
            Debug.Log("No keys available! Collect a key first.");
            return;
        }

        // Check if this computer is already unlocked
        if (computer2Unlocked)
        {
            Debug.Log("Computer 2 is already unlocked!");
            return;
        }

        // Use a key and unlock computer 2
        numOfKeys--;
        computer2Unlocked = true;
        Debug.Log("Computer 2 unlocked! Keys remaining: " + numOfKeys);

        GlobalAudio.PlaySFX("InsertKey");
        // Check if door should open
        CheckDoorUnlock();
    }

    private void CheckDoorUnlock()
    {
        // Only unlock door if BOTH computers are unlocked
        if (computer1Unlocked && computer2Unlocked)
        {
            UnlockDoorBool();
            Debug.Log("Door Unlocked! Both computers activated!");

            GameObject hint = GameObject.Find("Hint4");
            GameObject keyHint = GameObject.Find("Hint5");

            if (hint != null && keyHint != null)
            {
                hint.SetActive(false);
                keyHint.SetActive(true);
            }
        }
    }
    public int GetKeyCount()
    {
        return numOfKeys;
    }

    public bool IsComputer1Unlocked()
    {
        return computer1Unlocked;
    }

    public bool IsComputer2Unlocked()
    {
        return computer2Unlocked;
    }
}