#pragma once
/* Start Header ************************************************************************/
/*!
\file       MathVector.h
\author     Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu
\co-authors WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sept 02, 2025
\brief      This file contains the declaration of the Math structure.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
namespace Ermine
{
#ifdef _MSC_VER
    // Supress warning: nonstandard extension used : nameless struct/union
#pragma warning( disable : 4201 )
#endif

    /**********************************Vector2D***************************************/
#pragma region Vector2D

    /*!***********************************************************************
    \brief
     Represents a 2D vector with x and y components, or as an array.
    *************************************************************************/
    typedef union Vector2D
    {
        struct
        {
            float x, y;
        };

        float m[2];

        /*!***********************************************************************
        \brief
         Default constructor.
        *************************************************************************/
        Vector2D() : m{ 0.0f, 0.0f } {}

        /*!***********************************************************************
        \brief
         Constructor with x and y parameters.
        \param[in] x
         The x coordinate.
        \param[in] y
         The y coordinate.
        *************************************************************************/
        Vector2D(float x, float y) : m{ x, y } {}

        /*!***********************************************************************
        \brief
         Copy constructor.
        \param[in] rhs
         The vector to copy.
        *************************************************************************/
        Vector2D(const Vector2D& rhs) = default;

        /*!***********************************************************************
        \brief
         Copy assignment operator.
        \param[in] rhs
         The vector to copy.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector2D& operator=(const Vector2D& rhs) = default;

        /*!***********************************************************************
        \brief
         Addition assignment operator.
        \param[in] rhs
         The vector to add.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector2D& operator+=(const Vector2D& rhs);

        /*!***********************************************************************
        \brief
         Subtraction assignment operator.
        \param[in] rhs
         The vector to subtract.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector2D& operator-=(const Vector2D& rhs);

        /*!***********************************************************************
        \brief
         Multiplication assignment operator.
        \param[in] rhs
         The scalar to multiply.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector2D& operator*=(float rhs);

        /*!***********************************************************************
        \brief
         Division assignment operator.
        \param[in] rhs
         The scalar to divide.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector2D& operator/=(float rhs);

        /*!***********************************************************************
        \brief
         Unary negation operator.
        \return
         The negated vector.
        *************************************************************************/
        Vector2D operator-() const;

        /*!***********************************************************************
        \brief
         Get the perpendicular vector. To get the normal of an edge.
        \return
         The perpendicular vector.
        *************************************************************************/
        Vector2D perpendicular() const;

    } Vector2D, Vec2;

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
    Vector2D operator+(const Vector2D& lhs, const Vector2D& rhs);

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
    Vector2D operator-(const Vector2D& lhs, const Vector2D& rhs);

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
    Vector2D operator*(const Vector2D& lhs, float rhs);

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
    Vector2D operator*(float lhs, const Vector2D& rhs);

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
    Vector2D operator/(const Vector2D& lhs, float rhs);

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
    bool operator==(const Vector2D& lhs, const Vector2D& rhs);

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
    bool operator!=(const Vector2D& lhs, const Vector2D& rhs);
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
    void Vec2Normalize(Vector2D& pResult, const Vector2D& vec);

    /*!***********************************************************************
    \brief
     Calculate the length of a vector.
    \param[in] vec
     The input vector.
    \return
     The length of the vector.
    *************************************************************************/
    float Vec2Length(const Vector2D& vec);

    /*!***********************************************************************
    \brief
     Calculate the squared length of a vector.
    \param[in] vec
     The input vector.
    \return
     The squared length of the vector.
    *************************************************************************/
    float Vec2SquareLength(const Vector2D& vec);

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
    float Vec2Distance(const Vector2D& lhs, const Vector2D& rhs);

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
    float Vec2SquareDistance(const Vector2D& lhs, const Vector2D& rhs);

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
    float Vec2DotProduct(const Vector2D& lhs, const Vector2D& rhs);

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
    float Vec2CrossProductMagnitude(const Vector2D& lhs, const Vector2D& rhs);

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
    Vector2D Vec2Rotate(const Vector2D& vec, const float angle);
    
    /*!***********************************************************************
    \brief
     Linearly interpolates between two vectors.
    \param[in] a
     The starting vector.
    \param[in] b
     The ending vector.
    \param[in] t
     The interpolation factor (0.0f = a, 1.0f = b).
    \return
     The interpolated vector.
    *************************************************************************/
    Vector2D Vec2Lerp(const Vector2D& a, const Vector2D& b, float t);
    
    
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
    Vector2D Vec2Clamp(const Vector2D& v, float minVal, float maxVal);
    
    
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
    float Vec2Angle(const Vector2D& a, const Vector2D& b);
