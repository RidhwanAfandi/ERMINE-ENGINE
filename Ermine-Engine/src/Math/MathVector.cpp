/* Start Header ************************************************************************/
/*!
\file       MathVector.cpp
\author     Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu
\co-authors WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sept 02, 2025
\brief      This file contains the definition of the Math structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "MathVector.h"
#include <cmath>

namespace Ermine
{
  /**********************************Vector2D***************************************/
#pragma region Vector2D

#pragma region Operators
  /*!***********************************************************************
  \brief
   Addition assignment operator.
  \param[in] rhs
   The vector to add.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector2D &Vector2D::operator+=(const Vector2D &rhs)
  {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Subtraction assignment operator.
  \param[in] rhs
   The vector to subtract.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector2D &Vector2D::operator-=(const Vector2D &rhs)
  {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Multiplication assignment operator.
  \param[in] rhs
   The scalar to multiply.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector2D &Vector2D::operator*=(float rhs)
  {
    x *= rhs;
    y *= rhs;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Division assignment operator.
  \param[in] rhs
   The scalar to divide.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector2D &Vector2D::operator/=(float rhs)
  {
    if (rhs != 0.0f)
    {
      x /= rhs;
      y /= rhs;
    }
    return *this;
  }

  /*!***********************************************************************
  \brief
   Unary negation operator.
  \return
   The negated vector.
  *************************************************************************/
  Vector2D Vector2D::operator-() const
  {
    return Vector2D(-x, -y);
  }

  /*!***********************************************************************
  \brief
   Get the perpendicular vector. To get the normal of an edge.
  \return
   The perpendicular vector.
  *************************************************************************/
  Vector2D Vector2D::perpendicular() const
  {
    return Vector2D(y, -x);
  }
#pragma endregion Operators

#pragma region Binary operators
  /*!***********************************************************************
  \brief
   Binary addition operator for two vectors.
  \param[in] lhs
   The first vector to add.
  \param[in] rhs
   The second vector to add.
  \return
   The vector of the sum of the two vectors.
  *************************************************************************/
  Vector2D operator+(const Vector2D &lhs, const Vector2D &rhs)
  {
    return Vector2D(lhs.x + rhs.x, lhs.y + rhs.y);
  }

  /*!***********************************************************************
  \brief
   Binary subtraction operator for two vectors.
  \param[in] lhs
   The first vector to subtract.
  \param[in] rhs
   The second vector to subtract.
  \return
   The vector of the difference of the two vectors.
  *************************************************************************/
  Vector2D operator-(const Vector2D &lhs, const Vector2D &rhs)
  {
    return Vector2D(lhs.x - rhs.x, lhs.y - rhs.y);
  }

  /*!***********************************************************************
  \brief
   Scalar multiplication operator for a vector and a scalar.
  \param[in] lhs
   The vector to multiply.
  \param[in] rhs
   The scalar to multiply.
  \return
   The vector of the product of the vector and scalar.
  *************************************************************************/
  Vector2D operator*(const Vector2D &lhs, float rhs)
  {
    return Vector2D(lhs.x * rhs, lhs.y * rhs);
  }

  /*!***********************************************************************
  \brief
   Scalar multiplication operator for a scalar and a vector.
  \param[in] lhs
   The scalar to multiply.
  \param[in] rhs
   The vector to multiply.
  \return
   The vector of the product of the scalar and vector.
  *************************************************************************/
  Vector2D operator*(float lhs, const Vector2D &rhs)
  {
    return Vector2D(lhs * rhs.x, lhs * rhs.y);
  }

  /*!***********************************************************************
  \brief
   Scalar division operator for a vector and a scalar.
  \param[in] lhs
   The vector to divide.
  \param[in] rhs
   The scalar to divide.
  \return
   The vector of the division of the vector and scalar.
  *************************************************************************/
  Vector2D operator/(const Vector2D &lhs, float rhs)
  {
    if (rhs == 0.0f)
      return lhs;
    return Vector2D(lhs.x / rhs, lhs.y / rhs);
  }

  /*!***********************************************************************
  \brief
   Equality operator for two Vector2D instances.
  \param[in] lhs
   The first vector to compare.
  \param[in] rhs
   The second vector to compare.
  \return
   True if both x and y components of lhs and rhs are equal, false otherwise.
  *************************************************************************/
  bool operator==(const Vector2D &lhs, const Vector2D &rhs)
  {
    return lhs.x == rhs.x && lhs.y == rhs.y;
  }

  /*!***********************************************************************
  \brief
   Inequality operator for two Vector2D instances.
  \param[in] lhs
   The first vector to compare.
  \param[in] rhs
   The second vector to compare.
  \return
   True if any component of lhs and rhs differ, false if both are equal.
  *************************************************************************/
  bool operator!=(const Vector2D &lhs, const Vector2D &rhs)
  {
    return !(lhs == rhs);
  }

