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

#ifndef EIGEN_SPARSEPRODUCT_H
#define EIGEN_SPARSEPRODUCT_H

template<typename Lhs, typename Rhs> struct ei_sparse_product_mode
{
  enum {

    value = (Rhs::Flags&Lhs::Flags&SparseBit)==SparseBit
          ? SparseTimeSparseProduct
          : (Lhs::Flags&SparseBit)==SparseBit
          ? SparseTimeDenseProduct
          : DenseTimeSparseProduct };
};

template<typename Lhs, typename Rhs, int ProductMode>
struct SparseProductReturnType
{
  typedef const typename ei_nested<Lhs,Rhs::RowsAtCompileTime>::type LhsNested;
  typedef const typename ei_nested<Rhs,Lhs::RowsAtCompileTime>::type RhsNested;

  typedef SparseProduct<LhsNested, RhsNested, ProductMode> Type;
};

// sparse product return type specialization
template<typename Lhs, typename Rhs>
struct SparseProductReturnType<Lhs,Rhs,SparseTimeSparseProduct>
{
  typedef typename ei_traits<Lhs>::Scalar Scalar;
  enum {
    LhsRowMajor = ei_traits<Lhs>::Flags & RowMajorBit,
    RhsRowMajor = ei_traits<Rhs>::Flags & RowMajorBit,
    TransposeRhs = (!LhsRowMajor) && RhsRowMajor,
    TransposeLhs = LhsRowMajor && (!RhsRowMajor)
  };

  // FIXME if we transpose let's evaluate to a LinkedVectorMatrix since it is the
  // type of the temporary to perform the transpose op
  typedef typename ei_meta_if<TransposeLhs,
    SparseMatrix<Scalar,0>,
    const typename ei_nested<Lhs,Rhs::RowsAtCompileTime>::type>::ret LhsNested;

  typedef typename ei_meta_if<TransposeRhs,
    SparseMatrix<Scalar,0>,
    const typename ei_nested<Rhs,Lhs::RowsAtCompileTime>::type>::ret RhsNested;

  typedef SparseProduct<LhsNested, RhsNested, SparseTimeSparseProduct> Type;
};

template<typename LhsNested, typename RhsNested, int ProductMode>
struct ei_traits<SparseProduct<LhsNested, RhsNested, ProductMode> >
{
  // clean the nested types:
  typedef typename ei_cleantype<LhsNested>::type _LhsNested;
  typedef typename ei_cleantype<RhsNested>::type _RhsNested;
  typedef typename _LhsNested::Scalar Scalar;

  enum {
    LhsCoeffReadCost = _LhsNested::CoeffReadCost,
    RhsCoeffReadCost = _RhsNested::CoeffReadCost,
    LhsFlags = _LhsNested::Flags,
    RhsFlags = _RhsNested::Flags,

    RowsAtCompileTime = _LhsNested::RowsAtCompileTime,
    ColsAtCompileTime = _RhsNested::ColsAtCompileTime,
    InnerSize = EIGEN_ENUM_MIN(_LhsNested::ColsAtCompileTime, _RhsNested::RowsAtCompileTime),

    MaxRowsAtCompileTime = _LhsNested::MaxRowsAtCompileTime,
    MaxColsAtCompileTime = _RhsNested::MaxColsAtCompileTime,

//     LhsIsRowMajor = (LhsFlags & RowMajorBit)==RowMajorBit,
//     RhsIsRowMajor = (RhsFlags & RowMajorBit)==RowMajorBit,

    EvalToRowMajor = (RhsFlags & LhsFlags & RowMajorBit),
    ResultIsSparse = ProductMode==SparseTimeSparseProduct,

    RemovedBits = ~( (EvalToRowMajor ? 0 : RowMajorBit) | (ResultIsSparse ? 0 : SparseBit) ),

    Flags = (int(LhsFlags | RhsFlags) & HereditaryBits & RemovedBits)
          | EvalBeforeAssigningBit
          | EvalBeforeNestingBit,

    CoeffReadCost = Dynamic
  };
  
  typedef typename ei_meta_if<ResultIsSparse,
    SparseMatrixBase<SparseProduct<LhsNested, RhsNested, ProductMode> >,
    MatrixBase<SparseProduct<LhsNested, RhsNested, ProductMode> > >::ret Base;
};

