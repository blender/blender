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

#ifndef EIGEN_QR_H
#define EIGEN_QR_H

/** \ingroup QR_Module
  * \nonstableyet
  *
  * \class QR
  *
  * \brief QR decomposition of a matrix
  *
  * \param MatrixType the type of the matrix of which we are computing the QR decomposition
  *
  * This class performs a QR decomposition using Householder transformations. The result is
  * stored in a compact way compatible with LAPACK.
  *
  * \sa MatrixBase::qr()
  */
template<typename MatrixType> class QR
{
  public:

    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef Block<MatrixType, MatrixType::ColsAtCompileTime, MatrixType::ColsAtCompileTime> MatrixRBlockType;
    typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, MatrixType::ColsAtCompileTime> MatrixTypeR;
    typedef Matrix<Scalar, MatrixType::ColsAtCompileTime, 1> VectorType;

    /** 
    * \brief Default Constructor.
    *
    * The default constructor is useful in cases in which the user intends to
    * perform decompositions via QR::compute(const MatrixType&).
    */
    QR() : m_qr(), m_hCoeffs(), m_isInitialized(false) {}

    QR(const MatrixType& matrix)
      : m_qr(matrix.rows(), matrix.cols()),
        m_hCoeffs(matrix.cols()),
        m_isInitialized(false)
    {
      compute(matrix);
    }
    
    /** \deprecated use isInjective()
      * \returns whether or not the matrix is of full rank
      *
      * \note Since the rank is computed only once, i.e. the first time it is needed, this
      *       method almost does not perform any further computation.
      */
    EIGEN_DEPRECATED bool isFullRank() const 
    { 
      ei_assert(m_isInitialized && "QR is not initialized.");
      return rank() == m_qr.cols(); 
    }
    
    /** \returns the rank of the matrix of which *this is the QR decomposition.
      *
      * \note Since the rank is computed only once, i.e. the first time it is needed, this
      *       method almost does not perform any further computation.
      */
    int rank() const;
    
    /** \returns the dimension of the kernel of the matrix of which *this is the QR decomposition.
      *
      * \note Since the rank is computed only once, i.e. the first time it is needed, this
      *       method almost does not perform any further computation.
      */
    inline int dimensionOfKernel() const
    {
      ei_assert(m_isInitialized && "QR is not initialized.");
      return m_qr.cols() - rank();
    }
    
    /** \returns true if the matrix of which *this is the QR decomposition represents an injective
      *          linear map, i.e. has trivial kernel; false otherwise.
      *
      * \note Since the rank is computed only once, i.e. the first time it is needed, this
      *       method almost does not perform any further computation.
      */
    inline bool isInjective() const
    {
      ei_assert(m_isInitialized && "QR is not initialized.");
      return rank() == m_qr.cols();
    }
    
    /** \returns true if the matrix of which *this is the QR decomposition represents a surjective
      *          linear map; false otherwise.
      *
      * \note Since the rank is computed only once, i.e. the first time it is needed, this
      *       method almost does not perform any further computation.
      */
    inline bool isSurjective() const
    {
      ei_assert(m_isInitialized && "QR is not initialized.");
      return rank() == m_qr.rows();
    }

    /** \returns true if the matrix of which *this is the QR decomposition is invertible.
      *
      * \note Since the rank is computed only once, i.e. the first time it is needed, this
      *       method almost does not perform any further computation.
      */
    inline bool isInvertible() const
    {
      ei_assert(m_isInitialized && "QR is not initialized.");
      return isInjective() && isSurjective();
    }
    
    /** \returns a read-only expression of the matrix R of the actual the QR decomposition */
    const Part<NestByValue<MatrixRBlockType>, UpperTriangular>
    matrixR(void) const
    {
      ei_assert(m_isInitialized && "QR is not initialized.");
      int cols = m_qr.cols();
      return MatrixRBlockType(m_qr, 0, 0, cols, cols).nestByValue().template part<UpperTriangular>();
    }

    /** This method finds a solution x to the equation Ax=b, where A is the matrix of which
      * *this is the QR decomposition, if any exists.
      *
      * \param b the right-hand-side of the equation to solve.
      *
      * \param result a pointer to the vector/matrix in which to store the solution, if any exists.
      *          Resized if necessary, so that result->rows()==A.cols() and result->cols()==b.cols().
      *          If no solution exists, *result is left with undefined coefficients.
      *
      * \returns true if any solution exists, false if no solution exists.
      *
      * \note If there exist more than one solution, this method will arbitrarily choose one.
      *       If you need a complete analysis of the space of solutions, take the one solution obtained
      *       by this method and add to it elements of the kernel, as determined by kernel().
      *
      * \note The case where b is a matrix is not yet implemented. Also, this
      *       code is space inefficient.
      *
      * Example: \include QR_solve.cpp
      * Output: \verbinclude QR_solve.out
      *
      * \sa MatrixBase::solveTriangular(), kernel(), computeKernel(), inverse(), computeInverse()
      */
    template<typename OtherDerived, typename ResultType>
    bool solve(const MatrixBase<OtherDerived>& b, ResultType *result) const;

    MatrixType matrixQ(void) const;

    void compute(const MatrixType& matrix);

  protected:
    MatrixType m_qr;
    VectorType m_hCoeffs;
    mutable int m_rank;
    mutable bool m_rankIsUptodate;
    bool m_isInitialized;
};

