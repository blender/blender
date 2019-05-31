/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 */

#ifndef __CONSTRAINEDCONJUGATEGRADIENT_H__
#define __CONSTRAINEDCONJUGATEGRADIENT_H__

#include <Eigen/Core>

namespace Eigen {

namespace internal {

/** \internal Low-level conjugate gradient algorithm
 * \param mat: The matrix A
 * \param rhs: The right hand side vector b
 * \param x: On input and initial solution, on output the computed solution.
 * \param precond: A preconditioner being able to efficiently solve for an
 * approximation of Ax=b (regardless of b)
 * \param iters: On input the max number of iteration,
 * on output the number of performed iterations.
 * \param tol_error: On input the tolerance error,
 * on output an estimation of the relative error.
 */
template<typename MatrixType,
         typename Rhs,
         typename Dest,
         typename FilterMatrixType,
         typename Preconditioner>
EIGEN_DONT_INLINE void constrained_conjugate_gradient(const MatrixType &mat,
                                                      const Rhs &rhs,
                                                      Dest &x,
                                                      const FilterMatrixType &filter,
                                                      const Preconditioner &precond,
                                                      int &iters,
                                                      typename Dest::RealScalar &tol_error)
{
  using std::abs;
  using std::sqrt;
  typedef typename Dest::RealScalar RealScalar;
  typedef typename Dest::Scalar Scalar;
  typedef Matrix<Scalar, Dynamic, 1> VectorType;

  RealScalar tol = tol_error;
  int maxIters = iters;

  int n = mat.cols();

  VectorType residual = filter * (rhs - mat * x);  // initial residual

  RealScalar rhsNorm2 = (filter * rhs).squaredNorm();
  if (rhsNorm2 == 0) {
    /* XXX TODO set constrained result here */
    x.setZero();
    iters = 0;
    tol_error = 0;
    return;
  }
  RealScalar threshold = tol * tol * rhsNorm2;
  RealScalar residualNorm2 = residual.squaredNorm();
  if (residualNorm2 < threshold) {
    iters = 0;
    tol_error = sqrt(residualNorm2 / rhsNorm2);
    return;
  }

  VectorType p(n);
  p = filter * precond.solve(residual);  // initial search direction

  VectorType z(n), tmp(n);
  RealScalar absNew = numext::real(
      residual.dot(p));  // the square of the absolute value of r scaled by invM
  int i = 0;
  while (i < maxIters) {
    tmp.noalias() = filter * (mat * p);  // the bottleneck of the algorithm

    Scalar alpha = absNew / p.dot(tmp);  // the amount we travel on dir
    x += alpha * p;                      // update solution
    residual -= alpha * tmp;             // update residue

    residualNorm2 = residual.squaredNorm();
    if (residualNorm2 < threshold) {
      break;
    }

    z = precond.solve(residual);  // approximately solve for "A z = residual"

    RealScalar absOld = absNew;
    absNew = numext::real(residual.dot(z));  // update the absolute value of r
    RealScalar beta =
        absNew /
        absOld;  // calculate the Gram-Schmidt value used to create the new search direction
    p = filter * (z + beta * p);  // update search direction
    i++;
  }
  tol_error = sqrt(residualNorm2 / rhsNorm2);
  iters = i;
}

}  // namespace internal

#if 0 /* unused */
template<typename MatrixType> struct MatrixFilter {
  MatrixFilter() : m_cmat(NULL)
  {
  }

  MatrixFilter(const MatrixType &cmat) : m_cmat(&cmat)
  {
  }

  void setMatrix(const MatrixType &cmat)
  {
    m_cmat = &cmat;
  }

  template<typename VectorType> void apply(VectorType v) const
  {
    v = (*m_cmat) * v;
  }

 protected:
  const MatrixType *m_cmat;
};
#endif

template<typename _MatrixType,
         int _UpLo = Lower,
         typename _FilterMatrixType = _MatrixType,
         typename _Preconditioner = DiagonalPreconditioner<typename _MatrixType::Scalar>>
class ConstrainedConjugateGradient;

namespace internal {

template<typename _MatrixType, int _UpLo, typename _FilterMatrixType, typename _Preconditioner>
struct traits<
    ConstrainedConjugateGradient<_MatrixType, _UpLo, _FilterMatrixType, _Preconditioner>> {
  typedef _MatrixType MatrixType;
  typedef _FilterMatrixType FilterMatrixType;
  typedef _Preconditioner Preconditioner;
};

}  // namespace internal

/** \ingroup IterativeLinearSolvers_Module
 * \brief A conjugate gradient solver for sparse self-adjoint problems with additional constraints
 *
 * This class allows to solve for A.x = b sparse linear problems using a conjugate gradient
 * algorithm. The sparse matrix A must be selfadjoint. The vectors x and b can be either dense or
 * sparse.
 *
 * \tparam _MatrixType the type of the sparse matrix A, can be a dense or a sparse matrix.
 * \tparam _UpLo the triangular part that will be used for the computations. It can be Lower
 *               or Upper. Default is Lower.
 * \tparam _Preconditioner the type of the preconditioner. Default is DiagonalPreconditioner
 *
 * The maximal number of iterations and tolerance value can be controlled via the
 * setMaxIterations() and setTolerance() methods. The defaults are the size of the problem for the
 * maximal number of iterations and NumTraits<Scalar>::epsilon() for the tolerance.
 *
 * This class can be used as the direct solver classes. Here is a typical usage example:
 * \code
 * int n = 10000;
 * VectorXd x(n), b(n);
 * SparseMatrix<double> A(n,n);
 * // fill A and b
 * ConjugateGradient<SparseMatrix<double> > cg;
 * cg.compute(A);
 * x = cg.solve(b);
 * std::cout << "#iterations:     " << cg.iterations() << std::endl;
 * std::cout << "estimated error: " << cg.error()      << std::endl;
 * // update b, and solve again
 * x = cg.solve(b);
 * \endcode
 *
 * By default the iterations start with x=0 as an initial guess of the solution.
 * One can control the start using the solveWithGuess() method. Here is a step by
 * step execution example starting with a random guess and printing the evolution
 * of the estimated error:
 * * \code
 * x = VectorXd::Random(n);
 * cg.setMaxIterations(1);
 * int i = 0;
 * do {
 *   x = cg.solveWithGuess(b,x);
 *   std::cout << i << " : " << cg.error() << std::endl;
 *   ++i;
 * } while (cg.info()!=Success && i<100);
 * \endcode
 * Note that such a step by step execution is slightly slower.
 *
 * \sa class SimplicialCholesky, DiagonalPreconditioner, IdentityPreconditioner
 */
template<typename _MatrixType, int _UpLo, typename _FilterMatrixType, typename _Preconditioner>
class ConstrainedConjugateGradient
    : public IterativeSolverBase<
          ConstrainedConjugateGradient<_MatrixType, _UpLo, _FilterMatrixType, _Preconditioner>> {
  typedef IterativeSolverBase<ConstrainedConjugateGradient> Base;
  using Base::m_error;
  using Base::m_info;
  using Base::m_isInitialized;
  using Base::m_iterations;
  using Base::mp_matrix;

 public:
  typedef _MatrixType MatrixType;
  typedef typename MatrixType::Scalar Scalar;
  typedef typename MatrixType::Index Index;
  typedef typename MatrixType::RealScalar RealScalar;
  typedef _FilterMatrixType FilterMatrixType;
  typedef _Preconditioner Preconditioner;

  enum { UpLo = _UpLo };

 public:
  /** Default constructor. */
  ConstrainedConjugateGradient() : Base()
  {
  }

  /** Initialize the solver with matrix \a A for further \c Ax=b solving.
   *
   * This constructor is a shortcut for the default constructor followed
   * by a call to compute().
   *
   * \warning this class stores a reference to the matrix A as well as some
   * precomputed values that depend on it. Therefore, if \a A is changed
   * this class becomes invalid. Call compute() to update it with the new
   * matrix A, or modify a copy of A.
   */
  ConstrainedConjugateGradient(const MatrixType &A) : Base(A)
  {
  }

  ~ConstrainedConjugateGradient()
  {
  }

  FilterMatrixType &filter()
  {
    return m_filter;
  }
  const FilterMatrixType &filter() const
  {
    return m_filter;
  }

  /** \returns the solution x of \f$ A x = b \f$ using the current decomposition of A
   * \a x0 as an initial solution.
   *
   * \sa compute()
   */
  template<typename Rhs, typename Guess>
  inline const internal::solve_retval_with_guess<ConstrainedConjugateGradient, Rhs, Guess>
  solveWithGuess(const MatrixBase<Rhs> &b, const Guess &x0) const
  {
    eigen_assert(m_isInitialized && "ConjugateGradient is not initialized.");
    eigen_assert(
        Base::rows() == b.rows() &&
        "ConjugateGradient::solve(): invalid number of rows of the right hand side matrix b");
    return internal::solve_retval_with_guess<ConstrainedConjugateGradient, Rhs, Guess>(
        *this, b.derived(), x0);
  }

  /** \internal */
  template<typename Rhs, typename Dest> void _solveWithGuess(const Rhs &b, Dest &x) const
  {
    m_iterations = Base::maxIterations();
    m_error = Base::m_tolerance;

    for (int j = 0; j < b.cols(); ++j) {
      m_iterations = Base::maxIterations();
      m_error = Base::m_tolerance;

      typename Dest::ColXpr xj(x, j);
      internal::constrained_conjugate_gradient(mp_matrix->template selfadjointView<UpLo>(),
                                               b.col(j),
                                               xj,
                                               m_filter,
                                               Base::m_preconditioner,
                                               m_iterations,
                                               m_error);
    }

    m_isInitialized = true;
    m_info = m_error <= Base::m_tolerance ? Success : NoConvergence;
  }

  /** \internal */
  template<typename Rhs, typename Dest> void _solve(const Rhs &b, Dest &x) const
  {
    x.setOnes();
    _solveWithGuess(b, x);
  }

 protected:
  FilterMatrixType m_filter;
};

namespace internal {

template<typename _MatrixType, int _UpLo, typename _Filter, typename _Preconditioner, typename Rhs>
struct solve_retval<ConstrainedConjugateGradient<_MatrixType, _UpLo, _Filter, _Preconditioner>,
                    Rhs>
    : solve_retval_base<ConstrainedConjugateGradient<_MatrixType, _UpLo, _Filter, _Preconditioner>,
                        Rhs> {
  typedef ConstrainedConjugateGradient<_MatrixType, _UpLo, _Filter, _Preconditioner> Dec;
  EIGEN_MAKE_SOLVE_HELPERS(Dec, Rhs)

  template<typename Dest> void evalTo(Dest &dst) const
  {
    dec()._solve(rhs(), dst);
  }
};

}  // end namespace internal

}  // end namespace Eigen

#endif  // __CONSTRAINEDCONJUGATEGRADIENT_H__
