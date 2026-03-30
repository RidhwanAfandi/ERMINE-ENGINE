using ErmineEngine;
using System;

public class PlayerController2 : MonoBehaviour
{
    private Transform cam;
    private AudioComponent audioComp;
    private Animator anim;

    public float mouseHorSens = 0.1f;
    public float mouseVertSens = 0.1f;

    public float moveSpeed = 5f;
    public float jumpspeed = 5f;
    public float walkSpeed = 8f;
    public float sprintSpeed = 14f;
    private bool isSprinting = false;

    public float crouchLerpSpeed = 6f;

    private bool isGrounded = true;
    private bool isCrouching = false;
    private bool isKeyJump = false;

    private float xRotation = 0f;
    public float camDefaultY = 4.5f;
    private float camCrouchY = 50f;

    private bool movementKeyPressed = false;
    private Vector3 move;
    private Vector2 lookInput;

    private float footstepTimer = 0f;
    private float footstepInterval = 0.5f;

    private float minPitch = -1.5f;
    private float maxPitch = 1.5f;

    public float jumpHeight = 2f;

    private float interactRange = 5f;

    private bool flipSwitch1 = false;
    private bool flipSwitch2 = false;
    private bool flipSwitch3 = false;
    private float interactTimer = 0f;

    private GameObject activeClueMessage = null;
    private float clueMessageTimer = 0f;
    private float clueMessageDuration = 5f;

    private int gearKeysPickedUp = 0;

    private GameObject activeSubtitle = null;
    private float subtitleTimer = 0f;
    private float currentSubtitleDuration = 5f;

    private GameObject bgDarken;
    private bool isPaused = false;

    void Start()
    {
        GameObject camObj = GameObject.Find("Main Camera");
        if (camObj != null)
            cam = camObj.GetComponent<Transform>();
        else
            Console.WriteLine("Warning: Main Camera not found!");

        GameObject animObj = GameObject.Find("PlayerAnim");
        if (animObj != null)
            anim = animObj.GetComponent<Animator>();
        if (anim == null)
            Console.WriteLine("Warning: Animator not found on PlayerAnim.");
        //HandleCameraLerp();
        audioComp = GetComponent<AudioComponent>();
        if (audioComp == null)
            Console.WriteLine("Warning: No AudioComponent found on player!");

        Cursor.lockState = Cursor.CursorLockState.Confined;

        bgDarken = GameObject.Find("BackgroundDarken");
    }

    void Update()
    {
        HandleInput();
        HandleLook();
        HandleCameraLerp();
        HandleFootstepAudio();
        HandleInteract();
        UpdateAudioListener();
        HandleAnimUpdate();
        HandleClueMessageTimer();
        HandleSubtitleTimer();
    }

