using ErmineEngine;
using System;

public class CutscenePlayer : MonoBehaviour
{
    // Configuration
    private float imageDisplayTime = 3.0f;  // Duration each slide is shown (seconds)
    public string nextScenePath = "../Resources/Scenes/m4-test_copy_copy.scene";

    // Slide tracking
    private int currentSlideIndex = 0;  // Which slide we're currently showing (0-based)
    private int totalSlides = 2;        // Total number of cutscene slides
    private float elapsedTime = 0.0f;   // Timer for current slide
    private bool isPlaying = true;      // Is cutscene active?
    
    // References to slide GameObjects (cached for performance)
    private GameObject cutscene1;
    private GameObject cutscene2;

    void Start()
    {
        Debug.Log("[CutscenePlayer] Starting cutscene sequence");

        // Find all cutscene slide entities
        cutscene1 = GameObject.Find("Slide1");
        cutscene2 = GameObject.Find("Slide2");

        // Verify all slides exist
        if (cutscene1 == null) Debug.LogError("[CutscenePlayer] ERROR: Could not find 'Slide1' entity!");
        if (cutscene2 == null) Debug.LogError("[CutscenePlayer] ERROR: Could not find 'Slide2' entity!");

        // Start with slide 1 visible, others hidden
        ShowSlide(0);

        Debug.Log("[CutscenePlayer] Press SPACE or ENTER to skip cutscene");
    }

    void Update()
    {
        if (!isPlaying) return;

        // Accumulate time
        elapsedTime += Time.deltaTime;

        // Check for skip input (SPACE or ENTER)
        if (Input.GetKeyDown(KeyCode.Space) || Input.GetKeyDown(KeyCode.Enter))
        {
            Debug.Log("[CutscenePlayer] Cutscene skipped by player");
            EndCutscene();
            return;
        }

        // Check if current slide duration has elapsed
        if (elapsedTime >= imageDisplayTime)
        {
            // Move to next slide
            currentSlideIndex++;
            elapsedTime = 0.0f;  // Reset timer for next slide

            if (currentSlideIndex >= totalSlides)
            {
                // All slides shown - end cutscene
                Debug.Log("[CutscenePlayer] All slides shown, ending cutscene");
                EndCutscene();
            }
            else
            {
                // Show next slide
                Debug.Log($"[CutscenePlayer] Advancing to slide {currentSlideIndex + 1}/{totalSlides}");
                ShowSlide(currentSlideIndex);
            }
        }
    }

    /// <summary>
    /// Shows only the specified slide, hiding all others.
    /// </summary>
    /// <param name="slideIndex">Zero-based index of the slide to show (0, 1, or 2)</param>
    void ShowSlide(int slideIndex)
    {
        // Hide all slides first
        if (cutscene1 != null) cutscene1.SetActive(false);
        if (cutscene2 != null) cutscene2.SetActive(false);

        // Show only the requested slide
        switch (slideIndex)
        {
            case 0:
                if (cutscene1 != null)
                {
                    cutscene1.SetActive(true);
                    Debug.Log("[CutscenePlayer] Showing Slide 1");
                }
                break;

            case 1:
                if (cutscene2 != null)
                {
                    cutscene2.SetActive(true);
                    Debug.Log("[CutscenePlayer] Showing Slide 2");
                }
                break;

            default:
                Debug.LogWarning($"[CutscenePlayer] Invalid slide index: {slideIndex}");
                break;
        }
    }

    /// <summary>
    /// Ends the cutscene and loads the next scene.
    /// </summary>
    void EndCutscene()
    {
        isPlaying = false;

        Debug.Log($"[CutscenePlayer] Loading next scene: {nextScenePath}");
        
        // Load the gameplay scene
        SceneManager.LoadScene(nextScenePath);
    }

    // Public configuration methods (optional - for runtime changes)
    public void SetSlideDuration(float duration)
    {
        imageDisplayTime = duration;
        Debug.Log($"[CutscenePlayer] Slide duration set to {duration} seconds");
    }

    public void SetNextScene(string scenePath)
    {
        nextScenePath = scenePath;
        Debug.Log($"[CutscenePlayer] Next scene path set to: {scenePath}");
    }
}
