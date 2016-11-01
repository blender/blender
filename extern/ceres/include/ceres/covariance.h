// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_COVARIANCE_H_
#define CERES_PUBLIC_COVARIANCE_H_

#include <utility>
#include <vector>
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"
#include "ceres/internal/disable_warnings.h"

namespace ceres {

class Problem;

namespace internal {
class CovarianceImpl;
}  // namespace internal

// WARNING
// =======
// It is very easy to use this class incorrectly without understanding
// the underlying mathematics. Please read and understand the
// documentation completely before attempting to use this class.
//
//
// This class allows the user to evaluate the covariance for a
// non-linear least squares problem and provides random access to its
// blocks
//
// Background
// ==========
// One way to assess the quality of the solution returned by a
// non-linear least squares solve is to analyze the covariance of the
// solution.
//
// Let us consider the non-linear regression problem
//
//   y = f(x) + N(0, I)
//
// i.e., the observation y is a random non-linear function of the
// independent variable x with mean f(x) and identity covariance. Then
// the maximum likelihood estimate of x given observations y is the
// solution to the non-linear least squares problem:
//
//  x* = arg min_x |f(x)|^2
//
// And the covariance of x* is given by
//
//  C(x*) = inverse[J'(x*)J(x*)]
//
// Here J(x*) is the Jacobian of f at x*. The above formula assumes
// that J(x*) has full column rank.
//
// If J(x*) is rank deficient, then the covariance matrix C(x*) is
// also rank deficient and is given by
//
//  C(x*) =  pseudoinverse[J'(x*)J(x*)]
//
// Note that in the above, we assumed that the covariance
// matrix for y was identity. This is an important assumption. If this
// is not the case and we have
//
//  y = f(x) + N(0, S)
//
// Where S is a positive semi-definite matrix denoting the covariance
// of y, then the maximum likelihood problem to be solved is
//
//  x* = arg min_x f'(x) inverse[S] f(x)
//
// and the corresponding covariance estimate of x* is given by
//
//  C(x*) = inverse[J'(x*) inverse[S] J(x*)]
//
// So, if it is the case that the observations being fitted to have a
// covariance matrix not equal to identity, then it is the user's
// responsibility that the corresponding cost functions are correctly
// scaled, e.g. in the above case the cost function for this problem
// should evaluate S^{-1/2} f(x) instead of just f(x), where S^{-1/2}
// is the inverse square root of the covariance matrix S.
//
// This class allows the user to evaluate the covariance for a
// non-linear least squares problem and provides random access to its
// blocks. The computation assumes that the CostFunctions compute
// residuals such that their covariance is identity.
//
// Since the computation of the covariance matrix requires computing
// the inverse of a potentially large matrix, this can involve a
// rather large amount of time and memory. However, it is usually the
// case that the user is only interested in a small part of the
// covariance matrix. Quite often just the block diagonal. This class
// allows the user to specify the parts of the covariance matrix that
// she is interested in and then uses this information to only compute
// and store those parts of the covariance matrix.
//
// Rank of the Jacobian
// --------------------
// As we noted above, if the jacobian is rank deficient, then the
// inverse of J'J is not defined and instead a pseudo inverse needs to
// be computed.
//
// The rank deficiency in J can be structural -- columns which are
// always known to be zero or numerical -- depending on the exact
// values in the Jacobian.
//
// Structural rank deficiency occurs when the problem contains
// parameter blocks that are constant. This class correctly handles
// structural rank deficiency like that.
//
// Numerical rank deficiency, where the rank of the matrix cannot be
// predicted by its sparsity structure and requires looking at its
// numerical values is more complicated. Here again there are two
// cases.
//
//   a. The rank deficiency arises from overparameterization. e.g., a
//   four dimensional quaternion used to parameterize SO(3), which is
//   a three dimensional manifold. In cases like this, the user should
//   use an appropriate LocalParameterization. Not only will this lead
//   to better numerical behaviour of the Solver, it will also expose
//   the rank deficiency to the Covariance object so that it can
//   handle it correctly.
//
//   b. More general numerical rank deficiency in the Jacobian
//   requires the computation of the so called Singular Value
//   Decomposition (SVD) of J'J. We do not know how to do this for
//   large sparse matrices efficiently. For small and moderate sized
//   problems this is done using dense linear algebra.
//
// Gauge Invariance
// ----------------
// In structure from motion (3D reconstruction) problems, the
// reconstruction is ambiguous upto a similarity transform. This is
// known as a Gauge Ambiguity. Handling Gauges correctly requires the
// use of SVD or custom inversion algorithms. For small problems the
// user can use the dense algorithm. For more details see
//
// Ken-ichi Kanatani, Daniel D. Morris: Gauges and gauge
// transformations for uncertainty description of geometric structure
// with indeterminacy. IEEE Transactions on Information Theory 47(5):
// 2017-2028 (2001)
//
// Example Usage
// =============
//
//  double x[3];
//  double y[2];
//
//  Problem problem;
//  problem.AddParameterBlock(x, 3);
//  problem.AddParameterBlock(y, 2);
//  <Build Problem>
//  <Solve Problem>
//
//  Covariance::Options options;
//  Covariance covariance(options);
//
//  std::vector<std::pair<const double*, const double*> > covariance_blocks;
//  covariance_blocks.push_back(make_pair(x, x));
//  covariance_blocks.push_back(make_pair(y, y));
//  covariance_blocks.push_back(make_pair(x, y));
//
//  CHECK(covariance.Compute(covariance_blocks, &problem));
//
//  double covariance_xx[3 * 3];
//  double covariance_yy[2 * 2];
//  double covariance_xy[3 * 2];
//  covariance.GetCovarianceBlock(x, x, covariance_xx)
//  covariance.GetCovarianceBlock(y, y, covariance_yy)
//  covariance.GetCovarianceBlock(x, y, covariance_xy)
//
class CERES_EXPORT Covariance {
 public:
  struct CERES_EXPORT Options {
    Options()
#ifndef CERES_NO_SUITESPARSE
        : algorithm_type(SUITE_SPARSE_QR),
#else
        : algorithm_type(EIGEN_SPARSE_QR),
#endif
          min_reciprocal_condition_number(1e-14),
          null_space_rank(0),
          num_threads(1),
          apply_loss_function(true) {
    }

