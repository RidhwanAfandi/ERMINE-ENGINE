using System;
using ErmineEngine;

public class Sandbox : MonoBehaviour
{
    public float speed = 2.0f;

    [SerializeField] private Vector3 MoveAxis = Vector3.up;

    void Start()
    {
        //Debug.Log("ID: " + GetInstanceID());
        //this.tag = "Box";
        //this.name = "Box2";
        //Debug.Log("Name: " + gameObject.name);
        //Debug.Log("Tag: " + this.tag);
        //Debug.Log("Transform position: " + gameObject.transform.position.ToString());
        //Debug.Log("Transform rotation: " + transform.rotation.ToString());

        //Debug.Log("Changes");
        //GetComponent<PlayerController>().TestFunction();
        //GetComponent<Sandbox2>().TestFunction();

        // Testing new transform API
        Debug.Log("ChildCount: " + transform.childCount); // Script is on parent
        Debug.Log("eulerAngles: " + transform.eulerAngles); // Transform calls on rotation property which is quaternion.eulerAngle
        Debug.Log("Transform Find child transform " + transform.Find("child").GetInstanceID());
        Debug.Log("Transform GetChild by index " + transform.GetChild(0).name);
    }

    void Update()
    {
        //transform.Rotate(MoveAxis * (Time.deltaTime * speed));
        //Debug.Log("Internal Quaternion: " + transform.rotation.ToString());
        //if (Input.GetKeyDown(KeyCode.A))
        //    transform.Translate(new Vector3(-1f,0,0) * Time.deltaTime);
        //if(Input.GetKeyDown(KeyCode.D))
        //    transform.Translate(new Vector3(1f,0f,0f) * Time.deltaTime);
        //if(Input.GetKeyDown(KeyCode.W))
        //    transform.Translate(new Vector3(0f,1f,0) * Time.deltaTime);
        //if(Input.GetKeyDown(KeyCode.S))
        //    transform.Translate(new Vector3(0f,-1f,0) * Time.deltaTime);
    }

    //void OnCollisionEnter(Collision col)
    //{
    //    Debug.Log("Yes me lord? : " + gameObject.name);

    //    Debug.Log("Jobs done : " + col.gameObject.name);
    //    //col.gameObject.transform.position = new Vector3(0,3,0);
    //    Physics.SetPosition((ulong)col.gameObject.GetInstanceID(), new Vector3(0, 3, 0));
    //}
}
