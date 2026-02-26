using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class SceneManager
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LoadSceneInternal(string scenePath);

        /// <summary>
        /// Loads a scene by file path.
        /// </summary>
        /// <param name="scenePath">Path to the scene file (e.g., "../Resources/Scenes/level.scene")</param>
        public static void LoadScene(string scenePath)
        {
            if (string.IsNullOrEmpty(scenePath))
            {
                Debug.LogError("SceneManager.LoadScene: Scene path is null or empty!");
                return;
            }

            Debug.Log($"SceneManager: Loading scene '{scenePath}'");
            LoadSceneInternal(scenePath);
        }
    }
}
