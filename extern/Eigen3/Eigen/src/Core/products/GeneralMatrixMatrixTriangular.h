// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_GENERAL_MATRIX_MATRIX_TRIANGULAR_H
#define EIGEN_GENERAL_MATRIX_MATRIX_TRIANGULAR_H

namespace internal {

/**********************************************************************
* This file implements a general A * B product while
* evaluating only one triangular part of the product.
* This is more general version of self adjoint product (C += A A^T)
* as the level 3 SYRK Blas routine.
**********************************************************************/

// forward declarations (defined at the end of this file)
template<typename LhsScalar, typename RhsScalar, typename Index, int mr, int nr, bool ConjLhs, bool ConjRhs, int UpLo>
struct tribb_kernel;
  
/* Optimized matrix-matrix product evaluating only one triangular half */
template <typename Index,
          typename LhsScalar, int LhsStorageOrder, bool ConjugateLhs,
          typename RhsScalar, int RhsStorageOrder, bool ConjugateRhs,
                              int ResStorageOrder, int  UpLo>
struct general_matrix_matrix_triangular_product;

// as usual if the result is row major => we transpose the product
template <typename Index, typename LhsScalar, int LhsStorageOrder, bool ConjugateLhs,
                          typename RhsScalar, int RhsStorageOrder, bool ConjugateRhs, int  UpLo>
struct general_matrix_matrix_triangular_product<Index,LhsScalar,LhsStorageOrder,ConjugateLhs,RhsScalar,RhsStorageOrder,ConjugateRhs,RowMajor,UpLo>
{  
  typedef typename scalar_product_traits<LhsScalar, RhsScalar>::ReturnType ResScalar;
  static EIGEN_STRONG_INLINE void run(Index size, Index depth,const LhsScalar* lhs, Index lhsStride,
                                      const RhsScalar* rhs, Index rhsStride, ResScalar* res, Index resStride, ResScalar alpha)
  {
    general_matrix_matrix_triangular_product<Index,
        RhsScalar, RhsStorageOrder==RowMajor ? ColMajor : RowMajor, ConjugateRhs,
        LhsScalar, LhsStorageOrder==RowMajor ? ColMajor : RowMajor, ConjugateLhs,
        ColMajor, UpLo==Lower?Upper:Lower>
      ::run(size,depth,rhs,rhsStride,lhs,lhsStride,res,resStride,alpha);
  }
};

template <typename Index, typename LhsScalar, int LhsStorageOrder, bool ConjugateLhs,
                          typename RhsScalar, int RhsStorageOrder, bool ConjugateRhs, int  UpLo>
struct general_matrix_matrix_triangular_product<Index,LhsScalar,LhsStorageOrder,ConjugateLhs,RhsScalar,RhsStorageOrder,ConjugateRhs,ColMajor,UpLo>
{
  typedef typename scalar_product_traits<LhsScalar, RhsScalar>::ReturnType ResScalar;
  static EIGEN_STRONG_INLINE void run(Index size, Index depth,const LhsScalar* _lhs, Index lhsStride,
                                      const RhsScalar* _rhs, Index rhsStride, ResScalar* res, Index resStride, ResScalar alpha)
  {
    const_blas_data_mapper<LhsScalar, Index, LhsStorageOrder> lhs(_lhs,lhsStride);
    const_blas_data_mapper<RhsScalar, Index, RhsStorageOrder> rhs(_rhs,rhsStride);

    typedef gebp_traits<LhsScalar,RhsScalar> Traits;

    Index kc = depth; // cache block size along the K direction
    Index mc = size;  // cache block size along the M direction
    Index nc = size;  // cache block size along the N direction
    computeProductBlockingSizes<LhsScalar,RhsScalar>(kc, mc, nc);
    // !!! mc must be a multiple of nr:
    if(mc > Traits::nr)
      mc = (mc/Traits::nr)*Traits::nr;

    std::size_t sizeW = kc*Traits::WorkSpaceFactor;
    std::size_t sizeB = sizeW + kc*size;
    ei_declare_aligned_stack_constructed_variable(LhsScalar, blockA, kc*mc, 0);
    ei_declare_aligned_stack_constructed_variable(RhsScalar, allocatedBlockB, sizeB, 0);
    RhsScalar* blockB = allocatedBlockB + sizeW;
    
    gemm_pack_lhs<LhsScalar, Index, Traits::mr, Traits::LhsProgress, LhsStorageOrder> pack_lhs;
    gemm_pack_rhs<RhsScalar, Index, Traits::nr, RhsStorageOrder> pack_rhs;
    gebp_kernel <LhsScalar, RhsScalar, Index, Traits::mr, Traits::nr, ConjugateLhs, ConjugateRhs> gebp;
    tribb_kernel<LhsScalar, RhsScalar, Index, Traits::mr, Traits::nr, ConjugateLhs, ConjugateRhs, UpLo> sybb;

    for(Index k2=0; k2<depth; k2+=kc)
    {
      const Index actual_kc = std::min(k2+kc,depth)-k2;

      // note that the actual rhs is the transpose/adjoint of mat
      pack_rhs(blockB, &rhs(k2,0), rhsStride, actual_kc, size);

      for(Index i2=0; i2<size; i2+=mc)
      {
        const Index actual_mc = std::min(i2+mc,size)-i2;

        pack_lhs(blockA, &lhs(i2, k2), lhsStride, actual_kc, actual_mc);

        // the selected actual_mc * size panel of res is split into three different part:
        //  1 - before the diagonal => processed with gebp or skipped
        //  2 - the actual_mc x actual_mc symmetric block => processed with a special kernel
        //  3 - after the diagonal => processed with gebp or skipped
        if (UpLo==Lower)
          gebp(res+i2, resStride, blockA, blockB, actual_mc, actual_kc, std::min(size,i2), alpha,
               -1, -1, 0, 0, allocatedBlockB);

        sybb(res+resStride*i2 + i2, resStride, blockA, blockB + actual_kc*i2, actual_mc, actual_kc, alpha, allocatedBlockB);

        if (UpLo==Upper)
        {
          Index j2 = i2+actual_mc;
          gebp(res+resStride*j2+i2, resStride, blockA, blockB+actual_kc*j2, actual_mc, actual_kc, std::max(Index(0), size-j2), alpha,
               -1, -1, 0, 0, allocatedBlockB);
        }
      }
    }
  }
};

// Optimized packed Block * packed Block product kernel evaluating only one given triangular part
// This kernel is built on top of the gebp kernel:
// - the current destination block is processed per panel of actual_mc x BlockSize
//   where BlockSize is set to the minimal value allowing gebp to be as fast as possible
// - then, as usual, each panel is split into three parts along the diagonal,
//   the sub blocks above and below the diagonal are processed as usual,
//   while the triangular block overlapping the diagonal is evaluated into a
//   small temporary buffer which is then accumulated into the result using a
//   triangular traversal.
template<typename LhsScalar, typename RhsScalar, typename Index, int mr, int nr, bool ConjLhs, bool ConjRhs, int UpLo>
struct tribb_kernel
{
  typedef gebp_traits<LhsScalar,RhsScalar,ConjLhs,ConjRhs> Traits;
  typedef typename Traits::ResScalar ResScalar;
  
