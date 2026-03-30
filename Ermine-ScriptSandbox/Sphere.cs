using ErmineEngine;
using System;

public class Sphere : MonoBehaviour
{
    private static readonly Vector3 ParkedPosition = new Vector3(0.0f, -1000.0f, 0.0f);

    public Vector3 direction;
    public float speed = 20.0f;

    private float timeAlive = 1.0f;
    private Material materialComponent;
    private bool flickerEnabled;

    private void Start()
    {
        ResolveMaterial();
    }

    public void Launch(Vector3 spawnPosition, Quaternion spawnRotation, Vector3 shootDirection)
    {
        ResolveMaterial();
        gameObject.SetActive(true);
        transform.position = spawnPosition;
        transform.rotation = spawnRotation;
        direction = shootDirection;
        timeAlive = 1.0f;
        SetFlicker(false);
        Physics.SetPosition((ulong)gameObject.GetInstanceID(), transform.position);
    }

    public void Deactivate()
    {
        timeAlive = 0.0f;
        direction = Vector3.zero;
        SetFlicker(false);
        transform.position = ParkedPosition;
        Physics.SetPosition((ulong)gameObject.GetInstanceID(), transform.position);
        gameObject.SetActive(false);
    }

    private void Update()
    {
        timeAlive -= Time.deltaTime;
        transform.position -= direction * speed * Time.deltaTime;
        SetFlicker(timeAlive <= 0.5f);

        Physics.SetPosition((ulong)gameObject.GetInstanceID(), transform.position);
        if (timeAlive < 0.0f)
        {
            Deactivate();
        }

    }

    void OnCollisionEnter(Collision col)
    {
        if (Physics.CheckMotionType((ulong)col.gameObject.GetInstanceID()) == 0
            && !col.gameObject.name.Contains("Bars")
            && !col.gameObject.name.Contains("gate")
            && !col.gameObject.name.Contains("BARS")
            && !col.gameObject.name.Contains("Fence")
            ) //static obj
        {
            Debug.Log("Sphere: Hit static object " + col.gameObject.name + " (" + col.gameObject.GetInstanceID() + ")");
            Debug.Log(Physics.CheckMotionType((ulong)col.gameObject.GetInstanceID()));
            
            // Try to find the exact contact point via raycast for better accuracy
            Vector3 explosionPos = transform.position;
            RaycastHit hit;
            // Raycast in the direction of movement to find the surface
            if (Physics.Raycast(transform.position + direction * 2.0f, -direction, out hit, 4.0f))
            {
                explosionPos = hit.point;
            }

            SpawnExplosionLayers(explosionPos);

            Deactivate();
        }
        /*
        Debug.Log("Yes me lord? : " + gameObject.name);

        Debug.Log("Jobs done : " + col.gameObject.name);
        if(col.gameObject.name != "Player")
        {
            col.gameObject.transform.position = new Vector3(0, 30, 0);

            Physics.SetPosition((ulong)col.gameObject.GetInstanceID(), col.gameObject.transform.position);
        }
        */
    }

    private void SpawnExplosionLayers(Vector3 position)
    {
        GameObject coreExplosion = Prefab.Instantiate("../Resources/Prefabs/ParticleExplosion.prefab");
        if (coreExplosion != null)
        {
            coreExplosion.transform.position = position;
        }

        GameObject smokeExplosion = Prefab.Instantiate("../Resources/Prefabs/ParticleExplosionSmoke.prefab");
        if (smokeExplosion != null)
        {
            smokeExplosion.transform.position = position;
        }
    }

    private void ResolveMaterial()
    {
        if (materialComponent == null && gameObject != null)
        {
            materialComponent = gameObject.GetComponent<Material>();
        }
    }

    private void SetFlicker(bool enabled)
    {
        ResolveMaterial();
        if (materialComponent == null || flickerEnabled == enabled)
        {
            return;
        }

        materialComponent.flickerEmissive = enabled;
        flickerEnabled = enabled;
    }
}
