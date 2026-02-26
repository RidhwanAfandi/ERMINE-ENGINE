/* Start Header ************************************************************************/
/*!
\file       Vector3.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       16/09/2025
\brief      a vector3 structure with functions

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

namespace ErmineEngine
{
    public struct Vector3
    {
        public float x;
        public float y;
        public float z;

        public Vector3(float v)
        {
            x = y = z = v;
        }

        public Vector3(float x, float y, float z)
        {
            this.x = x;
            this.y = y;
            this.z = z;
        }

        public Vector3(Vector3 v)
        {
            x = v.x;
            y = v.y;
            z = v.z;
        }

        public static readonly Vector3 zero = new Vector3(0f, 0f, 0f);
        public static readonly Vector3 one = new Vector3(1f, 1f, 1f);
        public static readonly Vector3 up = new Vector3(0f, 1f, 0f);
        public static readonly Vector3 down = new Vector3(0f, -1f, 0f);
        public static readonly Vector3 right = new Vector3(1f, 0f, 0f);
        public static readonly Vector3 left = new Vector3(-1f, 0f, 0f);
        public static readonly Vector3 forward = new Vector3(0f, 0f, 1f);
        public static readonly Vector3 back = new Vector3(0f, 0f, -1f);

        public float Magnitude => (float)System.Math.Sqrt(x * x + y * y + z * z);
        public float SqrMagnitude => x * x + y * y + z * z;

        public Vector3 normalized
        {
            get
            {
                float mag = Magnitude;
                return mag > 1e-6f ? this / mag : zero;
            }
        }

        public static float Dot(Vector3 a, Vector3 b) => a.x * b.x + a.y * b.y + a.z * b.z;
        public static Vector3 Cross(Vector3 a, Vector3 b) =>
        new Vector3(a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
        public static Vector3 ProjectOnPlane(Vector3 vector, Vector3 planeNormal)
        {
            float dot = Dot(vector, planeNormal);
            return vector - planeNormal * dot;
        }

        public static Vector3 operator +(Vector3 a, Vector3 b) => new Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
        public static Vector3 operator -(Vector3 v) => new Vector3(-v.x, -v.y, -v.z);
        public static Vector3 operator *(Vector3 v, float scalar) => new Vector3(v.x * scalar, v.y * scalar, v.z * scalar);
        public static Vector3 operator *(float scalar, Vector3 v) => new Vector3(v.x * scalar, v.y * scalar, v.z * scalar);
        public static Vector3 operator /(Vector3 v, float scalar) => new Vector3(v.x / scalar, v.y / scalar, v.z / scalar);

        public override string ToString() => $"({x:0.###}, {y:0.###}, {z:0.###})";
    }
}