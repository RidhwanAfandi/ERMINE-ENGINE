using ErmineEngine;

public class DisableLightCone : MonoBehaviour
{
    public float timer = 3.0f;
    //private Vector3 oldPos;
    //private Vector3 oldConePos;
    private bool disabled = false;
    private bool orbInside = false;
    private bool currentlyColliding = false;

    private Animator anim;
    private GameObject lightCone = null;

    private void Start()
    {
        //oldPos = transform.position;

        // Find matching LightConeX
        //lightCone = FindMatchingCone();
        //if (lightCone != null)
        //    oldConePos = lightCone.transform.position;

        anim = GameObject.Find("PlayerAnim").GetComponent<Animator>();

        if (transform.childCount > 0)
            lightCone = gameObject.transform.GetChild(0).gameObject;

        //if (lightCone != null)
        //    oldConePos = lightCone.transform.position;
    }

    private void Update()
    {
        currentlyColliding = false;
        //PLS FOR THE LOVE OF GOD FIX THIS
        // Right click to disable light cone if orb is inside
        if (!disabled && orbInside && Input.GetMouseButtonDown(1))
        {
            GameObject sphere = GameObject.Find("Sphere");
            if (sphere != null)
            {
                // Capture world position before the light cone is moved off-world
                Rigidbody rb = GetComponent<Rigidbody>();
                Vector3 shockwavePos = rb != null ? rb.position : transform.position;

                DisableLight();
                Sphere sphereComponent = sphere.GetComponent<Sphere>();
                if (sphereComponent != null)
                    sphereComponent.Deactivate();
                else
                    sphere.SetActive(false);

                // Spawn shockwave ring at the light cone's position
                GameObject shockwave = Prefab.Instantiate("../Resources/Prefabs/LightConeShockwave.prefab");
                if (shockwave != null)
                    shockwave.transform.position = shockwavePos;

                // Play explode animation
                anim.SetTrigger("explode");
            }
        }

        // if u want the lightcone to come back after disabling
        if (disabled)
        {
            timer -= Time.deltaTime;

            if (timer <= 0.0f)
            {
                //RespawnLight();
            }
        }

        if (!currentlyColliding)
            orbInside = false;
    }

    private void DisableLight()
    {
        disabled = true;

        if (lightCone.activeSelf)
        {
            lightCone.transform.position = new Vector3(0, -100, 0);
            //Physics.SetPosition((ulong)lightCone.GetInstanceID(), lightCone.transform.position);
            lightCone.SetActive(false);
        }

        GlobalAudio.StopSFX("LightDamageLoop");
        GlobalAudio.PlaySFX("LightDisable");
    }

    void RespawnLight()
    {
        disabled = false;
        timer = 3.0f;

        lightCone.SetActive(true);

        //// Respawn original light
        //gameObject.transform.position = oldPos;
        //Physics.SetPosition((ulong)gameObject.GetInstanceID(), gameObject.transform.position);

        //// Respawn cone in original place
        //if (lightCone != null)
        //{
        //    lightCone.transform.position = oldConePos;
        //    Physics.SetPosition((ulong)lightCone.GetInstanceID(), lightCone.transform.position);
        //}
    }

    void OnCollisionEnter(Collision col)
    {
        if (col.gameObject.name == "Sphere")
        {
            orbInside = true;
        }
    }

    void OnCollisionStay(Collision col)
    {
        if (col.gameObject.name == "Sphere")
        {
            orbInside = true;
            currentlyColliding = true;
        }
    }

    void OnCollisionExit(Collision col)
    {
        if (col.gameObject.name == "Sphere")
        {
            orbInside = false;
        }
    }

    /*
    private GameObject FindMatchingCone()
    {
        string name = gameObject.name;
        string number = "";

        // Read digits from the end of the string
        for (int i = name.Length - 1; i >= 0; i--)
        {
            char c = name[i];
            if (c >= '0' && c <= '9')
            {
                number = c + number;
            }
            else
            {
                break; // stop when we hit first non-digit
            }
        }

        if (number == "")
            return null; // no trailing number found

        string coneName = "LightCone" + number;
        return GameObject.Find(coneName);
    }*/
}
