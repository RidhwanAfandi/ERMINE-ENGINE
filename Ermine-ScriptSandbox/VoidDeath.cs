using ErmineEngine;

public class VoidDeath : MonoBehaviour
{
    void OnCollisionEnter(Collision collision)
    {
        if (collision.gameObject.name == "Player")
        {
            GameManager.I.PlayerRespawn();
        }
    }
}