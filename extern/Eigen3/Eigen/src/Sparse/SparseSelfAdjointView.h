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

#ifndef EIGEN_SPARSE_SELFADJOINTVIEW_H
#define EIGEN_SPARSE_SELFADJOINTVIEW_H

/** \class SparseSelfAdjointView
  *
  *
  * \brief Pseudo expression to manipulate a triangular sparse matrix as a selfadjoint matrix.
  *
  * \param MatrixType the type of the dense matrix storing the coefficients
  * \param UpLo can be either \c #Lower or \c #Upper
  *
  * This class is an expression of a sefladjoint matrix from a triangular part of a matrix
  * with given dense storage of the coefficients. It is the return type of MatrixBase::selfadjointView()
  * and most of the time this is the only way that it is used.
  *
  * \sa SparseMatrixBase::selfadjointView()
  */
template<typename Lhs, typename Rhs, int UpLo>
class SparseSelfAdjointTimeDenseProduct;

template<typename Lhs, typename Rhs, int UpLo>
class DenseTimeSparseSelfAdjointProduct;

template<typename MatrixType,int UpLo>
class SparseSymmetricPermutationProduct;

namespace internal {
  
template<typename MatrixType, unsigned int UpLo>
struct traits<SparseSelfAdjointView<MatrixType,UpLo> > : traits<MatrixType> {
};

template<int SrcUpLo,int DstUpLo,typename MatrixType,int DestOrder>
void permute_symm_to_symm(const MatrixType& mat, SparseMatrix<typename MatrixType::Scalar,DestOrder,typename MatrixType::Index>& _dest, const typename MatrixType::Index* perm = 0);

template<int UpLo,typename MatrixType,int DestOrder>
void permute_symm_to_fullsymm(const MatrixType& mat, SparseMatrix<typename MatrixType::Scalar,DestOrder,typename MatrixType::Index>& _dest, const typename MatrixType::Index* perm = 0);

}

template<typename MatrixType, unsigned int UpLo> class SparseSelfAdjointView
  : public EigenBase<SparseSelfAdjointView<MatrixType,UpLo> >
{
  public:

    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef Matrix<Index,Dynamic,1> VectorI;
    typedef typename MatrixType::Nested MatrixTypeNested;
    typedef typename internal::remove_all<MatrixTypeNested>::type _MatrixTypeNested;

    inline SparseSelfAdjointView(const MatrixType& matrix) : m_matrix(matrix)
    {
      eigen_assert(rows()==cols() && "SelfAdjointView is only for squared matrices");
    }

    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }

    /** \internal \returns a reference to the nested matrix */
    const _MatrixTypeNested& matrix() const { return m_matrix; }
    _MatrixTypeNested& matrix() { return m_matrix.const_cast_derived(); }

    /** Efficient sparse self-adjoint matrix times dense vector/matrix product */
    template<typename OtherDerived>
    SparseSelfAdjointTimeDenseProduct<MatrixType,OtherDerived,UpLo>
    operator*(const MatrixBase<OtherDerived>& rhs) const
    {
      return SparseSelfAdjointTimeDenseProduct<MatrixType,OtherDerived,UpLo>(m_matrix, rhs.derived());
    }

    /** Efficient dense vector/matrix times sparse self-adjoint matrix product */
    template<typename OtherDerived> friend
    DenseTimeSparseSelfAdjointProduct<OtherDerived,MatrixType,UpLo>
    operator*(const MatrixBase<OtherDerived>& lhs, const SparseSelfAdjointView& rhs)
    {
      return DenseTimeSparseSelfAdjointProduct<OtherDerived,_MatrixTypeNested,UpLo>(lhs.derived(), rhs.m_matrix);
    }

    /** Perform a symmetric rank K update of the selfadjoint matrix \c *this:
      * \f$ this = this + \alpha ( u u^* ) \f$ where \a u is a vector or matrix.
      *
      * \returns a reference to \c *this
      *
      * Note that it is faster to set alpha=0 than initializing the matrix to zero
      * and then keep the default value alpha=1.
      *
      * To perform \f$ this = this + \alpha ( u^* u ) \f$ you can simply
      * call this function with u.adjoint().
      */
    template<typename DerivedU>
    SparseSelfAdjointView& rankUpdate(const SparseMatrixBase<DerivedU>& u, Scalar alpha = Scalar(1));
    
    /** \internal triggered by sparse_matrix = SparseSelfadjointView; */
    template<typename DestScalar> void evalTo(SparseMatrix<DestScalar>& _dest) const
    {
      internal::permute_symm_to_fullsymm<UpLo>(m_matrix, _dest);
    }
    
    template<typename DestScalar> void evalTo(DynamicSparseMatrix<DestScalar>& _dest) const
    {
      // TODO directly evaluate into _dest;
      SparseMatrix<DestScalar> tmp(_dest.rows(),_dest.cols());
      internal::permute_symm_to_fullsymm<UpLo>(m_matrix, tmp);
      _dest = tmp;
    }
    
    /** \returns an expression of P^-1 H P */
    SparseSymmetricPermutationProduct<_MatrixTypeNested,UpLo> twistedBy(const PermutationMatrix<Dynamic>& perm) const
    {
      return SparseSymmetricPermutationProduct<_MatrixTypeNested,UpLo>(m_matrix, perm);
    }
    
    template<typename SrcMatrixType,int SrcUpLo>
    SparseSelfAdjointView& operator=(const SparseSymmetricPermutationProduct<SrcMatrixType,SrcUpLo>& permutedMatrix)
    {
      permutedMatrix.evalTo(*this);
      return *this;
    }
    

    // const SparseLLT<PlainObject, UpLo> llt() const;
    // const SparseLDLT<PlainObject, UpLo> ldlt() const;

  protected:

    const typename MatrixType::Nested m_matrix;
    mutable VectorI m_countPerRow;
    mutable VectorI m_countPerCol;
};

