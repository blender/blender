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

#ifndef EIGEN_LDLT_H
#define EIGEN_LDLT_H

/** \ingroup cholesky_Module
  *
  * \class LDLT
  *
  * \brief Robust Cholesky decomposition of a matrix and associated features
  *
  * \param MatrixType the type of the matrix of which we are computing the LDL^T Cholesky decomposition
  *
  * This class performs a Cholesky decomposition without square root of a symmetric, positive definite
  * matrix A such that A = L D L^* = U^* D U, where L is lower triangular with a unit diagonal
  * and D is a diagonal matrix.
  *
  * Compared to a standard Cholesky decomposition, avoiding the square roots allows for faster and more
  * stable computation.
  *
  * Note that during the decomposition, only the upper triangular part of A is considered. Therefore,
  * the strict lower part does not have to store correct values.
  *
  * \sa MatrixBase::ldlt(), class LLT
  */
template<typename MatrixType> class LDLT
{
  public:

    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, 1> VectorType;

    LDLT(const MatrixType& matrix)
      : m_matrix(matrix.rows(), matrix.cols())
    {
      compute(matrix);
    }

    /** \returns the lower triangular matrix L */
    inline Part<MatrixType, UnitLowerTriangular> matrixL(void) const { return m_matrix; }

    /** \returns the coefficients of the diagonal matrix D */
    inline DiagonalCoeffs<MatrixType> vectorD(void) const { return m_matrix.diagonal(); }

    /** \returns true if the matrix is positive definite */
    inline bool isPositiveDefinite(void) const { return m_isPositiveDefinite; }

    template<typename RhsDerived, typename ResDerived>
    bool solve(const MatrixBase<RhsDerived> &b, MatrixBase<ResDerived> *result) const;

    template<typename Derived>
    bool solveInPlace(MatrixBase<Derived> &bAndX) const;

    void compute(const MatrixType& matrix);

  protected:
    /** \internal
      * Used to compute and store the cholesky decomposition A = L D L^* = U^* D U.
      * The strict upper part is used during the decomposition, the strict lower
      * part correspond to the coefficients of L (its diagonal is equal to 1 and
      * is not stored), and the diagonal entries correspond to D.
      */
    MatrixType m_matrix;

    bool m_isPositiveDefinite;
};

/** Compute / recompute the LLT decomposition A = L D L^* = U^* D U of \a matrix
  */
template<typename MatrixType>
void LDLT<MatrixType>::compute(const MatrixType& a)
{
  assert(a.rows()==a.cols());
  const int size = a.rows();
  m_matrix.resize(size, size);
  m_isPositiveDefinite = true;
  const RealScalar eps = ei_sqrt(precision<Scalar>());

  if (size<=1)
  {
    m_matrix = a;
    return;
  }

  // Let's preallocate a temporay vector to evaluate the matrix-vector product into it.
  // Unlike the standard LLT decomposition, here we cannot evaluate it to the destination
  // matrix because it a sub-row which is not compatible suitable for efficient packet evaluation.
  // (at least if we assume the matrix is col-major)
  Matrix<Scalar,MatrixType::RowsAtCompileTime,1> _temporary(size);

  // Note that, in this algorithm the rows of the strict upper part of m_matrix is used to store
  // column vector, thus the strange .conjugate() and .transpose()...

  m_matrix.row(0) = a.row(0).conjugate();
  m_matrix.col(0).end(size-1) = m_matrix.row(0).end(size-1) / m_matrix.coeff(0,0);
  for (int j = 1; j < size; ++j)
  {
    RealScalar tmp = ei_real(a.coeff(j,j) - (m_matrix.row(j).start(j) * m_matrix.col(j).start(j).conjugate()).coeff(0,0));
    m_matrix.coeffRef(j,j) = tmp;

    if (tmp < eps)
    {
      m_isPositiveDefinite = false;
      return;
    }

    int endSize = size-j-1;
    if (endSize>0)
    {
      _temporary.end(endSize) = ( m_matrix.block(j+1,0, endSize, j)
                                  * m_matrix.col(j).start(j).conjugate() ).lazy();

      m_matrix.row(j).end(endSize) = a.row(j).end(endSize).conjugate()
                                   - _temporary.end(endSize).transpose();

      m_matrix.col(j).end(endSize) = m_matrix.row(j).end(endSize) / tmp;
    }
  }
}

/** Computes the solution x of \f$ A x = b \f$ using the current decomposition of A.
  * The result is stored in \a result
  *
  * \returns true in case of success, false otherwise.
  *
  * In other words, it computes \f$ b = A^{-1} b \f$ with
  * \f$ {L^{*}}^{-1} D^{-1} L^{-1} b \f$ from right to left.
  *
  * \sa LDLT::solveInPlace(), MatrixBase::ldlt()
  */
template<typename MatrixType>
template<typename RhsDerived, typename ResDerived>
bool LDLT<MatrixType>
::solve(const MatrixBase<RhsDerived> &b, MatrixBase<ResDerived> *result) const
{
  const int size = m_matrix.rows();
  ei_assert(size==b.rows() && "LLT::solve(): invalid number of rows of the right hand side matrix b");
  *result = b;
  return solveInPlace(*result);
}

/** This is the \em in-place version of solve().
  *
  * \param bAndX represents both the right-hand side matrix b and result x.
  *
  * This version avoids a copy when the right hand side matrix b is not
  * needed anymore.
  *
  * \sa LDLT::solve(), MatrixBase::ldlt()
  */
template<typename MatrixType>
template<typename Derived>
bool LDLT<MatrixType>::solveInPlace(MatrixBase<Derived> &bAndX) const
{
  const int size = m_matrix.rows();
  ei_assert(size==bAndX.rows());
  if (!m_isPositiveDefinite)
    return false;
  matrixL().solveTriangularInPlace(bAndX);
  bAndX = (m_matrix.cwise().inverse().template part<Diagonal>() * bAndX).lazy();
  m_matrix.adjoint().template part<UnitUpperTriangular>().solveTriangularInPlace(bAndX);
  return true;
}

/** \cholesky_module
  * \returns the Cholesky decomposition without square root of \c *this
  */
template<typename Derived>
inline const LDLT<typename MatrixBase<Derived>::PlainMatrixType>
MatrixBase<Derived>::ldlt() const
{
  return derived();
}

#endif // EIGEN_LDLT_H
