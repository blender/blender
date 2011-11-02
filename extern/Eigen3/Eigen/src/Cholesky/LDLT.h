// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2008-2010 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Keir Mierle <mierle@gmail.com>
// Copyright (C) 2009 Benoit Jacob <jacob.benoit.1@gmail.com>
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

namespace internal {
template<typename MatrixType, int UpLo> struct LDLT_Traits;
}

/** \ingroup cholesky_Module
  *
  * \class LDLT
  *
  * \brief Robust Cholesky decomposition of a matrix with pivoting
  *
  * \param MatrixType the type of the matrix of which to compute the LDL^T Cholesky decomposition
  *
  * Perform a robust Cholesky decomposition of a positive semidefinite or negative semidefinite
  * matrix \f$ A \f$ such that \f$ A =  P^TLDL^*P \f$, where P is a permutation matrix, L
  * is lower triangular with a unit diagonal and D is a diagonal matrix.
  *
  * The decomposition uses pivoting to ensure stability, so that L will have
  * zeros in the bottom right rank(A) - n submatrix. Avoiding the square root
  * on D also stabilizes the computation.
  *
  * Remember that Cholesky decompositions are not rank-revealing. Also, do not use a Cholesky
	* decomposition to determine whether a system of equations has a solution.
  *
  * \sa MatrixBase::ldlt(), class LLT
  */
 /* THIS PART OF THE DOX IS CURRENTLY DISABLED BECAUSE INACCURATE BECAUSE OF BUG IN THE DECOMPOSITION CODE
  * Note that during the decomposition, only the upper triangular part of A is considered. Therefore,
  * the strict lower part does not have to store correct values.
  */
