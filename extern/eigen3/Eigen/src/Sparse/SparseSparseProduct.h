// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
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

#ifndef EIGEN_SPARSESPARSEPRODUCT_H
#define EIGEN_SPARSESPARSEPRODUCT_H

namespace internal {

template<typename Lhs, typename Rhs, typename ResultType>
static void sparse_product_impl2(const Lhs& lhs, const Rhs& rhs, ResultType& res)
{
  typedef typename remove_all<Lhs>::type::Scalar Scalar;
  typedef typename remove_all<Lhs>::type::Index Index;

  // make sure to call innerSize/outerSize since we fake the storage order.
  Index rows = lhs.innerSize();
  Index cols = rhs.outerSize();
  eigen_assert(lhs.outerSize() == rhs.innerSize());

  std::vector<bool> mask(rows,false);
  Matrix<Scalar,Dynamic,1> values(rows);
  Matrix<Index,Dynamic,1>    indices(rows);

  // estimate the number of non zero entries
  float ratioLhs = float(lhs.nonZeros())/(float(lhs.rows())*float(lhs.cols()));
  float avgNnzPerRhsColumn = float(rhs.nonZeros())/float(cols);
  float ratioRes = std::min(ratioLhs * avgNnzPerRhsColumn, 1.f);

//  int t200 = rows/(log2(200)*1.39);
//  int t = (rows*100)/139;

  res.resize(rows, cols);
  res.reserve(Index(ratioRes*rows*cols));
  // we compute each column of the result, one after the other
  for (Index j=0; j<cols; ++j)
  {

    res.startVec(j);
    Index nnz = 0;
    for (typename Rhs::InnerIterator rhsIt(rhs, j); rhsIt; ++rhsIt)
    {
      Scalar y = rhsIt.value();
      Index k = rhsIt.index();
      for (typename Lhs::InnerIterator lhsIt(lhs, k); lhsIt; ++lhsIt)
      {
        Index i = lhsIt.index();
        Scalar x = lhsIt.value();
        if(!mask[i])
        {
          mask[i] = true;
//           values[i] = x * y;
//           indices[nnz] = i;
          ++nnz;
        }
        else
          values[i] += x * y;
      }
    }
    // FIXME reserve nnz non zeros
    // FIXME implement fast sort algorithms for very small nnz
    // if the result is sparse enough => use a quick sort
    // otherwise => loop through the entire vector
    // In order to avoid to perform an expensive log2 when the
    // result is clearly very sparse we use a linear bound up to 200.
//     if((nnz<200 && nnz<t200) || nnz * log2(nnz) < t)
//     {
//       if(nnz>1) std::sort(indices.data(),indices.data()+nnz);
//       for(int k=0; k<nnz; ++k)
//       {
//         int i = indices[k];
//         res.insertBackNoCheck(j,i) = values[i];
//         mask[i] = false;
//       }
//     }
//     else
//     {
//       // dense path
//       for(int i=0; i<rows; ++i)
//       {
//         if(mask[i])
//         {
//           mask[i] = false;
//           res.insertBackNoCheck(j,i) = values[i];
//         }
//       }
//     }

  }
  res.finalize();
}

// perform a pseudo in-place sparse * sparse product assuming all matrices are col major
template<typename Lhs, typename Rhs, typename ResultType>
static void sparse_product_impl(const Lhs& lhs, const Rhs& rhs, ResultType& res)
{
//   return sparse_product_impl2(lhs,rhs,res);

  typedef typename remove_all<Lhs>::type::Scalar Scalar;
  typedef typename remove_all<Lhs>::type::Index Index;

  // make sure to call innerSize/outerSize since we fake the storage order.
  Index rows = lhs.innerSize();
  Index cols = rhs.outerSize();
  //int size = lhs.outerSize();
  eigen_assert(lhs.outerSize() == rhs.innerSize());

  // allocate a temporary buffer
  AmbiVector<Scalar,Index> tempVector(rows);

  // estimate the number of non zero entries
  float ratioLhs = float(lhs.nonZeros())/(float(lhs.rows())*float(lhs.cols()));
  float avgNnzPerRhsColumn = float(rhs.nonZeros())/float(cols);
  float ratioRes = std::min(ratioLhs * avgNnzPerRhsColumn, 1.f);

  // mimics a resizeByInnerOuter:
  if(ResultType::IsRowMajor)
    res.resize(cols, rows);
  else
    res.resize(rows, cols);

  res.reserve(Index(ratioRes*rows*cols));
  for (Index j=0; j<cols; ++j)
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
    res.startVec(j);
    for (typename AmbiVector<Scalar,Index>::Iterator it(tempVector); it; ++it)
      res.insertBackByOuterInner(j,it.index()) = it.value();
  }
  res.finalize();
}