#pragma endregion Utility functions

#pragma endregion Vector2D
    /**********************************Vector2D***************************************/

    /**********************************Vector3D***************************************/
#pragma region Vector3D

    /*!***********************************************************************
    \brief
     Represents a 3D vector with x, y and z components, or as an array.
    *************************************************************************/
    typedef union Vector3D
    {
        struct
        {
            float x, y, z;
        };

        float m[3];

        /*!***********************************************************************
        \brief
         Default constructor.
        *************************************************************************/
        Vector3D() : m{ 0.0f, 0.0f, 0.0f } {}

        /*!***********************************************************************
        \brief
         Constructor with x and y parameters.
        \param[in] x
         The x coordinate.
        \param[in] y
         The y coordinate.
        \param[in] z
         The z coordinate.
        *************************************************************************/
        Vector3D(float x, float y, float z) : m{ x, y, z } {}

        /*!***********************************************************************
        \brief
         Copy constructor.
        \param[in] rhs
         The vector to copy.
        *************************************************************************/
        Vector3D(const Vector3D& rhs) = default;

        /*!***********************************************************************
        \brief
         Copy assignment operator.
        \param[in] rhs
         The vector to copy.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector3D& operator=(const Vector3D& rhs) = default;

        /*!***********************************************************************
        \brief
         Addition assignment operator.
        \param[in] rhs
         The vector to add.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector3D& operator+=(const Vector3D& rhs);

        /*!***********************************************************************
        \brief
         Subtraction assignment operator.
        \param[in] rhs
         The vector to subtract.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector3D& operator-=(const Vector3D& rhs);

        /*!***********************************************************************
        \brief
         Multiplication assignment operator.
        \param[in] rhs
         The scalar to multiply.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector3D& operator*=(float rhs);

        /*!***********************************************************************
        \brief
         Division assignment operator.
        \param[in] rhs
         The scalar to divide.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector3D& operator/=(float rhs);

        /*!***********************************************************************
        \brief
         Unary negation operator.
        \return
         The negated vector.
        *************************************************************************/
        Vector3D operator-() const;

    } Vector3D, Vec3;

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
    Vector3D operator+(const Vector3D& lhs, const Vector3D& rhs);

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
    Vector3D operator-(const Vector3D& lhs, const Vector3D& rhs);

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
    Vector3D operator*(const Vector3D& lhs, float rhs);

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
    Vector3D operator*(float lhs, const Vector3D& rhs);

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
    Vector3D operator/(const Vector3D& lhs, float rhs);

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
    bool operator==(const Vector3D& lhs, const Vector3D& rhs);

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
    bool operator!=(const Vector3D& lhs, const Vector3D& rhs);

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
    void Vec3Normalize(Vector3D& pResult, const Vector3D& vec);

    /*!***********************************************************************
    \brief
     Calculate the length of a vector.
    \param[in] vec
     The input vector.
    \return
     The length of the vector.
    *************************************************************************/
    float Vec3Length(const Vector3D& vec);

    /*!***********************************************************************
    \brief
     Calculate the squared length of a vector.
    \param[in] vec
     The input vector.
    \return
     The squared length of the vector.
    *************************************************************************/
    float Vec3SquareLength(const Vector3D& vec);

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
    float Vec3Distance(const Vector3D& lhs, const Vector3D& rhs);

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
    float Vec3SquareDistance(const Vector3D& lhs, const Vector3D& rhs);

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
    float Vec3DotProduct(const Vector3D& lhs, const Vector3D& rhs);

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
    float Vec3CrossProductMagnitude(const Vector3D& lhs, const Vector3D& rhs);

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
    void Vec3CrossProduct(Vector3D& pResult, const Vector3D& lhs, const Vector3D& rhs);

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
    Vector3D Vec3Lerp(const Vector3D& a, const Vector3D& b, float t);

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
    Vector3D Vec3Clamp(const Vector3D& v, float minVal, float maxVal);

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
    float Vec3Angle(const Vector3D& a, const Vector3D& b);

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
    Vector3D Vec3Reflect(const Vector3D& v, const Vector3D& normal);

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
    Vector3D Vec3Project(const Vector3D& v, const Vector3D& onto);