/** \returns the rank of the matrix of which *this is the QR decomposition. */
template<typename MatrixType>
int QR<MatrixType>::rank() const
{
  ei_assert(m_isInitialized && "QR is not initialized.");
  if (!m_rankIsUptodate)
  {
    RealScalar maxCoeff = m_qr.diagonal().cwise().abs().maxCoeff();
    int n = m_qr.cols();
    m_rank = 0;
    while(m_rank<n && !ei_isMuchSmallerThan(m_qr.diagonal().coeff(m_rank), maxCoeff))
      ++m_rank;
    m_rankIsUptodate = true;
  }
  return m_rank;
}

#ifndef EIGEN_HIDE_HEAVY_CODE

template<typename MatrixType>
void QR<MatrixType>::compute(const MatrixType& matrix)
{ 
  m_rankIsUptodate = false;
  m_qr = matrix;
  m_hCoeffs.resize(matrix.cols());

  int rows = matrix.rows();
  int cols = matrix.cols();
  RealScalar eps2 = precision<RealScalar>()*precision<RealScalar>();

  for (int k = 0; k < cols; ++k)
  {
    int remainingSize = rows-k;

    RealScalar beta;
    Scalar v0 = m_qr.col(k).coeff(k);

    if (remainingSize==1)
    {
      if (NumTraits<Scalar>::IsComplex)
      {
        // Householder transformation on the remaining single scalar
        beta = ei_abs(v0);
        if (ei_real(v0)>0)
          beta = -beta;
        m_qr.coeffRef(k,k) = beta;
        m_hCoeffs.coeffRef(k) = (beta - v0) / beta;
      }
      else
      {
        m_hCoeffs.coeffRef(k) = 0;
      }
    }
    else if ((beta=m_qr.col(k).end(remainingSize-1).squaredNorm())>eps2)
    // FIXME what about ei_imag(v0) ??
    {
      // form k-th Householder vector
      beta = ei_sqrt(ei_abs2(v0)+beta);
      if (ei_real(v0)>=0.)
        beta = -beta;
      m_qr.col(k).end(remainingSize-1) /= v0-beta;
      m_qr.coeffRef(k,k) = beta;
      Scalar h = m_hCoeffs.coeffRef(k) = (beta - v0) / beta;

      // apply the Householder transformation (I - h v v') to remaining columns, i.e.,
      // R <- (I - h v v') * R   where v = [1,m_qr(k+1,k), m_qr(k+2,k), ...]
      int remainingCols = cols - k -1;
      if (remainingCols>0)
      {
        m_qr.coeffRef(k,k) = Scalar(1);
        m_qr.corner(BottomRight, remainingSize, remainingCols) -= ei_conj(h) * m_qr.col(k).end(remainingSize)
            * (m_qr.col(k).end(remainingSize).adjoint() * m_qr.corner(BottomRight, remainingSize, remainingCols));
        m_qr.coeffRef(k,k) = beta;
      }
    }
    else
    {
      m_hCoeffs.coeffRef(k) = 0;
    }
  }
  m_isInitialized = true;
}

template<typename MatrixType>
template<typename OtherDerived, typename ResultType>
bool QR<MatrixType>::solve(
  const MatrixBase<OtherDerived>& b,
  ResultType *result
) const
{
  ei_assert(m_isInitialized && "QR is not initialized.");
  const int rows = m_qr.rows();
  ei_assert(b.rows() == rows);
  result->resize(rows, b.cols());
  
  // TODO(keir): There is almost certainly a faster way to multiply by
  // Q^T without explicitly forming matrixQ(). Investigate.
  *result = matrixQ().transpose()*b;
  
  if(!isSurjective())
  {
    // is result is in the image of R ?
    RealScalar biggest_in_res = result->corner(TopLeft, m_rank, result->cols()).cwise().abs().maxCoeff();
    for(int col = 0; col < result->cols(); ++col)
      for(int row = m_rank; row < result->rows(); ++row)
        if(!ei_isMuchSmallerThan(result->coeff(row,col), biggest_in_res))
          return false;
  }
  m_qr.corner(TopLeft, m_rank, m_rank)
      .template marked<UpperTriangular>()
      .solveTriangularInPlace(result->corner(TopLeft, m_rank, result->cols()));

  return true;
}

/** \returns the matrix Q */
template<typename MatrixType>
MatrixType QR<MatrixType>::matrixQ() const
{
  ei_assert(m_isInitialized && "QR is not initialized.");
  // compute the product Q_0 Q_1 ... Q_n-1,
  // where Q_k is the k-th Householder transformation I - h_k v_k v_k'
  // and v_k is the k-th Householder vector [1,m_qr(k+1,k), m_qr(k+2,k), ...]
  int rows = m_qr.rows();
  int cols = m_qr.cols();
  MatrixType res = MatrixType::Identity(rows, cols);
  for (int k = cols-1; k >= 0; k--)
  {
    // to make easier the computation of the transformation, let's temporarily
    // overwrite m_qr(k,k) such that the end of m_qr.col(k) is exactly our Householder vector.
    Scalar beta = m_qr.coeff(k,k);
    m_qr.const_cast_derived().coeffRef(k,k) = 1;
    int endLength = rows-k;
    res.corner(BottomRight,endLength, cols-k) -= ((m_hCoeffs.coeff(k) * m_qr.col(k).end(endLength))
      * (m_qr.col(k).end(endLength).adjoint() * res.corner(BottomRight,endLength, cols-k)).lazy()).lazy();
    m_qr.const_cast_derived().coeffRef(k,k) = beta;
  }
  return res;
}

#endif // EIGEN_HIDE_HEAVY_CODE

/** \return the QR decomposition of \c *this.
  *
  * \sa class QR
  */
template<typename Derived>
const QR<typename MatrixBase<Derived>::PlainMatrixType>
MatrixBase<Derived>::qr() const
{
  return QR<PlainMatrixType>(eval());
}


#endif // EIGEN_QR_H