  enum {
    BlockSize  = EIGEN_PLAIN_ENUM_MAX(mr,nr)
  };
  void operator()(ResScalar* res, Index resStride, const LhsScalar* blockA, const RhsScalar* blockB, Index size, Index depth, ResScalar alpha, RhsScalar* workspace)
  {
    gebp_kernel<LhsScalar, RhsScalar, Index, mr, nr, ConjLhs, ConjRhs> gebp_kernel;
    Matrix<ResScalar,BlockSize,BlockSize,ColMajor> buffer;

    // let's process the block per panel of actual_mc x BlockSize,
    // again, each is split into three parts, etc.
    for (Index j=0; j<size; j+=BlockSize)
    {
      Index actualBlockSize = std::min<Index>(BlockSize,size - j);
      const RhsScalar* actual_b = blockB+j*depth;

      if(UpLo==Upper)
        gebp_kernel(res+j*resStride, resStride, blockA, actual_b, j, depth, actualBlockSize, alpha,
                    -1, -1, 0, 0, workspace);

      // selfadjoint micro block
      {
        Index i = j;
        buffer.setZero();
        // 1 - apply the kernel on the temporary buffer
        gebp_kernel(buffer.data(), BlockSize, blockA+depth*i, actual_b, actualBlockSize, depth, actualBlockSize, alpha,
                    -1, -1, 0, 0, workspace);
        // 2 - triangular accumulation
        for(Index j1=0; j1<actualBlockSize; ++j1)
        {
          ResScalar* r = res + (j+j1)*resStride + i;
          for(Index i1=UpLo==Lower ? j1 : 0;
              UpLo==Lower ? i1<actualBlockSize : i1<=j1; ++i1)
            r[i1] += buffer(i1,j1);
        }
      }

      if(UpLo==Lower)
      {
        Index i = j+actualBlockSize;
        gebp_kernel(res+j*resStride+i, resStride, blockA+depth*i, actual_b, size-i, depth, actualBlockSize, alpha,
                    -1, -1, 0, 0, workspace);
      }
    }
  }
};

} // end namespace internal

