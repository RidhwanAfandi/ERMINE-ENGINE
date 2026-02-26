using ErmineEngine;
using System;
using static ErmineEngine.Physics;

public class FakePlayer : MonoBehaviour
{
    private float verticalVelocity = 0f;
    public float gravity = -9.81f;

    public float groundCheckDistance = 0.5f;

    // Ground state
    public bool isGrounded = false;
    public Rigidbody connectedBody;           // platform you stand on
    private Rigidbody previousConnectedBody;  // platform last frame

    // Platform tracking
    private Vector3 connectionWorldPos;       // player position last frame (world)
    private Vector3 connectionLocalPos;       // player position last frame (local to platform)
    public Vector3 connectionVelocity;        // platform motion this frame

    private void Update()
    {
        BeginPlatformStep();
        GroundCheck();
        UpdatePlatformMotion();
        HandleGravity();
        ApplyVerticalMovement();
    }

    void HandleGravity()
    {
        if (!isGrounded)
            verticalVelocity += gravity * Time.deltaTime;
    }

    // Reset platform tracking at the start of the frame
    private void BeginPlatformStep()
    {
        previousConnectedBody = connectedBody;
        connectedBody = null;
        connectionVelocity = Vector3.zero;
    }

    private void GroundCheck()
    {
        // Raycast from slightly above feet downward
        Vector3 origin = transform.position + Vector3.up * 0.1f;

        if (Physics.Raycast(origin, Vector3.down, out RaycastHit hit, groundCheckDistance))
        {
            
            isGrounded = true;

            // Reset velocity only when moving downward
            if (verticalVelocity < 0f)
                verticalVelocity = 0f;

            // If standing on a rigidbody, treat it as a moving platform
            //connectedBody = hit.;
        }
        else
        {
            isGrounded = false;
        }
    }

    void ApplyVerticalMovement()
    {
        // Move object according to vertical velocity
        transform.position += new Vector3(0f, verticalVelocity, 0f) * Time.deltaTime;
    }
    private void UpdatePlatformMotion()
    {
        Rigidbody rb = connectedBody;

        // If we are still standing on the same platform as last frame
        if (rb != null && rb == previousConnectedBody)
        {
            // Convert last frame's local position back into world space
            Vector3 newWorldPos = TransformPoint(connectionLocalPos, rb.position, rb.rotation);

            // Movement of the platform from last frame to this frame
            Vector3 movement = newWorldPos - connectionWorldPos;

            // Safe deltaTime
            float dt = (Time.deltaTime > 0f) ? Time.deltaTime : 0.0001f;

            // Platform velocity (used by your movement system)
            connectionVelocity = movement / dt;
        }

        // Update tracking positions
        if (rb != null)
        {
            // Store where the player is *now* in world space
            connectionWorldPos = transform.position;

            // Convert the player's world position into the platform's local space
            connectionLocalPos = InverseTransformPoint(transform.position, rb.position, rb.rotation);
        }
        else
        {
            connectionVelocity = Vector3.zero;
        }
    }
    float Max(float a, float b)
    {
        return (a > b) ? a : b;
    }

    Vector3 TransformPoint(Vector3 localPoint, Vector3 worldPosition, Quaternion worldRotation)
    {
        return worldPosition + RotateVector(worldRotation, localPoint);
    }
    Vector3 InverseTransformPoint(Vector3 worldPoint, Vector3 worldPosition, Quaternion worldRotation)
    {
        // Convert world→local by reversing transform
        Quaternion invRot = InverseRotation(worldRotation);
        return RotateVector(invRot, worldPoint - worldPosition);
    }
    Vector3 RotateVector(Quaternion q, Vector3 v)
    {
        // Extract quaternion components
        float x = q.x, y = q.y, z = q.z, w = q.w;

        // Quaternion * Vector3 formula
        float num = x * 2f;
        float num2 = y * 2f;
        float num3 = z * 2f;
        float num4 = x * num;
        float num5 = y * num2;
        float num6 = z * num3;
        float num7 = x * num2;
        float num8 = x * num3;
        float num9 = y * num3;
        float num10 = w * num;
        float num11 = w * num2;
        float num12 = w * num3;

        Vector3 result;
        result.x = (1f - (num5 + num6)) * v.x + (num7 - num12) * v.y + (num8 + num11) * v.z;
        result.y = (num7 + num12) * v.x + (1f - (num4 + num6)) * v.y + (num9 - num10) * v.z;
        result.z = (num8 - num11) * v.x + (num9 + num10) * v.y + (1f - (num4 + num5)) * v.z;

        return result;
    }
    Quaternion InverseRotation(Quaternion q)
    {
        float lengthSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        return new Quaternion(-q.x / lengthSq, -q.y / lengthSq, -q.z / lengthSq, q.w / lengthSq);
    }
}