#pragma endregion Binary operators

#pragma region Utility functions
  /*!***********************************************************************
  \brief
   Normalize a vector.
  \param[out] pResult
   The normalized vector.
  \param[in] vec
   The input vector.
  *************************************************************************/
  void Vec2Normalize(Vector2D &pResult, const Vector2D &vec)
  {
    float length = Vec2Length(vec);
    if (length != 0.0f)
    {
      pResult.x = vec.x / length;
      pResult.y = vec.y / length;
    }
  }

  /*!***********************************************************************
  \brief
   Calculate the length of a vector.
  \param[in] vec
   The input vector.
  \return
   The length of the vector.
  *************************************************************************/
  float Vec2Length(const Vector2D &vec)
  {
    return sqrt(vec.x * vec.x + vec.y * vec.y);
  }

  /*!***********************************************************************
  \brief
   Calculate the squared length of a vector.
  \param[in] vec
   The input vector.
  \return
   The squared length of the vector.
  *************************************************************************/
  float Vec2SquareLength(const Vector2D &vec)
  {
    return vec.x * vec.x + vec.y * vec.y;
  }

  /*!***********************************************************************
  \brief
   Calculate the distance of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The distance of the two vectors.
  *************************************************************************/
  float Vec2Distance(const Vector2D &lhs, const Vector2D &rhs)
  {
    return Vec2Length(lhs - rhs);
  }

  /*!***********************************************************************
  \brief
   Calculate the squared distance of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The squared distance of the two vectors.
  *************************************************************************/
  float Vec2SquareDistance(const Vector2D &lhs, const Vector2D &rhs)
  {
    return Vec2SquareLength(lhs - rhs);
  }

  /*!***********************************************************************
  \brief
   Calculate the dot product of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The dot product of the two vectors.
  *************************************************************************/
  float Vec2DotProduct(const Vector2D &lhs, const Vector2D &rhs)
  {
    return lhs.x * rhs.x + lhs.y * rhs.y;
  }

  /*!***********************************************************************
  \brief
   Calculate the magnitude of the cross product of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The magnitude of the cross product of the two vectors.
  *************************************************************************/
  float Vec2CrossProductMagnitude(const Vector2D &lhs, const Vector2D &rhs)
  {
    return lhs.x * rhs.y - lhs.y * rhs.x;
  }

  /*!***********************************************************************
  \brief
   Rotate a vector by the given angle.
  \param[in] vec
   The vector to rotate.
  \param[in] angle
   The given angle to rotate by.
  \return
   The rotated vector.
  *************************************************************************/
  Vector2D Vec2Rotate(const Vector2D &vec, const float angle)
  {
    // x' = x * cos(theta) - y * sin(theta)
    // y' = x * sin(theta) - y * cos(theta)
    return {
        vec.x * std::cos(angle) - vec.y * std::sin(angle),
        vec.x * std::sin(angle) + vec.y * std::cos(angle)};
  }

  /*!***********************************************************************
  \brief
   Linearly interpolates between two vectors.
  \param[in] a
   The starting vector.
  \param[in] b
   The ending vector.
  \param[in] t
   Interpolation factor (0.0 = a, 1.0 = b).
  \return
   The interpolated vector.
  *************************************************************************/
  Vector2D Vec2Lerp(const Vector2D &a, const Vector2D &b, float t)
  {
    return Vector2D(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t);
  }

  /*!***********************************************************************
  \brief
   Clamps each component of a vector to a given range.
  \param[in] v
   The input vector.
  \param[in] minVal
   Minimum value for each component.
  \param[in] maxVal
   Maximum value for each component.
  \return
   A vector with each component clamped between minVal and maxVal.
  *************************************************************************/
  Vector2D Vec2Clamp(const Vector2D &v, float minVal, float maxVal)
  {
    return Vector2D(
        (v.x < minVal) ? minVal : (v.x > maxVal ? maxVal : v.x),
        (v.y < minVal) ? minVal : (v.y > maxVal ? maxVal : v.y));
  }

  /*!***********************************************************************
  \brief
   Calculates the angle (in radians) between two vectors.
  \param[in] a
   The first vector.
  \param[in] b
   The second vector.
  \return
   The angle in radians between vectors a and b.
  *************************************************************************/
  float Vec2Angle(const Vector2D &a, const Vector2D &b)
  {
    float dot = Vec2DotProduct(a, b);
    float len = Vec2Length(a) * Vec2Length(b);
    return (len == 0.0f) ? 0.0f : acos(dot / len);
  }
