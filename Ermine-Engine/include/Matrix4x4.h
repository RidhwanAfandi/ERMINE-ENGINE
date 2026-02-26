/* Start Header ************************************************************************/
/*!
\file       Matrix4x4.h
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (60%)
\co-authors Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu (40%)
\date       Sept 02, 2025
\brief      This file contains the declaration of the Matrix4x4 structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "MathVector.h" // for Vector3D

namespace Ermine
{
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

    //struct Quaternion
    //{
    //    float x, y, z, w;

    //    Quaternion(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f, float _w = 1.0f)
    //        : x(_x), y(_y), z(_z), w(_w) {}
    //};

#ifdef _MSC_VER
// Supress warning: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )
#endif
    typedef union Quaternion
    {
        struct
        {
            float x, y, z, w;
        };
        float m[4];

        Quaternion(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f, float _w = 1.0f)
            : x(_x), y(_y), z(_z), w(_w) {}
	} Quaternion;

#ifdef _MSC_VER
    // Supress warning: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )
#endif
    /*!***********************************************************************
    \brief
     Represents a 4x4 matrix with individual components, or as an single or double array.
    *************************************************************************/
    typedef union Matrix4x4
    {
        struct
        {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        };

        float m[16];
        float m2[4][4];

        /*!***********************************************************************
        \brief
         Default constructor.
        *************************************************************************/
        Matrix4x4() : m00(0.0f), m01(0.0f), m02(0.0f), m03(0.0f),
            m10(0.0f), m11(0.0f), m12(0.0f), m13(0.0f),
            m20(0.0f), m21(0.0f), m22(0.0f), m23(0.0f),
            m30(0.0f), m31(0.0f), m32(0.0f), m33(0.0f) 
        {
            m[0] = m00;  m[1] = m01;  m[2] = m02;  m[3] = m03;
            m[4] = m10;  m[5] = m11;  m[6] = m12;  m[7] = m13;
            m[8] = m20;  m[9] = m21;  m[10] = m22; m[11] = m23;
            m[12] = m30; m[13] = m31; m[14] = m32; m[15] = m33;

            m2[0][0] = m00; m2[0][1] = m01; m2[0][2] = m02; m2[0][3] = m03;
            m2[1][0] = m10; m2[1][1] = m11; m2[1][2] = m12; m2[1][3] = m13;
            m2[2][0] = m20; m2[2][1] = m21; m2[2][2] = m22; m2[2][3] = m23;
            m2[3][0] = m30; m2[3][1] = m31; m2[3][2] = m32; m2[3][3] = m33;
        }

        /*!***********************************************************************
        \brief
         Constructor with a single scalar.
        \param[in] s
         The scalar value to initialize all elements.
        *************************************************************************/
        Matrix4x4(float s) : m00(s), m01(s), m02(s), m03(s),
            m10(s), m11(s), m12(s), m13(s),
            m20(s), m21(s), m22(s), m23(s),
            m30(s), m31(s), m32(s), m33(s)
        {
           m[0] = m00;  m[1] = m01;  m[2] = m02;  m[3] = m03;
           m[4] = m10;  m[5] = m11;  m[6] = m12;  m[7] = m13;
           m[8] = m20;  m[9] = m21;  m[10] = m22; m[11] = m23;
           m[12] = m30; m[13] = m31; m[14] = m32; m[15] = m33;

           m2[0][0] = m00; m2[0][1] = m01; m2[0][2] = m02; m2[0][3] = m03;
           m2[1][0] = m10; m2[1][1] = m11; m2[1][2] = m12; m2[1][3] = m13;
           m2[2][0] = m20; m2[2][1] = m21; m2[2][2] = m22; m2[2][3] = m23;
           m2[3][0] = m30; m2[3][1] = m31; m2[3][2] = m32; m2[3][3] = m33;
        }

        /*!***********************************************************************
        \brief
         Constructor with individual parameters.
        \param[in] m00
         The 0,0 element.
        \param[in] m01
         The 0,1 element.
        \param[in] m02
         The 0,2 element.
        \param[in] m03
         The 0,3 element.
        \param[in] m10
         The 1,0 element.
        \param[in] m11
         The 1,1 element.
        \param[in] m12
         The 1,2 element.
        \param[in] m13
         The 1,3 element.
        \param[in] m20
         The 2,0 element.
        \param[in] m21
         The 2,1 element.
        \param[in] m22
         The 2,2 element.
        \param[in] m23
         The 2,3 element.
        \param[in] m30
         The 3,0 element.
        \param[in] m31
         The 3,1 element.
        \param[in] m32
         The 3,2 element.
        \param[in] m33
         The 3,3 element.
        *************************************************************************/
        Matrix4x4(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33)
            : m00(m00), m01(m01), m02(m02), m03(m03),
            m10(m10), m11(m11), m12(m12), m13(m13),
            m20(m20), m21(m21), m22(m22), m23(m23),
            m30(m30), m31(m31), m32(m32), m33(m33) 
        {
            m[0] = m00; m[1] = m01; m[2] = m02; m[3] = m03;
            m[4] = m10; m[5] = m11; m[6] = m12; m[7] = m13;
            m[8] = m20; m[9] = m21; m[10] = m22; m[11] = m23;
            m[12] = m30; m[13] = m31; m[14] = m32; m[15] = m33;

            m2[0][0] = m00; m2[0][1] = m01; m2[0][2] = m02; m2[0][3] = m03;
            m2[1][0] = m10; m2[1][1] = m11; m2[1][2] = m12; m2[1][3] = m13;
            m2[2][0] = m20; m2[2][1] = m21; m2[2][2] = m22; m2[2][3] = m23;
            m2[3][0] = m30; m2[3][1] = m31; m2[3][2] = m32; m2[3][3] = m33;
        }

        /*!***********************************************************************
        \brief
         Constructor with a single array.
        \param[in] pArr
         The array of 16 floats.
        *************************************************************************/
        Matrix4x4(const float* pArr);

        /*!***********************************************************************
        \brief
         Copy constructor.
        \param[in] rhs
         The matrix to copy.
        *************************************************************************/
        Matrix4x4(const Matrix4x4& rhs) = default;

        /*!***********************************************************************
        \brief
         Copy assignment operator.
        \param[in] rhs
         The matrix to copy.
        \return
         A reference to the matrix.
        *************************************************************************/
        Matrix4x4& operator=(const Matrix4x4& rhs);

        /*!***********************************************************************
        \brief
         Multiplication assignment operator for two 4x4 matrices.
        \param[in] rhs
         The matrix to multiply.
        \return
         A reference to the matrix.
        *************************************************************************/
        Matrix4x4& operator*=(const Matrix4x4& rhs);

    } Matrix4x4, Mtx44;

