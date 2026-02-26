/* Start Header ************************************************************************/
/*!
\file       MathUtils.tpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       Sept 02, 2025
\brief      This file contains the definition of the math utility functions.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

namespace Ermine
{
    /*!***********************************************************************
    \brief
     Get the min of two values.
    \param[in] T
     The type of input values.
    \param[in] lhs
     The first value.
    \param[in] rhs
     The second value.
    \return
     The smaller value of the two values.
    *************************************************************************/
    template <typename T>
    T min(const T& lhs, const T& rhs)
    {
        return (lhs < rhs) ? lhs : rhs;
    }

    /*!***********************************************************************
    \brief
     Get the max of two values.
    \param[in] T
     The type of input values.
    \param[in] lhs
     The first value.
    \param[in] rhs
     The second value.
    \return
     The larger value of the two values.
    *************************************************************************/
    template <typename T>
    T max(const T& lhs, const T& rhs)
    {
        return (lhs > rhs) ? lhs : rhs;
    }

    /*!***********************************************************************
    \brief
     Get the absolute value of x.
    \param[in] T
     The type of input values.
    \param[in] x
     The input value to absolute.
    \return
     The absolute value.
    *************************************************************************/
    template<typename T>
    T abs(const T& x)
    {
        return x >= 0 ? x : -x;
    }

    /*!***********************************************************************
    \brief
     Clamp a value between a minimum and maximum range.
    \tparam[in] T
     The type of input values.
    \param[in] value
     The value to be clamped.
    \param[in] min
     The minimum value.
    \param[in] max
     The maximum value.
    \return
     The clamped value within the range.
    *************************************************************************/
    template<typename T>
    T clamp(const T& value, const T& min, const T& max)
    {
        if (value < min)
            return min;
        else if (value > max) 
            return max;
        else
            return value;
    }

    /*!***********************************************************************
    \brief
     Convert a value from radian to degree.
    \tparam[in] T
     The type of input value.
    \param[in] radian
     The value to be converted.
    \return
     The value in degree.
    *************************************************************************/
    template<typename T>
    T degree(const T& radian)
    {
        return radian * (static_cast<T>(180.0) / PI<T>);
    }

    /*!***********************************************************************
    \brief
     Convert a value from degree to radian.
    \tparam[in] T
     The type of input value.
    \param[in] degree
     The value to be converted.
    \return
     The value in radian.
    *************************************************************************/
    template<typename T>
    T radian(const T& degree)
    {
        return degree * PIOVER180<T>;
    }
}