// high level API

template<typename MatrixType, unsigned int UpLo>
template<typename ProductDerived, typename _Lhs, typename _Rhs>
TriangularView<MatrixType,UpLo>& TriangularView<MatrixType,UpLo>::assignProduct(const ProductBase<ProductDerived, _Lhs,_Rhs>& prod, const Scalar& alpha)
{
  typedef typename internal::remove_all<typename ProductDerived::LhsNested>::type Lhs;
  typedef internal::blas_traits<Lhs> LhsBlasTraits;
  typedef typename LhsBlasTraits::DirectLinearAccessType ActualLhs;
  typedef typename internal::remove_all<ActualLhs>::type _ActualLhs;
  const ActualLhs actualLhs = LhsBlasTraits::extract(prod.lhs());
  
  typedef typename internal::remove_all<typename ProductDerived::RhsNested>::type Rhs;
  typedef internal::blas_traits<Rhs> RhsBlasTraits;
  typedef typename RhsBlasTraits::DirectLinearAccessType ActualRhs;
  typedef typename internal::remove_all<ActualRhs>::type _ActualRhs;
  const ActualRhs actualRhs = RhsBlasTraits::extract(prod.rhs());

  typename ProductDerived::Scalar actualAlpha = alpha * LhsBlasTraits::extractScalarFactor(prod.lhs().derived()) * RhsBlasTraits::extractScalarFactor(prod.rhs().derived());

  internal::general_matrix_matrix_triangular_product<Index,
    typename Lhs::Scalar, _ActualLhs::Flags&RowMajorBit ? RowMajor : ColMajor, LhsBlasTraits::NeedToConjugate,
    typename Rhs::Scalar, _ActualRhs::Flags&RowMajorBit ? RowMajor : ColMajor, RhsBlasTraits::NeedToConjugate,
    MatrixType::Flags&RowMajorBit ? RowMajor : ColMajor, UpLo>
    ::run(m_matrix.cols(), actualLhs.cols(),
          &actualLhs.coeffRef(0,0), actualLhs.outerStride(), &actualRhs.coeffRef(0,0), actualRhs.outerStride(),
          const_cast<Scalar*>(m_matrix.data()), m_matrix.outerStride(), actualAlpha);
  
  return *this;
}

#endif // EIGEN_GENERAL_MATRIX_MATRIX_TRIANGULAR_H
