// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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

#include <memory>

#ifdef CERES_USE_EIGEN_SPARSE

#include <sstream>

#ifndef CERES_NO_EIGEN_METIS
#include <iostream>  // This is needed because MetisSupport depends on iostream.

#include "Eigen/MetisSupport"
#endif

#include "Eigen/SparseCholesky"
#include "Eigen/SparseCore"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/linear_solver.h"

namespace ceres::internal {

template <typename Solver>
class EigenSparseCholeskyTemplate final : public SparseCholesky {
 public:
  EigenSparseCholeskyTemplate() = default;
  CompressedRowSparseMatrix::StorageType StorageType() const final {
    return CompressedRowSparseMatrix::StorageType::LOWER_TRIANGULAR;
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
        return LinearSolverTerminationType::FATAL_ERROR;
      }

      analyzed_ = true;
    }

    solver_.factorize(lhs);
    if (solver_.info() != Eigen::Success) {
      *message = "Eigen failure. Unable to find numeric factorization.";
      return LinearSolverTerminationType::FAILURE;
    }
    return LinearSolverTerminationType::SUCCESS;
  }

  LinearSolverTerminationType Solve(const double* rhs_ptr,
                                    double* solution_ptr,
                                    std::string* message) override {
    CHECK(analyzed_) << "Solve called without a call to Factorize first.";

    // Avoid copying when the scalar type is double
    if constexpr (std::is_same_v<typename Solver::Scalar, double>) {
      ConstVectorRef scalar_rhs(rhs_ptr, solver_.cols());
      VectorRef(solution_ptr, solver_.cols()) = solver_.solve(scalar_rhs);
    } else {
      auto scalar_rhs = ConstVectorRef(rhs_ptr, solver_.cols())
                            .template cast<typename Solver::Scalar>();
      auto scalar_solution = solver_.solve(scalar_rhs);
      VectorRef(solution_ptr, solver_.cols()) =
          scalar_solution.template cast<double>();
    }

    if (solver_.info() != Eigen::Success) {
      *message = "Eigen failure. Unable to do triangular solve.";
      return LinearSolverTerminationType::FAILURE;
    }
    return LinearSolverTerminationType::SUCCESS;
  }

  LinearSolverTerminationType Factorize(CompressedRowSparseMatrix* lhs,
                                        std::string* message) final {
    CHECK_EQ(lhs->storage_type(), StorageType());

    typename Solver::Scalar* values_ptr = nullptr;
    if constexpr (std::is_same_v<typename Solver::Scalar, double>) {
      values_ptr = lhs->mutable_values();
    } else {
      // In the case where the scalar used in this class is not
      // double. In that case, make a copy of the values array in the
      // CompressedRowSparseMatrix and cast it to Scalar along the way.
      values_ = ConstVectorRef(lhs->values(), lhs->num_nonzeros())
                    .cast<typename Solver::Scalar>();
      values_ptr = values_.data();
    }

    Eigen::Map<
        const Eigen::SparseMatrix<typename Solver::Scalar, Eigen::ColMajor>>
        eigen_lhs(lhs->num_rows(),
                  lhs->num_rows(),
                  lhs->num_nonzeros(),
                  lhs->rows(),
                  lhs->cols(),
                  values_ptr);
    return Factorize(eigen_lhs, message);
  }

 private:
  Eigen::Matrix<typename Solver::Scalar, Eigen::Dynamic, 1> values_;

  bool analyzed_{false};
  Solver solver_;
};

std::unique_ptr<SparseCholesky> EigenSparseCholesky::Create(
    const OrderingType ordering_type) {
  using WithAMDOrdering = Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>,
                                                Eigen::Upper,
                                                Eigen::AMDOrdering<int>>;
  using WithNaturalOrdering =
      Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>,
                            Eigen::Upper,
                            Eigen::NaturalOrdering<int>>;

  if (ordering_type == OrderingType::AMD) {
    return std::make_unique<EigenSparseCholeskyTemplate<WithAMDOrdering>>();
  } else if (ordering_type == OrderingType::NESDIS) {
#ifndef CERES_NO_EIGEN_METIS
    using WithMetisOrdering = Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>,
                                                    Eigen::Upper,
                                                    Eigen::MetisOrdering<int>>;
    return std::make_unique<EigenSparseCholeskyTemplate<WithMetisOrdering>>();
#else
    LOG(FATAL)
        << "Congratulations you have found a bug in Ceres Solver. Please "
           "report it to the Ceres Solver developers.";
    return nullptr;
#endif  // CERES_NO_EIGEN_METIS
  }
  return std::make_unique<EigenSparseCholeskyTemplate<WithNaturalOrdering>>();
}

EigenSparseCholesky::~EigenSparseCholesky() = default;

std::unique_ptr<SparseCholesky> FloatEigenSparseCholesky::Create(
    const OrderingType ordering_type) {
  using WithAMDOrdering = Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>,
                                                Eigen::Upper,
                                                Eigen::AMDOrdering<int>>;
  using WithNaturalOrdering =
      Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>,
                            Eigen::Upper,
                            Eigen::NaturalOrdering<int>>;
  if (ordering_type == OrderingType::AMD) {
    return std::make_unique<EigenSparseCholeskyTemplate<WithAMDOrdering>>();
  } else if (ordering_type == OrderingType::NESDIS) {
#ifndef CERES_NO_EIGEN_METIS
    using WithMetisOrdering = Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>,
                                                    Eigen::Upper,
                                                    Eigen::MetisOrdering<int>>;
    return std::make_unique<EigenSparseCholeskyTemplate<WithMetisOrdering>>();
#else
    LOG(FATAL)
        << "Congratulations you have found a bug in Ceres Solver. Please "
           "report it to the Ceres Solver developers.";
    return nullptr;
#endif  // CERES_NO_EIGEN_METIS
  }
  return std::make_unique<EigenSparseCholeskyTemplate<WithNaturalOrdering>>();
}

FloatEigenSparseCholesky::~FloatEigenSparseCholesky() = default;

}  // namespace ceres::internal

#endif  // CERES_USE_EIGEN_SPARSE
