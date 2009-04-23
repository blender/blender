// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_LU_H
#define EIGEN_LU_H

/** \ingroup LU_Module
  *
  * \class LU
  *
  * \brief LU decomposition of a matrix with complete pivoting, and related features
  *
  * \param MatrixType the type of the matrix of which we are computing the LU decomposition
  *
  * This class represents a LU decomposition of any matrix, with complete pivoting: the matrix A
  * is decomposed as A = PLUQ where L is unit-lower-triangular, U is upper-triangular, and P and Q
  * are permutation matrices. This is a rank-revealing LU decomposition. The eigenvalues (diagonal
  * coefficients) of U are sorted in such a way that any zeros are at the end, so that the rank
  * of A is the index of the first zero on the diagonal of U (with indices starting at 0) if any.
  *
  * This decomposition provides the generic approach to solving systems of linear equations, computing
  * the rank, invertibility, inverse, kernel, and determinant.
  *
  * This LU decomposition is very stable and well tested with large matrices. Even exact rank computation
  * works at sizes larger than 1000x1000. However there are use cases where the SVD decomposition is inherently
  * more stable when dealing with numerically damaged input. For example, computing the kernel is more stable with
  * SVD because the SVD can determine which singular values are negligible while LU has to work at the level of matrix
  * coefficients that are less meaningful in this respect.
  *
  * The data of the LU decomposition can be directly accessed through the methods matrixLU(),
  * permutationP(), permutationQ().
  *
  * As an exemple, here is how the original matrix can be retrieved:
  * \include class_LU.cpp
  * Output: \verbinclude class_LU.out
  *
  * \sa MatrixBase::lu(), MatrixBase::determinant(), MatrixBase::inverse(), MatrixBase::computeInverse()
  */
