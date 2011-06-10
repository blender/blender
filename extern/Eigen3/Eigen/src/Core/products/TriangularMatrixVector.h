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

#ifndef EIGEN_TRIANGULARMATRIXVECTOR_H
#define EIGEN_TRIANGULARMATRIXVECTOR_H

namespace internal {

template<typename Index, int Mode, typename LhsScalar, bool ConjLhs, typename RhsScalar, bool ConjRhs, int StorageOrder>
struct product_triangular_matrix_vector;

template<typename Index, int Mode, typename LhsScalar, bool ConjLhs, typename RhsScalar, bool ConjRhs>
struct product_triangular_matrix_vector<Index,Mode,LhsScalar,ConjLhs,RhsScalar,ConjRhs,ColMajor>
{
  typedef typename scalar_product_traits<LhsScalar, RhsScalar>::ReturnType ResScalar;
  enum {
    IsLower = ((Mode&Lower)==Lower),
    HasUnitDiag = (Mode & UnitDiag)==UnitDiag
  };
  static EIGEN_DONT_INLINE  void run(Index rows, Index cols, const LhsScalar* _lhs, Index lhsStride,
                                     const RhsScalar* _rhs, Index rhsIncr, ResScalar* _res, Index resIncr, ResScalar alpha)
  {
    static const Index PanelWidth = EIGEN_TUNE_TRIANGULAR_PANEL_WIDTH;

    typedef Map<const Matrix<LhsScalar,Dynamic,Dynamic,ColMajor>, 0, OuterStride<> > LhsMap;
    const LhsMap lhs(_lhs,rows,cols,OuterStride<>(lhsStride));
    typename conj_expr_if<ConjLhs,LhsMap>::type cjLhs(lhs);
    
    typedef Map<const Matrix<RhsScalar,Dynamic,1>, 0, InnerStride<> > RhsMap;
    const RhsMap rhs(_rhs,cols,InnerStride<>(rhsIncr));
    typename conj_expr_if<ConjRhs,RhsMap>::type cjRhs(rhs);

    typedef Map<Matrix<ResScalar,Dynamic,1> > ResMap;
    ResMap res(_res,rows);

    for (Index pi=0; pi<cols; pi+=PanelWidth)
    {
      Index actualPanelWidth = std::min(PanelWidth, cols-pi);
      for (Index k=0; k<actualPanelWidth; ++k)
      {
        Index i = pi + k;
        Index s = IsLower ? (HasUnitDiag ? i+1 : i ) : pi;
        Index r = IsLower ? actualPanelWidth-k : k+1;
        if ((!HasUnitDiag) || (--r)>0)
          res.segment(s,r) += (alpha * cjRhs.coeff(i)) * cjLhs.col(i).segment(s,r);
        if (HasUnitDiag)
          res.coeffRef(i) += alpha * cjRhs.coeff(i);
      }
      Index r = IsLower ? cols - pi - actualPanelWidth : pi;
      if (r>0)
      {
        Index s = IsLower ? pi+actualPanelWidth : 0;
        general_matrix_vector_product<Index,LhsScalar,ColMajor,ConjLhs,RhsScalar,ConjRhs>::run(
            r, actualPanelWidth,
            &lhs.coeffRef(s,pi), lhsStride,
            &rhs.coeffRef(pi), rhsIncr,
            &res.coeffRef(s), resIncr, alpha);
      }
    }
  }
};

template<typename Index, int Mode, typename LhsScalar, bool ConjLhs, typename RhsScalar, bool ConjRhs>
struct product_triangular_matrix_vector<Index,Mode,LhsScalar,ConjLhs,RhsScalar,ConjRhs,RowMajor>
{
  typedef typename scalar_product_traits<LhsScalar, RhsScalar>::ReturnType ResScalar;
  enum {
    IsLower = ((Mode&Lower)==Lower),
    HasUnitDiag = (Mode & UnitDiag)==UnitDiag
  };
  static void run(Index rows, Index cols, const LhsScalar* _lhs, Index lhsStride,
                  const RhsScalar* _rhs, Index rhsIncr, ResScalar* _res, Index resIncr, ResScalar alpha)
  {
    static const Index PanelWidth = EIGEN_TUNE_TRIANGULAR_PANEL_WIDTH;

    typedef Map<const Matrix<LhsScalar,Dynamic,Dynamic,RowMajor>, 0, OuterStride<> > LhsMap;
    const LhsMap lhs(_lhs,rows,cols,OuterStride<>(lhsStride));
    typename conj_expr_if<ConjLhs,LhsMap>::type cjLhs(lhs);

    typedef Map<const Matrix<RhsScalar,Dynamic,1> > RhsMap;
    const RhsMap rhs(_rhs,cols);
    typename conj_expr_if<ConjRhs,RhsMap>::type cjRhs(rhs);

    typedef Map<Matrix<ResScalar,Dynamic,1>, 0, InnerStride<> > ResMap;
    ResMap res(_res,rows,InnerStride<>(resIncr));
    
    for (Index pi=0; pi<cols; pi+=PanelWidth)
    {
      Index actualPanelWidth = std::min(PanelWidth, cols-pi);
      for (Index k=0; k<actualPanelWidth; ++k)
      {
        Index i = pi + k;
        Index s = IsLower ? pi  : (HasUnitDiag ? i+1 : i);
        Index r = IsLower ? k+1 : actualPanelWidth-k;
        if ((!HasUnitDiag) || (--r)>0)
          res.coeffRef(i) += alpha * (cjLhs.row(i).segment(s,r).cwiseProduct(cjRhs.segment(s,r).transpose())).sum();
        if (HasUnitDiag)
          res.coeffRef(i) += alpha * cjRhs.coeff(i);
      }
      Index r = IsLower ? pi : cols - pi - actualPanelWidth;
      if (r>0)
      {
        Index s = IsLower ? 0 : pi + actualPanelWidth;
        general_matrix_vector_product<Index,LhsScalar,RowMajor,ConjLhs,RhsScalar,ConjRhs>::run(
            actualPanelWidth, r,
            &lhs.coeffRef(pi,s), lhsStride,
            &rhs.coeffRef(s), rhsIncr,
            &res.coeffRef(pi), resIncr, alpha);
      }
    }
  }
};

/***************************************************************************
* Wrapper to product_triangular_vector
***************************************************************************/

template<int Mode, bool LhsIsTriangular, typename Lhs, typename Rhs>
struct traits<TriangularProduct<Mode,LhsIsTriangular,Lhs,false,Rhs,true> >
 : traits<ProductBase<TriangularProduct<Mode,LhsIsTriangular,Lhs,false,Rhs,true>, Lhs, Rhs> >
{};

template<int Mode, bool LhsIsTriangular, typename Lhs, typename Rhs>
struct traits<TriangularProduct<Mode,LhsIsTriangular,Lhs,true,Rhs,false> >
 : traits<ProductBase<TriangularProduct<Mode,LhsIsTriangular,Lhs,true,Rhs,false>, Lhs, Rhs> >
{};


template<int StorageOrder>
struct trmv_selector;

} // end namespace internal

