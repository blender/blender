// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_FUZZY_H
#define EIGEN_FUZZY_H

#ifndef EIGEN_LEGACY_COMPARES

/** \returns \c true if \c *this is approximately equal to \a other, within the precision
  * determined by \a prec.
  *
  * \note The fuzzy compares are done multiplicatively. Two vectors \f$ v \f$ and \f$ w \f$
  * are considered to be approximately equal within precision \f$ p \f$ if
  * \f[ \Vert v - w \Vert \leqslant p\,\min(\Vert v\Vert, \Vert w\Vert). \f]
  * For matrices, the comparison is done using the Hilbert-Schmidt norm (aka Frobenius norm
  * L2 norm).
  *
  * \note Because of the multiplicativeness of this comparison, one can't use this function
  * to check whether \c *this is approximately equal to the zero matrix or vector.
  * Indeed, \c isApprox(zero) returns false unless \c *this itself is exactly the zero matrix
  * or vector. If you want to test whether \c *this is zero, use ei_isMuchSmallerThan(const
  * RealScalar&, RealScalar) instead.
  *
  * \sa ei_isMuchSmallerThan(const RealScalar&, RealScalar) const
  */
template<typename Derived>
template<typename OtherDerived>
bool MatrixBase<Derived>::isApprox(
  const MatrixBase<OtherDerived>& other,
  typename NumTraits<Scalar>::Real prec
) const
{
  const typename ei_nested<Derived,2>::type nested(derived());
  const typename ei_nested<OtherDerived,2>::type otherNested(other.derived());
  return (nested - otherNested).cwise().abs2().sum() <= prec * prec * std::min(nested.cwise().abs2().sum(), otherNested.cwise().abs2().sum());
}

/** \returns \c true if the norm of \c *this is much smaller than \a other,
  * within the precision determined by \a prec.
  *
  * \note The fuzzy compares are done multiplicatively. A vector \f$ v \f$ is
  * considered to be much smaller than \f$ x \f$ within precision \f$ p \f$ if
  * \f[ \Vert v \Vert \leqslant p\,\vert x\vert. \f]
  *
  * For matrices, the comparison is done using the Hilbert-Schmidt norm. For this reason,
  * the value of the reference scalar \a other should come from the Hilbert-Schmidt norm
  * of a reference matrix of same dimensions.
  *
  * \sa isApprox(), isMuchSmallerThan(const MatrixBase<OtherDerived>&, RealScalar) const
  */
template<typename Derived>
bool MatrixBase<Derived>::isMuchSmallerThan(
  const typename NumTraits<Scalar>::Real& other,
  typename NumTraits<Scalar>::Real prec
) const
{
  return cwise().abs2().sum() <= prec * prec * other * other;
}

/** \returns \c true if the norm of \c *this is much smaller than the norm of \a other,
  * within the precision determined by \a prec.
  *
  * \note The fuzzy compares are done multiplicatively. A vector \f$ v \f$ is
  * considered to be much smaller than a vector \f$ w \f$ within precision \f$ p \f$ if
  * \f[ \Vert v \Vert \leqslant p\,\Vert w\Vert. \f]
  * For matrices, the comparison is done using the Hilbert-Schmidt norm.
  *
  * \sa isApprox(), isMuchSmallerThan(const RealScalar&, RealScalar) const
  */
template<typename Derived>
template<typename OtherDerived>
bool MatrixBase<Derived>::isMuchSmallerThan(
  const MatrixBase<OtherDerived>& other,
  typename NumTraits<Scalar>::Real prec
) const
{
  return this->cwise().abs2().sum() <= prec * prec * other.cwise().abs2().sum();
}

#else

template<typename Derived, typename OtherDerived=Derived, bool IsVector=Derived::IsVectorAtCompileTime>
struct ei_fuzzy_selector;

/** \returns \c true if \c *this is approximately equal to \a other, within the precision
  * determined by \a prec.
  *
  * \note The fuzzy compares are done multiplicatively. Two vectors \f$ v \f$ and \f$ w \f$
  * are considered to be approximately equal within precision \f$ p \f$ if
  * \f[ \Vert v - w \Vert \leqslant p\,\min(\Vert v\Vert, \Vert w\Vert). \f]
  * For matrices, the comparison is done on all columns.
  *
  * \note Because of the multiplicativeness of this comparison, one can't use this function
  * to check whether \c *this is approximately equal to the zero matrix or vector.
  * Indeed, \c isApprox(zero) returns false unless \c *this itself is exactly the zero matrix
  * or vector. If you want to test whether \c *this is zero, use ei_isMuchSmallerThan(const
  * RealScalar&, RealScalar) instead.
  *
  * \sa ei_isMuchSmallerThan(const RealScalar&, RealScalar) const
  */