template<typename MatrixType> class LU
{
  public:

    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef Matrix<int, 1, MatrixType::ColsAtCompileTime> IntRowVectorType;
    typedef Matrix<int, MatrixType::RowsAtCompileTime, 1> IntColVectorType;
    typedef Matrix<Scalar, 1, MatrixType::ColsAtCompileTime> RowVectorType;
    typedef Matrix<Scalar, MatrixType::RowsAtCompileTime, 1> ColVectorType;

    enum { MaxSmallDimAtCompileTime = EIGEN_ENUM_MIN(
             MatrixType::MaxColsAtCompileTime,
             MatrixType::MaxRowsAtCompileTime)
    };

    typedef Matrix<typename MatrixType::Scalar,
                  MatrixType::ColsAtCompileTime, // the number of rows in the "kernel matrix" is the number of cols of the original matrix
                                                 // so that the product "matrix * kernel = zero" makes sense
                  Dynamic,                       // we don't know at compile-time the dimension of the kernel
                  MatrixType::Options,
                  MatrixType::MaxColsAtCompileTime, // see explanation for 2nd template parameter
                  MatrixType::MaxColsAtCompileTime // the kernel is a subspace of the domain space, whose dimension is the number
                                                   // of columns of the original matrix
    > KernelResultType;

    typedef Matrix<typename MatrixType::Scalar,
                   MatrixType::RowsAtCompileTime, // the image is a subspace of the destination space, whose dimension is the number
                                                  // of rows of the original matrix
                   Dynamic,                       // we don't know at compile time the dimension of the image (the rank)
                   MatrixType::Options,
                   MatrixType::MaxRowsAtCompileTime, // the image matrix will consist of columns from the original matrix,
                   MatrixType::MaxColsAtCompileTime  // so it has the same number of rows and at most as many columns.
    > ImageResultType;

    /** Constructor.
      *
      * \param matrix the matrix of which to compute the LU decomposition.
      */
    LU(const MatrixType& matrix);

    /** \returns the LU decomposition matrix: the upper-triangular part is U, the
      * unit-lower-triangular part is L (at least for square matrices; in the non-square
      * case, special care is needed, see the documentation of class LU).
      *
      * \sa matrixL(), matrixU()
      */
    inline const MatrixType& matrixLU() const
    {
      return m_lu;
    }

    /** \returns a vector of integers, whose size is the number of rows of the matrix being decomposed,
      * representing the P permutation i.e. the permutation of the rows. For its precise meaning,
      * see the examples given in the documentation of class LU.
      *
      * \sa permutationQ()
      */
    inline const IntColVectorType& permutationP() const
    {
      return m_p;
    }

    /** \returns a vector of integers, whose size is the number of columns of the matrix being
      * decomposed, representing the Q permutation i.e. the permutation of the columns.
      * For its precise meaning, see the examples given in the documentation of class LU.
      *
      * \sa permutationP()
      */
    inline const IntRowVectorType& permutationQ() const
    {
      return m_q;
    }

    /** Computes a basis of the kernel of the matrix, also called the null-space of the matrix.
      *
      * \note This method is only allowed on non-invertible matrices, as determined by
      * isInvertible(). Calling it on an invertible matrix will make an assertion fail.
      *
      * \param result a pointer to the matrix in which to store the kernel. The columns of this
      * matrix will be set to form a basis of the kernel (it will be resized
      * if necessary).
      *
      * Example: \include LU_computeKernel.cpp
      * Output: \verbinclude LU_computeKernel.out
      *
      * \sa kernel(), computeImage(), image()
      */
    template<typename KernelMatrixType>
    void computeKernel(KernelMatrixType *result) const;

    /** Computes a basis of the image of the matrix, also called the column-space or range of he matrix.
      *
      * \note Calling this method on the zero matrix will make an assertion fail.
      *
      * \param result a pointer to the matrix in which to store the image. The columns of this
      * matrix will be set to form a basis of the image (it will be resized
      * if necessary).
      *
      * Example: \include LU_computeImage.cpp
      * Output: \verbinclude LU_computeImage.out
      *
      * \sa image(), computeKernel(), kernel()
      */
    template<typename ImageMatrixType>
    void computeImage(ImageMatrixType *result) const;

    /** \returns the kernel of the matrix, also called its null-space. The columns of the returned matrix
      * will form a basis of the kernel.
      *
      * \note: this method is only allowed on non-invertible matrices, as determined by
      * isInvertible(). Calling it on an invertible matrix will make an assertion fail.
      *
      * \note: this method returns a matrix by value, which induces some inefficiency.
      *        If you prefer to avoid this overhead, use computeKernel() instead.
      *
      * Example: \include LU_kernel.cpp
      * Output: \verbinclude LU_kernel.out
      *
      * \sa computeKernel(), image()
      */
    const KernelResultType kernel() const;

    /** \returns the image of the matrix, also called its column-space. The columns of the returned matrix
      * will form a basis of the kernel.
      *
      * \note: Calling this method on the zero matrix will make an assertion fail.
      *
      * \note: this method returns a matrix by value, which induces some inefficiency.
      *        If you prefer to avoid this overhead, use computeImage() instead.
      *
      * Example: \include LU_image.cpp
      * Output: \verbinclude LU_image.out
      *
      * \sa computeImage(), kernel()
      */
    const ImageResultType image() const;

    /** This method finds a solution x to the equation Ax=b, where A is the matrix of which
      * *this is the LU decomposition, if any exists.
      *
      * \param b the right-hand-side of the equation to solve. Can be a vector or a matrix,
      *          the only requirement in order for the equation to make sense is that
      *          b.rows()==A.rows(), where A is the matrix of which *this is the LU decomposition.
      * \param result a pointer to the vector or matrix in which to store the solution, if any exists.
      *          Resized if necessary, so that result->rows()==A.cols() and result->cols()==b.cols().
      *          If no solution exists, *result is left with undefined coefficients.
      *
      * \returns true if any solution exists, false if no solution exists.
      *
      * \note If there exist more than one solution, this method will arbitrarily choose one.
      *       If you need a complete analysis of the space of solutions, take the one solution obtained
      *       by this method and add to it elements of the kernel, as determined by kernel().
      *
      * Example: \include LU_solve.cpp
      * Output: \verbinclude LU_solve.out
      *
      * \sa MatrixBase::solveTriangular(), kernel(), computeKernel(), inverse(), computeInverse()
      */
    template<typename OtherDerived, typename ResultType>
    bool solve(const MatrixBase<OtherDerived>& b, ResultType *result) const;

    /** \returns the determinant of the matrix of which
      * *this is the LU decomposition. It has only linear complexity
      * (that is, O(n) where n is the dimension of the square matrix)
      * as the LU decomposition has already been computed.
      *
      * \note This is only for square matrices.
      *
      * \note For fixed-size matrices of size up to 4, MatrixBase::determinant() offers
      *       optimized paths.
      *
      * \warning a determinant can be very big or small, so for matrices
      * of large enough dimension, there is a risk of overflow/underflow.
      *
      * \sa MatrixBase::determinant()
      */
    typename ei_traits<MatrixType>::Scalar determinant() const;

    /** \returns the rank of the matrix of which *this is the LU decomposition.
      *
      * \note This is computed at the time of the construction of the LU decomposition. This
      *       method does not perform any further computation.
      */
    inline int rank() const
    {
      return m_rank;
    }

    /** \returns the dimension of the kernel of the matrix of which *this is the LU decomposition.
      *
      * \note Since the rank is computed at the time of the construction of the LU decomposition, this
      *       method almost does not perform any further computation.
      */
    inline int dimensionOfKernel() const
    {
      return m_lu.cols() - m_rank;
    }

    /** \returns true if the matrix of which *this is the LU decomposition represents an injective
      *          linear map, i.e. has trivial kernel; false otherwise.
      *
      * \note Since the rank is computed at the time of the construction of the LU decomposition, this
      *       method almost does not perform any further computation.
      */
    inline bool isInjective() const
    {
      return m_rank == m_lu.cols();
    }

    /** \returns true if the matrix of which *this is the LU decomposition represents a surjective
      *          linear map; false otherwise.
      *
      * \note Since the rank is computed at the time of the construction of the LU decomposition, this
      *       method almost does not perform any further computation.
      */
    inline bool isSurjective() const
    {
      return m_rank == m_lu.rows();
    }

    /** \returns true if the matrix of which *this is the LU decomposition is invertible.
      *
      * \note Since the rank is computed at the time of the construction of the LU decomposition, this
      *       method almost does not perform any further computation.
      */
    inline bool isInvertible() const
    {
      return isInjective() && isSurjective();
    }

    /** Computes the inverse of the matrix of which *this is the LU decomposition.
      *
      * \param result a pointer to the matrix into which to store the inverse. Resized if needed.
      *
      * \note If this matrix is not invertible, *result is left with undefined coefficients.
      *       Use isInvertible() to first determine whether this matrix is invertible.
      *
      * \sa MatrixBase::computeInverse(), inverse()
      */
    inline void computeInverse(MatrixType *result) const
    {
      solve(MatrixType::Identity(m_lu.rows(), m_lu.cols()), result);
    }

    /** \returns the inverse of the matrix of which *this is the LU decomposition.
      *
      * \note If this matrix is not invertible, the returned matrix has undefined coefficients.
      *       Use isInvertible() to first determine whether this matrix is invertible.
      *
      * \sa computeInverse(), MatrixBase::inverse()
      */
    inline MatrixType inverse() const
    {
      MatrixType result;
      computeInverse(&result);
      return result;
    }

  protected:
    const MatrixType& m_originalMatrix;
    MatrixType m_lu;
    IntColVectorType m_p;
    IntRowVectorType m_q;
    int m_det_pq;
    int m_rank;
};

