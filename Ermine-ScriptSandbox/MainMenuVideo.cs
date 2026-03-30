using ErmineEngine;

public class MainMenuVideo : MonoBehaviour
{
    private const string VideoName = "mainmenu_bg";
    private const string VideoPath = "../Resources/Videos/mainmenu.mpeg";

    void Start()
    {
        Cursor.lockState = Cursor.CursorLockState.None;

        if (VideoManager.Load(VideoName, VideoPath, true))
        {
            VideoManager.SetCurrent(VideoName);
            VideoManager.SetFitMode(VideoFitMode.StretchToFill);
            VideoManager.SetRenderEnabled(true);
            VideoManager.Play();
        }
        else
        {
            Debug.LogError("MainMenuVideo: Failed to load video '" + VideoPath + "'.");
        }
    }

    void OnDestroy()
    {
        VideoManager.Stop();
        VideoManager.Free(VideoName);
    }
}