#ifdef _MSC_VER
    // Supress warning: nonstandard extension used : nameless struct/union
#pragma warning( default : 4201 )
#endif

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
    Matrix4x4 operator*(const Matrix4x4& lhs, const Matrix4x4& rhs);

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
    Vector3D operator*(const Matrix4x4& pMtx, const Vector3D& rhs);

    /*!***********************************************************************
    \brief
     Set the matrix to an identity matrix.
    \param[out] pResult
     The identity matrix.
    *************************************************************************/
    void Mtx44Identity(Matrix4x4& pResult);

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
    void Mtx44Translate(Matrix4x4& pResult, float x, float y, float z);

    /*!***********************************************************************
    \brief
     Create a rotation matrix around the x-axis with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx44RotXRad(Matrix4x4& pResult, float angle);

    /*!***********************************************************************
    \brief
     Create a rotation matrix around the y-axis with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx44RotYRad(Matrix4x4& pResult, float angle);

    /*!***********************************************************************
    \brief
     Create a rotation matrix around the z-axis with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx44RotZRad(Matrix4x4& pResult, float angle);

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
    void Mtx44Scale(Matrix4x4& pResult, float x, float y, float z);

    /*!***********************************************************************
    \brief
     Transpose the given matrix.
    \param[out] pResult
     The transposed matrix.
    \param[in] pMtx
     The input matrix to transpose.
    *************************************************************************/
    void Mtx44Transpose(Matrix4x4& pResult, const Matrix4x4& pMtx);

    /*!***********************************************************************
    \brief
     Invert the given matrix.
    \param[out] pResult
     The perspective matrix.
    \param[in] fovy
     The field of view in the y direction.
    \param[in] aspect
     The aspect ratio.
    \param[in] zn
     The near plane.
    \param[in] zf
     The far plane.
    *************************************************************************/
    void Mtx44Perspective(Matrix4x4& pResult, float fovy, float aspect, float zn, float zf);

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
    void Mtx44LookAt(Matrix4x4& pResult, const Vector3D& eye,
        const Vector3D& center, const Vector3D& up);

    /*!*************************************************************************
      \brief
        Builds a rotation matrix from a quaternion.
      \param[out] pResult
        The resulting rotation matrix.
      \param[in] q
        Input quaternion.
    ***************************************************************************/
    void Mtx44SetFromQuaternion(Matrix4x4& pResult, const Quaternion& q);

    /*!*************************************************************************
      \brief
        Converts a rotation matrix into a quaternion.
      \param[in] m
        Input rotation matrix.
      \return
        The equivalent quaternion.
    ***************************************************************************/
    Quaternion Mtx44GetQuaternion(const Matrix4x4& m);

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
    Vec3 QuaternionToEuler(const Quaternion& q, bool inDegrees);

    /*!*************************************************************************
      \brief
        Converts an angle from degrees to radians.
      \param[in] deg
        Angle in degrees.
      \return
        Angle in radians.
    ***************************************************************************/
    inline float DegToRad(float deg)
    {
        return deg * (static_cast<float>(M_PI) / 180.0f);
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
    Quaternion FromEulerDegrees(float pitch, float yaw, float roll);

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
    bool Mtx44Inverse(Matrix4x4& pResult, const Matrix4x4& pMtx);

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
    Quaternion operator*(const Quaternion& lhs, const Quaternion& rhs);

    /*!*************************************************************************
      \brief
        Creates a quaternion from Euler angles given in degrees.
      \param[in] eulerDeg
        Rotation about (X, Y, Z) axes in degrees.
      \return
		The resulting quaternion.
	***************************************************************************/
	Quaternion FromEulerDegrees(const Vec3& eulerDeg);

    /*!*************************************************************************
      \brief
        Normalizes a quaternion.
      \param[in] q
        Input quaternion.
      \return
        Normalized quaternion.
    ***************************************************************************/
    Quaternion QuaternionNormalize(const Quaternion& q);

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
    Quaternion QuaternionMultiply(const Quaternion& a, const Quaternion& b);

    /*!*************************************************************************
      \brief
        Returns the conjugate of a quaternion.
      \param[in] q
        Input quaternion.
      \return
        Conjugated quaternion.
    ***************************************************************************/
    Quaternion QuaternionConjugate(const Quaternion& q);

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
    Vector3D QuaternionRotateVector(const Quaternion& q, const Vector3D& v);

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
    Quaternion QuaternionFromAxisAngle(const Vector3D& axis, float angleRad);

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
    void QuaternionToAxisAngle(const Quaternion& q, Vector3D& axis, float& angleRad); }