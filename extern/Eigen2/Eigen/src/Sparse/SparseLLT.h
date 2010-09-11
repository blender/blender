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

#ifndef EIGEN_SPARSELLT_H
#define EIGEN_SPARSELLT_H

/** \ingroup Sparse_Module
  *
  * \class SparseLLT
  *
  * \brief LLT Cholesky decomposition of a sparse matrix and associated features
  *
  * \param MatrixType the type of the matrix of which we are computing the LLT Cholesky decomposition
  *
  * \sa class LLT, class LDLT
  */
template<typename MatrixType, int Backend = DefaultBackend>
class SparseLLT
{
  protected:
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef SparseMatrix<Scalar,LowerTriangular> CholMatrixType;

    enum {
      SupernodalFactorIsDirty      = 0x10000,
      MatrixLIsDirty               = 0x20000
    };

  public:

    /** Creates a dummy LLT factorization object with flags \a flags. */
    SparseLLT(int flags = 0)
      : m_flags(flags), m_status(0)
    {
      m_precision = RealScalar(0.1) * Eigen::precision<RealScalar>();
    }

    /** Creates a LLT object and compute the respective factorization of \a matrix using
      * flags \a flags. */
    SparseLLT(const MatrixType& matrix, int flags = 0)
      : m_matrix(matrix.rows(), matrix.cols()), m_flags(flags), m_status(0)
    {
      m_precision = RealScalar(0.1) * Eigen::precision<RealScalar>();
      compute(matrix);
    }

    /** Sets the relative threshold value used to prune zero coefficients during the decomposition.
      *
      * Setting a value greater than zero speeds up computation, and yields to an imcomplete
      * factorization with fewer non zero coefficients. Such approximate factors are especially
      * useful to initialize an iterative solver.
      *
      * \warning if precision is greater that zero, the LLT factorization is not guaranteed to succeed
      * even if the matrix is positive definite.
      *
      * Note that the exact meaning of this parameter might depends on the actual
      * backend. Moreover, not all backends support this feature.
      *
      * \sa precision() */
    void setPrecision(RealScalar v) { m_precision = v; }

    /** \returns the current precision.
      *
      * \sa setPrecision() */
    RealScalar precision() const { return m_precision; }

    /** Sets the flags. Possible values are:
      *  - CompleteFactorization
      *  - IncompleteFactorization
      *  - MemoryEfficient          (hint to use the memory most efficient method offered by the backend)
      *  - SupernodalMultifrontal   (implies a complete factorization if supported by the backend,
      *                              overloads the MemoryEfficient flags)
      *  - SupernodalLeftLooking    (implies a complete factorization  if supported by the backend,
      *                              overloads the MemoryEfficient flags)
      *
      * \sa flags() */
    void setFlags(int f) { m_flags = f; }
    /** \returns the current flags */
    int flags() const { return m_flags; }

    /** Computes/re-computes the LLT factorization */
    void compute(const MatrixType& matrix);

    /** \returns the lower triangular matrix L */
    inline const CholMatrixType& matrixL(void) const { return m_matrix; }

    template<typename Derived>
    bool solveInPlace(MatrixBase<Derived> &b) const;

    /** \returns true if the factorization succeeded */
    inline bool succeeded(void) const { return m_succeeded; }

  protected:
    CholMatrixType m_matrix;
    RealScalar m_precision;
    int m_flags;
    mutable int m_status;
    bool m_succeeded;
};

/** Computes / recomputes the LLT decomposition of matrix \a a
  * using the default algorithm.
  */
template<typename MatrixType, int Backend>
void SparseLLT<MatrixType,Backend>::compute(const MatrixType& a)
{
  assert(a.rows()==a.cols());
  const int size = a.rows();
  m_matrix.resize(size, size);

  // allocate a temporary vector for accumulations
  AmbiVector<Scalar> tempVector(size);
  RealScalar density = a.nonZeros()/RealScalar(size*size);

  // TODO estimate the number of non zeros
  m_matrix.startFill(a.nonZeros()*2);
  for (int j = 0; j < size; ++j)
  {
    Scalar x = ei_real(a.coeff(j,j));

    // TODO better estimate of the density !
    tempVector.init(density>0.001? IsDense : IsSparse);
    tempVector.setBounds(j+1,size);
    tempVector.setZero();
    // init with current matrix a
    {
      typename MatrixType::InnerIterator it(a,j);
      ++it; // skip diagonal element
      for (; it; ++it)
        tempVector.coeffRef(it.index()) = it.value();
    }
    for (int k=0; k<j+1; ++k)
    {
      typename CholMatrixType::InnerIterator it(m_matrix, k);
      while (it && it.index()<j)
        ++it;
      if (it && it.index()==j)
      {
        Scalar y = it.value();
        x -= ei_abs2(y);
        ++it; // skip j-th element, and process remaining column coefficients
        tempVector.restart();
        for (; it; ++it)
        {
          tempVector.coeffRef(it.index()) -= it.value() * y;
        }
      }
    }
    // copy the temporary vector to the respective m_matrix.col()
    // while scaling the result by 1/real(x)
    RealScalar rx = ei_sqrt(ei_real(x));
    m_matrix.fill(j,j) = rx;
    Scalar y = Scalar(1)/rx;
    for (typename AmbiVector<Scalar>::Iterator it(tempVector, m_precision*rx); it; ++it)
    {
      m_matrix.fill(it.index(), j) = it.value() * y;
    }
  }
  m_matrix.endFill();
}

/** Computes b = L^-T L^-1 b */
template<typename MatrixType, int Backend>
template<typename Derived>
bool SparseLLT<MatrixType, Backend>::solveInPlace(MatrixBase<Derived> &b) const
{
  const int size = m_matrix.rows();
  ei_assert(size==b.rows());

  m_matrix.solveTriangularInPlace(b);
  // FIXME should be simply .adjoint() but it fails to compile...
  if (NumTraits<Scalar>::IsComplex)
  {
    CholMatrixType aux = m_matrix.conjugate();
    aux.transpose().solveTriangularInPlace(b);
  }
  else
    m_matrix.transpose().solveTriangularInPlace(b);

  return true;
}

#endif // EIGEN_SPARSELLT_H