template<typename _MatrixType, int _UpLo> class LDLT
{
  public:
    typedef _MatrixType MatrixType;
    enum {
      RowsAtCompileTime = MatrixType::RowsAtCompileTime,
      ColsAtCompileTime = MatrixType::ColsAtCompileTime,
      Options = MatrixType::Options & ~RowMajorBit, // these are the options for the TmpMatrixType, we need a ColMajor matrix here!
      MaxRowsAtCompileTime = MatrixType::MaxRowsAtCompileTime,
      MaxColsAtCompileTime = MatrixType::MaxColsAtCompileTime,
      UpLo = _UpLo
    };
    typedef typename MatrixType::Scalar Scalar;
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;
    typedef typename MatrixType::Index Index;
    typedef Matrix<Scalar, RowsAtCompileTime, 1, Options, MaxRowsAtCompileTime, 1> TmpMatrixType;

    typedef Transpositions<RowsAtCompileTime, MaxRowsAtCompileTime> TranspositionType;
    typedef PermutationMatrix<RowsAtCompileTime, MaxRowsAtCompileTime> PermutationType;

    typedef internal::LDLT_Traits<MatrixType,UpLo> Traits;

    /** \brief Default Constructor.
      *
      * The default constructor is useful in cases in which the user intends to
      * perform decompositions via LDLT::compute(const MatrixType&).
      */
    LDLT() : m_matrix(), m_transpositions(), m_isInitialized(false) {}

    /** \brief Default Constructor with memory preallocation
      *
      * Like the default constructor but with preallocation of the internal data
      * according to the specified problem \a size.
      * \sa LDLT()
      */
    LDLT(Index size)
      : m_matrix(size, size),
        m_transpositions(size),
        m_temporary(size),
        m_isInitialized(false)
    {}

    LDLT(const MatrixType& matrix)
      : m_matrix(matrix.rows(), matrix.cols()),
        m_transpositions(matrix.rows()),
        m_temporary(matrix.rows()),
        m_isInitialized(false)
    {
      compute(matrix);
    }

    /** \returns a view of the upper triangular matrix U */
    inline typename Traits::MatrixU matrixU() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return Traits::getU(m_matrix);
    }

    /** \returns a view of the lower triangular matrix L */
    inline typename Traits::MatrixL matrixL() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return Traits::getL(m_matrix);
    }

    /** \returns the permutation matrix P as a transposition sequence.
      */
    inline const TranspositionType& transpositionsP() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_transpositions;
    }

    /** \returns the coefficients of the diagonal matrix D */
    inline Diagonal<const MatrixType> vectorD(void) const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_matrix.diagonal();
    }

    /** \returns true if the matrix is positive (semidefinite) */
    inline bool isPositive(void) const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_sign == 1;
    }
    
    #ifdef EIGEN2_SUPPORT
    inline bool isPositiveDefinite() const
    {
      return isPositive();
    }
    #endif

    /** \returns true if the matrix is negative (semidefinite) */
    inline bool isNegative(void) const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_sign == -1;
    }

    /** \returns a solution x of \f$ A x = b \f$ using the current decomposition of A.
      *
      * This function also supports in-place solves using the syntax <tt>x = decompositionObject.solve(x)</tt> .
      *
      * \note_about_checking_solutions
      *
      * More precisely, this method solves \f$ A x = b \f$ using the decomposition \f$ A = P^T L D L^* P \f$
      * by solving the systems \f$ P^T y_1 = b \f$, \f$ L y_2 = y_1 \f$, \f$ D y_3 = y_2 \f$, 
      * \f$ L^* y_4 = y_3 \f$ and \f$ P x = y_4 \f$ in succession. If the matrix \f$ A \f$ is singular, then
      * \f$ D \f$ will also be singular (all the other matrices are invertible). In that case, the
      * least-square solution of \f$ D y_3 = y_2 \f$ is computed. This does not mean that this function
      * computes the least-square solution of \f$ A x = b \f$ is \f$ A \f$ is singular.
      *
      * \sa MatrixBase::ldlt()
      */
    template<typename Rhs>
    inline const internal::solve_retval<LDLT, Rhs>
    solve(const MatrixBase<Rhs>& b) const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      eigen_assert(m_matrix.rows()==b.rows()
                && "LDLT::solve(): invalid number of rows of the right hand side matrix b");
      return internal::solve_retval<LDLT, Rhs>(*this, b.derived());
    }

    #ifdef EIGEN2_SUPPORT
    template<typename OtherDerived, typename ResultType>
    bool solve(const MatrixBase<OtherDerived>& b, ResultType *result) const
    {
      *result = this->solve(b);
      return true;
    }
    #endif

    template<typename Derived>
    bool solveInPlace(MatrixBase<Derived> &bAndX) const;

    LDLT& compute(const MatrixType& matrix);

    /** \returns the internal LDLT decomposition matrix
      *
      * TODO: document the storage layout
      */
    inline const MatrixType& matrixLDLT() const
    {
      eigen_assert(m_isInitialized && "LDLT is not initialized.");
      return m_matrix;
    }

    MatrixType reconstructedMatrix() const;

    inline Index rows() const { return m_matrix.rows(); }
    inline Index cols() const { return m_matrix.cols(); }

  protected:

    /** \internal
      * Used to compute and store the Cholesky decomposition A = L D L^* = U^* D U.
      * The strict upper part is used during the decomposition, the strict lower
      * part correspond to the coefficients of L (its diagonal is equal to 1 and
      * is not stored), and the diagonal entries correspond to D.
      */
    MatrixType m_matrix;
    TranspositionType m_transpositions;
    TmpMatrixType m_temporary;
    int m_sign;
    bool m_isInitialized;
};

