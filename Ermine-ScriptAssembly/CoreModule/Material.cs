using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    /// <summary>
    /// Script wrapper for native Material component.
    /// </summary>
    public class Material : Component
    {
        /// <summary>
        /// Mesh fill amount in [0, 1]. 1 = fully visible, 0 = fully unfilled.
        /// </summary>
        public float fill
        {
            get => Internal_GetFill(this);
            set => Internal_SetFill(this, value);
        }

        public bool flickerEmissive
        {
            get => Internal_GetFlickerEmissive(this);
            set => Internal_SetFlickerEmissive(this, value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float Internal_GetFill(Material self);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFill(Material self, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetFlickerEmissive(Material self);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFlickerEmissive(Material self, bool value);
    }
}