/***************************************************************************
* Implementation of SparseMatrixBase methods
***************************************************************************/

template<typename Derived>
template<unsigned int UpLo>
const SparseSelfAdjointView<Derived, UpLo> SparseMatrixBase<Derived>::selfadjointView() const
{
  return derived();
}

template<typename Derived>
template<unsigned int UpLo>
SparseSelfAdjointView<Derived, UpLo> SparseMatrixBase<Derived>::selfadjointView()
{
  return derived();
}

/***************************************************************************
* Implementation of SparseSelfAdjointView methods
***************************************************************************/

template<typename MatrixType, unsigned int UpLo>
template<typename DerivedU>
SparseSelfAdjointView<MatrixType,UpLo>&
SparseSelfAdjointView<MatrixType,UpLo>::rankUpdate(const SparseMatrixBase<DerivedU>& u, Scalar alpha)
{
  SparseMatrix<Scalar,MatrixType::Flags&RowMajorBit?RowMajor:ColMajor> tmp = u * u.adjoint();
  if(alpha==Scalar(0))
    m_matrix.const_cast_derived() = tmp.template triangularView<UpLo>();
  else
    m_matrix.const_cast_derived() += alpha * tmp.template triangularView<UpLo>();

  return *this;
}

/***************************************************************************
* Implementation of sparse self-adjoint time dense matrix
***************************************************************************/

namespace internal {
template<typename Lhs, typename Rhs, int UpLo>
struct traits<SparseSelfAdjointTimeDenseProduct<Lhs,Rhs,UpLo> >
 : traits<ProductBase<SparseSelfAdjointTimeDenseProduct<Lhs,Rhs,UpLo>, Lhs, Rhs> >
{
  typedef Dense StorageKind;
};
}

