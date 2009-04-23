// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_EULERANGLES_H
#define EIGEN_EULERANGLES_H

/** \geometry_module \ingroup Geometry_Module
  * \nonstableyet
  *
  * \returns the Euler-angles of the rotation matrix \c *this using the convention defined by the triplet (\a a0,\a a1,\a a2)
  *
  * Each of the three parameters \a a0,\a a1,\a a2 represents the respective rotation axis as an integer in {0,1,2}.
  * For instance, in:
  * \code Vector3f ea = mat.eulerAngles(2, 0, 2); \endcode
  * "2" represents the z axis and "0" the x axis, etc. The returned angles are such that
  * we have the following equality:
  * \code
  * mat == AngleAxisf(ea[0], Vector3f::UnitZ())
  *      * AngleAxisf(ea[1], Vector3f::UnitX())
  *      * AngleAxisf(ea[2], Vector3f::UnitZ()); \endcode
  * This corresponds to the right-multiply conventions (with right hand side frames).
  */
template<typename Derived>
inline Matrix<typename MatrixBase<Derived>::Scalar,3,1>
MatrixBase<Derived>::eulerAngles(int a0, int a1, int a2) const
{
  /* Implemented from Graphics Gems IV */
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Derived,3,3)

  Matrix<Scalar,3,1> res;
  typedef Matrix<typename Derived::Scalar,2,1> Vector2;
  const Scalar epsilon = precision<Scalar>();

  const int odd = ((a0+1)%3 == a1) ? 0 : 1;
  const int i = a0;
  const int j = (a0 + 1 + odd)%3;
  const int k = (a0 + 2 - odd)%3;

  if (a0==a2)
  {
    Scalar s = Vector2(coeff(j,i) , coeff(k,i)).norm();
    res[1] = std::atan2(s, coeff(i,i));
    if (s > epsilon)
    {
      res[0] = std::atan2(coeff(j,i), coeff(k,i));
      res[2] = std::atan2(coeff(i,j),-coeff(i,k));
    }
    else
    {
      res[0] = Scalar(0);
      res[2] = (coeff(i,i)>0?1:-1)*std::atan2(-coeff(k,j), coeff(j,j));
    }
  }
  else
  {
    Scalar c = Vector2(coeff(i,i) , coeff(i,j)).norm();
    res[1] = std::atan2(-coeff(i,k), c);
    if (c > epsilon)
    {
      res[0] = std::atan2(coeff(j,k), coeff(k,k));
      res[2] = std::atan2(coeff(i,j), coeff(i,i));
    }
    else
    {
      res[0] = Scalar(0);
      res[2] = (coeff(i,k)>0?1:-1)*std::atan2(-coeff(k,j), coeff(j,j));
    }
  }
  if (!odd)
    res = -res;
  return res;
}


#endif // EIGEN_EULERANGLES_H
