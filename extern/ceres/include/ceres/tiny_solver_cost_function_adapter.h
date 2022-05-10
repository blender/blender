// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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

#ifndef CERES_PUBLIC_TINY_SOLVER_COST_FUNCTION_ADAPTER_H_
#define CERES_PUBLIC_TINY_SOLVER_COST_FUNCTION_ADAPTER_H_

#include "Eigen/Core"
#include "ceres/cost_function.h"
#include "glog/logging.h"

namespace ceres {

// An adapter class that lets users of TinySolver use
// ceres::CostFunction objects that have exactly one parameter block.
//
// The adapter allows for the number of residuals and the size of the
// parameter block to be specified at compile or run-time.
//
// WARNING: This object is not thread-safe.
//
// Example usage:
//
//  CostFunction* cost_function = ...
//
// Number of residuals and parameter block size known at compile time:
//
//   TinySolverCostFunctionAdapter<kNumResiduals, kNumParameters>
//   cost_function_adapter(*cost_function);
//
// Number of residuals known at compile time and the parameter block
// size not known at compile time.
//
//   TinySolverCostFunctionAdapter<kNumResiduals, Eigen::Dynamic>
//   cost_function_adapter(*cost_function);
//
// Number of residuals not known at compile time and the parameter
// block size known at compile time.
//
//   TinySolverCostFunctionAdapter<Eigen::Dynamic, kParameterBlockSize>
//   cost_function_adapter(*cost_function);
//
// Number of residuals not known at compile time and the parameter
// block size not known at compile time.
//
//   TinySolverCostFunctionAdapter cost_function_adapter(*cost_function);
//
template <int kNumResiduals = Eigen::Dynamic,
          int kNumParameters = Eigen::Dynamic>
class TinySolverCostFunctionAdapter {
 public:
  using Scalar = double;
  enum ComponentSizeType {
    NUM_PARAMETERS = kNumParameters,
    NUM_RESIDUALS = kNumResiduals
  };

  // This struct needs to have an Eigen aligned operator new as it contains
  // fixed-size Eigen types.
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  explicit TinySolverCostFunctionAdapter(const CostFunction& cost_function)
      : cost_function_(cost_function) {
    CHECK_EQ(cost_function_.parameter_block_sizes().size(), 1)
        << "Only CostFunctions with exactly one parameter blocks are allowed.";

    const int parameter_block_size = cost_function_.parameter_block_sizes()[0];
    if (NUM_PARAMETERS == Eigen::Dynamic || NUM_RESIDUALS == Eigen::Dynamic) {
      if (NUM_RESIDUALS != Eigen::Dynamic) {
        CHECK_EQ(cost_function_.num_residuals(), NUM_RESIDUALS);
      }
      if (NUM_PARAMETERS != Eigen::Dynamic) {
        CHECK_EQ(parameter_block_size, NUM_PARAMETERS);
      }

      row_major_jacobian_.resize(cost_function_.num_residuals(),
                                 parameter_block_size);
    }
  }

  bool operator()(const double* parameters,
                  double* residuals,
                  double* jacobian) const {
    if (!jacobian) {
      return cost_function_.Evaluate(&parameters, residuals, nullptr);
    }

    double* jacobians[1] = {row_major_jacobian_.data()};
    if (!cost_function_.Evaluate(&parameters, residuals, jacobians)) {
      return false;
    }

    // The Function object used by TinySolver takes its Jacobian in a
    // column-major layout, and the CostFunction objects use row-major
    // Jacobian matrices. So the following bit of code does the
    // conversion from row-major Jacobians to column-major Jacobians.
    Eigen::Map<Eigen::Matrix<double, NUM_RESIDUALS, NUM_PARAMETERS>>
        col_major_jacobian(jacobian, NumResiduals(), NumParameters());
    col_major_jacobian = row_major_jacobian_;
    return true;
  }

  int NumResiduals() const { return cost_function_.num_residuals(); }
  int NumParameters() const {
    return cost_function_.parameter_block_sizes()[0];
  }

 private:
  const CostFunction& cost_function_;
  mutable Eigen::Matrix<double, NUM_RESIDUALS, NUM_PARAMETERS, Eigen::RowMajor>
      row_major_jacobian_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_TINY_SOLVER_COST_FUNCTION_ADAPTER_H_
