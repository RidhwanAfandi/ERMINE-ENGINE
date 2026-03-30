using ErmineEngine;
using System.Resources;

public class CutsceneVideo: MonoBehaviour
{
    public string nextSceneName = "../Resources/Scenes/m4-test_copy_copy.scene";

    // TEMP FIX PARAMETER (set video duration here in seconds)
    public float videoDurationSeconds = 16f;

    public string VideoFileName = "intro_cinematic.mpeg";

    public float assumedFPS = 60;

    // How many seconds before the end to start fading audio
    private const float kFadeDuration = 2.0f;

    private bool finished = false;
    private float elapsedTime = 0f;
    private bool fadingAudio = false;

    private GameObject blackScreen;

    void Start()
    {
        
        if (VideoManager.Load(VideoFileName, "../Resources/Videos/" + VideoFileName, false))
        {
            VideoManager.SetCurrent(VideoFileName);
            VideoManager.SetFitMode(VideoFitMode.StretchToFill);
            VideoManager.SetRenderEnabled(true);
            VideoManager.Play();
        }
        else
        {
            Debug.LogError("CutsceneVideo: Failed to load video '" + VideoFileName + "'.");
        }

        blackScreen = transform.GetChild(0).gameObject;
    }

    void Update()
    {
        if (finished) return;

        elapsedTime += Time.deltaTime;

        if (elapsedTime >= 0.2f && blackScreen.activeSelf)
        {
            blackScreen.SetActive(false);
        }

        // Begin audio fade before the video ends
        float fadeStart = videoDurationSeconds - kFadeDuration;
        if (elapsedTime >= fadeStart && !fadingAudio)
            fadingAudio = true;

        if (fadingAudio)
        {
            float fadeProgress = (elapsedTime - fadeStart) / kFadeDuration;
            float vol = 1.0f - System.Math.Min(fadeProgress, 1.0f);
            VideoManager.SetAudioVolume(VideoFileName, vol);
        }

        if (elapsedTime >= videoDurationSeconds)
        {
            Debug.Log("[CutsceneVideo] Timeout reached (" + videoDurationSeconds + "s)");

            finished = true;
            blackScreen.SetActive(true);
            VideoManager.Stop();
            VideoManager.Free(VideoFileName);
            SceneManager.LoadScene(nextSceneName);
            return;
        }

        bool done = !VideoManager.IsPlaying();
        

        if (done)
        {
            Debug.Log("[CutsceneVideo] Video finished!");

            finished = true;

            Debug.Log("[CutsceneVideo] Stopping video");
            VideoManager.Stop();

            Debug.Log("[CutsceneVideo] Freeing video");
            VideoManager.Free(VideoFileName);

            Debug.Log("[CutsceneVideo] Loading next scene: " + nextSceneName);
            SceneManager.LoadScene(nextSceneName);
        }
    }

    void OnDestroy()
    {
        VideoManager.Stop();
        VideoManager.Free(VideoFileName);
    }
}
