using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public class UIImage : Component
    {
        public Vector3 position
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }
    }
}