template<typename Lhs, typename Rhs, int UpLo>
class SparseSelfAdjointTimeDenseProduct
  : public ProductBase<SparseSelfAdjointTimeDenseProduct<Lhs,Rhs,UpLo>, Lhs, Rhs>
{
  public:
    EIGEN_PRODUCT_PUBLIC_INTERFACE(SparseSelfAdjointTimeDenseProduct)

    SparseSelfAdjointTimeDenseProduct(const Lhs& lhs, const Rhs& rhs) : Base(lhs,rhs)
    {}

    template<typename Dest> void scaleAndAddTo(Dest& dest, Scalar alpha) const
    {
      // TODO use alpha
      eigen_assert(alpha==Scalar(1) && "alpha != 1 is not implemented yet, sorry");
      typedef typename internal::remove_all<Lhs>::type _Lhs;
      typedef typename internal::remove_all<Rhs>::type _Rhs;
      typedef typename _Lhs::InnerIterator LhsInnerIterator;
      enum {
        LhsIsRowMajor = (_Lhs::Flags&RowMajorBit)==RowMajorBit,
        ProcessFirstHalf =
                 ((UpLo&(Upper|Lower))==(Upper|Lower))
              || ( (UpLo&Upper) && !LhsIsRowMajor)
              || ( (UpLo&Lower) && LhsIsRowMajor),
        ProcessSecondHalf = !ProcessFirstHalf
      };
      for (Index j=0; j<m_lhs.outerSize(); ++j)
      {
        LhsInnerIterator i(m_lhs,j);
        if (ProcessSecondHalf && i && (i.index()==j))
        {
          dest.row(j) += i.value() * m_rhs.row(j);
          ++i;
        }
        Block<Dest,1,Dest::ColsAtCompileTime> dest_j(dest.row(LhsIsRowMajor ? j : 0));
        for(; (ProcessFirstHalf ? i && i.index() < j : i) ; ++i)
        {
          Index a = LhsIsRowMajor ? j : i.index();
          Index b = LhsIsRowMajor ? i.index() : j;
          typename Lhs::Scalar v = i.value();
          dest.row(a) += (v) * m_rhs.row(b);
          dest.row(b) += internal::conj(v) * m_rhs.row(a);
        }
        if (ProcessFirstHalf && i && (i.index()==j))
          dest.row(j) += i.value() * m_rhs.row(j);
      }
    }

  private:
    SparseSelfAdjointTimeDenseProduct& operator=(const SparseSelfAdjointTimeDenseProduct&);
};

namespace internal {
template<typename Lhs, typename Rhs, int UpLo>
struct traits<DenseTimeSparseSelfAdjointProduct<Lhs,Rhs,UpLo> >
 : traits<ProductBase<DenseTimeSparseSelfAdjointProduct<Lhs,Rhs,UpLo>, Lhs, Rhs> >
{};
}

template<typename Lhs, typename Rhs, int UpLo>
class DenseTimeSparseSelfAdjointProduct
  : public ProductBase<DenseTimeSparseSelfAdjointProduct<Lhs,Rhs,UpLo>, Lhs, Rhs>
{
  public:
    EIGEN_PRODUCT_PUBLIC_INTERFACE(DenseTimeSparseSelfAdjointProduct)

    DenseTimeSparseSelfAdjointProduct(const Lhs& lhs, const Rhs& rhs) : Base(lhs,rhs)
    {}

    template<typename Dest> void scaleAndAddTo(Dest& /*dest*/, Scalar /*alpha*/) const
    {
      // TODO
    }

  private:
    DenseTimeSparseSelfAdjointProduct& operator=(const DenseTimeSparseSelfAdjointProduct&);
};

