using ErmineEngine;

public class Sphere : MonoBehaviour
{
    public Vector3 direction;
    public float speed = 20.0f;

    private float timeAlive = 1.0f;

    private void Start()
    {
    }

    private void Update()
    {
        timeAlive -= Time.deltaTime;
        transform.position -= direction * speed * Time.deltaTime;

        Physics.SetPosition((ulong)gameObject.GetInstanceID(), transform.position);
        if (timeAlive < 0.0f)
        {
            Physics.RemovePhysic((ulong)gameObject.GetInstanceID());
            GameObject.Destroy(gameObject);
        }

    }

    void OnCollisionEnter(Collision col)
    {
        if (Physics.CheckMotionType((ulong)col.gameObject.GetInstanceID()) == 0
            && !col.gameObject.name.Contains("Bars")
            && !col.gameObject.name.Contains("gate")) //static obj
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

            // Spawn explosion effect on contact
            GameObject explosion = Prefab.Instantiate("../Resources/Prefabs/ParticleExplosion.prefab");
            if (explosion != null)
            {
                explosion.transform.position = explosionPos;
            }

            timeAlive = 0f;
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
}