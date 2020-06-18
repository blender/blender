// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
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

#include "ceres/eigensparse.h"

#ifdef CERES_USE_EIGEN_SPARSE

#include <sstream>

#include "Eigen/SparseCholesky"
#include "Eigen/SparseCore"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/linear_solver.h"

namespace ceres {
namespace internal {

// TODO(sameeragarwal): Use enable_if to clean up the implementations
// for when Scalar == double.
template <typename Solver>
class EigenSparseCholeskyTemplate : public SparseCholesky {
 public:
  EigenSparseCholeskyTemplate() : analyzed_(false) {}
  virtual ~EigenSparseCholeskyTemplate() {}
  CompressedRowSparseMatrix::StorageType StorageType() const final {
    return CompressedRowSparseMatrix::LOWER_TRIANGULAR;
  }

  LinearSolverTerminationType Factorize(
      const Eigen::SparseMatrix<typename Solver::Scalar>& lhs,
      std::string* message) {
    if (!analyzed_) {
      solver_.analyzePattern(lhs);

      if (VLOG_IS_ON(2)) {
        std::stringstream ss;
        solver_.dumpMemory(ss);
        VLOG(2) << "Symbolic Analysis\n" << ss.str();
      }

      if (solver_.info() != Eigen::Success) {
        *message = "Eigen failure. Unable to find symbolic factorization.";
        return LINEAR_SOLVER_FATAL_ERROR;
      }

      analyzed_ = true;
    }

    solver_.factorize(lhs);
    if (solver_.info() != Eigen::Success) {
      *message = "Eigen failure. Unable to find numeric factorization.";
      return LINEAR_SOLVER_FAILURE;
    }
    return LINEAR_SOLVER_SUCCESS;
  }

  LinearSolverTerminationType Solve(const double* rhs_ptr,
                                    double* solution_ptr,
                                    std::string* message) {
    CHECK(analyzed_) << "Solve called without a call to Factorize first.";

    scalar_rhs_ = ConstVectorRef(rhs_ptr, solver_.cols())
                      .template cast<typename Solver::Scalar>();

    // The two casts are needed if the Scalar in this class is not
    // double. For code simplicity we are going to assume that Eigen
    // is smart enough to figure out that casting a double Vector to a
    // double Vector is a straight copy. If this turns into a
    // performance bottleneck (unlikely), we can revisit this.
    scalar_solution_ = solver_.solve(scalar_rhs_);
    VectorRef(solution_ptr, solver_.cols()) =
        scalar_solution_.template cast<double>();

    if (solver_.info() != Eigen::Success) {
      *message = "Eigen failure. Unable to do triangular solve.";
      return LINEAR_SOLVER_FAILURE;
    }
    return LINEAR_SOLVER_SUCCESS;
  }

  LinearSolverTerminationType Factorize(CompressedRowSparseMatrix* lhs,
                                        std::string* message) final {
    CHECK_EQ(lhs->storage_type(), StorageType());

    typename Solver::Scalar* values_ptr = NULL;
    if (std::is_same<typename Solver::Scalar, double>::value) {
      values_ptr =
          reinterpret_cast<typename Solver::Scalar*>(lhs->mutable_values());
    } else {
      // In the case where the scalar used in this class is not
      // double. In that case, make a copy of the values array in the
      // CompressedRowSparseMatrix and cast it to Scalar along the way.
      values_ = ConstVectorRef(lhs->values(), lhs->num_nonzeros())
                    .cast<typename Solver::Scalar>();
      values_ptr = values_.data();
    }

    Eigen::MappedSparseMatrix<typename Solver::Scalar, Eigen::ColMajor>
        eigen_lhs(lhs->num_rows(),
                  lhs->num_rows(),
                  lhs->num_nonzeros(),
                  lhs->mutable_rows(),
                  lhs->mutable_cols(),
                  values_ptr);
    return Factorize(eigen_lhs, message);
  }

 private:
  Eigen::Matrix<typename Solver::Scalar, Eigen::Dynamic, 1> values_,
      scalar_rhs_, scalar_solution_;
  bool analyzed_;
  Solver solver_;
};

std::unique_ptr<SparseCholesky> EigenSparseCholesky::Create(
    const OrderingType ordering_type) {
  std::unique_ptr<SparseCholesky> sparse_cholesky;

  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>,
                                Eigen::Upper,
                                Eigen::AMDOrdering<int>>
      WithAMDOrdering;
  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>,
                                Eigen::Upper,
                                Eigen::NaturalOrdering<int>>
      WithNaturalOrdering;
  if (ordering_type == AMD) {
    sparse_cholesky.reset(new EigenSparseCholeskyTemplate<WithAMDOrdering>());
  } else {
    sparse_cholesky.reset(
        new EigenSparseCholeskyTemplate<WithNaturalOrdering>());
  }
  return sparse_cholesky;
}

EigenSparseCholesky::~EigenSparseCholesky() {}

std::unique_ptr<SparseCholesky> FloatEigenSparseCholesky::Create(
    const OrderingType ordering_type) {
  std::unique_ptr<SparseCholesky> sparse_cholesky;
  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>,
                                Eigen::Upper,
                                Eigen::AMDOrdering<int>>
      WithAMDOrdering;
  typedef Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>,
                                Eigen::Upper,
                                Eigen::NaturalOrdering<int>>
      WithNaturalOrdering;
  if (ordering_type == AMD) {
    sparse_cholesky.reset(new EigenSparseCholeskyTemplate<WithAMDOrdering>());
  } else {
    sparse_cholesky.reset(
        new EigenSparseCholeskyTemplate<WithNaturalOrdering>());
  }
  return sparse_cholesky;
}

FloatEigenSparseCholesky::~FloatEigenSparseCholesky() {}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_USE_EIGEN_SPARSE
