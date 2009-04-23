// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_ARRAY_NORMS_H
#define EIGEN_ARRAY_NORMS_H

template<typename Derived, int p>
struct ei_lpNorm_selector
{
  typedef typename NumTraits<typename ei_traits<Derived>::Scalar>::Real RealScalar;
  inline static RealScalar run(const MatrixBase<Derived>& m)
  {
    return ei_pow(m.cwise().abs().cwise().pow(p).sum(), RealScalar(1)/p);
  }
};

template<typename Derived>
struct ei_lpNorm_selector<Derived, 1>
{
  inline static typename NumTraits<typename ei_traits<Derived>::Scalar>::Real run(const MatrixBase<Derived>& m)
  {
    return m.cwise().abs().sum();
  }
};

template<typename Derived>
struct ei_lpNorm_selector<Derived, 2>
{
  inline static typename NumTraits<typename ei_traits<Derived>::Scalar>::Real run(const MatrixBase<Derived>& m)
  {
    return m.norm();
  }
};

template<typename Derived>
struct ei_lpNorm_selector<Derived, Infinity>
{
  inline static typename NumTraits<typename ei_traits<Derived>::Scalar>::Real run(const MatrixBase<Derived>& m)
  {
    return m.cwise().abs().maxCoeff();
  }
};

/** \array_module
  * 
  * \returns the \f$ \ell^p \f$ norm of *this, that is, returns the p-th root of the sum of the p-th powers of the absolute values
  *          of the coefficients of *this. If \a p is the special value \a Eigen::Infinity, this function returns the \f$ \ell^p\infty \f$
  *          norm, that is the maximum of the absolute values of the coefficients of *this.
  *
  * \sa norm()
  */
template<typename Derived>
template<int p>
inline typename NumTraits<typename ei_traits<Derived>::Scalar>::Real MatrixBase<Derived>::lpNorm() const
{
  return ei_lpNorm_selector<Derived, p>::run(*this);
}

#endif // EIGEN_ARRAY_NORMS_H