    private void HandleInput()
    {
        if (isPaused) return;
        move = Vector3.zero;
        movementKeyPressed = false;

        if (Input.GetKeyDown(KeyCode.W)) { move += transform.forward; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.S)) { move += -transform.forward; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.A)) { move += transform.right; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.D)) { move += -transform.right; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.LeftShift))
        {
            moveSpeed = sprintSpeed; // Sprint while holding
            //Need to speed up walk animaiton
        }
        else
        {
            moveSpeed = walkSpeed;
        }

        if (move.SqrMagnitude > 0f)
            move = move.normalized * moveSpeed * Time.deltaTime;

        Vector3 newPos = transform.position + new Vector3(move.x, 0, move.z);

        // Request jump
        if (isGrounded == true && Input.GetKeyDown(KeyCode.Space) && !isKeyJump)
        {
            isKeyJump = true;
            isGrounded = false;
            GlobalAudio.PlaySFXWithReverbSimple("Jump", wetLevel: -6.0f, decayTime: 3.5f, earlyDelay: 0.001f, lateDelay: 0.1f);
            Physics.Jump((ulong)gameObject.GetInstanceID(), jumpspeed);
        }

        transform.position = newPos;

        // Sync physics collider
        Physics.SetPosition((ulong)gameObject.GetInstanceID(), newPos);

        
    }

    private void HandleLook()
    {
        if (cam == null) return;
        if (isPaused) return;

        lookInput = Input.mousePositionDelta;

        float mouseX = -lookInput.x * mouseHorSens * Time.deltaTime;
        float mouseY = lookInput.y * mouseVertSens * Time.deltaTime;

        // rotate player horizontally
        transform.Rotate(Vector3.up * mouseX);
        Physics.SetRotationQuat((ulong)gameObject.GetInstanceID(), transform.rotation);

        // clamp vertical look
        xRotation -= mouseY;
        xRotation = Mathf.Clamp(xRotation, minPitch, maxPitch);

        cam.rotation = Quaternion.Euler(xRotation, 0f, 0f);
    }

    private void HandleCameraLerp()
    {
        if (cam == null) return;

        float targetY = isCrouching ? camCrouchY : camDefaultY*100f;
        Vector3 camPos = cam.position;
        camPos.y = Mathf.Lerp(camPos.y, targetY, Time.deltaTime * crouchLerpSpeed);
        cam.position = camPos;
    }

    private void UpdateAudioListener()
    {
        if (cam == null) return;

        // Update listener position to camera/player position
        Vector3 listenerPos = new Vector3(
            transform.position.x,
            cam.position.y,  // Use camera height for better vertical audio
            transform.position.z
        );
        
        // Update listener orientation to match camera direction
        AudioListener.SetAttributes(
            transform.position,  // Player's actual position
            Vector3.zero,
            cam.forward,
            cam.up
        );
    }
    private void HandleFootstepAudio()
    {
        if (audioComp == null) return;

        footstepTimer += Time.deltaTime;

        if (movementKeyPressed && footstepTimer >= footstepInterval)
        {
            if (!audioComp.isPlaying)
            {
                // Apply reverb to footstep audio (minimal pre-delay to avoid echo)
                audioComp.SetReverb(wetLevel: -1.0f, dryLevel: 0.0f, decayTime: 0.5f);
                audioComp.shouldPlay = true;
                footstepTimer = 0f;
            }
        }
    }

    private void HandleInteract()
    {
        if (cam == null) return;
        if (isPaused) return;

        interactTimer += Time.deltaTime;

        if (Input.GetKeyDown(KeyCode.E))
        {
            RaycastHit hit;

            bool hitSomething = Physics.Raycast(
                gameObject.transform.position + new Vector3(0, cam.position.y * 0.01f, 0),
                cam.forward,
                out hit,
                interactRange
            );

            if (!hitSomething) return;

            GameObject obj = hit.transform.gameObject;
            Debug.LogError("hit");
            if (obj!=null)
            {
                //GameObject obj = GameObject.FromEntityID(id);
                //Debug.Log("You are looking at: " + obj.name);

                // Switch -> ElectricFence 1
                if (obj.name == "Switch" && interactTimer >= 1f)
                {
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch1)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch1 = true;
                        GameObject fence1 = GameObject.Find("ElectricFenc 1");
                        if (fence1 != null) fence1.SetActive(false);
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch1 = false;
                        GameObject fence1 = GameObject.Find("ElectricFenc 1");
                        if (fence1 != null) fence1.SetActive(true);
                    }
                    interactTimer = 0f;
                }
                // Switch (1) -> ElectricFence 2
                if (obj.name == "Switch (1)" && interactTimer >= 1f)
                {
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch2)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch2 = true;
                        GameObject fence2 = GameObject.Find("ElectricFenc 2");
                        if (fence2 != null) fence2.SetActive(false);
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch2 = false;
                        GameObject fence2 = GameObject.Find("ElectricFenc 2");
                        if (fence2 != null) fence2.SetActive(true);
                    }
                    interactTimer = 0f;
                }
                // Switch (2) -> ElectricFence 3 and 4
                if (obj.name == "Switch (2)" && interactTimer >= 1f)
                {
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch3)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch3 = true;
                        GameObject fence3 = GameObject.Find("ElectricFenc 3");
                        if (fence3 != null) fence3.SetActive(false);
                        GameObject fence4 = GameObject.Find("ElectricFenc 4");
                        if (fence4 != null) fence4.SetActive(false);
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch3 = false;
                        GameObject fence3 = GameObject.Find("ElectricFenc 3");
                        if (fence3 != null) fence3.SetActive(true);
                        GameObject fence4 = GameObject.Find("ElectricFenc 4");
                        if (fence4 != null) fence4.SetActive(true);
                    }
                    interactTimer = 0f;
                }

                // m4-test-copy_copy.scene switches (legacy naming)
                // Switch1 -> ElectricFence (1)
                if (obj.name == "Switch1" && interactTimer >= 1f)
                {
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch1)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch1 = true;
                        GameObject fence = GameObject.Find("ElectricFenc 1");
                        if (fence != null) fence.SetActive(false);
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch1 = false;
                        GameObject fence = GameObject.Find("ElectricFenc 1");
                        if (fence != null) fence.SetActive(true);
                    }
                    interactTimer = 0f;
                }
                // Switch2 -> ElectricFence
                if (obj.name == "Switch2" && interactTimer >= 1f)
                {
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch2)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch2 = true;
                        GameObject fence = GameObject.Find("ElectricFenc");
                        if (fence != null) fence.SetActive(false);
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch2 = false;
                        GameObject fence = GameObject.Find("ElectricFenc");
                        if (fence != null) fence.SetActive(true);
                    }
                    interactTimer = 0f;
                }

                if (obj.name == "Paper1" && interactTimer > 1f)
                {
                    // Collect book
                    GlobalAudio.PlaySFX("BookPickUp");
                    PlayVoiceWithSubtitle("Clue1", 7f);
                    GameObject msg = GameObject.Find("Clue1");
                    GameObject hint1 = GameObject.Find("Hint1");
                    GameObject hint2 = GameObject.Find("Hint2");


                    if (msg != null && hint1 != null && hint2 != null)
                    {
                        msg.SetActive(true);
                        hint1.SetActive(false);
                        hint2.SetActive(true);
                        bgDarken.SetActive(true);
                        activeClueMessage = msg;
                        clueMessageTimer = 0f;

                        isPaused = true;
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;
                }
                if (obj.name == "Paper2" && interactTimer > 1f)
                {
                    // Collect book
                    GlobalAudio.PlaySFX("BookPickUp");
                    PlayVoiceWithSubtitle("Clue3", 10f);
                    GameObject msg = GameObject.Find("Clue2");
                    GameObject hint2 = GameObject.Find("Hint2");
                    GameObject hint3 = GameObject.Find("Hint3");

                    if (msg != null)
                    {
                        msg.SetActive(true);
                        hint2.SetActive(false);
                        hint3.SetActive(true);
                        bgDarken.SetActive(true);
                        activeClueMessage = msg;
                        clueMessageTimer = 0f;

                        isPaused = true;
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;
                }
                if (obj.name == "Paper3" && interactTimer > 1f)
                {
                    // Collect book
                    GlobalAudio.PlaySFX("BookPickUp");
                    PlayVoiceWithSubtitle("Clue2", 9f);
                    GameObject msg = GameObject.Find("Clue3");

                    if (msg != null)
                    {
                        msg.SetActive(true);
                        bgDarken.SetActive(true);
                        activeClueMessage = msg;
                        clueMessageTimer = 0f;

                        isPaused = true;
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;
                }
                if (obj.name == "Paper4" && interactTimer > 1f)
                {
                    // Collect book
                    GlobalAudio.PlaySFX("BookPickUp");
                    PlayVoiceWithSubtitle("Clue4", 7f);
                    GameObject msg = GameObject.Find("Clue4");

                    if (msg != null)
                    {
                        msg.SetActive(true);
                        bgDarken.SetActive(true);
                        activeClueMessage = msg;
                        clueMessageTimer = 0f;

                        isPaused = true;
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;

                }
                if (obj.name == "Paper5" && interactTimer > 1f)
                {
                    // Collect book
                    GlobalAudio.PlaySFX("BookPickUp");
                    PlayVoiceWithSubtitle("Locks", 6f);
                    GameObject msg = GameObject.Find("Clue5");

                    if (msg != null)
                    {
                        msg.SetActive(true);
                        bgDarken.SetActive(true);
                        activeClueMessage = msg;
                        clueMessageTimer = 0f;

                        isPaused = true;
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;

                }
                if (obj.name == "Book" && interactTimer > 1f)
                {
                    // Collect book
                    GlobalAudio.PlaySFX("BookPickUp");
                    GameObject msg = GameObject.Find("BookCollected");

                    if (msg != null)
                    {
                        msg.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 2f, obj.transform.position.z);
                        msg.transform.rotation = Quaternion.RotateTowards(transform.rotation, msg.transform.rotation, 0f);
                        msg.transform.rotation *= Quaternion.Euler(0f, 135f, 0f);
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;
                }

                if (obj.name == "GearKeyPrefab" && interactTimer > 1f)
                {
                    // Collect key
                    //GlobalAudio.PlaySFX("BookPickUp");
                    UnlockDoor.I.IncrementKeys();
                    gearKeysPickedUp++;

                    if (gearKeysPickedUp == 1)
                    {
                        PlayVoiceWithSubtitle("Key1", 6f);
                    }
                    else if (gearKeysPickedUp == 2)
                    {
                        PlayVoiceWithSubtitle("Key2", 5f);
                    }

                    //obj.SetActive(false);
                    interactTimer = 0f;
                }

                if (obj.name == "ComputerDoorUnlock1" && interactTimer > 1f)
                {
                    // Check if already unlocked
                    if (UnlockDoor.I.IsComputer1Unlocked())
                    {
                        Debug.Log("Computer 1 is already unlocked!");
                        // Optional: Play a sound or show a message
                    }
                    else if (UnlockDoor.I.GetKeyCount() <= 0)
                    {
                        Debug.Log("Need a key to unlock this computer!");
                        // Optional: Play a sound or show a message indicating no keys
                    }
                    else
                    {
                        // Unlock computer 1
                        GlobalAudio.PlaySFX("BookPickUp");
                        UnlockDoor.I.UnlockComputer1();

                        // Optional: Add visual feedback here (change color, play animation, etc.)
                        Debug.Log("Computer 1 activated!");
                    }

                    interactTimer = 0f;
                }

                if (obj.name == "ComputerDoorUnlock2" && interactTimer > 1f)
                {
                    // Check if already unlocked
                    if (UnlockDoor.I.IsComputer2Unlocked())
                    {
                        Debug.Log("Computer 2 is already unlocked!");
                        // Optional: Play a sound or show a message
                    }
                    else if (UnlockDoor.I.GetKeyCount() <= 0)
                    {
                        Debug.Log("Need a key to unlock this computer!");
                        // Optional: Play a sound or show a message indicating no keys
                    }
                    else
                    {
                        // Unlock computer 2
                        GlobalAudio.PlaySFX("BookPickUp");
                        UnlockDoor.I.UnlockComputer2();

                        // Optional: Add visual feedback here (change color, play animation, etc.)
                        Debug.Log("Computer 2 activated!");
                    }

                    interactTimer = 0f;
                }
            }
            /*else
            {
                Debug.Log("No valid entity hit.");
            }*/
        }
    }

    void HandleAnimUpdate()
    {
        if (anim == null) return;

        anim.SetBool("IsGrounded", isGrounded);
        anim.SetBool("IsMoving", movementKeyPressed);
    }

    //private ulong GetEntityID(RaycastHit hit)
    //{
    //    var field = typeof(RaycastHit).GetField(
    //        "entityID",
    //        System.Reflection.BindingFlags.Public |
    //        System.Reflection.BindingFlags.Instance);

    //    return (ulong)field.GetValue(hit);
    //}

    private void HandleClueMessageTimer()
    {
        if (activeClueMessage != null)
        {
            clueMessageTimer += Time.deltaTime;

            if (clueMessageTimer >= clueMessageDuration)
            {
                activeClueMessage.SetActive(false);
                bgDarken.SetActive(false);
                activeClueMessage = null;
                clueMessageTimer = 0f;

                isPaused = false;
            }
        }
    }

    private void HandleSubtitleTimer()
    {
        if (activeSubtitle != null)
        {
            subtitleTimer += Time.deltaTime;

            if (subtitleTimer >= currentSubtitleDuration)
            {
                activeSubtitle.SetActive(false);
                activeSubtitle = null;
                subtitleTimer = 0f;
            }
        }
    }

    private void ShowSubtitle(string subtitleObjectName, float duration)
    {
        GameObject msg = GameObject.Find(subtitleObjectName + "Sub");

        if (msg != null)
        {
            if (activeSubtitle != null)
                activeSubtitle.SetActive(false);

            msg.SetActive(true);
            activeSubtitle = msg;
            subtitleTimer = 0f;
            currentSubtitleDuration = duration;
        }
    }

    private void PlayVoiceWithSubtitle(string voiceName, float duration)
    {
        GlobalAudio.PlayVoice(voiceName);
        ShowSubtitle(voiceName, duration);
    }

    void OnCollisionEnter(Collision col)
    {
        if (col.gameObject.name.Contains("floor") || col.gameObject.name.Contains("Rotating"))
        {
            isGrounded = true;
            isKeyJump = false;
        }
    }
    void OnCollisionStay(Collision col)
    {
        if (col.gameObject.name.Contains("floor") || col.gameObject.name.Contains("Rotating"))
        {
            isGrounded = true;
        }
    }
    void OnCollisionExit(Collision col)
    {
        if (col.gameObject.name.Contains("floor") || col.gameObject.name.Contains("Rotating"))
        {
            isGrounded = false;
        }
    }
}
