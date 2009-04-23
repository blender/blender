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

#ifndef EIGEN_SPARSELU_H
#define EIGEN_SPARSELU_H

/** \ingroup Sparse_Module
  *
  * \class SparseLU
  *
  * \brief LU decomposition of a sparse matrix and associated features
  *
  * \param MatrixType the type of the matrix of which we are computing the LU factorization
  *
  * \sa class LU, class SparseLLT
  */
template<typename MatrixType, int Backend = DefaultBackend>
class SparseLU
{
  protected:
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef SparseMatrix<Scalar,LowerTriangular> LUMatrixType;

    enum {
      MatrixLUIsDirty             = 0x10000
    };

  public:

    /** Creates a dummy LU factorization object with flags \a flags. */
    SparseLU(int flags = 0)
      : m_flags(flags), m_status(0)
    {
      m_precision = RealScalar(0.1) * Eigen::precision<RealScalar>();
    }

    /** Creates a LU object and compute the respective factorization of \a matrix using
      * flags \a flags. */
    SparseLU(const MatrixType& matrix, int flags = 0)
      : /*m_matrix(matrix.rows(), matrix.cols()),*/ m_flags(flags), m_status(0)
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
      *  - MemoryEfficient
      *  - one of the ordering methods
      *  - etc...
      *
      * \sa flags() */
    void setFlags(int f) { m_flags = f; }
    /** \returns the current flags */
    int flags() const { return m_flags; }

    void setOrderingMethod(int m)
    {
      ei_assert(m&~OrderingMask == 0 && m!=0 && "invalid ordering method");
      m_flags = m_flags&~OrderingMask | m&OrderingMask;
    }

    int orderingMethod() const
    {
      return m_flags&OrderingMask;
    }

    /** Computes/re-computes the LU factorization */
    void compute(const MatrixType& matrix);

    /** \returns the lower triangular matrix L */
    //inline const MatrixType& matrixL() const { return m_matrixL; }

    /** \returns the upper triangular matrix U */
    //inline const MatrixType& matrixU() const { return m_matrixU; }

    template<typename BDerived, typename XDerived>
    bool solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived>* x) const;

    /** \returns true if the factorization succeeded */
    inline bool succeeded(void) const { return m_succeeded; }

  protected:
    RealScalar m_precision;
    int m_flags;
    mutable int m_status;
    bool m_succeeded;
};

/** Computes / recomputes the LU decomposition of matrix \a a
  * using the default algorithm.
  */
template<typename MatrixType, int Backend>
void SparseLU<MatrixType,Backend>::compute(const MatrixType& a)
{
  ei_assert(false && "not implemented yet");
}

/** Computes *x = U^-1 L^-1 b */
template<typename MatrixType, int Backend>
template<typename BDerived, typename XDerived>
bool SparseLU<MatrixType,Backend>::solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived>* x) const
{
  ei_assert(false && "not implemented yet");
  return false;
}

#endif // EIGEN_SPARSELU_H