#pragma endregion Utility functions

#pragma endregion Vector2D

  /**********************************Vector2D***************************************/

  /**********************************Vector3D***************************************/
#pragma region Vector3D

#pragma region Operators
  /*!***********************************************************************
  \brief
   Addition assignment operator.
  \param[in] rhs
   The vector to add.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector3D &Vector3D::operator+=(const Vector3D &rhs)
  {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Subtraction assignment operator.
  \param[in] rhs
   The vector to subtract.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector3D &Vector3D::operator-=(const Vector3D &rhs)
  {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Multiplication assignment operator.
  \param[in] rhs
   The scalar to multiply.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector3D &Vector3D::operator*=(float rhs)
  {
    x *= rhs;
    y *= rhs;
    z *= rhs;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Division assignment operator.
  \param[in] rhs
   The scalar to divide.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector3D &Vector3D::operator/=(float rhs)
  {
    if (rhs != 0.0f)
    {
      x /= rhs;
      y /= rhs;
      z /= rhs;
    }
    return *this;
  }

  /*!***********************************************************************
  \brief
   Unary negation operator.
  \return
   The negated vector.
  *************************************************************************/
  Vector3D Vector3D::operator-() const
  {
    return Vector3D(-x, -y, -z);
  }
#pragma endregion Operators

