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

#ifndef EIGEN_SELFADJOINT_PRODUCT_H
#define EIGEN_SELFADJOINT_PRODUCT_H

/**********************************************************************
* This file implements a self adjoint product: C += A A^T updating only
* half of the selfadjoint matrix C.
* It corresponds to the level 3 SYRK and level 2 SYR Blas routines.
**********************************************************************/

template<typename Scalar, typename Index, int StorageOrder, int UpLo, bool ConjLhs, bool ConjRhs>
struct selfadjoint_rank1_update;

template<typename Scalar, typename Index, int UpLo, bool ConjLhs, bool ConjRhs>
struct selfadjoint_rank1_update<Scalar,Index,ColMajor,UpLo,ConjLhs,ConjRhs>
{
  static void run(Index size, Scalar* mat, Index stride, const Scalar* vec, Scalar alpha)
  {
    internal::conj_if<ConjRhs> cj;
    typedef Map<const Matrix<Scalar,Dynamic,1> > OtherMap;
    typedef typename internal::conditional<ConjLhs,typename OtherMap::ConjugateReturnType,const OtherMap&>::type ConjRhsType;
    for (Index i=0; i<size; ++i)
    {
      Map<Matrix<Scalar,Dynamic,1> >(mat+stride*i+(UpLo==Lower ? i : 0), (UpLo==Lower ? size-i : (i+1)))
          += (alpha * cj(vec[i])) * ConjRhsType(OtherMap(vec+(UpLo==Lower ? i : 0),UpLo==Lower ? size-i : (i+1)));
    }
  }
};

template<typename Scalar, typename Index, int UpLo, bool ConjLhs, bool ConjRhs>
struct selfadjoint_rank1_update<Scalar,Index,RowMajor,UpLo,ConjLhs,ConjRhs>
{
  static void run(Index size, Scalar* mat, Index stride, const Scalar* vec, Scalar alpha)
  {
    selfadjoint_rank1_update<Scalar,Index,ColMajor,UpLo==Lower?Upper:Lower,ConjRhs,ConjLhs>::run(size,mat,stride,vec,alpha);
  }
};

template<typename MatrixType, typename OtherType, int UpLo, bool OtherIsVector = OtherType::IsVectorAtCompileTime>
struct selfadjoint_product_selector;

template<typename MatrixType, typename OtherType, int UpLo>
struct selfadjoint_product_selector<MatrixType,OtherType,UpLo,true>
{
  static void run(MatrixType& mat, const OtherType& other, typename MatrixType::Scalar alpha)
  {
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef internal::blas_traits<OtherType> OtherBlasTraits;
    typedef typename OtherBlasTraits::DirectLinearAccessType ActualOtherType;
    typedef typename internal::remove_all<ActualOtherType>::type _ActualOtherType;
    const ActualOtherType actualOther = OtherBlasTraits::extract(other.derived());

    Scalar actualAlpha = alpha * OtherBlasTraits::extractScalarFactor(other.derived());

    enum {
      StorageOrder = (internal::traits<MatrixType>::Flags&RowMajorBit) ? RowMajor : ColMajor,
      UseOtherDirectly = _ActualOtherType::InnerStrideAtCompileTime==1
    };
    internal::gemv_static_vector_if<Scalar,OtherType::SizeAtCompileTime,OtherType::MaxSizeAtCompileTime,!UseOtherDirectly> static_other;
    
    bool freeOtherPtr = false;
    Scalar* actualOtherPtr;
    if(UseOtherDirectly)
      actualOtherPtr = const_cast<Scalar*>(actualOther.data());
    else
    {
      if((actualOtherPtr=static_other.data())==0)
      {
        freeOtherPtr = true;
        actualOtherPtr = ei_aligned_stack_new(Scalar,other.size());
      }
      Map<typename _ActualOtherType::PlainObject>(actualOtherPtr, actualOther.size()) = actualOther;
    }
    
    selfadjoint_rank1_update<Scalar,Index,StorageOrder,UpLo,
                              OtherBlasTraits::NeedToConjugate  && NumTraits<Scalar>::IsComplex,
                            (!OtherBlasTraits::NeedToConjugate) && NumTraits<Scalar>::IsComplex>
          ::run(other.size(), mat.data(), mat.outerStride(), actualOtherPtr, actualAlpha);
    
    if((!UseOtherDirectly) && freeOtherPtr) ei_aligned_stack_delete(Scalar, actualOtherPtr, other.size());
  }
};

template<typename MatrixType, typename OtherType, int UpLo>
struct selfadjoint_product_selector<MatrixType,OtherType,UpLo,false>
{
  static void run(MatrixType& mat, const OtherType& other, typename MatrixType::Scalar alpha)
  {
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef internal::blas_traits<OtherType> OtherBlasTraits;
    typedef typename OtherBlasTraits::DirectLinearAccessType ActualOtherType;
    typedef typename internal::remove_all<ActualOtherType>::type _ActualOtherType;
    const ActualOtherType actualOther = OtherBlasTraits::extract(other.derived());

    Scalar actualAlpha = alpha * OtherBlasTraits::extractScalarFactor(other.derived());

    enum { IsRowMajor = (internal::traits<MatrixType>::Flags&RowMajorBit) ? 1 : 0 };
    
    internal::general_matrix_matrix_triangular_product<Index,
      Scalar, _ActualOtherType::Flags&RowMajorBit ? RowMajor : ColMajor,   OtherBlasTraits::NeedToConjugate  && NumTraits<Scalar>::IsComplex,
      Scalar, _ActualOtherType::Flags&RowMajorBit ? ColMajor : RowMajor, (!OtherBlasTraits::NeedToConjugate) && NumTraits<Scalar>::IsComplex,
      MatrixType::Flags&RowMajorBit ? RowMajor : ColMajor, UpLo>
      ::run(mat.cols(), actualOther.cols(),
            &actualOther.coeffRef(0,0), actualOther.outerStride(), &actualOther.coeffRef(0,0), actualOther.outerStride(),
            mat.data(), mat.outerStride(), actualAlpha);
  }
};

// high level API

template<typename MatrixType, unsigned int UpLo>
template<typename DerivedU>
SelfAdjointView<MatrixType,UpLo>& SelfAdjointView<MatrixType,UpLo>
::rankUpdate(const MatrixBase<DerivedU>& u, Scalar alpha)
{
  selfadjoint_product_selector<MatrixType,DerivedU,UpLo>::run(_expression().const_cast_derived(), u.derived(), alpha);

  return *this;
}

#endif // EIGEN_SELFADJOINT_PRODUCT_H