namespace internal {

template<int UpLo> struct ldlt_inplace;

template<> struct ldlt_inplace<Lower>
{
  template<typename MatrixType, typename TranspositionType, typename Workspace>
  static bool unblocked(MatrixType& mat, TranspositionType& transpositions, Workspace& temp, int* sign=0)
  {
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::RealScalar RealScalar;
    typedef typename MatrixType::Index Index;
    eigen_assert(mat.rows()==mat.cols());
    const Index size = mat.rows();

    if (size <= 1)
    {
      transpositions.setIdentity();
      if(sign)
        *sign = real(mat.coeff(0,0))>0 ? 1:-1;
      return true;
    }

    RealScalar cutoff = 0, biggest_in_corner;

    for (Index k = 0; k < size; ++k)
    {
      // Find largest diagonal element
      Index index_of_biggest_in_corner;
      biggest_in_corner = mat.diagonal().tail(size-k).cwiseAbs().maxCoeff(&index_of_biggest_in_corner);
      index_of_biggest_in_corner += k;

      if(k == 0)
      {
        // The biggest overall is the point of reference to which further diagonals
        // are compared; if any diagonal is negligible compared
        // to the largest overall, the algorithm bails.
        cutoff = abs(NumTraits<Scalar>::epsilon() * biggest_in_corner);

        if(sign)
          *sign = real(mat.diagonal().coeff(index_of_biggest_in_corner)) > 0 ? 1 : -1;
      }

      // Finish early if the matrix is not full rank.
      if(biggest_in_corner < cutoff)
      {
        for(Index i = k; i < size; i++) transpositions.coeffRef(i) = i;
        break;
      }

      transpositions.coeffRef(k) = index_of_biggest_in_corner;
      if(k != index_of_biggest_in_corner)
      {
        // apply the transposition while taking care to consider only
        // the lower triangular part
        Index s = size-index_of_biggest_in_corner-1; // trailing size after the biggest element
        mat.row(k).head(k).swap(mat.row(index_of_biggest_in_corner).head(k));
        mat.col(k).tail(s).swap(mat.col(index_of_biggest_in_corner).tail(s));
        std::swap(mat.coeffRef(k,k),mat.coeffRef(index_of_biggest_in_corner,index_of_biggest_in_corner));
        for(int i=k+1;i<index_of_biggest_in_corner;++i)
        {
          Scalar tmp = mat.coeffRef(i,k);
          mat.coeffRef(i,k) = conj(mat.coeffRef(index_of_biggest_in_corner,i));
          mat.coeffRef(index_of_biggest_in_corner,i) = conj(tmp);
        }
        if(NumTraits<Scalar>::IsComplex)
          mat.coeffRef(index_of_biggest_in_corner,k) = conj(mat.coeff(index_of_biggest_in_corner,k));
      }

      // partition the matrix:
      //       A00 |  -  |  -
      // lu  = A10 | A11 |  -
      //       A20 | A21 | A22
      Index rs = size - k - 1;
      Block<MatrixType,Dynamic,1> A21(mat,k+1,k,rs,1);
      Block<MatrixType,1,Dynamic> A10(mat,k,0,1,k);
      Block<MatrixType,Dynamic,Dynamic> A20(mat,k+1,0,rs,k);

      if(k>0)
      {
        temp.head(k) = mat.diagonal().head(k).asDiagonal() * A10.adjoint();
        mat.coeffRef(k,k) -= (A10 * temp.head(k)).value();
        if(rs>0)
          A21.noalias() -= A20 * temp.head(k);
      }
      if((rs>0) && (abs(mat.coeffRef(k,k)) > cutoff))
        A21 /= mat.coeffRef(k,k);
    }

    return true;
  }
};

template<> struct ldlt_inplace<Upper>
{
  template<typename MatrixType, typename TranspositionType, typename Workspace>
  static EIGEN_STRONG_INLINE bool unblocked(MatrixType& mat, TranspositionType& transpositions, Workspace& temp, int* sign=0)
  {
    Transpose<MatrixType> matt(mat);
    return ldlt_inplace<Lower>::unblocked(matt, transpositions, temp, sign);
  }
};

template<typename MatrixType> struct LDLT_Traits<MatrixType,Lower>
{
  typedef TriangularView<MatrixType, UnitLower> MatrixL;
  typedef TriangularView<typename MatrixType::AdjointReturnType, UnitUpper> MatrixU;
  inline static MatrixL getL(const MatrixType& m) { return m; }
  inline static MatrixU getU(const MatrixType& m) { return m.adjoint(); }
};

template<typename MatrixType> struct LDLT_Traits<MatrixType,Upper>
{
  typedef TriangularView<typename MatrixType::AdjointReturnType, UnitLower> MatrixL;
  typedef TriangularView<MatrixType, UnitUpper> MatrixU;
  inline static MatrixL getL(const MatrixType& m) { return m.adjoint(); }
  inline static MatrixU getU(const MatrixType& m) { return m; }
};

} // end namespace internal

/** Compute / recompute the LDLT decomposition A = L D L^* = U^* D U of \a matrix
  */
template<typename MatrixType, int _UpLo>
LDLT<MatrixType,_UpLo>& LDLT<MatrixType,_UpLo>::compute(const MatrixType& a)
{
  eigen_assert(a.rows()==a.cols());
  const Index size = a.rows();

  m_matrix = a;

  m_transpositions.resize(size);
  m_isInitialized = false;
  m_temporary.resize(size);

  internal::ldlt_inplace<UpLo>::unblocked(m_matrix, m_transpositions, m_temporary, &m_sign);

  m_isInitialized = true;
  return *this;
}