#pragma endregion Utility functions

#pragma endregion Vector3D
    /**********************************Vector3D***************************************/

    /**********************************Vector4D***************************************/
#pragma region Vector4D
    /*!***********************************************************************
    \brief
     Represents a 4D vector with x, y, z and w components, or as an array.
    *************************************************************************/
    typedef union Vector4D
    {

        struct
        {
            float x, y, z, w;
        };

        float m[4];

        /*!***********************************************************************
        \brief
         Default constructor.
        *************************************************************************/
        Vector4D() : m{ 0.0f, 0.0f, 0.0f, 0.0f } {}

        /*!***********************************************************************
        \brief
         Constructor with x and y parameters.
        \param[in] x
         The x coordinate.
        \param[in] y
         The y coordinate.
        \param[in] z
         The z coordinate.
        *************************************************************************/
        Vector4D(float x, float y, float z, float w) : m{ x, y, z, w } {}

        /*!***********************************************************************
        \brief
         Copy constructor.
        \param[in] rhs
         The vector to copy.
        *************************************************************************/
        Vector4D(const Vector4D& rhs) = default;

        /*!***********************************************************************
        \brief
         Copy assignment operator.
        \param[in] rhs
         The vector to copy.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector4D& operator=(const Vector4D& rhs) = default;

        /*!***********************************************************************
           \brief
            Addition assignment operator.
           \param[in] rhs
            The vector to add.
           \return
            A reference to the vector.
           *************************************************************************/
        Vector4D& operator+=(const Vector4D& rhs);

        /*!***********************************************************************
        \brief
         Subtraction assignment operator.
        \param[in] rhs
         The vector to subtract.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector4D& operator-=(const Vector4D& rhs);

        /*!***********************************************************************
        \brief
         Multiplication assignment operator.
        \param[in] rhs
         The scalar to multiply.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector4D& operator*=(float rhs);

        /*!***********************************************************************
        \brief
         Division assignment operator.
        \param[in] rhs
         The scalar to divide.
        \return
         A reference to the vector.
        *************************************************************************/
        Vector4D& operator/=(float rhs);

        /*!***********************************************************************
        \brief
         Unary negation operator.
        \return
         The negated vector.
        *************************************************************************/
        Vector4D operator-() const;
    } Vector4D, Vec4;

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
    Vector4D operator+(const Vector4D& lhs, const Vector4D& rhs);

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
    Vector4D operator-(const Vector4D& lhs, const Vector4D& rhs);

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
    Vector4D operator*(const Vector4D& lhs, float rhs);

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
    Vector4D operator*(float lhs, const Vector4D& rhs);

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
    Vector4D operator/(const Vector4D& lhs, float rhs);

    /*!***********************************************************************
    \brief
     Compares two vectors for equality.
    \param[in] lhs
     The first vector.
    \param[in] rhs
     The second vector.
    \return
     True if all components are equal, false otherwise.
    *************************************************************************/
    bool operator==(const Vector4D& lhs, const Vector4D& rhs);
    /*!***********************************************************************
\brief
 Compares two vectors for inequality.
\param[in] lhs
 The first vector.
\param[in] rhs
 The second vector.
\return
 True if any component differs, false otherwise.
*************************************************************************/
    bool operator!=(const Vector4D& lhs, const Vector4D& rhs);

#pragma endregion Binary operators