template<typename MatrixType>
LU<MatrixType>::LU(const MatrixType& matrix)
  : m_originalMatrix(matrix),
    m_lu(matrix),
    m_p(matrix.rows()),
    m_q(matrix.cols())
{
  const int size = matrix.diagonal().size();
  const int rows = matrix.rows();
  const int cols = matrix.cols();

  IntColVectorType rows_transpositions(matrix.rows());
  IntRowVectorType cols_transpositions(matrix.cols());
  int number_of_transpositions = 0;

  RealScalar biggest = RealScalar(0);
  m_rank = size;
  for(int k = 0; k < size; ++k)
  {
    int row_of_biggest_in_corner, col_of_biggest_in_corner;
    RealScalar biggest_in_corner;

    biggest_in_corner = m_lu.corner(Eigen::BottomRight, rows-k, cols-k)
                        .cwise().abs()
                        .maxCoeff(&row_of_biggest_in_corner, &col_of_biggest_in_corner);
    row_of_biggest_in_corner += k;
    col_of_biggest_in_corner += k;
    if(k==0) biggest = biggest_in_corner;

    // if the corner is negligible, then we have less than full rank, and we can finish early
    if(ei_isMuchSmallerThan(biggest_in_corner, biggest))
    {
      m_rank = k;
      for(int i = k; i < size; i++)
      {
        rows_transpositions.coeffRef(i) = i;
        cols_transpositions.coeffRef(i) = i;
      }
      break;
    }

    rows_transpositions.coeffRef(k) = row_of_biggest_in_corner;
    cols_transpositions.coeffRef(k) = col_of_biggest_in_corner;
    if(k != row_of_biggest_in_corner) {
      m_lu.row(k).swap(m_lu.row(row_of_biggest_in_corner));
      ++number_of_transpositions;
    }
    if(k != col_of_biggest_in_corner) {
      m_lu.col(k).swap(m_lu.col(col_of_biggest_in_corner));
      ++number_of_transpositions;
    }
    if(k<rows-1)
      m_lu.col(k).end(rows-k-1) /= m_lu.coeff(k,k);
    if(k<size-1)
      for(int col = k + 1; col < cols; ++col)
        m_lu.col(col).end(rows-k-1) -= m_lu.col(k).end(rows-k-1) * m_lu.coeff(k,col);
  }

  for(int k = 0; k < matrix.rows(); ++k) m_p.coeffRef(k) = k;
  for(int k = size-1; k >= 0; --k)
    std::swap(m_p.coeffRef(k), m_p.coeffRef(rows_transpositions.coeff(k)));

  for(int k = 0; k < matrix.cols(); ++k) m_q.coeffRef(k) = k;
  for(int k = 0; k < size; ++k)
    std::swap(m_q.coeffRef(k), m_q.coeffRef(cols_transpositions.coeff(k)));

  m_det_pq = (number_of_transpositions%2) ? -1 : 1;
}