template<int Mode, typename Lhs, typename Rhs>
struct TriangularProduct<Mode,true,Lhs,false,Rhs,true>
  : public ProductBase<TriangularProduct<Mode,true,Lhs,false,Rhs,true>, Lhs, Rhs >
{
  EIGEN_PRODUCT_PUBLIC_INTERFACE(TriangularProduct)

  TriangularProduct(const Lhs& lhs, const Rhs& rhs) : Base(lhs,rhs) {}

  template<typename Dest> void scaleAndAddTo(Dest& dst, Scalar alpha) const
  {
    eigen_assert(dst.rows()==m_lhs.rows() && dst.cols()==m_rhs.cols());
  
    internal::trmv_selector<(int(internal::traits<Lhs>::Flags)&RowMajorBit) ? RowMajor : ColMajor>::run(*this, dst, alpha);
  }
};

template<int Mode, typename Lhs, typename Rhs>
struct TriangularProduct<Mode,false,Lhs,true,Rhs,false>
  : public ProductBase<TriangularProduct<Mode,false,Lhs,true,Rhs,false>, Lhs, Rhs >
{
  EIGEN_PRODUCT_PUBLIC_INTERFACE(TriangularProduct)

  TriangularProduct(const Lhs& lhs, const Rhs& rhs) : Base(lhs,rhs) {}

  template<typename Dest> void scaleAndAddTo(Dest& dst, Scalar alpha) const
  {
    eigen_assert(dst.rows()==m_lhs.rows() && dst.cols()==m_rhs.cols());

    typedef TriangularProduct<(Mode & UnitDiag) | ((Mode & Lower) ? Upper : Lower),true,Transpose<const Rhs>,false,Transpose<const Lhs>,true> TriangularProductTranspose;
    Transpose<Dest> dstT(dst);
    internal::trmv_selector<(int(internal::traits<Rhs>::Flags)&RowMajorBit) ? ColMajor : RowMajor>::run(
      TriangularProductTranspose(m_rhs.transpose(),m_lhs.transpose()), dstT, alpha);
  }
};

