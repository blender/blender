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

#ifndef EIGEN_ALLANDANY_H
#define EIGEN_ALLANDANY_H

template<typename Derived, int UnrollCount>
struct ei_all_unroller
{
  enum {
    col = (UnrollCount-1) / Derived::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived::RowsAtCompileTime
  };

  inline static bool run(const Derived &mat)
  {
    return ei_all_unroller<Derived, UnrollCount-1>::run(mat) && mat.coeff(row, col);
  }
};

template<typename Derived>
struct ei_all_unroller<Derived, 1>
{
  inline static bool run(const Derived &mat) { return mat.coeff(0, 0); }
};

template<typename Derived>
struct ei_all_unroller<Derived, Dynamic>
{
  inline static bool run(const Derived &) { return false; }
};

template<typename Derived, int UnrollCount>
struct ei_any_unroller
{
  enum {
    col = (UnrollCount-1) / Derived::RowsAtCompileTime,
    row = (UnrollCount-1) % Derived::RowsAtCompileTime
  };

  inline static bool run(const Derived &mat)
  {
    return ei_any_unroller<Derived, UnrollCount-1>::run(mat) || mat.coeff(row, col);
  }
};

template<typename Derived>
struct ei_any_unroller<Derived, 1>
{
  inline static bool run(const Derived &mat) { return mat.coeff(0, 0); }
};

template<typename Derived>
struct ei_any_unroller<Derived, Dynamic>
{
  inline static bool run(const Derived &) { return false; }
};

/** \array_module
  * 
  * \returns true if all coefficients are true
  *
  * \addexample CwiseAll \label How to check whether a point is inside a box (using operator< and all())
  *
  * Example: \include MatrixBase_all.cpp
  * Output: \verbinclude MatrixBase_all.out
  *
  * \sa MatrixBase::any(), Cwise::operator<()
  */
template<typename Derived>
inline bool MatrixBase<Derived>::all() const
{
  const bool unroll = SizeAtCompileTime * (CoeffReadCost + NumTraits<Scalar>::AddCost)
                      <= EIGEN_UNROLLING_LIMIT;
  if(unroll)
    return ei_all_unroller<Derived,
                           unroll ? int(SizeAtCompileTime) : Dynamic
     >::run(derived());
  else
  {
    for(int j = 0; j < cols(); ++j)
      for(int i = 0; i < rows(); ++i)
        if (!coeff(i, j)) return false;
    return true;
  }
}

/** \array_module
  * 
  * \returns true if at least one coefficient is true
  *
  * \sa MatrixBase::all()
  */
template<typename Derived>
inline bool MatrixBase<Derived>::any() const
{
  const bool unroll = SizeAtCompileTime * (CoeffReadCost + NumTraits<Scalar>::AddCost)
                      <= EIGEN_UNROLLING_LIMIT;
  if(unroll)
    return ei_any_unroller<Derived,
                           unroll ? int(SizeAtCompileTime) : Dynamic
           >::run(derived());
  else
  {
    for(int j = 0; j < cols(); ++j)
      for(int i = 0; i < rows(); ++i)
        if (coeff(i, j)) return true;
    return false;
  }
}

/** \array_module
  * 
  * \returns the number of coefficients which evaluate to true
  *
  * \sa MatrixBase::all(), MatrixBase::any()
  */
template<typename Derived>
inline int MatrixBase<Derived>::count() const
{
  return this->cast<bool>().cast<int>().sum();
}

#endif // EIGEN_ALLANDANY_H
