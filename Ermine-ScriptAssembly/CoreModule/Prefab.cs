using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class Prefab
    {
        // resourcePath examples:
        // - "Prefabs/Bullet"           // ".prefab" auto-appended, rooted at Resources/
        // - "Resources/Prefabs/Bullet" // also works
        // - "Resources/Prefabs/Bullet.prefab"
        public static GameObject Instantiate(string resourcePath)
        {
            if (string.IsNullOrEmpty(resourcePath)) return null;
            return Internal_LoadPrefab(resourcePath);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GameObject Internal_LoadPrefab(string resourcePath);
    }
}