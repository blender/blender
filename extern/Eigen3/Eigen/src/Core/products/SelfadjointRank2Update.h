// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SELFADJOINTRANK2UPTADE_H
#define EIGEN_SELFADJOINTRANK2UPTADE_H

namespace internal {

/* Optimized selfadjoint matrix += alpha * uv' + conj(alpha)*vu'
 * It corresponds to the Level2 syr2 BLAS routine
 */

template<typename Scalar, typename Index, typename UType, typename VType, int UpLo>
struct selfadjoint_rank2_update_selector;

template<typename Scalar, typename Index, typename UType, typename VType>
struct selfadjoint_rank2_update_selector<Scalar,Index,UType,VType,Lower>
{
  static void run(Scalar* mat, Index stride, const UType& u, const VType& v, Scalar alpha)
  {
    const Index size = u.size();
    for (Index i=0; i<size; ++i)
    {
      Map<Matrix<Scalar,Dynamic,1> >(mat+stride*i+i, size-i) +=
                        (conj(alpha)  * conj(u.coeff(i))) * v.tail(size-i)
                      + (alpha * conj(v.coeff(i))) * u.tail(size-i);
    }
  }
};

template<typename Scalar, typename Index, typename UType, typename VType>
struct selfadjoint_rank2_update_selector<Scalar,Index,UType,VType,Upper>
{
  static void run(Scalar* mat, Index stride, const UType& u, const VType& v, Scalar alpha)
  {
    const Index size = u.size();
    for (Index i=0; i<size; ++i)
      Map<Matrix<Scalar,Dynamic,1> >(mat+stride*i, i+1) +=
                        (conj(alpha)  * conj(u.coeff(i))) * v.head(i+1)
                      + (alpha * conj(v.coeff(i))) * u.head(i+1);
  }
};

template<bool Cond, typename T> struct conj_expr_if
  : conditional<!Cond, const T&,
      CwiseUnaryOp<scalar_conjugate_op<typename traits<T>::Scalar>,T> > {};

} // end namespace internal

template<typename MatrixType, unsigned int UpLo>
template<typename DerivedU, typename DerivedV>
SelfAdjointView<MatrixType,UpLo>& SelfAdjointView<MatrixType,UpLo>
::rankUpdate(const MatrixBase<DerivedU>& u, const MatrixBase<DerivedV>& v, Scalar alpha)
{
  typedef internal::blas_traits<DerivedU> UBlasTraits;
  typedef typename UBlasTraits::DirectLinearAccessType ActualUType;
  typedef typename internal::remove_all<ActualUType>::type _ActualUType;
  const ActualUType actualU = UBlasTraits::extract(u.derived());

  typedef internal::blas_traits<DerivedV> VBlasTraits;
  typedef typename VBlasTraits::DirectLinearAccessType ActualVType;
  typedef typename internal::remove_all<ActualVType>::type _ActualVType;
  const ActualVType actualV = VBlasTraits::extract(v.derived());

  // If MatrixType is row major, then we use the routine for lower triangular in the upper triangular case and
  // vice versa, and take the complex conjugate of all coefficients and vector entries.

  enum { IsRowMajor = (internal::traits<MatrixType>::Flags&RowMajorBit) ? 1 : 0 };
  Scalar actualAlpha = alpha * UBlasTraits::extractScalarFactor(u.derived())
                             * internal::conj(VBlasTraits::extractScalarFactor(v.derived()));
  if (IsRowMajor)
    actualAlpha = internal::conj(actualAlpha);

  internal::selfadjoint_rank2_update_selector<Scalar, Index,
    typename internal::remove_all<typename internal::conj_expr_if<IsRowMajor ^ UBlasTraits::NeedToConjugate,_ActualUType>::type>::type,
    typename internal::remove_all<typename internal::conj_expr_if<IsRowMajor ^ VBlasTraits::NeedToConjugate,_ActualVType>::type>::type,
    (IsRowMajor ? int(UpLo==Upper ? Lower : Upper) : UpLo)>
    ::run(_expression().const_cast_derived().data(),_expression().outerStride(),actualU,actualV,actualAlpha);

  return *this;
}

#endif // EIGEN_SELFADJOINTRANK2UPTADE_H
