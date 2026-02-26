/* Start Header ************************************************************************/
/*!
\file       Vector2.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       07/11/2025
\brief      a vector2 structure with functions

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

namespace ErmineEngine
{
    public struct Vector2
    {
        public float x;
        public float y;

        public Vector2(float v)
        {
            x = y = v;
        }

        public Vector2(float x, float y)
        {
            this.x = x;
            this.y = y;
        }

        public static readonly Vector2 zero = new Vector2(0f, 0f);
        public static readonly Vector2 one = new Vector2(1f, 1f);
        public static readonly Vector2 up = new Vector2(0f, 1f);
        public static readonly Vector2 down = new Vector2(0f, -1f);
        public static readonly Vector2 right = new Vector2(1f, 0f);
        public static readonly Vector2 left = new Vector2(-1f, 0f);

        public float Magnitude => (float)System.Math.Sqrt(x * x + y * y);
        public float SqrMagnitude => x * x + y * y;

        public Vector2 Normalized
        {
            get
            {
                float mag = Magnitude;
                return mag > 1e-6f ? this / mag : zero;
            }
        }

        public static float Dot(Vector2 a, Vector2 b) => a.x * b.x + a.y * b.y;

        public static Vector2 operator +(Vector2 a, Vector2 b) => new Vector2(a.x + b.x, a.y + b.y);
        public static Vector2 operator -(Vector2 a, Vector2 b) => new Vector2(a.x - b.x, a.y - b.y);
        public static Vector2 operator -(Vector2 v) => new Vector2(-v.x, -v.y);
        public static Vector2 operator *(Vector2 v, float scalar) => new Vector2(v.x * scalar, v.y * scalar);
        public static Vector2 operator *(float scalar, Vector2 v) => new Vector2(v.x * scalar, v.y * scalar);
        public static Vector2 operator /(Vector2 v, float scalar) => new Vector2(v.x / scalar, v.y / scalar);
        
        public static implicit operator Vector2(Vector3 v) => new Vector2(v.x, v.y);

        public override string ToString() => $"({x:0.###}, {y:0.###})";
    }
}