template<typename Lhs, typename Rhs, typename ResultType,
  int LhsStorageOrder = traits<Lhs>::Flags&RowMajorBit,
  int RhsStorageOrder = traits<Rhs>::Flags&RowMajorBit,
  int ResStorageOrder = traits<ResultType>::Flags&RowMajorBit>
struct sparse_product_selector;

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,ColMajor>
{
  typedef typename traits<typename remove_all<Lhs>::type>::Scalar Scalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
//     std::cerr << __LINE__ << "\n";
    typename remove_all<ResultType>::type _res(res.rows(), res.cols());
    sparse_product_impl<Lhs,Rhs,ResultType>(lhs, rhs, _res);
    res.swap(_res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector<Lhs,Rhs,ResultType,ColMajor,ColMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
//     std::cerr << __LINE__ << "\n";
    // we need a col-major matrix to hold the result
    typedef SparseMatrix<typename ResultType::Scalar> SparseTemporaryType;
    SparseTemporaryType _res(res.rows(), res.cols());
    sparse_product_impl<Lhs,Rhs,SparseTemporaryType>(lhs, rhs, _res);
    res = _res;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
//     std::cerr << __LINE__ << "\n";
    // let's transpose the product to get a column x column product
    typename remove_all<ResultType>::type _res(res.rows(), res.cols());
    sparse_product_impl<Rhs,Lhs,ResultType>(rhs, lhs, _res);
    res.swap(_res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector<Lhs,Rhs,ResultType,RowMajor,RowMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
//     std::cerr << "here...\n";
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
    ColMajorMatrix colLhs(lhs);
    ColMajorMatrix colRhs(rhs);
//     std::cerr << "more...\n";
    sparse_product_impl<ColMajorMatrix,ColMajorMatrix,ResultType>(colLhs, colRhs, res);
//     std::cerr << "OK.\n";

    // let's transpose the product to get a column x column product

//     typedef SparseMatrix<typename ResultType::Scalar> SparseTemporaryType;
//     SparseTemporaryType _res(res.cols(), res.rows());
//     sparse_product_impl<Rhs,Lhs,SparseTemporaryType>(rhs, lhs, _res);
//     res = _res.transpose();
  }
};

// NOTE the 2 others cases (col row *) must never occur since they are caught
// by ProductReturnType which transforms it to (col col *) by evaluating rhs.

} // end namespace internal

// sparse = sparse * sparse
template<typename Derived>
template<typename Lhs, typename Rhs>
inline Derived& SparseMatrixBase<Derived>::operator=(const SparseSparseProduct<Lhs,Rhs>& product)
{
//   std::cerr << "there..." << typeid(Lhs).name() << "  " << typeid(Lhs).name() << " " << (Derived::Flags&&RowMajorBit) << "\n";
  internal::sparse_product_selector<
    typename internal::remove_all<Lhs>::type,
    typename internal::remove_all<Rhs>::type,
    Derived>::run(product.lhs(),product.rhs(),derived());
  return derived();
}