    // Ceres supports three different algorithms for covariance
    // estimation, which represent different tradeoffs in speed,
    // accuracy and reliability.
    //
    // 1. DENSE_SVD uses Eigen's JacobiSVD to perform the
    //    computations. It computes the singular value decomposition
    //
    //      U * S * V' = J
    //
    //    and then uses it to compute the pseudo inverse of J'J as
    //
    //      pseudoinverse[J'J]^ = V * pseudoinverse[S] * V'
    //
    //    It is an accurate but slow method and should only be used
    //    for small to moderate sized problems. It can handle
    //    full-rank as well as rank deficient Jacobians.
    //
    // 2. EIGEN_SPARSE_QR uses the sparse QR factorization algorithm
    //    in Eigen to compute the decomposition
    //
    //      Q * R = J
    //
    //    [J'J]^-1 = [R*R']^-1
    //
    //    It is a moderately fast algorithm for sparse matrices.
    //
    // 3. SUITE_SPARSE_QR uses the SuiteSparseQR sparse QR
    //    factorization algorithm. It uses dense linear algebra and is
    //    multi threaded, so for large sparse sparse matrices it is
    //    significantly faster than EIGEN_SPARSE_QR.
    //
    // Neither EIGEN_SPARSE_QR not SUITE_SPARSE_QR are capable of
    // computing the covariance if the Jacobian is rank deficient.
    CovarianceAlgorithmType algorithm_type;

