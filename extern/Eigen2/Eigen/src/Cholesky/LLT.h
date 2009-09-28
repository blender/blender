// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
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

#ifndef EIGEN_LLT_H
#define EIGEN_LLT_H

/** \ingroup cholesky_Module
  *
  * \class LLT
  *
  * \brief Standard Cholesky decomposition (LL^T) of a matrix and associated features
  *
  * \param MatrixType the type of the matrix of which we are computing the LL^T Cholesky decomposition
  *
  * This class performs a LL^T Cholesky decomposition of a symmetric, positive definite
  * matrix A such that A = LL^* = U^*U, where L is lower triangular.
  *
  * While the Cholesky decomposition is particularly useful to solve selfadjoint problems like  D^*D x = b,
  * for that purpose, we recommend the Cholesky decomposition without square root which is more stable
  * and even faster. Nevertheless, this standard Cholesky decomposition remains useful in many other
  * situations like generalised eigen problems with hermitian matrices.
  *
  * Remember that Cholesky decompositions are not rank-revealing. This LLT decomposition is only stable on positive definite matrices,
  * use LDLT instead for the semidefinite case. Also, do not use a Cholesky decomposition to determine whether a system of equations
  * has a solution.
  *
  * \sa MatrixBase::llt(), class LDLT
  */
 /* HEY THIS DOX IS DISABLED BECAUSE THERE's A BUG EITHER HERE OR IN LDLT ABOUT THAT (OR BOTH)
  * Note that during the decomposition, only the upper triangular part of A is considered. Therefore,
  * the strict lower part does not have to store correct values.
  */
template<typename MatrixType> class LLT
{
  private:
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, 1> VectorType;

    enum {
      PacketSize = ei_packet_traits<Scalar>::size,
      AlignmentMask = int(PacketSize)-1
    };

  public:

    /** 
    * \brief Default Constructor.
    *
    * The default constructor is useful in cases in which the user intends to
    * perform decompositions via LLT::compute(const MatrixType&).
    */
    LLT() : m_matrix(), m_isInitialized(false) {}

    LLT(const MatrixType& matrix)
      : m_matrix(matrix.rows(), matrix.cols()),
        m_isInitialized(false)
    {
      compute(matrix);
    }

    /** \returns the lower triangular matrix L */
    inline Part<MatrixType, LowerTriangular> matrixL(void) const 
    { 
      ei_assert(m_isInitialized && "LLT is not initialized.");
      return m_matrix; 
    }
    
    /** \deprecated */
    inline bool isPositiveDefinite(void) const { return m_isInitialized && m_isPositiveDefinite; }

    template<typename RhsDerived, typename ResultType>
    bool solve(const MatrixBase<RhsDerived> &b, ResultType *result) const;

    template<typename Derived>
    bool solveInPlace(MatrixBase<Derived> &bAndX) const;

    void compute(const MatrixType& matrix);

  protected:
    /** \internal
      * Used to compute and store L
      * The strict upper part is not used and even not initialized.
      */
    MatrixType m_matrix;
    bool m_isInitialized;
    bool m_isPositiveDefinite;
};

/** Computes / recomputes the Cholesky decomposition A = LL^* = U^*U of \a matrix
  */
template<typename MatrixType>
void LLT<MatrixType>::compute(const MatrixType& a)
{
  assert(a.rows()==a.cols());
  m_isPositiveDefinite = true;
  const int size = a.rows();
  m_matrix.resize(size, size);
  // The biggest overall is the point of reference to which further diagonals
  // are compared; if any diagonal is negligible compared
  // to the largest overall, the algorithm bails.  This cutoff is suggested
  // in "Analysis of the Cholesky Decomposition of a Semi-definite Matrix" by
  // Nicholas J. Higham. Also see "Accuracy and Stability of Numerical
  // Algorithms" page 217, also by Higham.
  const RealScalar cutoff = machine_epsilon<Scalar>() * size * a.diagonal().cwise().abs().maxCoeff();
  RealScalar x;
  x = ei_real(a.coeff(0,0));
  m_matrix.coeffRef(0,0) = ei_sqrt(x);
  if(size==1)
  {
    m_isInitialized = true;
    return;
  }
  m_matrix.col(0).end(size-1) = a.row(0).end(size-1).adjoint() / ei_real(m_matrix.coeff(0,0));
  for (int j = 1; j < size; ++j)
  {
    x = ei_real(a.coeff(j,j)) - m_matrix.row(j).start(j).squaredNorm();
    if (x < cutoff)
    {
      m_isPositiveDefinite = false;
      continue;
    }

    m_matrix.coeffRef(j,j) = x = ei_sqrt(x);

    int endSize = size-j-1;
    if (endSize>0) {
      // Note that when all matrix columns have good alignment, then the following
      // product is guaranteed to be optimal with respect to alignment.
      m_matrix.col(j).end(endSize) =
        (m_matrix.block(j+1, 0, endSize, j) * m_matrix.row(j).start(j).adjoint()).lazy();

      // FIXME could use a.col instead of a.row
      m_matrix.col(j).end(endSize) = (a.row(j).end(endSize).adjoint()
        - m_matrix.col(j).end(endSize) ) / x;
    }
  }

  m_isInitialized = true;
}

/** Computes the solution x of \f$ A x = b \f$ using the current decomposition of A.
  * The result is stored in \a result
  *
  * \returns true always! If you need to check for existence of solutions, use another decomposition like LU, QR, or SVD.
  *
  * In other words, it computes \f$ b = A^{-1} b \f$ with
  * \f$ {L^{*}}^{-1} L^{-1} b \f$ from right to left.
  *
  * Example: \include LLT_solve.cpp
  * Output: \verbinclude LLT_solve.out
  *
  * \sa LLT::solveInPlace(), MatrixBase::llt()
  */
template<typename MatrixType>
template<typename RhsDerived, typename ResultType>
bool LLT<MatrixType>::solve(const MatrixBase<RhsDerived> &b, ResultType *result) const
{
  ei_assert(m_isInitialized && "LLT is not initialized.");
  const int size = m_matrix.rows();
  ei_assert(size==b.rows() && "LLT::solve(): invalid number of rows of the right hand side matrix b");
  return solveInPlace((*result) = b);
}

/** This is the \em in-place version of solve().
  *
  * \param bAndX represents both the right-hand side matrix b and result x.
  *
  * \returns true always! If you need to check for existence of solutions, use another decomposition like LU, QR, or SVD.
  *
  * This version avoids a copy when the right hand side matrix b is not
  * needed anymore.
  *
  * \sa LLT::solve(), MatrixBase::llt()
  */
template<typename MatrixType>
template<typename Derived>
bool LLT<MatrixType>::solveInPlace(MatrixBase<Derived> &bAndX) const
{
  ei_assert(m_isInitialized && "LLT is not initialized.");
  const int size = m_matrix.rows();
  ei_assert(size==bAndX.rows());
  matrixL().solveTriangularInPlace(bAndX);
  m_matrix.adjoint().template part<UpperTriangular>().solveTriangularInPlace(bAndX);
  return true;
}

/** \cholesky_module
  * \returns the LLT decomposition of \c *this
  */
template<typename Derived>
inline const LLT<typename MatrixBase<Derived>::PlainMatrixType>
MatrixBase<Derived>::llt() const
{
  return LLT<PlainMatrixType>(derived());
}

#endif // EIGEN_LLT_H
