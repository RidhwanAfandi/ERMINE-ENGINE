/* Start Header ************************************************************************/
/*!
\file       Matrix3x3.cpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (60%)
\co-authors Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu (40%)
\date       Sept 02, 2025
\brief      This file contains the definition of the Matrix3x3 structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Matrix3x3.h" // for forward declaration
#include "MathUtils.h" // for PIOVER180
#include <cstring>     // for memcpy
#include <cmath>       // for sinf, cosf

namespace Ermine
{
    /*!***********************************************************************
    \brief
     Constructor with a single array.
    \param[in] pArr
     The array of 9 floats.
    *************************************************************************/
    Matrix3x3::Matrix3x3(const float* pArr) : m{ 0.0f }
    {
        if (pArr)
            memcpy(m, pArr, sizeof(float) * 9);
        else
            Mtx33Identity(*this);
    }

    /*!***********************************************************************
    \brief
     Copy assignment operator.
    \param[in] rhs
     The matrix to copy.
    \return
     A reference to the matrix.
    *************************************************************************/
    Matrix3x3& Matrix3x3::operator=(const Matrix3x3& rhs)
    {
        if (this != &rhs)
            memcpy(m, rhs.m, sizeof(float) * 9);
        return *this;
    }

    /*!***********************************************************************
    \brief
     Multiplication assignment operator for two 3x3 matrices.
    \param[in] rhs
     The matrix to multiply.
    \return
     A reference to the matrix.
    *************************************************************************/
    Matrix3x3& Matrix3x3::operator*=(const Matrix3x3& rhs)
    {
        Matrix3x3 result;
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                result.m2[row][col] = 0.0f;
                for (int i = 0; i < 3; ++i)
                    result.m2[row][col] += m2[row][i] * rhs.m2[i][col];
            }
        }
        *this = result;
        return *this;
    }

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
    Matrix3x3 operator*(const Matrix3x3& lhs, const Matrix3x3& rhs)
    {
        Matrix3x3 result;
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                result.m2[row][col] = 0.0f;
                for (int i = 0; i < 3; ++i)
                    result.m2[row][col] += lhs.m2[row][i] * rhs.m2[i][col];
            }
        }
        return result;
    }

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
    Vector2D operator*(const Matrix3x3& lhs, const Vector2D& rhs)
    {
        Vector2D result;
        result.x = lhs.m00 * rhs.x + lhs.m01 * rhs.y + lhs.m02;
        result.y = lhs.m10 * rhs.x + lhs.m11 * rhs.y + lhs.m12;
        return result;
    }

    /*!***********************************************************************
    \brief
     Set the matrix to an identity matrix.
    \param[out] pResult
     The identity matrix.
    *************************************************************************/
    void Mtx33Identity(Matrix3x3& pResult)
    {
        pResult.m00 = 1.0f; pResult.m01 = 0.0f; pResult.m02 = 0.0f;
        pResult.m10 = 0.0f; pResult.m11 = 1.0f; pResult.m12 = 0.0f;
        pResult.m20 = 0.0f; pResult.m21 = 0.0f; pResult.m22 = 1.0f;
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
    *************************************************************************/
    void Mtx33Translate(Matrix3x3& pResult, float x, float y)
    {
        Mtx33Identity(pResult);
        pResult.m02 = x;
        pResult.m12 = y;
    }

    /*!***********************************************************************
    \brief
     Create a rotation matrix with an angle in radians.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in radians.
    *************************************************************************/
    void Mtx33RotRad(Matrix3x3& pResult, float angle)
    {
        float cos_angle = cos(angle);
        float sin_angle = sin(angle);
        pResult.m00 = cos_angle; pResult.m01 = -sin_angle; pResult.m02 = 0.0f;
        pResult.m10 = sin_angle; pResult.m11 = cos_angle; pResult.m12 = 0.0f;
        pResult.m20 = 0.0f; pResult.m21 = 0.0f; pResult.m22 = 1.0f;
    }

    /*!***********************************************************************
    \brief
     Create a rotation matrix with an angle in degrees.
    \param[out] pResult
     The rotation matrix.
    \param[in] angle
     The angle of rotation in degrees.
    *************************************************************************/
    void Mtx33RotDeg(Matrix3x3& pResult, float angle)
    {
        Mtx33RotRad(pResult, angle * PIOVER180<float>);
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
    *************************************************************************/
    void Mtx33Scale(Matrix3x3& pResult, float x, float y)
    {
        Mtx33Identity(pResult);
        pResult.m00 = x;
        pResult.m11 = y;
    }

    /*!***********************************************************************
    \brief
     Transpose the given matrix.
    \param[out] pResult
     The transposed matrix.
    \param[in] pMtx
     The input matrix to transpose.
    *************************************************************************/
    void Mtx33Transpose(Matrix3x3& pResult, const Matrix3x3& pMtx)
    {
        pResult.m00 = pMtx.m00; pResult.m01 = pMtx.m10; pResult.m02 = pMtx.m20;
        pResult.m10 = pMtx.m01; pResult.m11 = pMtx.m11; pResult.m12 = pMtx.m21;
        pResult.m20 = pMtx.m02; pResult.m21 = pMtx.m12; pResult.m22 = pMtx.m22;
    }

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
    void Mtx33Inverse(Matrix3x3* pResult, float* determinant, const Matrix3x3& pMtx)
    {
        // Calculate the determinant (det(M))
        *determinant = pMtx.m00 * (pMtx.m11 * pMtx.m22 - pMtx.m21 * pMtx.m12)
            - pMtx.m01 * (pMtx.m10 * pMtx.m22 - pMtx.m20 * pMtx.m12)
            + pMtx.m02 * (pMtx.m10 * pMtx.m21 - pMtx.m20 * pMtx.m11);

        // Check if matrix is invertible
        if (*determinant == 0.0f)
        {
            pResult = NULL;
            return;
        }

        // Calculate the inverse (1 / det(M) * adj(M))
        Matrix3x3 inverse;
        inverse.m00 = (pMtx.m11 * pMtx.m22 - pMtx.m21 * pMtx.m12) / *determinant;
        inverse.m01 = (pMtx.m21 * pMtx.m02 - pMtx.m01 * pMtx.m22) / *determinant;
        inverse.m02 = (pMtx.m01 * pMtx.m12 - pMtx.m11 * pMtx.m02) / *determinant;

        inverse.m10 = (pMtx.m20 * pMtx.m12 - pMtx.m10 * pMtx.m22) / *determinant;
        inverse.m11 = (pMtx.m00 * pMtx.m22 - pMtx.m20 * pMtx.m02) / *determinant;
        inverse.m12 = (pMtx.m10 * pMtx.m02 - pMtx.m00 * pMtx.m12) / *determinant;

        inverse.m20 = (pMtx.m10 * pMtx.m21 - pMtx.m20 * pMtx.m11) / *determinant;
        inverse.m21 = (pMtx.m20 * pMtx.m01 - pMtx.m00 * pMtx.m21) / *determinant;
        inverse.m22 = (pMtx.m00 * pMtx.m11 - pMtx.m10 * pMtx.m01) / *determinant;

        *pResult = inverse;
    }

    /*!***********************************************************************
       \brief
        Invert the given matrix and return the inverse.
       \param[in] pMtx
        The input matrix to invert.
       \return
        The inverse matrix. If the matrix is not invertible, returns an identity matrix.
       *************************************************************************/
    Matrix3x3 Mtx33GetInverse(const Matrix3x3& pMtx)
       {
        // Calculate the determinant (det(M))
        float determinant = pMtx.m00 * (pMtx.m11 * pMtx.m22 - pMtx.m21 * pMtx.m12)
            - pMtx.m01 * (pMtx.m10 * pMtx.m22 - pMtx.m20 * pMtx.m12)
            + pMtx.m02 * (pMtx.m10 * pMtx.m21 - pMtx.m20 * pMtx.m11);

        // Check if matrix is invertible
        if (determinant == 0.0f)
        {
         // Return identity matrix if not invertible
         Matrix3x3 identity;
         Mtx33Identity(identity);
         return identity;
        }

        // Calculate the inverse (1 / det(M) * adj(M))
        Matrix3x3 inverse;
        inverse.m00 = (pMtx.m11 * pMtx.m22 - pMtx.m21 * pMtx.m12) / determinant;
        inverse.m01 = (pMtx.m21 * pMtx.m02 - pMtx.m01 * pMtx.m22) / determinant;
        inverse.m02 = (pMtx.m01 * pMtx.m12 - pMtx.m11 * pMtx.m02) / determinant;

        inverse.m10 = (pMtx.m20 * pMtx.m12 - pMtx.m10 * pMtx.m22) / determinant;
        inverse.m11 = (pMtx.m00 * pMtx.m22 - pMtx.m20 * pMtx.m02) / determinant;
        inverse.m12 = (pMtx.m10 * pMtx.m02 - pMtx.m00 * pMtx.m12) / determinant;

        inverse.m20 = (pMtx.m10 * pMtx.m21 - pMtx.m20 * pMtx.m11) / determinant;
        inverse.m21 = (pMtx.m20 * pMtx.m01 - pMtx.m00 * pMtx.m21) / determinant;
        inverse.m22 = (pMtx.m00 * pMtx.m11 - pMtx.m10 * pMtx.m01) / determinant;

        return inverse;
       }
}