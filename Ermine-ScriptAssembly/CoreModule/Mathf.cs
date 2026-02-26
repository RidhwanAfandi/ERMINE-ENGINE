using System;

namespace ErmineEngine
{
    public struct Mathf
    {
        #region properties
        public static float Deg2Rad = (float)(Math.PI * 2) / 360;
        public static float Epsilon = Single.Epsilon;
        public static float Infinity = Single.PositiveInfinity;
        public static float NegativeInfinity = Single.NegativeInfinity;
        public static float PI = (float)(Math.PI);
        public static float Rad2Deg = 360 / (float)(Math.PI * 2);
        #endregion

        #region methods

        public static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        public static float Clamp(float val, float min, float max)
        {
            if (val < min)
                return min;
            return val > max ? max : val;
        }

        public static float Sqrt(float f)
        {
            return (float)Math.Sqrt(f);
        }
        public static float Sin(float f)
        {
            return (float)Math.Sin(f);
        }

        public static float Cos(float f)
        {
            return (float)Math.Cos(f);
        }

        #endregion
    }
}