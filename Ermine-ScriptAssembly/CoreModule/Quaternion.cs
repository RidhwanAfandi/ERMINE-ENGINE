/* Start Header ************************************************************************/
/*!
\file       Quaternion.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       16/09/2025
\brief      a quaternion structure with functions

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Threading;
using static System.Single;

namespace ErmineEngine
{
    public struct Quaternion
    {
        public static readonly Quaternion identity = new Quaternion(0f, 0f, 0f, 1f);

        #region Properties
        public float x;
        public float y;
        public float z;
        public float w;

        public float this[int key]
        {
            get
            {
                switch (key)
                {
                    case 0: return x;
                    case 1: return y;
                    case 2: return z;
                    case 3: return w;
                    default: throw new IndexOutOfRangeException("Quaternion index must be in the range of 0-3");
                }
            }
            set
            {
                switch (key)
                {
                    case 0: x = value; break;
                    case 1: y = value; break;
                    case 2: z = value; break;
                    case 3: w = value; break;
                    default: throw new IndexOutOfRangeException("Quaternion index must be in the range of 0-3");
                }
            }
        }

        public Quaternion normalized
        {
            get
            {
                float mag = (float)Math.Sqrt(x * x + y * y + z * z + w * w);
                if (mag > 1e-6f)
                {
                    return new Quaternion(x / mag, y / mag, z / mag, w / mag);
                }
                return identity;
            }
        }

        public Vector3 eulerAngles
        {
            get
            {
                Vector3 angles;

                double sinrCosp = 2 * (w * x + y * z);
                double cosrCosp = 1 - 2 * (x * x + y * y);
                angles.x = (float)Math.Atan2(sinrCosp, cosrCosp);

                double sinp = Math.Sqrt(1 + 2 * (w * y - x * z));
                double cosp = Math.Sqrt(1 - 2 * (w * y - x * z));
                angles.y = 2 * (float)Math.Atan2(sinp, cosp) - (float)Math.PI / 2;

                double sinyCosp = 2 * (w * z + x * y);
                double cosyCosp = 1 - 2 * (y * y + z * z);
                angles.z = (float)Math.Atan2(sinyCosp, cosyCosp);

                return angles;
            }

            set
            {
                // Create a new quaternion qRotate from Euler angles (in radians)
                Quaternion qRotate = Euler(value.x, value.y, value.z);

                w *= qRotate.w;
                x *= qRotate.x;
                y *= qRotate.y;
                z *= qRotate.z;
            }
        }
        #endregion

        // Constructor
        public Quaternion(float x, float y, float z, float w)
        {
            this.x = x;
            this.y = y;
            this.z = z;
            this.w = w;
        }

        #region Public Methods
        public void Set(float newX, float newY, float newZ, float newW)
        {
            x = newX;
            y = newY;
            z = newZ;
            w = newW;
        }

        public void SetFromToRotation(Vector3 fromDirection, Vector3 toDirection)
        {
            const float EPS = 1e-6f;

            Vector3 f = fromDirection.normalized;
            Vector3 t = toDirection.normalized;

            if (f.SqrMagnitude < EPS || t.SqrMagnitude < EPS)
            {
                x = y = z = 0f;
                w = 1f;
                return;
            }

            float cosTheta = Vector3.Dot(f, t);

            if (cosTheta > 1f - EPS)
            {
                x = y = z = 0f;
                w = 1f;
                return;
            }

            if (cosTheta < -1f + EPS)
            {
                Vector3 axis = Vector3.Cross(Vector3.right, f);
                if (axis.SqrMagnitude < EPS)
                    axis = Vector3.Cross(Vector3.up, f);
                axis = axis.normalized;

                x = axis.x;
                y = axis.y;
                z = axis.z;
                w = 0f;
            }
            else
            {
                Vector3 axis = Vector3.Cross(f, t);
                float s = (float)Math.Sqrt((1f + cosTheta) * 2f);
                float invs = 1f / s;

                x = axis.x * invs;
                y = axis.y * invs;
                z = axis.z * invs;
                w = s * 0.5f;
            }

            float mag = (float)Math.Sqrt(x * x + y * y + z * z + w * w);
            if (mag > EPS)
            {
                x /= mag; y /= mag; z /= mag; w /= mag;
            }
            else
            {
                x = y = z = 0f;
                w = 1f;
            }
        }

        public void SetLookRotation(Vector3 view)
        {
            SetLookRotation(view, Vector3.up);
        }

        public void SetLookRotation(Vector3 view, Vector3 up)
        {
            const float EPS = 1e-6f;

            // Forward
            Vector3 f = view.normalized;
            if (f.SqrMagnitude < EPS)
            {
                x = y = z = 0f;
                w = 1f;
                return;
            }

            Vector3 u = (up.SqrMagnitude < EPS ? Vector3.up : up).normalized;

            Vector3 r = Vector3.Cross(u, f);
            if (r.SqrMagnitude < EPS)
            {
                u = Math.Abs(f.y) < 0.999f ? Vector3.up : Vector3.right;
                r = Vector3.Cross(u, f);
            }

            r = r.normalized;

            u = Vector3.Cross(f, r);

            float m00 = r.x, m01 = u.x, m02 = f.x;
            float m10 = r.y, m11 = u.y, m12 = f.y;
            float m20 = r.z, m21 = u.z, m22 = f.z;

            float tr = m00 + m11 + m22;

            if (tr > 0f)
            {
                float S = (float)Math.Sqrt(tr + 1.0f) * 2f;
                w = 0.25f * S;
                x = (m21 - m12) / S;
                y = (m02 - m20) / S;
                z = (m10 - m01) / S;
            }
            else if (m00 > m11 && m00 > m22)
            {
                float S = (float)Math.Sqrt(1.0f + m00 - m11 - m22) * 2f;
                w = (m21 - m12) / S;
                x = 0.25f * S;
                y = (m01 + m10) / S;
                z = (m02 + m20) / S;
            }
            else if (m11 > m22)
            {
                float S = (float)Math.Sqrt(1.0f + m11 - m00 - m22) * 2f;
                w = (m02 - m20) / S;
                x = (m01 + m10) / S;
                y = 0.25f * S;
                z = (m12 + m21) / S;
            }
            else
            {
                float S = (float)Math.Sqrt(1.0f + m22 - m00 - m11) * 2f;
                w = (m10 - m01) / S;
                x = (m02 + m20) / S;
                y = (m12 + m21) / S;
                z = 0.25f * S;
            }

            float mag = (float)Math.Sqrt(x * x + y * y + z * z + w * w);
            if (mag > EPS)
            {
                x /= mag;
                y /= mag; z /= mag;
                w /= mag;
            }
            else
            {
                x = y = z = 0f;
                w = 1f;
            }
        }

        public void ToAngleAxis(out float angle, out Vector3 axis)
        {
            const float EPS = 1e-6f;

            float qx = x, qy = y, qz = z, qw = w;
            float mag = (float)Math.Sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
            if (mag > EPS)
            {
                float inv = 1.0f / mag;
                qx *= inv;
                qy *= inv; qz *= inv; qw *= inv;
            }
            else
            {
                angle = 0f;
                axis = Vector3.right;
                return;
            }

            if (qw > 1f) qw = 1f;
            else if (qw < -1f) qw = -1f;

            float halfAngle = (float)Math.Acos(qw);
            float s = (float)Math.Sqrt(Math.Max(0.0, 1.0 - qw * qw));

            if (s < EPS)
            {
                axis = new Vector3(1f, 0f, 0f);
                angle = 0f;
            }
            else
            {
                axis = new Vector3(qx / s, qy / s, qz / s);
                angle = halfAngle * 2f * (180f / (float)Math.PI);
            }
        }

        public override string ToString() => $"({x:0.###}, {y:0.###}, {z:0.###}, {w:0.###})";

        #endregion

        #region Static Methods

        public static Quaternion Euler(float x, float y, float z)
        {
            double cr = Math.Cos(x * 0.5f);
            double sr = Math.Sin(x * 0.5f);
            double cp = Math.Cos(y * 0.5f);
            double sp = Math.Sin(y * 0.5f);
            double cy = Math.Cos(z * 0.5f);
            double sy = Math.Sin(z * 0.5f);

            float qw = (float)(cr * cp * cy + sr * sp * sy);
            float qx = (float)(sr * cp * cy - cr * sp * sy);
            float qy = (float)(cr * sp * cy + sr * cp * sy);
            float qz = (float)(cr * cp * sy - sr * sp * cy);

            return new Quaternion(qx, qy, qz, qw);
        }

        public static float Dot(Quaternion a, Quaternion b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        }

        public static Quaternion Lerp(Quaternion a, Quaternion b, float t)
        {
            // Clamp to [0,1]
            if (t <= 0f) return a;
            if (t >= 1f) return b;

            Quaternion qa = a.normalized;
            Quaternion qb = b.normalized;

            float dot = Dot(qa, qb);
            if (dot < 0f)
            {
                qb.x = -qb.x; qb.y = -qb.y; qb.z = -qb.z; qb.w = -qb.w;
            }

            Quaternion result = new Quaternion(
                qa.x + (qb.x - qa.x) * t,
                qa.y + (qb.y - qa.y) * t,
                qa.z + (qb.z - qa.z) * t,
                qa.w + (qb.w - qa.w) * t
            );

            return result.normalized;
        }

        public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
        {
            // Clamp to [0,1]
            if (t <= 0f) return a;
            if (t >= 1f) return b;

            Quaternion qa = a.normalized;
            Quaternion qb = b.normalized;

            // Dot product - the cosine of the angle between the 2 quaternions
            float dot = Dot(qa, qb);

            // If the dot product is negative, negate one quaternion to take the shorter 4D "arc"
            if (dot < 0f)
            {
                qb.x = -qb.x; qb.y = -qb.y; qb.z = -qb.z; qb.w = -qb.w;
                dot = -dot;
            }

            if (dot > 1f) dot = 1f;
            
            // Calculate coefficients
            if (dot > 0.9995f)
            {
                // Very close - use linear interpolation
                Quaternion lerp = new Quaternion(
                    qa.x + t * (qb.x - qa.x),
                    qa.y + t * (qb.y - qa.y),
                    qa.z + t * (qb.z - qa.z),
                    qa.w + t * (qb.w - qa.w)
                );
                return lerp.normalized;
            }

            float theta = (float)Math.Acos(dot);
            float sinTheta = (float)Math.Sin(theta);

            if (sinTheta < 1e-6f)
            {
                Quaternion lerp = new Quaternion(
                    qa.x + t * (qb.x - qa.x),
                    qa.y + t * (qb.y - qa.y),
                    qa.z + t * (qb.z - qa.z),
                    qa.w + t * (qb.w - qa.w)
                );

                return lerp.normalized;
            }

            float coeff0 = (float)Math.Sin((1f - t) * theta) / sinTheta;
            float coeff1 = (float)Math.Sin(t * theta) / sinTheta;

            Quaternion result = new Quaternion(
                coeff0 * qa.x + coeff1 * qb.x,
                coeff0 * qa.y + coeff1 * qb.y,
                coeff0 * qa.z + coeff1 * qb.z,
                coeff0 * qa.w + coeff1 * qb.w
            );

            return result.normalized;
        }

        public static Quaternion RotateTowards(Quaternion from, Quaternion to, float maxDelta)
        {
            const float Rad2Deg = (float)(180.0 / Math.PI);

            // Clamp negative steps to zero
            if (maxDelta <= 0f) return from;

            Quaternion a = from.normalized;
            Quaternion b = to.normalized;

            // Dot product - the cosine of the angle between the 2 quaternions
            float dot = Dot(a, b);

            // If the dot product is negative, negate one quaternion to take the shorter 4D "arc"
            if (dot < 0f)
            {
                b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
                dot = -dot;
            }

            if (dot > 1f) dot = 1f;
            if (dot < -1f) dot = -1f;

            // Remaining angular distance in degrees
            float angle = (float)Math.Acos(dot) * 2f * Rad2Deg;

            // Already there or tiny difference
            if (angle < 1e-6f) return to;

            // If we can reach the target this step, snap to it
            if (maxDelta >= angle) return b;

            // Fraction of the remaining arc to move along
            float t = maxDelta / angle;

            return Slerp(a, b, t);
        }
        #endregion

        public static Quaternion operator *(Quaternion lhs, Quaternion rhs)
        {
            return new Quaternion(
                lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
                lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
                lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
                lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z
            );
        }

        public Quaternion Conjugate
        {
            get => new Quaternion(-x, -y, -z, w);
        }

        public Quaternion Inversefunc
        {
            get
            {
                float magSq = x * x + y * y + z * z + w * w;
                if (magSq > 1e-6f)
                {
                    float invMag = 1f / magSq;
                    return new Quaternion(-x * invMag, -y * invMag, -z * invMag, w * invMag);
                }
                return identity; // fallback
            }
        }
        public static Quaternion Inverse(Quaternion q)
        {
            return q.Inversefunc;
        }

        public Vector3 Rotate(Vector3 v)
        {
            // Convert vector to quaternion (x, y, z, w=0)
            Quaternion qVec = new Quaternion(v.x, v.y, v.z, 0f);

            // rotated = q * v * q^-1
            Quaternion rotated = this * qVec * this.Inversefunc;

            return new Vector3(rotated.x, rotated.y, rotated.z);
        }

        public static Vector3 operator *(Quaternion q, Vector3 v)
        {
            return q.Rotate(v);
        }


    }
}