#pragma region Binary operators
  /*!***********************************************************************
  \brief
   Binary addition operator for two vectors.
  \param[in] lhs
   The first vector to add.
  \param[in] rhs
   The second vector to add.
  \return
   The vector of the sum of the two vectors.
  *************************************************************************/
  Vector3D operator+(const Vector3D &lhs, const Vector3D &rhs)
  {
    return Vector3D(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
  }

  /*!***********************************************************************
  \brief
   Binary subtraction operator for two vectors.
  \param[in] lhs
   The first vector to subtract.
  \param[in] rhs
   The second vector to subtract.
  \return
   The vector of the difference of the two vectors.
  *************************************************************************/
  Vector3D operator-(const Vector3D &lhs, const Vector3D &rhs)
  {
    return Vector3D(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
  }

  /*!***********************************************************************
  \brief
   Scalar multiplication operator for a vector and a scalar.
  \param[in] lhs
   The vector to multiply.
  \param[in] rhs
   The scalar to multiply.
  \return
   The vector of the product of the vector and scalar.
  *************************************************************************/
  Vector3D operator*(const Vector3D &lhs, float rhs)
  {
    return Vector3D(lhs.x * rhs, lhs.y * rhs, lhs.z * rhs);
  }

  /*!***********************************************************************
  \brief
   Scalar multiplication operator for a scalar and a vector.
  \param[in] lhs
   The scalar to multiply.
  \param[in] rhs
   The vector to multiply.
  \return
   The vector of the product of the scalar and vector.
  *************************************************************************/
  Vector3D operator*(float lhs, const Vector3D &rhs)
  {
    return Vector3D(lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
  }

  /*!***********************************************************************
  \brief
   Scalar division operator for a vector and a scalar.
  \param[in] lhs
   The vector to divide.
  \param[in] rhs
   The scalar to divide.
  \return
   The vector of the division of the vector and scalar.
  *************************************************************************/
  Vector3D operator/(const Vector3D &lhs, float rhs)
  {
    if (rhs == 0.0f)
      return lhs;
    return Vector3D(lhs.x / rhs, lhs.y / rhs, lhs.z / rhs);
  }

  /*!***********************************************************************
  \brief
   Equality operator for two vectors.
  \param[in] lhs
   The first vector to compare.
  \param[in] rhs
   The second vector to compare.
  \return
   True if the two vectors are equal, false otherwise.
  *************************************************************************/
  bool operator==(const Vector3D &lhs, const Vector3D &rhs)
  {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
  }

  /*!***********************************************************************
  \brief
   Inequality operator for two vectors.
  \param[in] lhs
   The first vector to compare.
  \param[in] rhs
   The second vector to compare.
  \return
   True if the two vectors are not equal, false otherwise.
  *************************************************************************/
  bool operator!=(const Vector3D &lhs, const Vector3D &rhs)
  {
    return !(lhs == rhs);
  }
#pragma endregion Binary operators

#pragma region Utility functions
  /*!***********************************************************************
  \brief
   Normalize a vector.
  \param[out] pResult
   The normalized vector.
  \param[in] vec
   The input vector.
  *************************************************************************/
  void Vec3Normalize(Vector3D &pResult, const Vector3D &vec)
  {
    float length = Vec3Length(vec);
    if (length != 0.0f)
    {
      pResult.x = vec.x / length;
      pResult.y = vec.y / length;
      pResult.z = vec.z / length;
    }
  }

  /*!***********************************************************************
  \brief
   Calculate the length of a vector.
  \param[in] vec
   The input vector.
  \return
   The length of the vector.
  *************************************************************************/
  float Vec3Length(const Vector3D &vec)
  {
    return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
  }

  /*!***********************************************************************
  \brief
   Calculate the squared length of a vector.
  \param[in] vec
   The input vector.
  \return
   The squared length of the vector.
  *************************************************************************/
  float Vec3SquareLength(const Vector3D &vec)
  {
    return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
  }

  /*!***********************************************************************
  \brief
   Calculate the distance of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The distance of the two vectors.
  *************************************************************************/
  float Vec3Distance(const Vector3D &lhs, const Vector3D &rhs)
  {
    return Vec3Length(lhs - rhs);
  }

  /*!***********************************************************************
  \brief
   Calculate the squared distance of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The squared distance of the two vectors.
  *************************************************************************/
  float Vec3SquareDistance(const Vector3D &lhs, const Vector3D &rhs)
  {
    return Vec3SquareLength(lhs - rhs);
  }

  /*!***********************************************************************
  \brief
   Calculate the dot product of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The dot product of the two vectors.
  *************************************************************************/
  float Vec3DotProduct(const Vector3D &lhs, const Vector3D &rhs)
  {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
  }

  /*!***********************************************************************
  \brief
   Calculate the magnitude of the cross product of two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  \return
   The magnitude of the cross product of the two vectors.
  *************************************************************************/
  float Vec3CrossProductMagnitude(const Vector3D &lhs, const Vector3D &rhs)
  {
    Vector3D cross_product = Vector3D(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x);
    return Vec3Length(cross_product);
  }

  /*!***********************************************************************
  \brief
   Calculate the cross product of two vectors.
  \param[out] pResult
   The cross product of the two vectors.
  \param[in] lhs
   The first vector.
  \param[in] rhs
   The second vector.
  *************************************************************************/
  void Vec3CrossProduct(Vector3D &pResult, const Vector3D &lhs, const Vector3D &rhs)
  {
    pResult.x = lhs.y * rhs.z - lhs.z * rhs.y;
    pResult.y = lhs.z * rhs.x - lhs.x * rhs.z;
    pResult.z = lhs.x * rhs.y - lhs.y * rhs.x;
  }

  /*!***********************************************************************
  \brief
   Linearly interpolates between two 3D vectors.
  \param[in] a
   The starting vector.
  \param[in] b
   The ending vector.
  \param[in] t
   The interpolation factor (0.0f = a, 1.0f = b).
  \return
   The interpolated vector.
  *************************************************************************/
  Vector3D Vec3Lerp(const Vector3D &a, const Vector3D &b, float t)
  {
    return a + (b - a) * t;
  }

  /*!***********************************************************************
  \brief
   Clamps the components of a 3D vector between a minimum and maximum value.
  \param[in] v
   The input vector.
  \param[in] minVal
   The minimum value to clamp to.
  \param[in] maxVal
   The maximum value to clamp to.
  \return
   The clamped vector.
  *************************************************************************/
  Vector3D Vec3Clamp(const Vector3D &v, float minVal, float maxVal)
  {
    auto clampf = [](float val, float minV, float maxV)
    {
      return (val < minV) ? minV : (val > maxV) ? maxV
                                                : val;
    };
    return Vector3D(
        clampf(v.x, minVal, maxVal),
        clampf(v.y, minVal, maxVal),
        clampf(v.z, minVal, maxVal));
  }

  /*!***********************************************************************
  \brief
   Calculates the angle in radians between two 3D vectors.
  \param[in] a
   The first vector.
  \param[in] b
   The second vector.
  \return
   The angle in radians between the two vectors.
  *************************************************************************/
  float Vec3Angle(const Vector3D &a, const Vector3D &b)
  {
    float dot = Vec3DotProduct(a, b);
    float lenA = Vec3Length(a);
    float lenB = Vec3Length(b);

    if (lenA == 0.0f || lenB == 0.0f)
      return 0.0f;

    float cosTheta = dot / (lenA * lenB);

    if (cosTheta < -1.0f)
      cosTheta = -1.0f;
    if (cosTheta > 1.0f)
      cosTheta = 1.0f;

    return acos(cosTheta);
  }

  /*!***********************************************************************
  \brief
   Reflects a vector about a given normal.
  \param[in] v
   The input vector to reflect.
  \param[in] normal
   The normal vector to reflect about (assumed to be normalized).
  \return
   The reflected vector.
  *************************************************************************/
  Vector3D Vec3Reflect(const Vector3D &v, const Vector3D &normal)
  {
    float dotVN = Vec3DotProduct(v, normal);
    return v - 2.0f * dotVN * normal;
  }

  /*!***********************************************************************
  \brief
   Projects one vector onto another.
  \param[in] v
   The vector to project.
  \param[in] onto
   The vector to project onto.
  \return
   The projection of the vector onto the other vector.
  *************************************************************************/
  Vector3D Vec3Project(const Vector3D &v, const Vector3D &onto)
  {
    float dotVV = Vec3DotProduct(onto, onto);
    if (dotVV == 0.0f)
      return Vector3D(0.0f, 0.0f, 0.0f);

    float scale = Vec3DotProduct(v, onto) / dotVV;
    return onto * scale;
  }
#pragma endregion Utility functions

#pragma endregion Vector3D
  /**********************************Vector3D***************************************/

  /**********************************Vector4D***************************************/

#pragma region Vector4D

#pragma region Binary operators
  /*!***********************************************************************
  \brief
   Addition assignment operator.
  \param[in] rhs
   The vector to add.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector4D &Vector4D::operator+=(const Vector4D &rhs)
  {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    w += rhs.w;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Subtraction assignment operator.
  \param[in] rhs
   The vector to subtract.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector4D &Vector4D::operator-=(const Vector4D &rhs)
  {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    w -= rhs.w;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Multiplication assignment operator.
  \param[in] rhs
   The scalar to multiply.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector4D &Vector4D::operator*=(float rhs)
  {
    x *= rhs;
    y *= rhs;
    z *= rhs;
    w *= rhs;
    return *this;
  }

  /*!***********************************************************************
  \brief
   Division assignment operator.
  \param[in] rhs
   The scalar to divide.
  \return
   A reference to the vector.
  *************************************************************************/
  Vector4D &Vector4D::operator/=(float rhs)
  {
    if (rhs != 0.0f)
    {
      x /= rhs;
      y /= rhs;
      z /= rhs;
      w /= rhs;
    }
    return *this;
  }

  /*!***********************************************************************
  \brief
   Unary negation operator.
  \return
   The negated vector.
  *************************************************************************/
  Vector4D Vector4D::operator-() const
  {
    return {-x, -y, -z, -w};
  }

  /*!***********************************************************************
  \brief
   Binary addition operator for two vectors.
  \param[in] lhs
   The first vector to add.
  \param[in] rhs
   The second vector to add.
  \return
   The vector of the sum of the two vectors.
  *************************************************************************/
  Vector4D operator+(const Vector4D &lhs, const Vector4D &rhs)
  {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
  }

  /*!***********************************************************************
  \brief
   Binary subtraction operator for two vectors.
  \param[in] lhs
   The first vector to subtract.
  \param[in] rhs
   The second vector to subtract.
  \return
   The vector of the difference of the two vectors.
  *************************************************************************/
  Vector4D operator-(const Vector4D &lhs, const Vector4D &rhs)
  {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};
  }

  /*!***********************************************************************
  \brief
   Scalar multiplication operator for a vector and a scalar.
  \param[in] lhs
   The vector to multiply.
  \param[in] rhs
   The scalar to multiply.
  \return
   The vector of the product of the vector and scalar.
  *************************************************************************/
  Vector4D operator*(const Vector4D &lhs, float rhs)
  {
    return {lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs};
  }

  /*!***********************************************************************
  \brief
   Scalar multiplication operator for a scalar and a vector.
  \param[in] lhs
   The scalar to multiply.
  \param[in] rhs
   The vector to multiply.
  \return
   The vector of the product of the scalar and vector.
  *************************************************************************/
  Vector4D operator*(float lhs, const Vector4D &rhs)
  {
    return {lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w};
  }

  /*!***********************************************************************
  \brief
   Scalar division operator for a vector and a scalar.
  \param[in] lhs
   The vector to divide.
  \param[in] rhs
   The scalar to divide.
  \return
   The vector of the division of the vector and scalar.
  *************************************************************************/
  Vector4D operator/(const Vector4D &lhs, float rhs)
  {
    if (rhs == 0.0f)
      return lhs;
    return {lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs};
  }
  /*!***********************************************************************
  \brief
    Equality operator for two 4D vectors.
  \param[in] lhs
    The first vector.
  \param[in] rhs
    The second vector.
  \return
    True if all components are equal, false otherwise.
  *************************************************************************/
  bool operator==(const Vector4D &lhs, const Vector4D &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w; }

  /*!***********************************************************************
  \brief
    Inequality operator for two 4D vectors.
  \param[in] lhs
    The first vector.
  \param[in] rhs
    The second vector.
  \return
    True if any component differs, false otherwise.
  *************************************************************************/
  bool operator!=(const Vector4D &lhs, const Vector4D &rhs) { return !(lhs == rhs); }

#pragma endregion Binary operators

#pragma region Utility functions
  /*!***********************************************************************
  \brief
    Normalize a 4D vector.
  \param[out] pResult
    The resulting normalized vector.
  \param[in] vec
    The vector to normalize.
  *************************************************************************/
  void Vec4Normalize(Vector4D &pResult, const Vector4D &vec)
  {
    float length = Vec4Length(vec);
    if (length != 0.0f)
    {
      pResult.x = vec.x / length;
      pResult.y = vec.y / length;
      pResult.z = vec.z / length;
      pResult.w = vec.w / length;
    }
  }

  /*!***********************************************************************
  \brief
    Compute the length (magnitude) of a 4D vector.
  \param[in] vec
    The input vector.
  \return
    The length of the vector.
  *************************************************************************/
  float Vec4Length(const Vector4D &vec) { return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w); }

  /*!***********************************************************************
  \brief
    Compute the squared length of a 4D vector.
  \param[in] vec
    The input vector.
  \return
    The squared length of the vector.
  *************************************************************************/
  float Vec4SquareLength(const Vector4D &vec) { return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z + vec.w * vec.w; }

  /*!***********************************************************************
  \brief
    Compute the distance between two 4D vectors.
  \param[in] lhs
    The first vector.
  \param[in] rhs
    The second vector.
  \return
    The distance between the two vectors.
  *************************************************************************/
  float Vec4Distance(const Vector4D &lhs, const Vector4D &rhs) { return Vec4Length(lhs - rhs); }

  /*!***********************************************************************
  \brief
    Compute the squared distance between two 4D vectors.
  \param[in] lhs
    The first vector.
  \param[in] rhs
    The second vector.
  \return
    The squared distance between the two vectors.
  *************************************************************************/
  float Vec4SquareDistance(const Vector4D &lhs, const Vector4D &rhs) { return Vec4SquareLength(lhs - rhs); }

  /*!***********************************************************************
  \brief
    Compute the dot product of two 4D vectors.
  \param[in] lhs
    The first vector.
  \param[in] rhs
    The second vector.
  \return
    The dot product of the two vectors.
  *************************************************************************/
  float Vec4DotProduct(const Vector4D &lhs, const Vector4D &rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w; }

  /*!***********************************************************************
  \brief
    Linear interpolation between two 4D vectors.
  \param[in] a
    The start vector.
  \param[in] b
    The end vector.
  \param[in] t
    Interpolation factor (0..1).
  \return
    The interpolated vector.
  *************************************************************************/
  Vector4D Vec4Lerp(const Vector4D &a, const Vector4D &b, float t) { return a + (b - a) * t; }

  /*!***********************************************************************
  \brief
    Clamp each component of a 4D vector between a minimum and maximum value.
  \param[in] v
    The vector to clamp.
  \param[in] minVal
    Minimum allowed value.
  \param[in] maxVal
    Maximum allowed value.
  \return
    The clamped vector.
  *************************************************************************/
  Vector4D Vec4Clamp(const Vector4D &v, float minVal, float maxVal)
  {
    Vector4D result;
    result.x = (v.x < minVal) ? minVal : (v.x > maxVal) ? maxVal
                                                        : v.x;
    result.y = (v.y < minVal) ? minVal : (v.y > maxVal) ? maxVal
                                                        : v.y;
    result.z = (v.z < minVal) ? minVal : (v.z > maxVal) ? maxVal
                                                        : v.z;
    result.w = (v.w < minVal) ? minVal : (v.w > maxVal) ? maxVal
                                                        : v.w;
    return result;
  }

  /*!***********************************************************************
  \brief
    Compute the angle between two 4D vectors.
  \param[in] a
    The first vector.
  \param[in] b
    The second vector.
  \return
    The angle in radians between the two vectors.
  *************************************************************************/
  float Vec4Angle(const Vector4D &a, const Vector4D &b)
  {
    float dot = Vec4DotProduct(a, b);
    float lengths = Vec4Length(a) * Vec4Length(b);
    return acos(dot / lengths);
  }

  /*!***********************************************************************
  \brief
    Project a 4D vector onto another 4D vector.
  \param[in] v
    The vector to project.
  \param[in] onto
    The vector to project onto.
  \return
    The projected vector.
  *************************************************************************/
  Vector4D Vec4Project(const Vector4D &v, const Vector4D &onto)
  {
    float scale = Vec4DotProduct(v, onto) / Vec4DotProduct(onto, onto);
    return onto * scale;
  }

  /*!***********************************************************************
  \brief
    Reflect a 4D vector about a normal vector.
  \param[in] v
    The vector to reflect.
  \param[in] normal
    The normal vector to reflect about.
  \return
    The reflected vector.
  *************************************************************************/
  Vector4D Vec4Reflect(const Vector4D &v, const Vector4D &normal)
  {
    return v - 2.0f * Vec4DotProduct(v, normal) * normal;
  }

#pragma endregion Utility functions

#pragma endregion Vector4D
  /**********************************Vector4D***************************************/
}