namespace internal {

template<typename Lhs, typename Rhs, typename ResultType,
  int LhsStorageOrder = traits<Lhs>::Flags&RowMajorBit,
  int RhsStorageOrder = traits<Rhs>::Flags&RowMajorBit,
  int ResStorageOrder = traits<ResultType>::Flags&RowMajorBit>
struct sparse_product_selector2;

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,ColMajor,ColMajor,ColMajor>
{
  typedef typename traits<typename remove_all<Lhs>::type>::Scalar Scalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    sparse_product_impl2<Lhs,Rhs,ResultType>(lhs, rhs, res);
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,RowMajor,ColMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
      // prevent warnings until the code is fixed
      EIGEN_UNUSED_VARIABLE(lhs);
      EIGEN_UNUSED_VARIABLE(rhs);
      EIGEN_UNUSED_VARIABLE(res);

//     typedef SparseMatrix<typename ResultType::Scalar,RowMajor> RowMajorMatrix;
//     RowMajorMatrix rhsRow = rhs;
//     RowMajorMatrix resRow(res.rows(), res.cols());
//     sparse_product_impl2<RowMajorMatrix,Lhs,RowMajorMatrix>(rhsRow, lhs, resRow);
//     res = resRow;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,ColMajor,RowMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,RowMajor> RowMajorMatrix;
    RowMajorMatrix lhsRow = lhs;
    RowMajorMatrix resRow(res.rows(), res.cols());
    sparse_product_impl2<Rhs,RowMajorMatrix,RowMajorMatrix>(rhs, lhsRow, resRow);
    res = resRow;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,RowMajor,RowMajor,ColMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,RowMajor> RowMajorMatrix;
    RowMajorMatrix resRow(res.rows(), res.cols());
    sparse_product_impl2<Rhs,Lhs,RowMajorMatrix>(rhs, lhs, resRow);
    res = resRow;
  }
};


template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,ColMajor,ColMajor,RowMajor>
{
  typedef typename traits<typename remove_all<Lhs>::type>::Scalar Scalar;

  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
    ColMajorMatrix resCol(res.rows(), res.cols());
    sparse_product_impl2<Lhs,Rhs,ColMajorMatrix>(lhs, rhs, resCol);
    res = resCol;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,RowMajor,ColMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
    ColMajorMatrix lhsCol = lhs;
    ColMajorMatrix resCol(res.rows(), res.cols());
    sparse_product_impl2<ColMajorMatrix,Rhs,ColMajorMatrix>(lhsCol, rhs, resCol);
    res = resCol;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,ColMajor,RowMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
    ColMajorMatrix rhsCol = rhs;
    ColMajorMatrix resCol(res.rows(), res.cols());
    sparse_product_impl2<Lhs,ColMajorMatrix,ColMajorMatrix>(lhs, rhsCol, resCol);
    res = resCol;
  }
};

template<typename Lhs, typename Rhs, typename ResultType>
struct sparse_product_selector2<Lhs,Rhs,ResultType,RowMajor,RowMajor,RowMajor>
{
  static void run(const Lhs& lhs, const Rhs& rhs, ResultType& res)
  {
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
//     ColMajorMatrix lhsTr(lhs);
//     ColMajorMatrix rhsTr(rhs);
//     ColMajorMatrix aux(res.rows(), res.cols());
//     sparse_product_impl2<Rhs,Lhs,ColMajorMatrix>(rhs, lhs, aux);
// //     ColMajorMatrix aux2 = aux.transpose();
//     res = aux;
    typedef SparseMatrix<typename ResultType::Scalar,ColMajor> ColMajorMatrix;
    ColMajorMatrix lhsCol(lhs);
    ColMajorMatrix rhsCol(rhs);
    ColMajorMatrix resCol(res.rows(), res.cols());
    sparse_product_impl2<ColMajorMatrix,ColMajorMatrix,ColMajorMatrix>(lhsCol, rhsCol, resCol);
    res = resCol;
  }
};

} // end namespace internal

template<typename Derived>
template<typename Lhs, typename Rhs>
inline void SparseMatrixBase<Derived>::_experimentalNewProduct(const Lhs& lhs, const Rhs& rhs)
{
  //derived().resize(lhs.rows(), rhs.cols());
  internal::sparse_product_selector2<
    typename internal::remove_all<Lhs>::type,
    typename internal::remove_all<Rhs>::type,
    Derived>::run(lhs,rhs,derived());
}

// sparse * sparse
template<typename Derived>
template<typename OtherDerived>
inline const typename SparseSparseProductReturnType<Derived,OtherDerived>::Type
SparseMatrixBase<Derived>::operator*(const SparseMatrixBase<OtherDerived> &other) const
{
  return typename SparseSparseProductReturnType<Derived,OtherDerived>::Type(derived(), other.derived());
}

#endif // EIGEN_SPARSESPARSEPRODUCT_H
