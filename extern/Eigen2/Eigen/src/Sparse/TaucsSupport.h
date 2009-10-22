// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008-2009 Gael Guennebaud <g.gael@free.fr>
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

#ifndef EIGEN_TAUCSSUPPORT_H
#define EIGEN_TAUCSSUPPORT_H

template<typename Derived>
taucs_ccs_matrix SparseMatrixBase<Derived>::asTaucsMatrix()
{
  taucs_ccs_matrix res;
  res.n         = cols();
  res.m         = rows();
  res.flags     = 0;
  res.colptr    = derived()._outerIndexPtr();
  res.rowind    = derived()._innerIndexPtr();
  res.values.v  = derived()._valuePtr();
  if (ei_is_same_type<Scalar,int>::ret)
    res.flags |= TAUCS_INT;
  else if (ei_is_same_type<Scalar,float>::ret)
    res.flags |= TAUCS_SINGLE;
  else if (ei_is_same_type<Scalar,double>::ret)
    res.flags |= TAUCS_DOUBLE;
  else if (ei_is_same_type<Scalar,std::complex<float> >::ret)
    res.flags |= TAUCS_SCOMPLEX;
  else if (ei_is_same_type<Scalar,std::complex<double> >::ret)
    res.flags |= TAUCS_DCOMPLEX;
  else
  {
    ei_assert(false && "Scalar type not supported by TAUCS");
  }

  if (Flags & UpperTriangular)
    res.flags |= TAUCS_UPPER;
  if (Flags & LowerTriangular)
    res.flags |= TAUCS_LOWER;
  if (Flags & SelfAdjoint)
    res.flags |= (NumTraits<Scalar>::IsComplex ? TAUCS_HERMITIAN : TAUCS_SYMMETRIC);
  else if ((Flags & UpperTriangular) || (Flags & LowerTriangular))
    res.flags |= TAUCS_TRIANGULAR;

  return res;
}

template<typename Scalar, int Flags>
MappedSparseMatrix<Scalar,Flags>::MappedSparseMatrix(taucs_ccs_matrix& taucsMat)
{
  m_innerSize = taucsMat.m;
  m_outerSize = taucsMat.n;
  m_outerIndex = taucsMat.colptr;
  m_innerIndices = taucsMat.rowind;
  m_values = reinterpret_cast<Scalar*>(taucsMat.values.v);
  m_nnz = taucsMat.colptr[taucsMat.n];
}

template<typename MatrixType>
class SparseLLT<MatrixType,Taucs> : public SparseLLT<MatrixType>
{
  protected:
    typedef SparseLLT<MatrixType> Base;
    typedef typename Base::Scalar Scalar;
    typedef typename Base::RealScalar RealScalar;
    using Base::MatrixLIsDirty;
    using Base::SupernodalFactorIsDirty;
    using Base::m_flags;
    using Base::m_matrix;
    using Base::m_status;

  public:

    SparseLLT(int flags = 0)
      : Base(flags), m_taucsSupernodalFactor(0)
    {
    }

    SparseLLT(const MatrixType& matrix, int flags = 0)
      : Base(flags), m_taucsSupernodalFactor(0)
    {
      compute(matrix);
    }

    ~SparseLLT()
    {
      if (m_taucsSupernodalFactor)
        taucs_supernodal_factor_free(m_taucsSupernodalFactor);
    }

    inline const typename Base::CholMatrixType& matrixL(void) const;

    template<typename Derived>
    void solveInPlace(MatrixBase<Derived> &b) const;

    void compute(const MatrixType& matrix);

  protected:
    void* m_taucsSupernodalFactor;
};

template<typename MatrixType>
void SparseLLT<MatrixType,Taucs>::compute(const MatrixType& a)
{
  if (m_taucsSupernodalFactor)
  {
    taucs_supernodal_factor_free(m_taucsSupernodalFactor);
    m_taucsSupernodalFactor = 0;
  }

  if (m_flags & IncompleteFactorization)
  {
    taucs_ccs_matrix taucsMatA = const_cast<MatrixType&>(a).asTaucsMatrix();
    taucs_ccs_matrix* taucsRes = taucs_ccs_factor_llt(&taucsMatA, Base::m_precision, 0);
    // the matrix returned by Taucs is not necessarily sorted,
    // so let's copy it in two steps
    DynamicSparseMatrix<Scalar,RowMajor> tmp = MappedSparseMatrix<Scalar>(*taucsRes);
    m_matrix = tmp;
    free(taucsRes);
    m_status = (m_status & ~(CompleteFactorization|MatrixLIsDirty))
             | IncompleteFactorization
             | SupernodalFactorIsDirty;
  }
  else
  {
    taucs_ccs_matrix taucsMatA = const_cast<MatrixType&>(a).asTaucsMatrix();
    if ( (m_flags & SupernodalLeftLooking)
      || ((!(m_flags & SupernodalMultifrontal)) && (m_flags & MemoryEfficient)) )
    {
      m_taucsSupernodalFactor = taucs_ccs_factor_llt_ll(&taucsMatA);
    }
    else
    {
      // use the faster Multifrontal routine
      m_taucsSupernodalFactor = taucs_ccs_factor_llt_mf(&taucsMatA);
    }
    m_status = (m_status & ~IncompleteFactorization) | CompleteFactorization | MatrixLIsDirty;
  }
}

template<typename MatrixType>
inline const typename SparseLLT<MatrixType>::CholMatrixType&
SparseLLT<MatrixType,Taucs>::matrixL() const
{
  if (m_status & MatrixLIsDirty)
  {
    ei_assert(!(m_status & SupernodalFactorIsDirty));

    taucs_ccs_matrix* taucsL = taucs_supernodal_factor_to_ccs(m_taucsSupernodalFactor);

    // the matrix returned by Taucs is not necessarily sorted,
    // so let's copy it in two steps
    DynamicSparseMatrix<Scalar,RowMajor> tmp = MappedSparseMatrix<Scalar>(*taucsL);
    const_cast<typename Base::CholMatrixType&>(m_matrix) = tmp;
    free(taucsL);
    m_status = (m_status & ~MatrixLIsDirty);
  }
  return m_matrix;
}

template<typename MatrixType>
template<typename Derived>
void SparseLLT<MatrixType,Taucs>::solveInPlace(MatrixBase<Derived> &b) const
{
  bool inputIsCompatibleWithTaucs = (Derived::Flags&RowMajorBit)==0;

  if (!inputIsCompatibleWithTaucs)
  {
    matrixL();
    Base::solveInPlace(b);
  }
  else if (m_flags & IncompleteFactorization)
  {
    taucs_ccs_matrix taucsLLT = const_cast<typename Base::CholMatrixType&>(m_matrix).asTaucsMatrix();
    typename ei_plain_matrix_type<Derived>::type x(b.rows());
    for (int j=0; j<b.cols(); ++j)
    {
      taucs_ccs_solve_llt(&taucsLLT,x.data(),&b.col(j).coeffRef(0));
      b.col(j) = x;
    }
  }
  else
  {
    typename ei_plain_matrix_type<Derived>::type x(b.rows());
    for (int j=0; j<b.cols(); ++j)
    {
      taucs_supernodal_solve_llt(m_taucsSupernodalFactor,x.data(),&b.col(j).coeffRef(0));
      b.col(j) = x;
    }
  }
}

#endif // EIGEN_TAUCSSUPPORT_H
