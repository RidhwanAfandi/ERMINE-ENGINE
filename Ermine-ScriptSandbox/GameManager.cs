using ErmineEngine;
using System;
using System.Collections.Generic;

public class GameManager : MonoBehaviour
{
    public static GameManager I;

    public GameObject player;
    private Vector3 startPos;
    private Vector3 respawnPos;
    private List<Vector3> teleportPoints = new List<Vector3>();

    void Awake()
    {
        I = this;
    }
    void Start()
    {
        player = GameObject.Find("Player");
        if (player != null)startPos = player.transform.position;
        respawnPos = startPos;
    }

    
    void Update()
    {
        if (Input.GetKeyDown(KeyCode.Alpha1)) TeleportToPoint(0);
        if (Input.GetKeyDown(KeyCode.Alpha2)) TeleportToPoint(1);
        if (Input.GetKeyDown(KeyCode.Alpha3)) TeleportToPoint(2);
        if (Input.GetKeyDown(KeyCode.Alpha4)) TeleportToPoint(3);
        if (Input.GetKeyDown(KeyCode.Alpha5)) TeleportToPoint(4);
        if (Input.GetKeyDown(KeyCode.Alpha6)) TeleportToPoint(5);
        if (Input.GetKeyDown(KeyCode.Alpha7)) TeleportToPoint(6);
        if (Input.GetKeyDown(KeyCode.Alpha8)) TeleportToPoint(7);
        if (Input.GetKeyDown(KeyCode.Alpha9)) TeleportToPoint(8);
        if (Input.GetKeyDown(KeyCode.Alpha0)) TeleportToPoint(9);
        if (Input.GetKeyDown(KeyCode.Equal)) TeleportToPoint(10);
        if (Input.GetKeyDown(KeyCode.Minus)) TeleportToPoint(11);

        if (player.transform.position.y <= -15f) PlayerRespawn();
    }

    public void PlayerRespawn()
    {
        if (player == null) return;
        player.transform.position = respawnPos;
        Physics.SetPosition((ulong)player.GetInstanceID(), respawnPos);
    }

    public void UpdateRespawnPoint(Vector3 pos)
    {
        respawnPos = pos;
    }

    public void SaveTeleportPoint(Transform t)
    {
        teleportPoints.Add(t.position);
        Debug.Log("Saved teleport point " + teleportPoints.Count + ": " + t.position);
    }
    private void TeleportToPoint(int index)
    {
        if (index >= teleportPoints.Count) return;

        Vector3 pos = teleportPoints[index];

        player.transform.position = pos;
        Physics.SetPosition((ulong)player.GetInstanceID(), pos);
    }
}

