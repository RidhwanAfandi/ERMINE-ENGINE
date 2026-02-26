using System;
using ErmineEngine;

public class GameManager : MonoBehaviour
{
    public GameObject player;
    private Vector3 startPos;
    void Start()
    {
        player = GameObject.Find("Player");
        startPos = player.transform.position;
    }

    void Update()
    {
        //if (player.transform.position.y < -ff)
        //{
        //    player.transform.position = startPos;   
        //}
    }
}

