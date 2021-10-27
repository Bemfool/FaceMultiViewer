#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include "Eigen/Core"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace utils
{


template <typename T>
Eigen::Matrix<T, 4, 4> scale(const Eigen::Matrix<T, 4, 4>& m, T s)
{
    Eigen::Matrix<T, 4, 4> mul;
    mul << s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1;
    return m * mul;
}


template <typename T>
Eigen::Matrix<T, 4, 4> scale(const Eigen::Matrix<T, 4, 4>& m, T sx, T sy, T sz)
{
    Eigen::Matrix<T, 4, 4> mul;
    mul << sx, 0, 0, 0,
        0, sy, 0, 0,
        0, 0, sz, 0,
        0, 0, 0, 1;
    return m * mul;
}


} 


#endif // MATH_UTILS_H