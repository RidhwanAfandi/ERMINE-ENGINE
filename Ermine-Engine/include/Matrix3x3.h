/* Start Header ************************************************************************/
/*!
\file       Matrix3x3.h
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (60%)
\co-authors Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu (40%)
\date       Sept 02, 2025
\brief      This file contains the declaration of the Matrix3x3 structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "MathVector.h" // for Vector2D

namespace Ermine
{
#ifdef _MSC_VER
    // Supress warning: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )
#endif

    /*!***********************************************************************
    \brief
     Represents a 3x3 matrix with individual components, or as an single or double array.
    *************************************************************************/
    typedef union Matrix3x3
    {
        struct
        {
            float m00, m01, m02;
            float m10, m11, m12;
            float m20, m21, m22;
        };

        float m[9];
        float m2[3][3];

        /*!***********************************************************************
        \brief
         Default constructor.
        *************************************************************************/
        Matrix3x3() : m00(0.0f), m01(0.0f), m02(0.0f),
            m10(0.0f), m11(0.0f), m12(0.0f),
            m20(0.0f), m21(0.0f), m22(0.0f) {}

        /*!***********************************************************************
        \brief
         Constructor with individual parameters.
        \param[in] m00
         The 0,0 element.
        \param[in] m01
         The 0,1 element.
        \param[in] m02
         The 0,2 element.
        \param[in] m10
         The 1,0 element.
        \param[in] m11
         The 1,1 element.
        \param[in] m12
         The 1,2 element.
        \param[in] m20
         The 2,0 element.
        \param[in] m21
         The 2,1 element.
        \param[in] m22
         The 2,2 element.
        *************************************************************************/
        Matrix3x3(float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22)
            : m00(m00), m01(m01), m02(m02),
            m10(m10), m11(m11), m12(m12),
            m20(m20), m21(m21), m22(m22) {}

        /*!***********************************************************************
        \brief
         Constructor with a single array.
        \param[in] pArr
         The array of 9 floats.
        *************************************************************************/
        Matrix3x3(const float* pArr);

        /*!***********************************************************************
        \brief
         Copy constructor.
        \param[in] rhs
         The matrix to copy.
        *************************************************************************/
        Matrix3x3(const Matrix3x3& rhs) = default;

        /*!***********************************************************************
        \brief
         Copy assignment operator.
        \param[in] rhs
         The matrix to copy.
        \return
         A reference to the matrix.
        *************************************************************************/
        Matrix3x3& operator=(const Matrix3x3& rhs);

        /*!***********************************************************************
        \brief
         Multiplication assignment operator for two 3x3 matrices.
        \param[in] rhs
         The matrix to multiply.
        \return
         A reference to the matrix.
        *************************************************************************/
        Matrix3x3& operator*=(const Matrix3x3& rhs);

    } Matrix3x3, Mtx33;

#ifdef _MSC_VER
    // Supress warning: nonstandard extension used : nameless struct/union
#pragma warning( default : 4201 )
#endif

    /*!***********************************************************************
    \brief
     Binary multiplication operator for two 3x3 matrices.
    \param[in] lhs
     The first matrix to multiply.
    \param[in] rhs
     The second matrix to multiply.
    \return
     The matrix of the product of the two matrices.
    *************************************************************************/
    Matrix3x3 operator*(const Matrix3x3& lhs, const Matrix3x3& rhs);

    /*!***********************************************************************
    \brief
     Binary multiplication operator for a 3x3 matrix and a 2D vector.
    \param[in] lhs
     The matrix to multiply.
    \param[in] rhs
     The vector to multiply.
    \return
     The vector of the product of the matrix and vector.
    *************************************************************************/
    Vector2D operator*(const Matrix3x3& pMtx, const Vector2D& rhs);

    /*!***********************************************************************
    \brief
     Set the matrix to an identity matrix.
    \param[out] pResult
     The identity matrix.
    *************************************************************************/
    void Mtx33Identity(Matrix3x3& pResult);

    /*!***********************************************************************
    \brief
     Create a translation matrix.
    \param[out] pResult
     The translation matrix.
    \param[in] x
     The translation in the x-axis.
    \param[in] y
     The translation in the y-axis.
    *************************************************************************/
    void Mtx33Translate(Matrix3x3& pResult, float x, float y);

    /*!***********************************************************************
    \brief
     Create a rotation matrix with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx33RotRad(Matrix3x3& pResult, float angle);

    /*!***********************************************************************
    \brief
     Create a rotation matrix with an angle in degrees.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in degrees.
    *************************************************************************/
    void Mtx33RotDeg(Matrix3x3& pResult, float angle);

    /*!***********************************************************************
    \brief
     Create a scaling matrix.
    \param[out] pResult
     The scaling matrix.
    \param[in] x
     The scale factor in the x-axis.
    \param[in] y
     The scale factor in the y-axis.
    *************************************************************************/
    void Mtx33Scale(Matrix3x3& pResult, float x, float y);

    /*!***********************************************************************
    \brief
     Transpose the given matrix.
    \param[out] pResult
     The transposed matrix.
    \param[in] pMtx
     The input matrix to transpose.
    *************************************************************************/
    void Mtx33Transpose(Matrix3x3& pResult, const Matrix3x3& pMtx);

    /*!***********************************************************************
    \brief
     Invert the given matrix.
    \param[out] pResult
     The inverse matrix.
    \param[in] determinant
     The determinant of the matrix.
    \param[in] pMtx
     The input matrix to invert.
    *************************************************************************/
    void Mtx33Inverse(Matrix3x3* pResult, float* determinant, const Matrix3x3& pMtx);

   /*!***********************************************************************
   \brief
    Invert the given matrix and return the inverse.
   \param[in] pMtx
    The input matrix to invert.
   \return
    The inverse matrix. If the matrix is not invertible, returns an identity matrix.
   *************************************************************************/
    Matrix3x3 Mtx33GetInverse(const Matrix3x3& pMtx);
}