template<typename LhsNested, typename RhsNested, int ProductMode>
class SparseProduct : ei_no_assignment_operator, public ei_traits<SparseProduct<LhsNested, RhsNested, ProductMode> >::Base
{
  public:

    EIGEN_GENERIC_PUBLIC_INTERFACE(SparseProduct)

  private:

    typedef typename ei_traits<SparseProduct>::_LhsNested _LhsNested;
    typedef typename ei_traits<SparseProduct>::_RhsNested _RhsNested;

  public:

    template<typename Lhs, typename Rhs>
    EIGEN_STRONG_INLINE SparseProduct(const Lhs& lhs, const Rhs& rhs)
      : m_lhs(lhs), m_rhs(rhs)
    {
      ei_assert(lhs.cols() == rhs.rows());
      
      enum {
        ProductIsValid = _LhsNested::ColsAtCompileTime==Dynamic
                      || _RhsNested::RowsAtCompileTime==Dynamic
                      || int(_LhsNested::ColsAtCompileTime)==int(_RhsNested::RowsAtCompileTime),
        AreVectors = _LhsNested::IsVectorAtCompileTime && _RhsNested::IsVectorAtCompileTime,
        SameSizes = EIGEN_PREDICATE_SAME_MATRIX_SIZE(_LhsNested,_RhsNested)
      };
      // note to the lost user:
      //    * for a dot product use: v1.dot(v2)
      //    * for a coeff-wise product use: v1.cwise()*v2
      EIGEN_STATIC_ASSERT(ProductIsValid || !(AreVectors && SameSizes),
        INVALID_VECTOR_VECTOR_PRODUCT__IF_YOU_WANTED_A_DOT_OR_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTIONS)
      EIGEN_STATIC_ASSERT(ProductIsValid || !(SameSizes && !AreVectors),
        INVALID_MATRIX_PRODUCT__IF_YOU_WANTED_A_COEFF_WISE_PRODUCT_YOU_MUST_USE_THE_EXPLICIT_FUNCTION)
      EIGEN_STATIC_ASSERT(ProductIsValid || SameSizes, INVALID_MATRIX_PRODUCT)
    }

    EIGEN_STRONG_INLINE int rows() const { return m_lhs.rows(); }
    EIGEN_STRONG_INLINE int cols() const { return m_rhs.cols(); }

    EIGEN_STRONG_INLINE const _LhsNested& lhs() const { return m_lhs; }
    EIGEN_STRONG_INLINE const _RhsNested& rhs() const { return m_rhs; }

  protected:
    LhsNested m_lhs;
    RhsNested m_rhs;
};

template<typename Lhs, typename Rhs, typename ResultType,
  int LhsStorageOrder = ei_traits<Lhs>::Flags&RowMajorBit,
  int RhsStorageOrder = ei_traits<Rhs>::Flags&RowMajorBit,
  int ResStorageOrder = ei_traits<ResultType>::Flags&RowMajorBit>
struct ei_sparse_product_selector;

template<typename Lhs, typename Rhs, typename ResultType>
struct ei_sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,ColMajor>
{
  typedef typename ei_traits<typename ei_cleantype<Lhs>::type>::Scalar Scalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    // make sure to call innerSize/outerSize since we fake the storage order.
    int rows = lhs.innerSize();
    int cols = rhs.outerSize();
    //int size = lhs.outerSize();
    ei_assert(lhs.outerSize() == rhs.innerSize());

    // allocate a temporary buffer
    AmbiVector<Scalar> tempVector(rows);

    // estimate the number of non zero entries
    float ratioLhs = float(lhs.nonZeros())/float(lhs.rows()*lhs.cols());
    float avgNnzPerRhsColumn = float(rhs.nonZeros())/float(cols);
    float ratioRes = std::min(ratioLhs * avgNnzPerRhsColumn, 1.f);

