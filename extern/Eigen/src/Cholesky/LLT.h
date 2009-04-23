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
  * Note that during the decomposition, only the upper triangular part of A is considered. Therefore,
  * the strict lower part does not have to store correct values.
  *
  * \sa MatrixBase::llt(), class LDLT
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

    LLT(const MatrixType& matrix)
      : m_matrix(matrix.rows(), matrix.cols())
    {
      compute(matrix);
    }

    /** \returns the lower triangular matrix L */
    inline Part<MatrixType, LowerTriangular> matrixL(void) const { return m_matrix; }

    /** \returns true if the matrix is positive definite */
    inline bool isPositiveDefinite(void) const { return m_isPositiveDefinite; }

    template<typename RhsDerived, typename ResDerived>
    bool solve(const MatrixBase<RhsDerived> &b, MatrixBase<ResDerived> *result) const;

    template<typename Derived>
    bool solveInPlace(MatrixBase<Derived> &bAndX) const;

    void compute(const MatrixType& matrix);

  protected:
    /** \internal
      * Used to compute and store L
      * The strict upper part is not used and even not initialized.
      */
    MatrixType m_matrix;
    bool m_isPositiveDefinite;
};

/** Computes / recomputes the Cholesky decomposition A = LL^* = U^*U of \a matrix
  */
template<typename MatrixType>
void LLT<MatrixType>::compute(const MatrixType& a)
{
  assert(a.rows()==a.cols());
  const int size = a.rows();
  m_matrix.resize(size, size);
  const RealScalar eps = ei_sqrt(precision<Scalar>());

  RealScalar x;
  x = ei_real(a.coeff(0,0));
  m_isPositiveDefinite = x > eps && ei_isMuchSmallerThan(ei_imag(a.coeff(0,0)), RealScalar(1));
  m_matrix.coeffRef(0,0) = ei_sqrt(x);
  m_matrix.col(0).end(size-1) = a.row(0).end(size-1).adjoint() / ei_real(m_matrix.coeff(0,0));
  for (int j = 1; j < size; ++j)
  {
    Scalar tmp = ei_real(a.coeff(j,j)) - m_matrix.row(j).start(j).squaredNorm();
    x = ei_real(tmp);
    if (x < eps || (!ei_isMuchSmallerThan(ei_imag(tmp), RealScalar(1))))
    {
      m_isPositiveDefinite = false;
      return;
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
}

/** Computes the solution x of \f$ A x = b \f$ using the current decomposition of A.
  * The result is stored in \a result
  *
  * \returns true in case of success, false otherwise.
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
template<typename RhsDerived, typename ResDerived>
bool LLT<MatrixType>::solve(const MatrixBase<RhsDerived> &b, MatrixBase<ResDerived> *result) const
{
  const int size = m_matrix.rows();
  ei_assert(size==b.rows() && "LLT::solve(): invalid number of rows of the right hand side matrix b");
  return solveInPlace((*result) = b);
}

/** This is the \em in-place version of solve().
  *
  * \param bAndX represents both the right-hand side matrix b and result x.
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
  const int size = m_matrix.rows();
  ei_assert(size==bAndX.rows());
  if (!m_isPositiveDefinite)
    return false;
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
