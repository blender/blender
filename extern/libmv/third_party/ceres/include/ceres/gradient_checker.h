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
// Copyright 2007 Google Inc. All Rights Reserved.
//
// Author: wjr@google.com (William Rucklidge)
//
// This file contains a class that exercises a cost function, to make sure
// that it is computing reasonable derivatives. It compares the Jacobians
// computed by the cost function with those obtained by finite
// differences.

#ifndef CERES_PUBLIC_GRADIENT_CHECKER_H_
#define CERES_PUBLIC_GRADIENT_CHECKER_H_

#include <cstddef>
#include <algorithm>
#include <vector>

#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/macros.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/numeric_diff_cost_function.h"
#include "glog/logging.h"

namespace ceres {

// An object that exercises a cost function, to compare the answers that it
// gives with derivatives estimated using finite differencing.
//
// The only likely usage of this is for testing.
//
// How to use: Fill in an array of pointers to parameter blocks for your
// CostFunction, and then call Probe(). Check that the return value is
// 'true'. See prober_test.cc for an example.
//
// This is templated similarly to NumericDiffCostFunction, as it internally
// uses that.
template <typename CostFunctionToProbe,
          int M = 0, int N0 = 0, int N1 = 0, int N2 = 0, int N3 = 0, int N4 = 0>
class GradientChecker {
 public:
  // Here we stash some results from the probe, for later
  // inspection.
  struct GradientCheckResults {
    // Computed cost.
    Vector cost;

    // The sizes of these matrices are dictated by the cost function's
    // parameter and residual block sizes. Each vector's length will
    // term->parameter_block_sizes().size(), and each matrix is the
    // Jacobian of the residual with respect to the corresponding parameter
    // block.

    // Derivatives as computed by the cost function.
    std::vector<Matrix> term_jacobians;

    // Derivatives as computed by finite differencing.
    std::vector<Matrix> finite_difference_jacobians;

    // Infinity-norm of term_jacobians - finite_difference_jacobians.
    double error_jacobians;
  };

  // Checks the Jacobian computed by a cost function.
  //
  // probe_point: The parameter values at which to probe.
  // error_tolerance: A threshold for the infinity-norm difference
  // between the Jacobians. If the Jacobians differ by more than
  // this amount, then the probe fails.
  //
  // term: The cost function to test. Not retained after this call returns.
  //
  // results: On return, the two Jacobians (and other information)
  // will be stored here.  May be NULL.
  //
  // Returns true if no problems are detected and the difference between the
  // Jacobians is less than error_tolerance.
  static bool Probe(double const* const* probe_point,
                    double error_tolerance,
                    CostFunctionToProbe *term,
                    GradientCheckResults* results) {
    CHECK_NOTNULL(probe_point);
    CHECK_NOTNULL(term);
    LOG(INFO) << "-------------------- Starting Probe() --------------------";

    // We need a GradientCheckeresults, whether or not they supplied one.
    internal::scoped_ptr<GradientCheckResults> owned_results;
    if (results == NULL) {
      owned_results.reset(new GradientCheckResults);
      results = owned_results.get();
    }

    // Do a consistency check between the term and the template parameters.
    CHECK_EQ(M, term->num_residuals());
    const int num_residuals = M;
    const std::vector<int32>& block_sizes = term->parameter_block_sizes();
    const int num_blocks = block_sizes.size();

    CHECK_LE(num_blocks, 5) << "Unable to test functions that take more "
                            << "than 5 parameter blocks";
    if (N0) {
      CHECK_EQ(N0, block_sizes[0]);
      CHECK_GE(num_blocks, 1);
    } else {
      CHECK_LT(num_blocks, 1);
    }
    if (N1) {
      CHECK_EQ(N1, block_sizes[1]);
      CHECK_GE(num_blocks, 2);
    } else {
      CHECK_LT(num_blocks, 2);
    }
    if (N2) {
      CHECK_EQ(N2, block_sizes[2]);
      CHECK_GE(num_blocks, 3);
    } else {
      CHECK_LT(num_blocks, 3);
    }
    if (N3) {
      CHECK_EQ(N3, block_sizes[3]);
      CHECK_GE(num_blocks, 4);
    } else {
      CHECK_LT(num_blocks, 4);
    }
    if (N4) {
      CHECK_EQ(N4, block_sizes[4]);
      CHECK_GE(num_blocks, 5);
    } else {
      CHECK_LT(num_blocks, 5);
    }

    results->term_jacobians.clear();
    results->term_jacobians.resize(num_blocks);
    results->finite_difference_jacobians.clear();
    results->finite_difference_jacobians.resize(num_blocks);

    internal::FixedArray<double*> term_jacobian_pointers(num_blocks);
    internal::FixedArray<double*>
        finite_difference_jacobian_pointers(num_blocks);
    for (int i = 0; i < num_blocks; i++) {
      results->term_jacobians[i].resize(num_residuals, block_sizes[i]);
      term_jacobian_pointers[i] = results->term_jacobians[i].data();
      results->finite_difference_jacobians[i].resize(
          num_residuals, block_sizes[i]);
      finite_difference_jacobian_pointers[i] =
          results->finite_difference_jacobians[i].data();
    }
    results->cost.resize(num_residuals, 1);

    CHECK(term->Evaluate(probe_point, results->cost.data(),
                         term_jacobian_pointers.get()));
    NumericDiffCostFunction<CostFunctionToProbe, CENTRAL, M, N0, N1, N2, N3, N4>
        numeric_term(term, DO_NOT_TAKE_OWNERSHIP);
    CHECK(numeric_term.Evaluate(probe_point, results->cost.data(),
                                finite_difference_jacobian_pointers.get()));

    results->error_jacobians = 0;
    for (int i = 0; i < num_blocks; i++) {
      Matrix jacobian_difference = results->term_jacobians[i] -
          results->finite_difference_jacobians[i];
      results->error_jacobians =
          std::max(results->error_jacobians,
                   jacobian_difference.lpNorm<Eigen::Infinity>());
    }

    LOG(INFO) << "========== term-computed derivatives ==========";
    for (int i = 0; i < num_blocks; i++) {
      LOG(INFO) << "term_computed block " << i;
      LOG(INFO) << "\n" << results->term_jacobians[i];
    }

    LOG(INFO) << "========== finite-difference derivatives ==========";
    for (int i = 0; i < num_blocks; i++) {
      LOG(INFO) << "finite_difference block " << i;
      LOG(INFO) << "\n" << results->finite_difference_jacobians[i];
    }

    LOG(INFO) << "========== difference ==========";
    for (int i = 0; i < num_blocks; i++) {
      LOG(INFO) << "difference block " << i;
      LOG(INFO) << (results->term_jacobians[i] -
                    results->finite_difference_jacobians[i]);
    }

    LOG(INFO) << "||difference|| = " << results->error_jacobians;

    return results->error_jacobians < error_tolerance;
  }

 private:
  CERES_DISALLOW_IMPLICIT_CONSTRUCTORS(GradientChecker);
};

}  // namespace ceres

#endif  // CERES_PUBLIC_GRADIENT_CHECKER_H_
