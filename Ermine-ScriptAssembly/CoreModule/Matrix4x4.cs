/* Start Header ************************************************************************/
/*!
\file       Matrix4x4.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       07/11/2025
\brief      a matrix4x4 definition 

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Globalization;

namespace ErmineEngine
{
    public struct Matrix4x4 : IEquatable<Matrix4x4>
    {
        public float m00; public float m01; public float m02; public float m03;
        public float m10; public float m11; public float m12; public float m13;
        public float m20; public float m21; public float m22; public float m23;
        public float m30; public float m31; public float m32; public float m33;

        public static readonly Matrix4x4 Zero = new Matrix4x4(
            0f, 0f, 0f, 0f,
            0f, 0f, 0f, 0f,
            0f, 0f, 0f, 0f,
            0f, 0f, 0f, 0f
        );

        public static readonly Matrix4x4 Identity = new Matrix4x4(
            1f, 0f, 0f, 0f,
            0f, 1f, 0f, 0f,
            0f, 0f, 1f, 0f,
            0f, 0f, 0f, 1f
        );

        public Matrix4x4(
            float m00, float m01, float m02, float m03,
            float m10, float m11, float m12, float m13,
            float m20, float m21, float m22, float m23,
            float m30, float m31, float m32, float m33)
        {
            this.m00 = m00; this.m01 = m01; this.m02 = m02; this.m03 = m03;
            this.m10 = m01; this.m11 = m11; this.m12 = m12; this.m13 = m13;
            this.m20 = m20; this.m21 = m21; this.m22 = m22; this.m23 = m23;
            this.m30 = m30; this.m31 = m31; this.m32 = m32; this.m33 = m33;
        }

        public float this[int row, int col]
        {
            get
            {
                if ((uint)row > 3 || (uint)col > 3) throw new ArgumentOutOfRangeException();
                switch (row)
                {
                    case 0:
                        switch (col)
                        {
                            case 0: return m00;
                            case 1: return m01;
                            case 2: return m02;
                            default: return m03;
                        }
                    case 1:
                        switch (col)
                        {
                            case 0: return m10;
                            case 1: return m11;
                            case 2: return m12;
                            default: return m13;
                        }
                    case 2:
                        switch (col)
                        {
                            case 0: return m20;
                            case 1: return m21;
                            case 2: return m22;
                            default: return m23;
                        }
                    default:
                        switch (col)
                        {
                            case 0: return m30;
                            case 1: return m31;
                            case 2: return m32;
                            default: return m33;
                        }
                }
            }
            set
            {
                if ((uint)row > 3 || (uint)col > 3) throw new ArgumentOutOfRangeException();
                switch (row)
                {
                    case 0:
                        switch (col)
                        {
                            case 0: m00 = value; break;
                            case 1: m01 = value; break;
                            case 2: m02 = value; break;
                            default: m03 = value; break;
                        }
                        break;
                    case 1:
                        switch (col)
                        {
                            case 0: m10 = value; break;
                            case 1: m11 = value; break;
                            case 2: m12 = value; break;
                            default: m13 = value; break;
                        }
                        break;
                    case 2:
                        switch (col)
                        {
                            case 0: m20 = value; break;
                            case 1: m21 = value; break;
                            case 2: m22 = value; break;
                            default: m23 = value; break;
                        }
                        break;
                    default:
                        switch (col)
                        {
                            case 0: m30 = value; break;
                            case 1: m31 = value; break;
                            case 2: m32 = value; break;
                            default: m33 = value; break;
                        }
                        break;
                }
            }
        }

        public bool IsIdentity
        {
            get
            {
                return m00 == 1f && m11 == 1f && m22 == 1f && m33 == 1f &&
                       m01 == 0f && m02 == 0f && m03 == 0f &&
                       m10 == 0f && m12 == 0f && m13 == 0f &&
                       m20 == 0f && m21 == 0f && m23 == 0f &&
                       m30 == 0f && m31 == 0f && m32 == 0f;
            }
        }

        public float Determinant
        {
            get
            {
                float a = m00, b = m01, c = m02, d = m03;
                float e = m10, f = m11, g = m12, h = m13;
                float i = m20, j = m21, k = m22, l = m23;
                float m = m30, n = m31, o = m32, p = m33;

                float det3_1 = f * (k * p - l * o) - g * (j * p - l * n) + h * (j * o - k * n);
                float det3_2 = e * (k * p - l * o) - g * (i * p - l * m) + h * (i * o - k * m);
                float det3_3 = e * (j * p - l * n) - f * (i * p - l * m) + h * (i * n - j * m);
                float det3_4 = e * (j * o - k * n) - f * (i * o - k * m) + g * (i * n - j * m);

                return a * det3_1 - b * det3_2 + c * det3_3 - d * det3_4;
            }
        }

        public static Matrix4x4 Transpose(Matrix4x4 m)
        {
            return new Matrix4x4(
                m.m00, m.m10, m.m20, m.m30,
                m.m01, m.m11, m.m21, m.m31,
                m.m02, m.m12, m.m22, m.m32,
                m.m03, m.m13, m.m23, m.m33
            );
        }

        public Matrix4x4 Transposed()
        {
            return Transpose(this);
        }

        public static Matrix4x4 Multiply(Matrix4x4 a, Matrix4x4 b)
        {
            Matrix4x4 r;
            r.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + a.m03 * b.m30;
            r.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + a.m03 * b.m31;
            r.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + a.m03 * b.m32;
            r.m03 = a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + a.m03 * b.m33;

            r.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + a.m13 * b.m30;
            r.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + a.m13 * b.m31;
            r.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + a.m13 * b.m32;
            r.m13 = a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + a.m13 * b.m33;

            r.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + a.m23 * b.m30;
            r.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + a.m23 * b.m31;
            r.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + a.m23 * b.m32;
            r.m23 = a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + a.m23 * b.m33;

            r.m30 = a.m30 * b.m00 + a.m31 * b.m10 + a.m32 * b.m20 + a.m33 * b.m30;
            r.m31 = a.m30 * b.m01 + a.m31 * b.m11 + a.m32 * b.m21 + a.m33 * b.m31;
            r.m32 = a.m30 * b.m02 + a.m31 * b.m12 + a.m32 * b.m22 + a.m33 * b.m32;
            r.m33 = a.m30 * b.m03 + a.m31 * b.m13 + a.m32 * b.m23 + a.m33 * b.m33;
            return r;
        }

        public static Matrix4x4 Add(Matrix4x4 a, Matrix4x4 b)
        {
            return new Matrix4x4(
                a.m00 + b.m00, a.m01 + b.m01, a.m02 + b.m02, a.m03 + b.m03,
                a.m10 + b.m10, a.m11 + b.m11, a.m12 + b.m12, a.m13 + b.m13,
                a.m20 + b.m20, a.m21 + b.m21, a.m22 + b.m22, a.m23 + b.m23,
                a.m30 + b.m30, a.m31 + b.m31, a.m32 + b.m32, a.m33 + b.m33
            );
        }

        public static Matrix4x4 Subtract(Matrix4x4 a, Matrix4x4 b)
        {
            return new Matrix4x4(
                a.m00 - b.m00, a.m01 - b.m01, a.m02 - b.m02, a.m03 - b.m03,
                a.m10 - b.m10, a.m11 - b.m11, a.m12 - b.m12, a.m13 - b.m13,
                a.m20 - b.m20, a.m21 - b.m21, a.m22 - b.m22, a.m23 - b.m23,
                a.m30 - b.m30, a.m31 - b.m31, a.m32 - b.m32, a.m33 - b.m33
            );
        }

        public static Matrix4x4 Negate(Matrix4x4 m)
        {
            return new Matrix4x4(
                -m.m00, -m.m01, -m.m02, -m.m03,
                -m.m10, -m.m11, -m.m12, -m.m13,
                -m.m20, -m.m21, -m.m22, -m.m23,
                -m.m30, -m.m31, -m.m32, -m.m33
            );
        }

        public static Matrix4x4 operator *(Matrix4x4 a, Matrix4x4 b) => Multiply(a, b);
        public static Matrix4x4 operator +(Matrix4x4 a, Matrix4x4 b) => Add(a, b);
        public static Matrix4x4 operator -(Matrix4x4 a, Matrix4x4 b) => Subtract(a, b);
        public static Matrix4x4 operator -(Matrix4x4 m) => Negate(m);

        public static Matrix4x4 operator *(Matrix4x4 m, float s)
        {
            return new Matrix4x4(
                m.m00 * s, m.m01 * s, m.m02 * s, m.m03 * s,
                m.m10 * s, m.m11 * s, m.m12 * s, m.m13 * s,
                m.m20 * s, m.m21 * s, m.m22 * s, m.m23 * s,
                m.m30 * s, m.m31 * s, m.m32 * s, m.m33 * s
            );
        }

        public static Matrix4x4 operator *(float s, Matrix4x4 m) => m * s;

        public static bool Invert(Matrix4x4 m, out Matrix4x4 result)
        {
            // Gauss-Jordan elimination on augmented matrix [m | I]
            float[,] a = new float[4, 8];

            a[0, 0] = m.m00; a[0, 1] = m.m01; a[0, 2] = m.m02; a[0, 3] = m.m03;
            a[1, 0] = m.m10; a[1, 1] = m.m11; a[1, 2] = m.m12; a[1, 3] = m.m13;
            a[2, 0] = m.m20; a[2, 1] = m.m21; a[2, 2] = m.m22; a[2, 3] = m.m23;
            a[3, 0] = m.m30; a[3, 1] = m.m31; a[3, 2] = m.m32; a[3, 3] = m.m33;

            a[0, 4] = 1f; a[0, 5] = 0f; a[0, 6] = 0f; a[0, 7] = 0f;
            a[1, 4] = 0f; a[1, 5] = 1f; a[1, 6] = 0f; a[1, 7] = 0f;
            a[2, 4] = 0f; a[2, 5] = 0f; a[2, 6] = 1f; a[2, 7] = 0f;
            a[3, 4] = 0f; a[3, 5] = 0f; a[3, 6] = 0f; a[3, 7] = 1f;

            for (int i = 0; i < 4; i++)
            {
                // Pivot selection (partial pivoting)
                int pivotRow = i;
                float maxAbs = Math.Abs(a[i, i]);
                for (int r = i + 1; r < 4; r++)
                {
                    float v = Math.Abs(a[r, i]);
                    if (v > maxAbs)
                    {
                        maxAbs = v;
                        pivotRow = r;
                    }
                }

                if (maxAbs <= Mathf.Epsilon)
                {
                    result = Zero;
                    return false;
                }

                if (pivotRow != i)
                {
                    for (int c = 0; c < 8; c++)
                    {
                        float tmp = a[i, c];
                        a[i, c] = a[pivotRow, c];
                        a[pivotRow, c] = tmp;
                    }
                }

                // Normalize pivot row
                float pivot = a[i, i];
                for (int c = 0; c < 8; c++)
                {
                    a[i, c] /= pivot;
                }

                // Eliminate other rows
                for (int r = 0; r < 4; r++)
                {
                    if (r == i) continue;
                    float factor = a[r, i];
                    if (factor == 0f) continue;
                    for (int c = 0; c < 8; c++)
                    {
                        a[r, c] -= factor * a[i, c];
                    }
                }
            }

            result = new Matrix4x4(
                a[0, 4], a[0, 5], a[0, 6], a[0, 7],
                a[1, 4], a[1, 5], a[1, 6], a[1, 7],
                a[2, 4], a[2, 5], a[2, 6], a[2, 7],
                a[3, 4], a[3, 5], a[3, 6], a[3, 7]
            );
            return true;
        }

        public bool Equals(Matrix4x4 other)
        {
            return m00 == other.m00 && m01 == other.m01 && m02 == other.m02 && m03 == other.m03 &&
                   m10 == other.m10 && m11 == other.m11 && m12 == other.m12 && m13 == other.m13 &&
                   m20 == other.m20 && m21 == other.m21 && m22 == other.m22 && m23 == other.m23 &&
                   m30 == other.m30 && m31 == other.m31 && m32 == other.m32 && m33 == other.m33;
        }

        public override bool Equals(object obj)
        {
            return obj is Matrix4x4 other && Equals(other);
        }

        public static bool operator ==(Matrix4x4 left, Matrix4x4 right) => left.Equals(right);
        public static bool operator !=(Matrix4x4 left, Matrix4x4 right) => !left.Equals(right);

        public override int GetHashCode()
        {
            unchecked
            {
                int hash = 17;
                hash = hash * 31 + m00.GetHashCode();
                hash = hash * 31 + m01.GetHashCode();
                hash = hash * 31 + m02.GetHashCode();
                hash = hash * 31 + m03.GetHashCode();

                hash = hash * 31 + m10.GetHashCode();
                hash = hash * 31 + m11.GetHashCode();
                hash = hash * 31 + m12.GetHashCode();
                hash = hash * 31 + m13.GetHashCode();

                hash = hash * 31 + m20.GetHashCode();
                hash = hash * 31 + m21.GetHashCode();
                hash = hash * 31 + m22.GetHashCode();
                hash = hash * 31 + m23.GetHashCode();

                hash = hash * 31 + m30.GetHashCode();
                hash = hash * 31 + m31.GetHashCode();
                hash = hash * 31 + m32.GetHashCode();
                hash = hash * 31 + m33.GetHashCode();
                return hash;
            }
        }

        public override string ToString()
        {
            var ci = CultureInfo.InvariantCulture;
            return string.Format(ci,
                "[[{0}, {1}, {2}, {3}], [{4}, {5}, {6}, {7}], [{8}, {9}, {10}, {11}], [{12}, {13}, {14}, {15}]]",
                m00, m01, m02, m03,
                m10, m11, m12, m13,
                m20, m21, m22, m23,
                m30, m31, m32, m33);
        }
    }
}