namespace internal {
template<typename _MatrixType, int _UpLo, typename Rhs>
struct solve_retval<LDLT<_MatrixType,_UpLo>, Rhs>
  : solve_retval_base<LDLT<_MatrixType,_UpLo>, Rhs>
{
  typedef LDLT<_MatrixType,_UpLo> LDLTType;
  EIGEN_MAKE_SOLVE_HELPERS(LDLTType,Rhs)

  template<typename Dest> void evalTo(Dest& dst) const
  {
    eigen_assert(rhs().rows() == dec().matrixLDLT().rows());
    // dst = P b
    dst = dec().transpositionsP() * rhs();

    // dst = L^-1 (P b)
    dec().matrixL().solveInPlace(dst);

    // dst = D^-1 (L^-1 P b)
    // more precisely, use pseudo-inverse of D (see bug 241)
    using std::abs;
    using std::max;
    typedef typename LDLTType::MatrixType MatrixType;
    typedef typename LDLTType::Scalar Scalar;
    typedef typename LDLTType::RealScalar RealScalar;
    const Diagonal<const MatrixType> vectorD = dec().vectorD();
    RealScalar tolerance = (max)(vectorD.array().abs().maxCoeff() * NumTraits<Scalar>::epsilon(),
				 RealScalar(1) / NumTraits<RealScalar>::highest()); // motivated by LAPACK's xGELSS
    for (Index i = 0; i < vectorD.size(); ++i) {
      if(abs(vectorD(i)) > tolerance)
	dst.row(i) /= vectorD(i);
      else
	dst.row(i).setZero();
    }

    // dst = L^-T (D^-1 L^-1 P b)
    dec().matrixU().solveInPlace(dst);

    // dst = P^-1 (L^-T D^-1 L^-1 P b) = A^-1 b
    dst = dec().transpositionsP().transpose() * dst;
  }
};
}

/** \internal use x = ldlt_object.solve(x);
  *
  * This is the \em in-place version of solve().
  *
  * \param bAndX represents both the right-hand side matrix b and result x.
  *
  * \returns true always! If you need to check for existence of solutions, use another decomposition like LU, QR, or SVD.
  *
  * This version avoids a copy when the right hand side matrix b is not
  * needed anymore.
  *
  * \sa LDLT::solve(), MatrixBase::ldlt()
  */
template<typename MatrixType,int _UpLo>
template<typename Derived>
bool LDLT<MatrixType,_UpLo>::solveInPlace(MatrixBase<Derived> &bAndX) const
{
  eigen_assert(m_isInitialized && "LDLT is not initialized.");
  const Index size = m_matrix.rows();
  eigen_assert(size == bAndX.rows());

  bAndX = this->solve(bAndX);

  return true;
}

/** \returns the matrix represented by the decomposition,
 * i.e., it returns the product: P^T L D L^* P.
 * This function is provided for debug purpose. */
template<typename MatrixType, int _UpLo>
MatrixType LDLT<MatrixType,_UpLo>::reconstructedMatrix() const
{
  eigen_assert(m_isInitialized && "LDLT is not initialized.");
  const Index size = m_matrix.rows();
  MatrixType res(size,size);

  // P
  res.setIdentity();
  res = transpositionsP() * res;
  // L^* P
  res = matrixU() * res;
  // D(L^*P)
  res = vectorD().asDiagonal() * res;
  // L(DL^*P)
  res = matrixL() * res;
  // P^T (LDL^*P)
  res = transpositionsP().transpose() * res;

  return res;
}

/** \cholesky_module
  * \returns the Cholesky decomposition with full pivoting without square root of \c *this
  */
template<typename MatrixType, unsigned int UpLo>
inline const LDLT<typename SelfAdjointView<MatrixType, UpLo>::PlainObject, UpLo>
SelfAdjointView<MatrixType, UpLo>::ldlt() const
{
  return LDLT<PlainObject,UpLo>(m_matrix);
}

/** \cholesky_module
  * \returns the Cholesky decomposition with full pivoting without square root of \c *this
  */
template<typename Derived>
inline const LDLT<typename MatrixBase<Derived>::PlainObject>
MatrixBase<Derived>::ldlt() const
{
  return LDLT<PlainObject>(derived());
}

#endif // EIGEN_LDLT_H
