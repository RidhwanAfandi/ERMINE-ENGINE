using ErmineEngine;

public class LoadingScreen : MonoBehaviour
{
    public string nextScenePath = "../Resources/Scenes/m4-test_copy_copy.scene";
    public float displayDuration = 3.0f;

    private float elapsedTime = 0f;
    private bool finished = false;

    void Update()
    {
        if (finished) return;

        elapsedTime += Time.deltaTime;

        if (elapsedTime >= displayDuration)
        {
            finished = true;
            Debug.Log("[LoadingScreen] Loading next scene: " + nextScenePath);
            SceneManager.LoadScene(nextScenePath);
        }
    }
}
