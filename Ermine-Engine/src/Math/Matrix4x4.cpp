/* Start Header ************************************************************************/
/*!
\file       Matrix4x4.cpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (60%)
\co-authors Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu (40%)
\date       Sept 02, 2025
\brief      This file contains the definition of the Matrix4x4 structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Matrix4x4.h" // for forward declaration
#include <cstring>     // for memcpy
#include <cmath>       // for sinf, cosf

namespace Ermine
{
    /*!***********************************************************************
    \brief
     Constructor with a single array.
    \param[in] pArr
     The array of 16 floats.
    *************************************************************************/
    Matrix4x4::Matrix4x4(const float* pArr) : m{ 0.0f }
    {
        if (pArr)
            memcpy(m, pArr, sizeof(float) * 16);
        else
            Mtx44Identity(*this);
    }

    /*!***********************************************************************
    \brief
     Copy assignment operator.
    \param[in] rhs
     The matrix to copy.
    \return
     A reference to the matrix.
    *************************************************************************/
    Matrix4x4& Matrix4x4::operator=(const Matrix4x4& rhs)
    {
        if (this != &rhs)
            memcpy(m, rhs.m, sizeof(float) * 16);
        return *this;
    }

    /*!***********************************************************************
    \brief
     Multiplication assignment operator for two 4x4 matrices.
    \param[in] rhs
     The matrix to multiply.
    \return
     A reference to the matrix.
    *************************************************************************/
    Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& rhs)
    {
        Matrix4x4 result;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                result.m2[row][col] = 0.0f;
                for (int i = 0; i < 4; ++i)
                    result.m2[row][col] += m2[row][i] * rhs.m2[i][col];
            }
        }
        *this = result;
        return *this;
    }

    /*!***********************************************************************
    \brief
     Binary multiplication operator for two 4x4 matrices.
    \param[in] lhs
     The first matrix to multiply.
    \param[in] rhs
     The second matrix to multiply.
    \return
     The matrix of the product of the two matrices.
    *************************************************************************/
    Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs)
    {
        Matrix4x4 result;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                result.m2[row][col] = 0.0f;
                for (int i = 0; i < 4; ++i)
                    result.m2[row][col] += lhs.m2[row][i] * rhs.m2[i][col];
            }
        }
        return result;
    }

    /*!***********************************************************************
    \brief
     Binary multiplication operator for a 4x4 matrix and a 3D vector.
    \param[in] lhs
     The matrix to multiply.
    \param[in] rhs
     The vector to multiply.
    \return
     The vector of the product of the matrix and vector.
    *************************************************************************/
    Vector3D operator*(const Matrix4x4& lhs, const Vector3D& rhs)
    {
        Vector3D result;
        result.x = lhs.m00 * rhs.x + lhs.m01 * rhs.y + lhs.m02 * rhs.z + lhs.m03;
        result.y = lhs.m10 * rhs.x + lhs.m11 * rhs.y + lhs.m12 * rhs.z + lhs.m13;
        result.z = lhs.m20 * rhs.x + lhs.m21 * rhs.y + lhs.m22 * rhs.z + lhs.m23;
        return result;
    }

    /*!***********************************************************************
    \brief
     Set the matrix to an identity matrix.
    \param[out] pResult
     The identity matrix.
    *************************************************************************/
    void Mtx44Identity(Matrix4x4& pResult)
    {
        pResult.m00 = 1.0f; pResult.m01 = 0.0f; pResult.m02 = 0.0f; pResult.m03 = 0.0f;
        pResult.m10 = 0.0f; pResult.m11 = 1.0f; pResult.m12 = 0.0f; pResult.m13 = 0.0f;
        pResult.m20 = 0.0f; pResult.m21 = 0.0f; pResult.m22 = 1.0f; pResult.m23 = 0.0f;
        pResult.m30 = 0.0f; pResult.m31 = 0.0f; pResult.m32 = 0.0f; pResult.m33 = 1.0f;
    }

    /*!***********************************************************************
    \brief
     Create a translation matrix.
    \param[out] pResult
     The translation matrix.
    \param[in] x
     The translation in the x-axis.
    \param[in] y
     The translation in the y-axis.
    \param[in] z
     The translation in the z-axis.
    *************************************************************************/
    void Mtx44Translate(Matrix4x4& pResult, float x, float y, float z)
    {
        // Mtx44Identity(pResult);
        pResult.m03 = x;
        pResult.m13 = y;
        pResult.m23 = z;
    }

    /*!***********************************************************************
    \brief
     Create a rotation matrix around the x-axis with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx44RotXRad(Matrix4x4& pResult, float angle)
    {
        // Mtx44Identity(pResult);
        pResult.m11 = cos(angle);
        pResult.m12 = -sin(angle);
        pResult.m21 = sin(angle);
        pResult.m22 = cos(angle);
    }

    /*!***********************************************************************
    \brief
     Create a rotation matrix around the y-axis with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx44RotYRad(Matrix4x4& pResult, float angle)
    {
        // Mtx44Identity(pResult);
        pResult.m00 = cos(angle);
        pResult.m02 = sin(angle);
        pResult.m20 = -sin(angle);
        pResult.m22 = cos(angle);
    }

    /*!***********************************************************************
    \brief
     Create a rotation matrix around the z-axis with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx44RotZRad(Matrix4x4& pResult, float angle)
    {
        // Mtx44Identity(pResult);
        pResult.m00 = cos(angle);
        pResult.m01 = -sin(angle);
        pResult.m10 = sin(angle);
        pResult.m11 = cos(angle);
    }

    /*!***********************************************************************
    \brief
     Create a scaling matrix.
    \param[out] pResult
     The scaling matrix.
    \param[in] x
     The scale factor in the x-axis.
    \param[in] y
     The scale factor in the y-axis.
    \param[in] z
     The scale factor in the z-axis.
    *************************************************************************/
    void Mtx44Scale(Matrix4x4& pResult, float x, float y, float z)
    {
        // Mtx44Identity(pResult);
        pResult.m00 = x;
        pResult.m11 = y;
        pResult.m22 = z;
    }

    /*!***********************************************************************
    \brief
     Transpose the given matrix.
    \param[out] pResult
     The transposed matrix.
    \param[in] pMtx
     The input matrix to transpose.
    *************************************************************************/
    void Mtx44Transpose(Matrix4x4& pResult, const Matrix4x4& pMtx)
    {
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                pResult.m2[row][col] = pMtx.m2[col][row];
    }

    /*!*************************************************************************
      \brief
        Builds a perspective projection matrix.
      \param[out] pResult
        The resulting projection matrix.
      \param[in] fovy
        Vertical field of view in radians.
      \param[in] aspect
        Aspect ratio (width / height).
      \param[in] zn
        Near clipping plane.
      \param[in] zf
        Far clipping plane.
    ***************************************************************************/
    void Mtx44Perspective(Matrix4x4& pResult, float fovy, float aspect, float zn, float zf)
    {
       // Calculate the scale based on the field of view
       float yScale = 1.0f / tan(fovy / 2.0f);
       float xScale = yScale / aspect;

       // Fill in the matrix elements
       pResult.m2[0][0] = xScale;
       pResult.m2[1][1] = yScale;
       pResult.m2[2][2] = zf / (zn - zf);
       pResult.m2[2][3] = -1.0f;
       pResult.m2[3][2] = (zn * zf) / (zn - zf);
       pResult.m2[3][3] = 0.0f;

       // Clear other elements to ensure proper perspective projection
       pResult.m2[0][1] = pResult.m2[0][2] = pResult.m2[0][3] = 0.0f;
       pResult.m2[1][0] = pResult.m2[1][2] = pResult.m2[1][3] = 0.0f;
       pResult.m2[2][0] = pResult.m2[2][1] = 0.0f;
       pResult.m2[3][0] = pResult.m2[3][1] = 0.0f;
    }

    /*!*************************************************************************
      \brief
        Builds a look-at (view) matrix.
      \param[out] pResult
        The resulting view matrix.
      \param[in] eye
        Position of the camera.
      \param[in] center
        Target point the camera looks at.
      \param[in] up
        Up direction vector.
    ***************************************************************************/
    void Mtx44LookAt(Matrix4x4& pResult, const Vector3D& eye, const Vector3D& center, const Vector3D& up)
    {
     // Calculate the forward vector (z-axis)
     Vector3D f = center - eye;
     Vec3Normalize(f, f);

     // Calculate the right vector (x-axis) as cross product of forward and up
     Vector3D r;
     Vec3CrossProduct(r, f, up);
     Vec3Normalize(r, r);

     // Calculate the actual up vector (y-axis)
     Vector3D u;
     Vec3CrossProduct(u, r,f);

     // Set the rotation part of the matrix
     pResult.m00 = r.x;  pResult.m01 = r.y;  pResult.m02 = r.z;  pResult.m03 = 0.0f;
     pResult.m10 = u.x;  pResult.m11 = u.y;  pResult.m12 = u.z;  pResult.m13 = 0.0f;
     pResult.m20 = -f.x; pResult.m21 = -f.y; pResult.m22 = -f.z; pResult.m23 = 0.0f;
     pResult.m30 = 0.0f; pResult.m31 = 0.0f; pResult.m32 = 0.0f; pResult.m33 = 1.0f;

     // Apply the translation (negative eye position)
     Matrix4x4 translation;
     Mtx44Identity(translation);
     Mtx44Translate(translation, -eye.x, -eye.y, -eye.z);

     // Combine rotation and translation
     pResult = pResult * translation;
    }

    /*!*************************************************************************
      \brief
        Builds a rotation matrix from a quaternion.
      \param[out] pResult
        The resulting rotation matrix.
      \param[in] q
        Input quaternion.
    ***************************************************************************/
    void Mtx44SetFromQuaternion(Matrix4x4& pResult, const Quaternion& q)
    {
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;

        pResult.m00 = 1.0f - 2.0f * (yy + zz);
        pResult.m01 = 2.0f * (xy - wz);
        pResult.m02 = 2.0f * (xz + wy);
        pResult.m03 = 0.0f;

        pResult.m10 = 2.0f * (xy + wz);
        pResult.m11 = 1.0f - 2.0f * (xx + zz);
        pResult.m12 = 2.0f * (yz - wx);
        pResult.m13 = 0.0f;

        pResult.m20 = 2.0f * (xz - wy);
        pResult.m21 = 2.0f * (yz + wx);
        pResult.m22 = 1.0f - 2.0f * (xx + yy);
        pResult.m23 = 0.0f;

        pResult.m30 = 0.0f;
        pResult.m31 = 0.0f;
        pResult.m32 = 0.0f;
        pResult.m33 = 1.0f;
    }

    /*!*************************************************************************
      \brief
        Converts a rotation matrix into a quaternion.
      \param[in] m
        Input rotation matrix.
      \return
        The equivalent quaternion.
    ***************************************************************************/
    Quaternion Mtx44GetQuaternion(const Matrix4x4& m)
    {
        Quaternion q;
        float trace = m.m00 + m.m11 + m.m22;

        if (trace > 0.0f)
        {
            float s = 0.5f / sqrtf(trace + 1.0f);
            q.w = 0.25f / s;
            q.x = (m.m21 - m.m12) * s;
            q.y = (m.m02 - m.m20) * s;
            q.z = (m.m10 - m.m01) * s;
        }
        else
        {
            if (m.m00 > m.m11 && m.m00 > m.m22)
            {
                float s = 2.0f * sqrtf(1.0f + m.m00 - m.m11 - m.m22);
                q.w = (m.m21 - m.m12) / s;
                q.x = 0.25f * s;
                q.y = (m.m01 + m.m10) / s;
                q.z = (m.m02 + m.m20) / s;
            }
            else if (m.m11 > m.m22)
            {
                float s = 2.0f * sqrtf(1.0f + m.m11 - m.m00 - m.m22);
                q.w = (m.m02 - m.m20) / s;
                q.x = (m.m01 + m.m10) / s;
                q.y = 0.25f * s;
                q.z = (m.m12 + m.m21) / s;
            }
            else
            {
                float s = 2.0f * sqrtf(1.0f + m.m22 - m.m00 - m.m11);
                q.w = (m.m10 - m.m01) / s;
                q.x = (m.m02 + m.m20) / s;
                q.y = (m.m12 + m.m21) / s;
                q.z = 0.25f * s;
            }
        }

        return QuaternionNormalize(q);
    }

    /*!*************************************************************************
      \brief
        Converts a quaternion into Euler angles.
      \param[in] q
        Input quaternion.
      \param[in] inDegrees
        True if result should be in degrees, false for radians.
      \return
        Euler angles (roll, pitch, yaw).
    ***************************************************************************/
    Vec3 QuaternionToEuler(const Quaternion& q, bool inDegrees)
    {
        float ysqr = q.y * q.y;

        // Roll (x-axis rotation)
        float t0 = +2.0f * (q.w * q.x + q.y * q.z);
        float t1 = +1.0f - 2.0f * (q.x * q.x + ysqr);
        float roll = std::atan2(t0, t1);

        // Pitch (y-axis rotation)
        float t2 = +2.0f * (q.w * q.y - q.z * q.x);
        t2 = std::clamp(t2, -1.0f, 1.0f);
        float pitch = std::asin(t2);

        // Yaw (z-axis rotation)
        float t3 = +2.0f * (q.w * q.z + q.x * q.y);
        float t4 = +1.0f - 2.0f * (ysqr + q.z * q.z);
        float yaw = std::atan2(t3, t4);

        if (inDegrees)
        {
            constexpr float Rad2Deg = 180.0f / static_cast<float>(std::numbers::pi);
            return {roll * Rad2Deg, pitch * Rad2Deg, yaw * Rad2Deg};
        }

        return {roll, pitch, yaw}; // radians
    }

    /*!*************************************************************************
	  \brief
	    Creates a quaternion from Euler angles given in degrees.
	  \param[in] eulerDeg
	    Rotation about (X, Y, Z) axes in degrees.
	  \return
	    The resulting quaternion.
	***************************************************************************/
    Quaternion FromEulerDegrees(const Vec3& eulerDeg)
    {
        return FromEulerDegrees(eulerDeg.x, eulerDeg.y, eulerDeg.z);
    }

    /*!*************************************************************************
      \brief
        Creates a quaternion from Euler angles given in degrees.
      \param[in] pitch
        Rotation about X-axis in degrees.
      \param[in] yaw
        Rotation about Y-axis in degrees.
      \param[in] roll
        Rotation about Z-axis in degrees.
      \return
        The resulting quaternion.
    ***************************************************************************/
    Quaternion FromEulerDegrees(float pitch, float yaw, float roll)
    {
        // Convert to radians
        float x = DegToRad(pitch);
        float y = DegToRad(yaw);
        float z = DegToRad(roll);

        float cx = cosf(x * 0.5f);
        float sx = sinf(x * 0.5f);
        float cy = cosf(y * 0.5f);
        float sy = sinf(y * 0.5f);
        float cz = cosf(z * 0.5f);
        float sz = sinf(z * 0.5f);

        Quaternion q;
        q.w = cx * cy * cz + sx * sy * sz;
        q.x = sx * cy * cz - cx * sy * sz;
        q.y = cx * sy * cz + sx * cy * sz;
        q.z = cx * cy * sz - sx * sy * cz;
        return q;
    }

    /*!***********************************************************************
    \brief
     Calculate the inverse of a 4x4 matrix.
    \param[out] pResult
     The inverted matrix.
    \param[in] pMtx
     The input matrix to invert.
    \return
     True if the matrix is invertible, false otherwise.
    *************************************************************************/
    bool Mtx44Inverse(Matrix4x4& pResult, const Matrix4x4& pMtx)
    {
        float aug[4][8]; // Augmented matrix [M | I]

        // Initialize augmented matrix
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                aug[i][j] = pMtx.m2[i][j];
                aug[i][j + 4] = (i == j) ? 1.0f : 0.0f;
            }
        }

        // Forward elimination with partial pivoting
        for (int i = 0; i < 4; i++) {
            // Find pivot
            int maxRow = i;
            for (int k = i + 1; k < 4; k++) {
                if (fabs(aug[k][i]) > fabs(aug[maxRow][i]))
                    maxRow = k;
            }

            // Swap rows
            if (maxRow != i) {
                for (int k = 0; k < 8; k++)
                    std::swap(aug[i][k], aug[maxRow][k]);
            }

            // Check for singular matrix
            if (fabs(aug[i][i]) < 1e-6f)
                return false;

            // Make diagonal 1
            float pivot = aug[i][i];
            for (int j = 0; j < 8; j++)
                aug[i][j] /= pivot;

            // Eliminate column
            for (int k = 0; k < 4; k++) {
                if (k != i) {
                    float factor = aug[k][i];
                    for (int j = 0; j < 8; j++)
                        aug[k][j] -= factor * aug[i][j];
                }
            }
        }

        // Extract result from right half
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                pResult.m2[i][j] = aug[i][j + 4];

        return true;
    }

    /*!***********************************************************************
    \brief
     Multiply two quaternions.
    \param[in] lhs
     The first quaternion.
    \param[in] rhs
     The second quaternion.
    \return
     The product of the two quaternions.
    *************************************************************************/
    Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs)
    {
        Quaternion result;
        result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
        result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
        result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
        result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
        return result;
    }

    // Add Vec4 multiplication operator
    Vec4 operator*(const Matrix4x4& mtx, const Vec4& v)
    {
        return Vec4(
            mtx.m00 * v.x + mtx.m01 * v.y + mtx.m02 * v.z + mtx.m03 * v.w,
            mtx.m10 * v.x + mtx.m11 * v.y + mtx.m12 * v.z + mtx.m13 * v.w,
            mtx.m20 * v.x + mtx.m21 * v.y + mtx.m22 * v.z + mtx.m23 * v.w,
            mtx.m30 * v.x + mtx.m31 * v.y + mtx.m32 * v.z + mtx.m33 * v.w
        );
    }
    /*!*************************************************************************
      \brief
        Normalizes a quaternion.
      \param[in] q
        Input quaternion.
      \return
        Normalized quaternion.
    ***************************************************************************/
    Quaternion QuaternionNormalize(const Quaternion& q)
    {
        float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len <= 1e-6f)
            return Quaternion(0, 0, 0, 1);
        float inv = 1.0f / len;
        return Quaternion(q.x * inv, q.y * inv, q.z * inv, q.w * inv);
    }

    /*!*************************************************************************
      \brief
        Multiplies two quaternions.
      \param[in] a
        First quaternion.
      \param[in] b
        Second quaternion.
      \return
        The product quaternion.
    ***************************************************************************/
    Quaternion QuaternionMultiply(const Quaternion& a, const Quaternion& b)
    {
        return Quaternion(
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        );
    }

    /*!*************************************************************************
      \brief
        Returns the conjugate of a quaternion.
      \param[in] q
        Input quaternion.
      \return
        Conjugated quaternion.
    ***************************************************************************/
    Quaternion QuaternionConjugate(const Quaternion& q)
    {
        return Quaternion(-q.x, -q.y, -q.z, q.w);
    }

    /*!*************************************************************************
      \brief
        Rotates a vector by a quaternion.
      \param[in] q
        Rotation quaternion.
      \param[in] v
        Vector to rotate.
      \return
        Rotated vector.
    ***************************************************************************/
    Vector3D QuaternionRotateVector(const Quaternion& q, const Vector3D& v)
    {
        Quaternion vq(v.x, v.y, v.z, 0.0f);
        Quaternion inv = QuaternionConjugate(q);
        Quaternion res = QuaternionMultiply(QuaternionMultiply(q, vq), inv);
        return Vector3D(res.x, res.y, res.z);
    }

    /*!*************************************************************************
      \brief
        Creates a quaternion from an axis and rotation angle.
      \param[in] axis
        Axis of rotation.
      \param[in] angleRad
        Rotation angle in radians.
      \return
        The resulting quaternion.
    ***************************************************************************/
    Quaternion QuaternionFromAxisAngle(const Vector3D& axis, float angleRad)
    {
        float half = angleRad * 0.5f;
        float s = sinf(half);
        return Quaternion(axis.x * s, axis.y * s, axis.z * s, cosf(half));
    }

    /*!*************************************************************************
      \brief
        Converts a quaternion into axis-angle representation.
      \param[in] q
        Input quaternion.
      \param[out] axis
        Resulting rotation axis.
      \param[out] angleRad
        Resulting rotation angle in radians.
    ***************************************************************************/
    void QuaternionToAxisAngle(const Quaternion& q, Vector3D& axis, float& angleRad)
    {
        Quaternion nq = QuaternionNormalize(q);
        angleRad = 2.0f * acosf(nq.w);
        float s = sqrtf(1.0f - nq.w * nq.w);
        if (s < 1e-6f)
        {
            axis = Vector3D(1.0f, 0.0f, 0.0f);
        }
        else
        {
            axis = Vector3D(nq.x / s, nq.y / s, nq.z / s);
        }
    }
}