template<typename MatrixType>
typename ei_traits<MatrixType>::Scalar LU<MatrixType>::determinant() const
{
  return Scalar(m_det_pq) * m_lu.diagonal().redux(ei_scalar_product_op<Scalar>());
}

template<typename MatrixType>
template<typename KernelMatrixType>
void LU<MatrixType>::computeKernel(KernelMatrixType *result) const
{
  ei_assert(!isInvertible());
  const int dimker = dimensionOfKernel(), cols = m_lu.cols();
  result->resize(cols, dimker);

  /* Let us use the following lemma:
    *
    * Lemma: If the matrix A has the LU decomposition PAQ = LU,
    * then Ker A = Q(Ker U).
    *
    * Proof: trivial: just keep in mind that P, Q, L are invertible.
    */

  /* Thus, all we need to do is to compute Ker U, and then apply Q.
    *
    * U is upper triangular, with eigenvalues sorted so that any zeros appear at the end.
    * Thus, the diagonal of U ends with exactly
    * m_dimKer zero's. Let us use that to construct m_dimKer linearly
    * independent vectors in Ker U.
    */

  Matrix<Scalar, Dynamic, Dynamic, MatrixType::Options,
         MatrixType::MaxColsAtCompileTime, MatrixType::MaxColsAtCompileTime>
    y(-m_lu.corner(TopRight, m_rank, dimker));

  m_lu.corner(TopLeft, m_rank, m_rank)
      .template marked<UpperTriangular>()
      .solveTriangularInPlace(y);

  for(int i = 0; i < m_rank; ++i) result->row(m_q.coeff(i)) = y.row(i);
  for(int i = m_rank; i < cols; ++i) result->row(m_q.coeff(i)).setZero();
  for(int k = 0; k < dimker; ++k) result->coeffRef(m_q.coeff(m_rank+k), k) = Scalar(1);
}