/***************************************************************************
* Implementation of symmetric copies and permutations
***************************************************************************/
namespace internal {
  
template<typename MatrixType, int UpLo>
struct traits<SparseSymmetricPermutationProduct<MatrixType,UpLo> > : traits<MatrixType> {
};

template<int UpLo,typename MatrixType,int DestOrder>
void permute_symm_to_fullsymm(const MatrixType& mat, SparseMatrix<typename MatrixType::Scalar,DestOrder,typename MatrixType::Index>& _dest, const typename MatrixType::Index* perm)
{
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  typedef SparseMatrix<Scalar,DestOrder,Index> Dest;
  typedef Matrix<Index,Dynamic,1> VectorI;
  
  Dest& dest(_dest.derived());
  enum {
    StorageOrderMatch = int(Dest::IsRowMajor) == int(MatrixType::IsRowMajor)
  };
  eigen_assert(perm==0);
  Index size = mat.rows();
  VectorI count;
  count.resize(size);
  count.setZero();
  dest.resize(size,size);
  for(Index j = 0; j<size; ++j)
  {
    Index jp = perm ? perm[j] : j;
    for(typename MatrixType::InnerIterator it(mat,j); it; ++it)
    {
      Index i = it.index();
      Index ip = perm ? perm[i] : i;
      if(i==j)
        count[ip]++;
      else if((UpLo==Lower && i>j) || (UpLo==Upper && i<j))
      {
        count[ip]++;
        count[jp]++;
      }
    }
  }
  Index nnz = count.sum();
  
  // reserve space
  dest.reserve(nnz);
  dest._outerIndexPtr()[0] = 0;
  for(Index j=0; j<size; ++j)
    dest._outerIndexPtr()[j+1] = dest._outerIndexPtr()[j] + count[j];
  for(Index j=0; j<size; ++j)
    count[j] = dest._outerIndexPtr()[j];
  
  // copy data
  for(Index j = 0; j<size; ++j)
  {
    Index jp = perm ? perm[j] : j;
    for(typename MatrixType::InnerIterator it(mat,j); it; ++it)
    {
      Index i = it.index();
      Index ip = perm ? perm[i] : i;
      if(i==j)
      {
        int k = count[ip]++;
        dest._innerIndexPtr()[k] = ip;
        dest._valuePtr()[k] = it.value();
      }
      else if((UpLo==Lower && i>j) || (UpLo==Upper && i<j))
      {
        int k = count[jp]++;
        dest._innerIndexPtr()[k] = ip;
        dest._valuePtr()[k] = it.value();
        k = count[ip]++;
        dest._innerIndexPtr()[k] = jp;
        dest._valuePtr()[k] = internal::conj(it.value());
      }
    }
  }
}

template<int SrcUpLo,int DstUpLo,typename MatrixType,int DestOrder>
void permute_symm_to_symm(const MatrixType& mat, SparseMatrix<typename MatrixType::Scalar,DestOrder,typename MatrixType::Index>& _dest, const typename MatrixType::Index* perm)
{
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::Scalar Scalar;
  typedef SparseMatrix<Scalar,DestOrder,Index> Dest;
  Dest& dest(_dest.derived());
  typedef Matrix<Index,Dynamic,1> VectorI;
  //internal::conj_if<SrcUpLo!=DstUpLo> cj;
  
  Index size = mat.rows();
  VectorI count(size);
  count.setZero();
  dest.resize(size,size);
  for(Index j = 0; j<size; ++j)
  {
    Index jp = perm ? perm[j] : j;
    for(typename MatrixType::InnerIterator it(mat,j); it; ++it)
    {
      Index i = it.index();
      if((SrcUpLo==Lower && i<j) || (SrcUpLo==Upper && i>j))
        continue;
                  
      Index ip = perm ? perm[i] : i;
      count[DstUpLo==Lower ? std::min(ip,jp) : std::max(ip,jp)]++;
    }
  }
  dest._outerIndexPtr()[0] = 0;
  for(Index j=0; j<size; ++j)
    dest._outerIndexPtr()[j+1] = dest._outerIndexPtr()[j] + count[j];
  dest.resizeNonZeros(dest._outerIndexPtr()[size]);
  for(Index j=0; j<size; ++j)
    count[j] = dest._outerIndexPtr()[j];
  
  for(Index j = 0; j<size; ++j)
  {
    Index jp = perm ? perm[j] : j;
    for(typename MatrixType::InnerIterator it(mat,j); it; ++it)
    {
      Index i = it.index();
      if((SrcUpLo==Lower && i<j) || (SrcUpLo==Upper && i>j))
        continue;
                  
      Index ip = perm? perm[i] : i;
      Index k = count[DstUpLo==Lower ? std::min(ip,jp) : std::max(ip,jp)]++;
      dest._innerIndexPtr()[k] = DstUpLo==Lower ? std::max(ip,jp) : std::min(ip,jp);
      
      if((DstUpLo==Lower && ip<jp) || (DstUpLo==Upper && ip>jp))
        dest._valuePtr()[k] = conj(it.value());
      else
        dest._valuePtr()[k] = it.value();
    }
  }
}

}

template<typename MatrixType,int UpLo>
class SparseSymmetricPermutationProduct
  : public EigenBase<SparseSymmetricPermutationProduct<MatrixType,UpLo> >
{
    typedef PermutationMatrix<Dynamic> Perm;
  public:
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    typedef Matrix<Index,Dynamic,1> VectorI;
    typedef typename MatrixType::Nested MatrixTypeNested;
    typedef typename internal::remove_all<MatrixTypeNested>::type _MatrixTypeNested;
    
    SparseSymmetricPermutationProduct(const MatrixType& mat, const Perm& perm)
      : m_matrix(mat), m_perm(perm)
    {}
    
    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }
    
    template<typename DestScalar> void evalTo(SparseMatrix<DestScalar>& _dest) const
    {
      internal::permute_symm_to_fullsymm<UpLo>(m_matrix,_dest,m_perm.indices().data());
    }
    
    template<typename DestType,unsigned int DestUpLo> void evalTo(SparseSelfAdjointView<DestType,DestUpLo>& dest) const
    {
      internal::permute_symm_to_symm<UpLo,DestUpLo>(m_matrix,dest.matrix(),m_perm.indices().data());
    }
    
  protected:
    const MatrixTypeNested m_matrix;
    const Perm& m_perm;

};

#endif // EIGEN_SPARSE_SELFADJOINTVIEW_H