template<typename Derived>
template<typename OtherDerived>
bool MatrixBase<Derived>::isApprox(
  const MatrixBase<OtherDerived>& other,
  typename NumTraits<Scalar>::Real prec
) const
{
  return ei_fuzzy_selector<Derived,OtherDerived>::isApprox(derived(), other.derived(), prec);
}

/** \returns \c true if the norm of \c *this is much smaller than \a other,
  * within the precision determined by \a prec.
  *
  * \note The fuzzy compares are done multiplicatively. A vector \f$ v \f$ is
  * considered to be much smaller than \f$ x \f$ within precision \f$ p \f$ if
  * \f[ \Vert v \Vert \leqslant p\,\vert x\vert. \f]
  * For matrices, the comparison is done on all columns.
  *
  * \sa isApprox(), isMuchSmallerThan(const MatrixBase<OtherDerived>&, RealScalar) const
  */
template<typename Derived>
bool MatrixBase<Derived>::isMuchSmallerThan(
  const typename NumTraits<Scalar>::Real& other,
  typename NumTraits<Scalar>::Real prec
) const
{
  return ei_fuzzy_selector<Derived>::isMuchSmallerThan(derived(), other, prec);
}

/** \returns \c true if the norm of \c *this is much smaller than the norm of \a other,
  * within the precision determined by \a prec.
  *
  * \note The fuzzy compares are done multiplicatively. A vector \f$ v \f$ is
  * considered to be much smaller than a vector \f$ w \f$ within precision \f$ p \f$ if
  * \f[ \Vert v \Vert \leqslant p\,\Vert w\Vert. \f]
  * For matrices, the comparison is done on all columns.
  *
  * \sa isApprox(), isMuchSmallerThan(const RealScalar&, RealScalar) const
  */
template<typename Derived>
template<typename OtherDerived>
bool MatrixBase<Derived>::isMuchSmallerThan(
  const MatrixBase<OtherDerived>& other,
  typename NumTraits<Scalar>::Real prec
) const
{
  return ei_fuzzy_selector<Derived,OtherDerived>::isMuchSmallerThan(derived(), other.derived(), prec);
}


template<typename Derived, typename OtherDerived>
struct ei_fuzzy_selector<Derived,OtherDerived,true>
{
  typedef typename Derived::RealScalar RealScalar;
  static bool isApprox(const Derived& self, const OtherDerived& other, RealScalar prec)
  {
    EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Derived,OtherDerived)
    ei_assert(self.size() == other.size());
    return((self - other).squaredNorm() <= std::min(self.squaredNorm(), other.squaredNorm()) * prec * prec);
  }
  static bool isMuchSmallerThan(const Derived& self, const RealScalar& other, RealScalar prec)
  {
    return(self.squaredNorm() <= ei_abs2(other * prec));
  }
  static bool isMuchSmallerThan(const Derived& self, const OtherDerived& other, RealScalar prec)
  {
    EIGEN_STATIC_ASSERT_SAME_VECTOR_SIZE(Derived,OtherDerived)
    ei_assert(self.size() == other.size());
    return(self.squaredNorm() <= other.squaredNorm() * prec * prec);
  }
};

template<typename Derived, typename OtherDerived>
struct ei_fuzzy_selector<Derived,OtherDerived,false>
{
  typedef typename Derived::RealScalar RealScalar;
  static bool isApprox(const Derived& self, const OtherDerived& other, RealScalar prec)
  {
    EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Derived,OtherDerived)
    ei_assert(self.rows() == other.rows() && self.cols() == other.cols());
    typename Derived::Nested nested(self);
    typename OtherDerived::Nested otherNested(other);
    for(int i = 0; i < self.cols(); ++i)
      if((nested.col(i) - otherNested.col(i)).squaredNorm()
          > std::min(nested.col(i).squaredNorm(), otherNested.col(i).squaredNorm()) * prec * prec)
        return false;
    return true;
  }
  static bool isMuchSmallerThan(const Derived& self, const RealScalar& other, RealScalar prec)
  {
    typename Derived::Nested nested(self);
    for(int i = 0; i < self.cols(); ++i)
      if(nested.col(i).squaredNorm() > ei_abs2(other * prec))
        return false;
    return true;
  }
  static bool isMuchSmallerThan(const Derived& self, const OtherDerived& other, RealScalar prec)
  {
    EIGEN_STATIC_ASSERT_SAME_MATRIX_SIZE(Derived,OtherDerived)
    ei_assert(self.rows() == other.rows() && self.cols() == other.cols());
    typename Derived::Nested nested(self);
    typename OtherDerived::Nested otherNested(other);
    for(int i = 0; i < self.cols(); ++i)
      if(nested.col(i).squaredNorm() > otherNested.col(i).squaredNorm() * prec * prec)
        return false;
    return true;
  }
};

#endif

#endif // EIGEN_FUZZY_H