    // If the Jacobian matrix is near singular, then inverting J'J
    // will result in unreliable results, e.g, if
    //
    //   J = [1.0 1.0         ]
    //       [1.0 1.0000001   ]
    //
    // which is essentially a rank deficient matrix, we have
    //
    //   inv(J'J) = [ 2.0471e+14  -2.0471e+14]
    //              [-2.0471e+14   2.0471e+14]
    //
    // This is not a useful result. Therefore, by default
    // Covariance::Compute will return false if a rank deficient
    // Jacobian is encountered. How rank deficiency is detected
    // depends on the algorithm being used.
    //
    // 1. DENSE_SVD
    //
    //      min_sigma / max_sigma < sqrt(min_reciprocal_condition_number)
    //
    //    where min_sigma and max_sigma are the minimum and maxiumum
    //    singular values of J respectively.
    //
    // 2. SUITE_SPARSE_QR and EIGEN_SPARSE_QR
    //
    //      rank(J) < num_col(J)
    //
    //   Here rank(J) is the estimate of the rank of J returned by the
    //   sparse QR factorization algorithm. It is a fairly reliable
    //   indication of rank deficiency.
    //
    double min_reciprocal_condition_number;

    // When using DENSE_SVD, the user has more control in dealing with
    // singular and near singular covariance matrices.
    //
    // As mentioned above, when the covariance matrix is near
    // singular, instead of computing the inverse of J'J, the
    // Moore-Penrose pseudoinverse of J'J should be computed.
    //
    // If J'J has the eigen decomposition (lambda_i, e_i), where
    // lambda_i is the i^th eigenvalue and e_i is the corresponding
    // eigenvector, then the inverse of J'J is
    //
    //   inverse[J'J] = sum_i e_i e_i' / lambda_i
    //
    // and computing the pseudo inverse involves dropping terms from
    // this sum that correspond to small eigenvalues.
    //
    // How terms are dropped is controlled by
    // min_reciprocal_condition_number and null_space_rank.
    //
    // If null_space_rank is non-negative, then the smallest
    // null_space_rank eigenvalue/eigenvectors are dropped
    // irrespective of the magnitude of lambda_i. If the ratio of the
    // smallest non-zero eigenvalue to the largest eigenvalue in the
    // truncated matrix is still below
    // min_reciprocal_condition_number, then the Covariance::Compute()
    // will fail and return false.
    //
    // Setting null_space_rank = -1 drops all terms for which
    //
    //   lambda_i / lambda_max < min_reciprocal_condition_number.
    //
    // This option has no effect on the SUITE_SPARSE_QR and
    // EIGEN_SPARSE_QR algorithms.
    int null_space_rank;

    int num_threads;

    // Even though the residual blocks in the problem may contain loss
    // functions, setting apply_loss_function to false will turn off
    // the application of the loss function to the output of the cost
    // function and in turn its effect on the covariance.
    //
    // TODO(sameergaarwal): Expand this based on Jim's experiments.
    bool apply_loss_function;
  };

  explicit Covariance(const Options& options);
  ~Covariance();

  // Compute a part of the covariance matrix.
  //
  // The vector covariance_blocks, indexes into the covariance matrix
  // block-wise using pairs of parameter blocks. This allows the
  // covariance estimation algorithm to only compute and store these
  // blocks.
  //
  // Since the covariance matrix is symmetric, if the user passes
  // (block1, block2), then GetCovarianceBlock can be called with
  // block1, block2 as well as block2, block1.
  //
  // covariance_blocks cannot contain duplicates. Bad things will
  // happen if they do.
  //
  // Note that the list of covariance_blocks is only used to determine
  // what parts of the covariance matrix are computed. The full
  // Jacobian is used to do the computation, i.e. they do not have an
  // impact on what part of the Jacobian is used for computation.
  //
  // The return value indicates the success or failure of the
  // covariance computation. Please see the documentation for
  // Covariance::Options for more on the conditions under which this
  // function returns false.
  bool Compute(
      const std::vector<std::pair<const double*,
                                  const double*> >& covariance_blocks,
      Problem* problem);