namespace internal {

// TODO: find a way to factorize this piece of code with gemv_selector since the logic is exactly the same.
  
template<> struct trmv_selector<ColMajor>
{
  template<int Mode, typename Lhs, typename Rhs, typename Dest>
  static void run(const TriangularProduct<Mode,true,Lhs,false,Rhs,true>& prod, Dest& dest, typename TriangularProduct<Mode,true,Lhs,false,Rhs,true>::Scalar alpha)
  {
    typedef TriangularProduct<Mode,true,Lhs,false,Rhs,true> ProductType;
    typedef typename ProductType::Index Index;
    typedef typename ProductType::LhsScalar   LhsScalar;
    typedef typename ProductType::RhsScalar   RhsScalar;
    typedef typename ProductType::Scalar      ResScalar;
    typedef typename ProductType::RealScalar  RealScalar;
    typedef typename ProductType::ActualLhsType ActualLhsType;
    typedef typename ProductType::ActualRhsType ActualRhsType;
    typedef typename ProductType::LhsBlasTraits LhsBlasTraits;
    typedef typename ProductType::RhsBlasTraits RhsBlasTraits;
    typedef Map<Matrix<ResScalar,Dynamic,1>, Aligned> MappedDest;

    const ActualLhsType actualLhs = LhsBlasTraits::extract(prod.lhs());
    const ActualRhsType actualRhs = RhsBlasTraits::extract(prod.rhs());

    ResScalar actualAlpha = alpha * LhsBlasTraits::extractScalarFactor(prod.lhs())
                                  * RhsBlasTraits::extractScalarFactor(prod.rhs());

    enum {
      // FIXME find a way to allow an inner stride on the result if packet_traits<Scalar>::size==1
      // on, the other hand it is good for the cache to pack the vector anyways...
      EvalToDestAtCompileTime = Dest::InnerStrideAtCompileTime==1,
      ComplexByReal = (NumTraits<LhsScalar>::IsComplex) && (!NumTraits<RhsScalar>::IsComplex),
      MightCannotUseDest = (Dest::InnerStrideAtCompileTime!=1) || ComplexByReal
    };

    gemv_static_vector_if<ResScalar,Dest::SizeAtCompileTime,Dest::MaxSizeAtCompileTime,MightCannotUseDest> static_dest;

    bool alphaIsCompatible = (!ComplexByReal) || (imag(actualAlpha)==RealScalar(0));
    bool evalToDest = EvalToDestAtCompileTime && alphaIsCompatible;
    
    RhsScalar compatibleAlpha = get_factor<ResScalar,RhsScalar>::run(actualAlpha);

    ei_declare_aligned_stack_constructed_variable(ResScalar,actualDestPtr,dest.size(),
                                                  evalToDest ? dest.data() : static_dest.data());

    if(!evalToDest)
    {
      #ifdef EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      int size = dest.size();
      EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      #endif
      if(!alphaIsCompatible)
      {
        MappedDest(actualDestPtr, dest.size()).setZero();
        compatibleAlpha = RhsScalar(1);
      }
      else
        MappedDest(actualDestPtr, dest.size()) = dest;
    }
    
    internal::product_triangular_matrix_vector
      <Index,Mode,
       LhsScalar, LhsBlasTraits::NeedToConjugate,
       RhsScalar, RhsBlasTraits::NeedToConjugate,
       ColMajor>
      ::run(actualLhs.rows(),actualLhs.cols(),
            actualLhs.data(),actualLhs.outerStride(),
            actualRhs.data(),actualRhs.innerStride(),
            actualDestPtr,1,compatibleAlpha);

    if (!evalToDest)
    {
      if(!alphaIsCompatible)
        dest += actualAlpha * MappedDest(actualDestPtr, dest.size());
      else
        dest = MappedDest(actualDestPtr, dest.size());
    }
  }
};

template<> struct trmv_selector<RowMajor>
{
  template<int Mode, typename Lhs, typename Rhs, typename Dest>
  static void run(const TriangularProduct<Mode,true,Lhs,false,Rhs,true>& prod, Dest& dest, typename TriangularProduct<Mode,true,Lhs,false,Rhs,true>::Scalar alpha)
  {
    typedef TriangularProduct<Mode,true,Lhs,false,Rhs,true> ProductType;
    typedef typename ProductType::LhsScalar LhsScalar;
    typedef typename ProductType::RhsScalar RhsScalar;
    typedef typename ProductType::Scalar    ResScalar;
    typedef typename ProductType::Index Index;
    typedef typename ProductType::ActualLhsType ActualLhsType;
    typedef typename ProductType::ActualRhsType ActualRhsType;
    typedef typename ProductType::_ActualRhsType _ActualRhsType;
    typedef typename ProductType::LhsBlasTraits LhsBlasTraits;
    typedef typename ProductType::RhsBlasTraits RhsBlasTraits;

    typename add_const<ActualLhsType>::type actualLhs = LhsBlasTraits::extract(prod.lhs());
    typename add_const<ActualRhsType>::type actualRhs = RhsBlasTraits::extract(prod.rhs());

    ResScalar actualAlpha = alpha * LhsBlasTraits::extractScalarFactor(prod.lhs())
                                  * RhsBlasTraits::extractScalarFactor(prod.rhs());

    enum {
      DirectlyUseRhs = _ActualRhsType::InnerStrideAtCompileTime==1
    };

    gemv_static_vector_if<RhsScalar,_ActualRhsType::SizeAtCompileTime,_ActualRhsType::MaxSizeAtCompileTime,!DirectlyUseRhs> static_rhs;

    ei_declare_aligned_stack_constructed_variable(RhsScalar,actualRhsPtr,actualRhs.size(),
        DirectlyUseRhs ? const_cast<RhsScalar*>(actualRhs.data()) : static_rhs.data());

    if(!DirectlyUseRhs)
    {
      #ifdef EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      int size = actualRhs.size();
      EIGEN_DENSE_STORAGE_CTOR_PLUGIN
      #endif
      Map<typename _ActualRhsType::PlainObject>(actualRhsPtr, actualRhs.size()) = actualRhs;
    }
    
    internal::product_triangular_matrix_vector
      <Index,Mode,
       LhsScalar, LhsBlasTraits::NeedToConjugate,
       RhsScalar, RhsBlasTraits::NeedToConjugate,
       RowMajor>
      ::run(actualLhs.rows(),actualLhs.cols(),
            actualLhs.data(),actualLhs.outerStride(),
            actualRhsPtr,1,
            dest.data(),dest.innerStride(),
            actualAlpha);
  }
};

} // end namespace internal

#endif // EIGEN_TRIANGULARMATRIXVECTOR_H