#pragma region Utility functions
    /*!***********************************************************************
    \brief
     Normalizes a vector.
    \param[out] pResult
     The normalized vector.
    \param[in] vec
     The input vector.
    *************************************************************************/
    void Vec4Normalize(Vector4D& pResult, const Vector4D& vec);

    /*!***********************************************************************
    \brief
     Computes the length (magnitude) of a vector.
    \param[in] vec
     The input vector.
    \return
     The length of the vector.
    *************************************************************************/
    float Vec4Length(const Vector4D& vec);

    /*!***********************************************************************
    \brief
     Computes the squared length of a vector.
    \param[in] vec
     The input vector.
    \return
     The squared length of the vector.
    *************************************************************************/
    float Vec4SquareLength(const Vector4D& vec);

    /*!***********************************************************************
    \brief
     Computes the distance between two vectors.
    \param[in] lhs
     The first vector.
    \param[in] rhs
     The second vector.
    \return
     The Euclidean distance between lhs and rhs.
    *************************************************************************/
    float Vec4Distance(const Vector4D& lhs, const Vector4D& rhs);

    /*!***********************************************************************
    \brief
     Computes the squared distance between two vectors.
    \param[in] lhs
     The first vector.
    \param[in] rhs
     The second vector.
    \return
     The squared Euclidean distance between lhs and rhs.
    *************************************************************************/
    float Vec4SquareDistance(const Vector4D& lhs, const Vector4D& rhs);

    /*!***********************************************************************
    \brief
     Computes the dot product of two vectors.
    \param[in] lhs
     The first vector.
    \param[in] rhs
     The second vector.
    \return
     The dot product of lhs and rhs.
    *************************************************************************/
    float Vec4DotProduct(const Vector4D& lhs, const Vector4D& rhs);

    /*!***********************************************************************
    \brief
     Linearly interpolates between two vectors.
    \param[in] a
     The start vector.
    \param[in] b
     The end vector.
    \param[in] t
     Interpolation factor (0.0f to 1.0f).
    \return
     The interpolated vector.
    *************************************************************************/
    Vector4D Vec4Lerp(const Vector4D& a, const Vector4D& b, float t);

    /*!***********************************************************************
    \brief
     Clamps each component of a vector between minVal and maxVal.
    \param[in] v
     The input vector.
    \param[in] minVal
     Minimum value.
    \param[in] maxVal
     Maximum value.
    \return
     The clamped vector.
    *************************************************************************/
    Vector4D Vec4Clamp(const Vector4D& v, float minVal, float maxVal);

    /*!***********************************************************************
    \brief
     Computes the angle (in radians) between two vectors.
    \param[in] a
     The first vector.
    \param[in] b
     The second vector.
    \return
     The angle between the vectors in radians.
    *************************************************************************/
    float Vec4Angle(const Vector4D& a, const Vector4D& b);

    /*!***********************************************************************
    \brief
     Projects vector v onto vector onto.
    \param[in] v
     The vector to project.
    \param[in] onto
     The vector to project onto.
    \return
     The projected vector.
    *************************************************************************/
    Vector4D Vec4Project(const Vector4D& v, const Vector4D& onto);

    /*!***********************************************************************
    \brief
     Reflects vector v about the given normal.
    \param[in] v
     The input vector.
    \param[in] normal
     The normal vector to reflect about.
    \return
     The reflected vector.
    *************************************************************************/
    Vector4D Vec4Reflect(const Vector4D& v, const Vector4D& normal);

#pragma endregion Utility functions

#pragma endregion Vector4D
    /**********************************Vector4D***************************************/

    /*!***********************************************************************
    \brief
     Converts an angle from radians to degrees.
    \param[in] radians
     The angle in radians.
    \return
     The angle in degrees.
    *************************************************************************/
    inline float Rad2Degree(float radians)
    {
        return radians * (180.0f / 3.14159265358979323846f);
    }

    /*!***********************************************************************
    \brief
     Converts an angle from degrees to radians.
    \param[in] degrees
     The angle in degrees.
    \return
     The angle in radians.
    *************************************************************************/
    inline float Degree2Rad(float degrees)
    {
        return degrees * (3.14159265358979323846f / 180.0f);
    }
}

#include "Matrix3x3.h"
#include "Matrix4x4.h"