  // Compute a part of the covariance matrix.
  //
  // The vector parameter_blocks contains the parameter blocks that
  // are used for computing the covariance matrix. From this vector
  // all covariance pairs are generated. This allows the covariance
  // estimation algorithm to only compute and store these blocks.
  //
  // parameter_blocks cannot contain duplicates. Bad things will
  // happen if they do.
  //
  // Note that the list of covariance_blocks is only used to determine
  // what parts of the covariance matrix are computed. The full
  // Jacobian is used to do the computation, i.e. they do not have an
  // impact on what part of the Jacobian is used for computation.
  //
  // The return value indicates the success or failure of the
  // covariance computation. Please see the documentation for
  // Covariance::Options for more on the conditions under which this
  // function returns false.
  bool Compute(const std::vector<const double*>& parameter_blocks,
               Problem* problem);

  // Return the block of the cross-covariance matrix corresponding to
  // parameter_block1 and parameter_block2.
  //
  // Compute must be called before the first call to
  // GetCovarianceBlock and the pair <parameter_block1,
  // parameter_block2> OR the pair <parameter_block2,
  // parameter_block1> must have been present in the vector
  // covariance_blocks when Compute was called. Otherwise
  // GetCovarianceBlock will return false.
  //
  // covariance_block must point to a memory location that can store a
  // parameter_block1_size x parameter_block2_size matrix. The
  // returned covariance will be a row-major matrix.
  bool GetCovarianceBlock(const double* parameter_block1,
                          const double* parameter_block2,
                          double* covariance_block) const;

  // Return the block of the cross-covariance matrix corresponding to
  // parameter_block1 and parameter_block2.
  // Returns cross-covariance in the tangent space if a local
  // parameterization is associated with either parameter block;
  // else returns cross-covariance in the ambient space.
  //
  // Compute must be called before the first call to
  // GetCovarianceBlock and the pair <parameter_block1,
  // parameter_block2> OR the pair <parameter_block2,
  // parameter_block1> must have been present in the vector
  // covariance_blocks when Compute was called. Otherwise
  // GetCovarianceBlock will return false.
  //
  // covariance_block must point to a memory location that can store a
  // parameter_block1_local_size x parameter_block2_local_size matrix. The
  // returned covariance will be a row-major matrix.
  bool GetCovarianceBlockInTangentSpace(const double* parameter_block1,
                                        const double* parameter_block2,
                                        double* covariance_block) const;

  // Return the covariance matrix corresponding to all parameter_blocks.
  //
  // Compute must be called before calling GetCovarianceMatrix and all
  // parameter_blocks must have been present in the vector
  // parameter_blocks when Compute was called. Otherwise
  // GetCovarianceMatrix returns false.
  //
  // covariance_matrix must point to a memory location that can store
  // the size of the covariance matrix. The covariance matrix will be
  // a square matrix whose row and column count is equal to the sum of
  // the sizes of the individual parameter blocks. The covariance
  // matrix will be a row-major matrix.
  bool GetCovarianceMatrix(const std::vector<const double *> &parameter_blocks,
                           double *covariance_matrix);

  // Return the covariance matrix corresponding to parameter_blocks
  // in the tangent space if a local parameterization is associated
  // with one of the parameter blocks else returns the covariance
  // matrix in the ambient space.
  //
  // Compute must be called before calling GetCovarianceMatrix and all
  // parameter_blocks must have been present in the vector
  // parameters_blocks when Compute was called. Otherwise
  // GetCovarianceMatrix returns false.
  //
  // covariance_matrix must point to a memory location that can store
  // the size of the covariance matrix. The covariance matrix will be
  // a square matrix whose row and column count is equal to the sum of
  // the sizes of the tangent spaces of the individual parameter
  // blocks. The covariance matrix will be a row-major matrix.
  bool GetCovarianceMatrixInTangentSpace(
      const std::vector<const double*>& parameter_blocks,
      double* covariance_matrix);

 private:
  internal::scoped_ptr<internal::CovarianceImpl> impl_;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_COVARIANCE_H_
