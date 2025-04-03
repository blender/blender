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
// Author: alexs.mac@gmail.com (Alex Stewart)

#ifndef CERES_INTERNAL_ACCELERATE_SPARSE_H_
#define CERES_INTERNAL_ACCELERATE_SPARSE_H_

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/config.h"

#ifndef CERES_NO_ACCELERATE_SPARSE

#include <memory>
#include <string>
#include <vector>

#include "Accelerate.h"
#include "ceres/linear_solver.h"
#include "ceres/sparse_cholesky.h"

namespace ceres {
namespace internal {

class CompressedRowSparseMatrix;
class TripletSparseMatrix;

template <typename Scalar>
struct SparseTypesTrait {};

template <>
struct SparseTypesTrait<double> {
  using DenseVector = DenseVector_Double;
  using SparseMatrix = SparseMatrix_Double;
  using SymbolicFactorization = SparseOpaqueSymbolicFactorization;
  using NumericFactorization = SparseOpaqueFactorization_Double;
};

template <>
struct SparseTypesTrait<float> {
  using DenseVector = DenseVector_Float;
  using SparseMatrix = SparseMatrix_Float;
  using SymbolicFactorization = SparseOpaqueSymbolicFactorization;
  using NumericFactorization = SparseOpaqueFactorization_Float;
};

template <typename Scalar>
class AccelerateSparse {
 public:
  using DenseVector = typename SparseTypesTrait<Scalar>::DenseVector;
  // Use ASSparseMatrix to avoid collision with ceres::internal::SparseMatrix.
  using ASSparseMatrix = typename SparseTypesTrait<Scalar>::SparseMatrix;
  using SymbolicFactorization =
      typename SparseTypesTrait<Scalar>::SymbolicFactorization;
  using NumericFactorization =
      typename SparseTypesTrait<Scalar>::NumericFactorization;

  // Solves a linear system given its symbolic (reference counted within
  // NumericFactorization) and numeric factorization.
  void Solve(NumericFactorization* numeric_factor,
             DenseVector* rhs_and_solution);

  // Note: Accelerate's API passes/returns its objects by value, but as the
  //       objects contain pointers to the underlying data these copies are
  //       all shallow (in some cases Accelerate also reference counts the
  //       objects internally).
  ASSparseMatrix CreateSparseMatrixTransposeView(CompressedRowSparseMatrix* A);
  // Computes a symbolic factorisation of A that can be used in Solve().
  SymbolicFactorization AnalyzeCholesky(OrderingType ordering_type,
                                        ASSparseMatrix* A);
  // Compute the numeric Cholesky factorization of A, given its
  // symbolic factorization.
  NumericFactorization Cholesky(ASSparseMatrix* A,
                                SymbolicFactorization* symbolic_factor);
  // Reuse the NumericFactorization from a previous matrix with the same
  // symbolic factorization to represent a new numeric factorization.
  void Cholesky(ASSparseMatrix* A, NumericFactorization* numeric_factor);

 private:
  std::vector<long> column_starts_;
  std::vector<uint8_t> solve_workspace_;
  std::vector<uint8_t> factorization_workspace_;
  // Storage for the values of A if Scalar != double (necessitating a copy).
  Eigen::Matrix<Scalar, Eigen::Dynamic, 1> values_;
};

// An implementation of SparseCholesky interface using Apple's Accelerate
// framework.
template <typename Scalar>
class AppleAccelerateCholesky final : public SparseCholesky {
 public:
  // Factory
  static std::unique_ptr<SparseCholesky> Create(OrderingType ordering_type);

  // SparseCholesky interface.
  virtual ~AppleAccelerateCholesky();
  CompressedRowSparseMatrix::StorageType StorageType() const;
  LinearSolverTerminationType Factorize(CompressedRowSparseMatrix* lhs,
                                        std::string* message) final;
  LinearSolverTerminationType Solve(const double* rhs,
                                    double* solution,
                                    std::string* message) final;

 private:
  AppleAccelerateCholesky(const OrderingType ordering_type);
  void FreeSymbolicFactorization();
  void FreeNumericFactorization();

  const OrderingType ordering_type_;
  AccelerateSparse<Scalar> as_;
  std::unique_ptr<typename AccelerateSparse<Scalar>::SymbolicFactorization>
      symbolic_factor_;
  std::unique_ptr<typename AccelerateSparse<Scalar>::NumericFactorization>
      numeric_factor_;
  // Copy of rhs/solution if Scalar != double (necessitating a copy).
  Eigen::Matrix<Scalar, Eigen::Dynamic, 1> scalar_rhs_and_solution_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_ACCELERATE_SPARSE

#endif  // CERES_INTERNAL_ACCELERATE_SPARSE_H_
