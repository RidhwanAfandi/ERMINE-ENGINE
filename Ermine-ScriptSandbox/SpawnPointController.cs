using ErmineEngine;

public class SpawnPointController : MonoBehaviour
{
    void Start()
    {
        GameManager.I.SaveTeleportPoint(transform);
    }
    void OnCollisionEnter(Collision col)
    {
        if (col.gameObject.name == "Player")
        {
            GameManager.I.UpdateRespawnPoint(transform.position);
        }
    }
}