    res.resize(rows, cols);
    res.startFill(int(ratioRes*rows*cols));
    for (int j=0; j<cols; ++j)
    {
      // let's do a more accurate determination of the nnz ratio for the current column j of res
      //float ratioColRes = std::min(ratioLhs * rhs.innerNonZeros(j), 1.f);
      // FIXME find a nice way to get the number of nonzeros of a sub matrix (here an inner vector)
      float ratioColRes = ratioRes;
      tempVector.init(ratioColRes);
      tempVector.setZero();
      for (typename Rhs::InnerIterator rhsIt(rhs, j); rhsIt; ++rhsIt)
      {
        // FIXME should be written like this: tmp += rhsIt.value() * lhs.col(rhsIt.index())
        tempVector.restart();
        Scalar x = rhsIt.value();
        for (typename Lhs::InnerIterator lhsIt(lhs, rhsIt.index()); lhsIt; ++lhsIt)
        {
          tempVector.coeffRef(lhsIt.index()) += lhsIt.value() * x;
        }
      }
      for (typename AmbiVector<Scalar>::Iterator it(tempVector); it; ++it)
        if (ResultType::Flags&RowMajorBit)
          res.fill(j,it.index()) = it.value();
        else
          res.fill(it.index(), j) = it.value();
    }
    res.endFill();
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct ei_sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,RowMajor>
{
  typedef SparseMatrix<typename ResultType::Scalar> SparseTemporaryType;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    SparseTemporaryType _res(res.rows(), res.cols());
    ei_sparse_product_selector<Lhs,Rhs,SparseTemporaryType,ColMajor,ColMajor,ColMajor>::run(lhs, rhs, _res);
    res = _res;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct ei_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    // let's transpose the product to get a column x column product
    ei_sparse_product_selector<Rhs,Lhs,ResultType,ColMajor,ColMajor,ColMajor>::run(rhs, lhs, res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct ei_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,ColMajor>
{
  typedef SparseMatrix<typename ResultType::Scalar> SparseTemporaryType;
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    // let's transpose the product to get a column x column product
    SparseTemporaryType _res(res.cols(), res.rows());
    ei_sparse_product_selector<Rhs,Lhs,SparseTemporaryType,ColMajor,ColMajor,ColMajor>
      ::run(rhs, lhs, _res);
    res = _res.transpose();
  }
};

// NOTE eventually let's transpose one argument even in this case since it might be expensive if
// the result is not dense.
// template<typename Lhs, typename Rhs, typename ResultType, int ResStorageOrder>
// struct ei_sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,ColMajor,ResStorageOrder>
// {
//   static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
//   {
//     // trivial product as lhs.row/rhs.col dot products
//     // loop over the preferred order of the result
//   }
// };

// NOTE the 2 others cases (col row *) must never occurs since they are caught
// by ProductReturnType which transform it to (col col *) by evaluating rhs.


// template<typename Derived>
// template<typename Lhs, typename Rhs>
// inline Derived& SparseMatrixBase<Derived>::lazyAssign(const SparseProduct<Lhs,Rhs>& product)
// {
// //   std::cout << "sparse product to dense\n";
//   ei_sparse_product_selector<
//     typename ei_cleantype<Lhs>::type,
//     typename ei_cleantype<Rhs>::type,
//     typename ei_cleantype<Derived>::type>::run(product.lhs(),product.rhs(),derived());
//   return derived();
// }

// sparse = sparse * sparse
template<typename Derived>
template<typename Lhs, typename Rhs>
inline Derived& SparseMatrixBase<Derived>::operator=(const SparseProduct<Lhs,Rhs,SparseTimeSparseProduct>& product)
{
//   std::cout << "sparse product to sparse\n";
  ei_sparse_product_selector<
    typename ei_cleantype<Lhs>::type,
    typename ei_cleantype<Rhs>::type,
    Derived>::run(product.lhs(),product.rhs(),derived());
  return derived();
}

// dense = sparse * dense
// template<typename Derived>
// template<typename Lhs, typename Rhs>
// Derived& MatrixBase<Derived>::lazyAssign(const SparseProduct<Lhs,Rhs,SparseTimeDenseProduct>& product)
// {
//   typedef typename ei_cleantype<Lhs>::type _Lhs;
//   typedef typename _Lhs::InnerIterator LhsInnerIterator;
//   enum { LhsIsRowMajor = (_Lhs::Flags&RowMajorBit)==RowMajorBit };
//   derived().setZero();
//   for (int j=0; j<product.lhs().outerSize(); ++j)
//     for (LhsInnerIterator i(product.lhs(),j); i; ++i)
//       derived().row(LhsIsRowMajor ? j : i.index()) += i.value() * product.rhs().row(LhsIsRowMajor ? i.index() : j);
//   return derived();
// }

template<typename Derived>
template<typename Lhs, typename Rhs>
Derived& MatrixBase<Derived>::lazyAssign(const SparseProduct<Lhs,Rhs,SparseTimeDenseProduct>& product)
{
  typedef typename ei_cleantype<Lhs>::type _Lhs;
  typedef typename ei_cleantype<Rhs>::type _Rhs;
  typedef typename _Lhs::InnerIterator LhsInnerIterator;
  enum {
    LhsIsRowMajor = (_Lhs::Flags&RowMajorBit)==RowMajorBit,
    LhsIsSelfAdjoint = (_Lhs::Flags&SelfAdjointBit)==SelfAdjointBit,
    ProcessFirstHalf = LhsIsSelfAdjoint
      && (   ((_Lhs::Flags&(UpperTriangularBit|LowerTriangularBit))==0)
          || ( (_Lhs::Flags&UpperTriangularBit) && !LhsIsRowMajor)
          || ( (_Lhs::Flags&LowerTriangularBit) && LhsIsRowMajor) ),
    ProcessSecondHalf = LhsIsSelfAdjoint && (!ProcessFirstHalf)
  };
  derived().setZero();
  for (int j=0; j<product.lhs().outerSize(); ++j)
  {
    LhsInnerIterator i(product.lhs(),j);
    if (ProcessSecondHalf && i && (i.index()==j))
    {
      derived().row(j) += i.value() * product.rhs().row(j);
      ++i;
    }
    Block<Derived,1,Derived::ColsAtCompileTime> foo = derived().row(j);
    for (; (ProcessFirstHalf ? i && i.index() < j : i) ; ++i)
    {
      if (LhsIsSelfAdjoint)
      {
        int a = LhsIsRowMajor ? j : i.index();
        int b = LhsIsRowMajor ? i.index() : j;
        Scalar v = i.value();
        derived().row(a) += (v) * product.rhs().row(b);
        derived().row(b) += ei_conj(v) * product.rhs().row(a);
      }
      else if (LhsIsRowMajor)
        foo += i.value() * product.rhs().row(i.index());
      else
        derived().row(i.index()) += i.value() * product.rhs().row(j);
    }
    if (ProcessFirstHalf && i && (i.index()==j))
      derived().row(j) += i.value() * product.rhs().row(j);
  }
  return derived();
}

// dense = dense * sparse
template<typename Derived>
template<typename Lhs, typename Rhs>
Derived& MatrixBase<Derived>::lazyAssign(const SparseProduct<Lhs,Rhs,DenseTimeSparseProduct>& product)
{
  typedef typename ei_cleantype<Rhs>::type _Rhs;
  typedef typename _Rhs::InnerIterator RhsInnerIterator;
  enum { RhsIsRowMajor = (_Rhs::Flags&RowMajorBit)==RowMajorBit };
  derived().setZero();
  for (int j=0; j<product.rhs().outerSize(); ++j)
    for (RhsInnerIterator i(product.rhs(),j); i; ++i)
      derived().col(RhsIsRowMajor ? i.index() : j) += i.value() * product.lhs().col(RhsIsRowMajor ? j : i.index());
  return derived();
}

// sparse * sparse
template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const typename SparseProductReturnType<Derived,OtherDerived>::Type
SparseMatrixBase<Derived>::operator*(const SparseMatrixBase<OtherDerived> &other) const
{
  return typename SparseProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

// sparse * dense
template<typename Derived>
template<typename OtherDerived>
EIGEN_STRONG_INLINE const typename SparseProductReturnType<Derived,OtherDerived>::Type
SparseMatrixBase<Derived>::operator*(const MatrixBase<OtherDerived> &other) const
{
  return typename SparseProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

#endif // EIGEN_SPARSEPRODUCT_H
