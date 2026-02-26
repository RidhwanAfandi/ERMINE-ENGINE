using ErmineEngine;
using System;
using System.Collections;

public class PlayerController : MonoBehaviour
{
    private Transform cam;
    private AudioComponent audioComp;

    public float moveSpeed = 5f;
    public float sprintSpeed = 8f;
    public float crouchSpeed = 2f;
    public float jumpHeight = 2f;
    public float gravity = -9.81f;

    public float mouseSensitivity = 0.01f;

    public float crouchLerpSpeed = 6f;

    private bool wasGrounded;
    private bool isGrounded;
    private Vector3 groundNormal = Vector3.up;

    private bool movementKeyPressed = false;

    // Moving-ground tracking (Catlike-style)
    private Rigidbody connectedBody, previousConnectedBody;
    private Vector3 connectionWorldPosition, connectionLocalPosition;
    private Vector3 connectionVelocity;     // platform velocity at our contact point (this frame)

    private Vector2 moveInput;
    private Vector2 lookInput;
    private float xRotation = 0f;
    private float yRotation = 0f;
    private Vector3 velocity;                    // vertical velocity is used; horizontal is per-frame input
    private bool isSprinting;
    private bool isCrouching;
    private float camDefaultY = 200f;
    private float camCrouchY = 50f;

    // Footstep timing
    private float footstepTimer = 0f;
    private float footstepInterval = 0.5f; // Time between footsteps (adjust this!)

    void Start()
    {
        cam = GameObject.Find("Main Camera").GetComponent<Transform>();
        //cam = GetComponent<Transform>();
        audioComp = GetComponent<AudioComponent>();
        if (audioComp == null)
        {
            Console.WriteLine("Warning: No AudioComponent found on player!");
        }
        else
        {
            Console.WriteLine("AudioComponent found successfully!");
        }
    }

    void Update()
    {
        moveInput = Vector2.zero;
        movementKeyPressed = false; // Reset each frame
        //lookInput = Vector2.zero;

        if (Input.GetKeyDown(KeyCode.A))
        {
            moveInput.x = Vector2.left.x;
            movementKeyPressed = true;
        }
        if (Input.GetKeyDown(KeyCode.D))
        {
            moveInput.x = Vector2.right.x;
            movementKeyPressed = true;
        }
        if(Input.GetKeyDown(KeyCode.W))
        {
            moveInput.y = Vector2.up.y;
            movementKeyPressed = true;
        }
        if(Input.GetKeyDown(KeyCode.S))
        {
            moveInput.y = Vector2.down.y;
            movementKeyPressed = true;
        }

        lookInput = Input.mousePositionDelta;

        // Jump
        if (Input.GetKeyDown(KeyCode.Space) && isGrounded)
        {
            velocity.y = Mathf.Sqrt(jumpHeight * -2f * gravity);
            isGrounded = false;
        }

        HandleMovement();
        HandleLook();
        HandleCameraLerp();
        HandleFootstepAudio();
        wasGrounded = isGrounded;
        //if (Input.GetMouseButton(0))
        //{
        //    var projectile = Prefab.Instantiate("../Resources/Prefabs/Sphere.prefab");
        //    if (projectile != null)
        //    {
        //        projectile.transform.position = transform.position + origin;
        //        projectile.transform.rotation = transform.rotation;
        //    }
        //}

        //if (Input.GetMouseButton(1))
        //{
        //    // Swap position with ball and destroy it
        //    GameObject sphere = GameObject.Find("Sphere");
        //    if (sphere == null)
        //        return;
        //    transform.position = sphere.transform.position;
        //    GameObject.Destroy(sphere);
        //}
    }

    private void HandleMovement()
    {
        if (isGrounded && velocity.y < 0f)
            velocity.y = -2f;

        float speed = moveSpeed;
        //if (isSprinting && !isCrouching) speed = sprintSpeed;
        //if (isCrouching) speed = crouchSpeed;

        Vector3 inputWorld = transform.right * -moveInput.x + transform.forward * moveInput.y;

        Vector3 xAxis = Vector3.ProjectOnPlane(transform.right, groundNormal).normalized;
        Vector3 zAxis = Vector3.ProjectOnPlane(transform.forward, groundNormal).normalized;
        Vector3 desiredRelative =
            (xAxis * Vector3.Dot(inputWorld, xAxis) + zAxis * Vector3.Dot(inputWorld, zAxis)).normalized * speed;

        Vector3 horizontalVelocity = connectionVelocity;
        if (desiredRelative.SqrMagnitude > 0f)
            horizontalVelocity += desiredRelative;

        // Move the character in world space using the computed horizontal velocity
        transform.Translate(horizontalVelocity * Time.deltaTime);
        Physics.SetPosition((ulong)gameObject.GetInstanceID(),transform.position);

        velocity.y += gravity * Time.deltaTime;
        transform.Translate(velocity * Time.deltaTime);
    }

    private void HandleFootstepAudio()
    {
        if (audioComp == null) return;

        footstepTimer += Time.deltaTime;

        // Play footstep when a movement key is pressed and player is grounded
        if (movementKeyPressed && footstepTimer >= footstepInterval && !audioComp.isPlaying)
        {
            Console.WriteLine("Playing footstep sound - Key pressed and grounded");
            audioComp.shouldPlay = true;
            footstepTimer = 0f;
        }
        else if (movementKeyPressed && !isGrounded)
        {
            Console.WriteLine("Key pressed but NOT grounded - no footstep");
        }
        else if (!movementKeyPressed)
        {
            // This will spam the console, but helps debug
            // Console.WriteLine("No movement key pressed this frame");
        }
    }

    private void HandleLook()
    {
        float mouseX = -lookInput.x * mouseSensitivity;
        float mouseY = lookInput.y * mouseSensitivity;

        xRotation = Mathf.Clamp(xRotation - mouseY, -80f, 80f);
        yRotation += mouseX;

        cam.rotation = Quaternion.Euler(xRotation, 0f, 0f); // camera child handles pitch
        transform.Rotate(Vector3.up * mouseX); // player object handles yaw
    }

    private void HandleCameraLerp()
    {
        float targetY = isCrouching ? camCrouchY : camDefaultY;
        Vector3 camPos = cam.position;
        camPos.y = Mathf.Lerp(camPos.y, targetY, Time.deltaTime * crouchLerpSpeed);
        cam.position = camPos;
    }

    void CheckGround(Collision col)
    {
        if (col.transform.position.y < transform.position.y - 0.1f)
        {
            isGrounded = true;
        }
    }

    void OnCollisionEnter(Collision col)
    {
        CheckGround(col);
    }

    void OnCollisionStay(Collision col)
    {
        CheckGround(col);
    }

    void OnCollisionExit(Collision col)
    {
        // When losing contact, you are no longer grounded
        isGrounded = false;
    }
}