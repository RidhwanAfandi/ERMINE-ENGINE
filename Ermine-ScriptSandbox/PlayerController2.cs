using ErmineEngine;
using System;

public class PlayerController2 : MonoBehaviour
{
    private Transform cam;
    private AudioComponent audioComp;
    //private Animator anim;

    public float mouseHorSens = 0.1f;
    public float mouseVertSens = 0.1f;

    public float moveSpeed = 5f;
    public float jumpspeed = 5f;

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
    private float interactTimer = 0f;

    void Start()
    {
        cam = GameObject.Find("Main Camera").GetComponent<Transform>();
        //anim = GameObject.Find("PlayerAnim").GetComponent<Animator>();
        //HandleCameraLerp();
        audioComp = GetComponent<AudioComponent>();
        if (audioComp == null)
            Console.WriteLine("Warning: No AudioComponent found on player!");

        Cursor.lockState = Cursor.CursorLockState.Confined;
    }

    void Update()
    {
        HandleInput();
        HandleLook();
        HandleCameraLerp();
        HandleFootstepAudio();
        HandleInteract();
        UpdateAudioListener();
    }

    private void HandleInput()
    {
        move = Vector3.zero;
        movementKeyPressed = false;

        if (Input.GetKeyDown(KeyCode.W)) { move += transform.forward; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.S)) { move += -transform.forward; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.A)) { move += transform.right; movementKeyPressed = true; }
        if (Input.GetKeyDown(KeyCode.D)) { move += -transform.right; movementKeyPressed = true; }

        if (move.SqrMagnitude > 0f)
            move = move.normalized * moveSpeed * Time.deltaTime;

        Vector3 newPos = transform.position + new Vector3(move.x, 0, move.z);

        // Request jump
        if (isGrounded == true && Input.GetKeyDown(KeyCode.Space) && !isKeyJump)
        {
            isKeyJump = true;
            isGrounded = false; // prevent double jump
            GlobalAudio.PlaySFX("Jump");
            Physics.Jump((ulong)gameObject.GetInstanceID(), jumpspeed);
        }

        transform.position = newPos;

        // Sync physics collider
        Physics.SetPosition((ulong)gameObject.GetInstanceID(), newPos);

        
    }

    private void HandleLook()
    {
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
        float targetY = isCrouching ? camCrouchY : camDefaultY*100f;
        Vector3 camPos = cam.position;
        camPos.y = Mathf.Lerp(camPos.y, targetY, Time.deltaTime * crouchLerpSpeed);
        cam.position = camPos;
    }

    private void UpdateAudioListener()
    {
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
                audioComp.shouldPlay = true;
                footstepTimer = 0f;
            }
        }
    }

    private void HandleInteract()
    {
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

                if (obj.name == "Switch1" && interactTimer >= 1f)
                {
                    // Play switch audio here Kai
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch1)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch1 = true;
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch1 = false;
                    }
                    interactTimer = 0f;
                }
                if (obj.name == "Switch2" && interactTimer >= 1f)
                {
                    // Play switch audio here Kai
                    GlobalAudio.PlaySFX("SwitchOn");

                    if (!flipSwitch2)
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y + 1.51f, obj.transform.position.z);
                        flipSwitch2 = true;
                    }
                    else
                    {
                        obj.transform.scale = new Vector3(obj.transform.scale.x, obj.transform.scale.y, obj.transform.scale.z * -1f);
                        obj.transform.position = new Vector3(obj.transform.position.x, obj.transform.position.y - 1.51f, obj.transform.position.z);
                        flipSwitch2 = false;
                    }
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

                    obj.SetActive(false);
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
        //anim.SetBool("IsGrounded", isGrounded);
        //anim.SetBool("IsMoving", movementKeyPressed);
    }

    //private ulong GetEntityID(RaycastHit hit)
    //{
    //    var field = typeof(RaycastHit).GetField(
    //        "entityID",
    //        System.Reflection.BindingFlags.Public |
    //        System.Reflection.BindingFlags.Instance);

    //    return (ulong)field.GetValue(hit);
    //}

    void OnCollisionEnter(Collision col)
    {
        if (col.gameObject.name.Contains("floor"))
        {
            isGrounded = true;
            isKeyJump = false;
        }
    }
    void OnCollisionStay(Collision col)
    {
        if (col.gameObject.name.Contains("floor"))
        {
            isGrounded = true;
        }
    }
    void OnCollisionExit(Collision col)
    {
        if (col.gameObject.name.Contains("floor"))
        {
            isGrounded = false;
        }
    }
}
