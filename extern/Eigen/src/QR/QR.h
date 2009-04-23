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

    QR(const MatrixType& matrix)
      : m_qr(matrix.rows(), matrix.cols()),
        m_hCoeffs(matrix.cols())
    {
      _compute(matrix);
    }

    /** \returns whether or not the matrix is of full rank */
    bool isFullRank() const { return rank() == std::min(m_qr.rows(),m_qr.cols()); }
    
    int rank() const;

    /** \returns a read-only expression of the matrix R of the actual the QR decomposition */
    const Part<NestByValue<MatrixRBlockType>, UpperTriangular>
    matrixR(void) const
    {
      int cols = m_qr.cols();
      return MatrixRBlockType(m_qr, 0, 0, cols, cols).nestByValue().template part<UpperTriangular>();
    }

    MatrixType matrixQ(void) const;

  private:

    void _compute(const MatrixType& matrix);

  protected:
    MatrixType m_qr;
    VectorType m_hCoeffs;
    mutable int m_rank;
    mutable bool m_rankIsUptodate;
};

/** \returns the rank of the matrix of which *this is the QR decomposition. */
template<typename MatrixType>
int QR<MatrixType>::rank() const
{
  if (!m_rankIsUptodate)
  {
    RealScalar maxCoeff = m_qr.diagonal().maxCoeff();
    int n = std::min(m_qr.rows(),m_qr.cols());
    m_rank = n;
    for (int i=0; i<n; ++i)
      if (ei_isMuchSmallerThan(m_qr.diagonal().coeff(i), maxCoeff))
        --m_rank;
    m_rankIsUptodate = true;
  }
  return m_rank;
}

#ifndef EIGEN_HIDE_HEAVY_CODE

template<typename MatrixType>
void QR<MatrixType>::_compute(const MatrixType& matrix)
{
  m_rankIsUptodate = false;
  m_qr = matrix;
  int rows = matrix.rows();
  int cols = matrix.cols();

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
    else if ( (!ei_isMuchSmallerThan(beta=m_qr.col(k).end(remainingSize-1).squaredNorm(),static_cast<Scalar>(1))) || ei_imag(v0)==0 )
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
}

/** \returns the matrix Q */
template<typename MatrixType>
MatrixType QR<MatrixType>::matrixQ(void) const
{
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