template<typename MatrixType>
const typename LU<MatrixType>::KernelResultType
LU<MatrixType>::kernel() const
{
  KernelResultType result(m_lu.cols(), dimensionOfKernel());
  computeKernel(&result);
  return result;
}

template<typename MatrixType>
template<typename ImageMatrixType>
void LU<MatrixType>::computeImage(ImageMatrixType *result) const
{
  ei_assert(m_rank > 0);
  result->resize(m_originalMatrix.rows(), m_rank);
  for(int i = 0; i < m_rank; ++i)
    result->col(i) = m_originalMatrix.col(m_q.coeff(i));
}

template<typename MatrixType>
const typename LU<MatrixType>::ImageResultType
LU<MatrixType>::image() const
{
  ImageResultType result(m_originalMatrix.rows(), m_rank);
  computeImage(&result);
  return result;
}

template<typename MatrixType>
template<typename OtherDerived, typename ResultType>
bool LU<MatrixType>::solve(
  const MatrixBase<OtherDerived>& b,
  ResultType *result
) const
{
  /* The decomposition PAQ = LU can be rewritten as A = P^{-1} L U Q^{-1}.
   * So we proceed as follows:
   * Step 1: compute c = Pb.
   * Step 2: replace c by the solution x to Lx = c. Exists because L is invertible.
   * Step 3: replace c by the solution x to Ux = c. Check if a solution really exists.
   * Step 4: result = Qc;
   */

  const int rows = m_lu.rows(), cols = m_lu.cols();
  ei_assert(b.rows() == rows);
  const int smalldim = std::min(rows, cols);

  typename OtherDerived::PlainMatrixType c(b.rows(), b.cols());

  // Step 1
  for(int i = 0; i < rows; ++i) c.row(m_p.coeff(i)) = b.row(i);

  // Step 2
  m_lu.corner(Eigen::TopLeft,smalldim,smalldim).template marked<UnitLowerTriangular>()
    .solveTriangularInPlace(
      c.corner(Eigen::TopLeft, smalldim, c.cols()));
  if(rows>cols)
  {
    c.corner(Eigen::BottomLeft, rows-cols, c.cols())
      -= m_lu.corner(Eigen::BottomLeft, rows-cols, cols) * c.corner(Eigen::TopLeft, cols, c.cols());
  }

  // Step 3
  if(!isSurjective())
  {
    // is c is in the image of U ?
    RealScalar biggest_in_c = c.corner(TopLeft, m_rank, c.cols()).cwise().abs().maxCoeff();
    for(int col = 0; col < c.cols(); ++col)
      for(int row = m_rank; row < c.rows(); ++row)
        if(!ei_isMuchSmallerThan(c.coeff(row,col), biggest_in_c))
          return false;
  }
  m_lu.corner(TopLeft, m_rank, m_rank)
      .template marked<UpperTriangular>()
      .solveTriangularInPlace(c.corner(TopLeft, m_rank, c.cols()));

  // Step 4
  result->resize(m_lu.cols(), b.cols());
  for(int i = 0; i < m_rank; ++i) result->row(m_q.coeff(i)) = c.row(i);
  for(int i = m_rank; i < m_lu.cols(); ++i) result->row(m_q.coeff(i)).setZero();
  return true;
}

/** \lu_module
  *
  * \return the LU decomposition of \c *this.
  *
  * \sa class LU
  */
template<typename Derived>
inline const LU<typename MatrixBase<Derived>::PlainMatrixType>
MatrixBase<Derived>::lu() const
{
  return LU<PlainMatrixType>(eval());
}

#endif // EIGEN_LU_H
