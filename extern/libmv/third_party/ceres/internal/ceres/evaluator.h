// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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
//         keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_EVALUATOR_H_
#define CERES_INTERNAL_EVALUATOR_H_

#include <map>
#include <string>
#include <vector>

#include "ceres/execution_summary.h"
#include "ceres/internal/port.h"
#include "ceres/types.h"

namespace ceres {

struct CRSMatrix;

namespace internal {

class Program;
class SparseMatrix;

// The Evaluator interface offers a way to interact with a least squares cost
// function that is useful for an optimizer that wants to minimize the least
// squares objective. This insulates the optimizer from issues like Jacobian
// storage, parameterization, etc.
class Evaluator {
 public:
  virtual ~Evaluator();

  struct Options {
    Options()
        : num_threads(1),
          num_eliminate_blocks(-1),
          linear_solver_type(DENSE_QR),
          dynamic_sparsity(false) {}

    int num_threads;
    int num_eliminate_blocks;
    LinearSolverType linear_solver_type;
    bool dynamic_sparsity;
  };

  static Evaluator* Create(const Options& options,
                           Program* program,
                           string* error);

  // This is used for computing the cost, residual and Jacobian for
  // returning to the user. For actually solving the optimization
  // problem, the optimization algorithm uses the ProgramEvaluator
  // objects directly.
  //
  // The residual, gradients and jacobian pointers can be NULL, in
  // which case they will not be evaluated. cost cannot be NULL.
  //
  // The parallelism of the evaluator is controlled by num_threads; it
  // should be at least 1.
  //
  // Note: That this function does not take a parameter vector as
  // input. The parameter blocks are evaluated on the values contained
  // in the arrays pointed to by their user_state pointers.
  //
  // Also worth noting is that this function mutates program by
  // calling Program::SetParameterOffsetsAndIndex() on it so that an
  // evaluator object can be constructed.
  static bool Evaluate(Program* program,
                       int num_threads,
                       double* cost,
                       vector<double>* residuals,
                       vector<double>* gradient,
                       CRSMatrix* jacobian);

  // Build and return a sparse matrix for storing and working with the Jacobian
  // of the objective function. The jacobian has dimensions
  // NumEffectiveParameters() by NumParameters(), and is typically extremely
  // sparse. Since the sparsity pattern of the Jacobian remains constant over
  // the lifetime of the optimization problem, this method is used to
  // instantiate a SparseMatrix object with the appropriate sparsity structure
  // (which can be an expensive operation) and then reused by the optimization
  // algorithm and the various linear solvers.
  //
  // It is expected that the classes implementing this interface will be aware
  // of their client's requirements for the kind of sparse matrix storage and
  // layout that is needed for an efficient implementation. For example
  // CompressedRowOptimizationProblem creates a compressed row representation of
  // the jacobian for use with CHOLMOD, where as BlockOptimizationProblem
  // creates a BlockSparseMatrix representation of the jacobian for use in the
  // Schur complement based methods.
  virtual SparseMatrix* CreateJacobian() const = 0;


  // Options struct to control Evaluator::Evaluate;
  struct EvaluateOptions {
    EvaluateOptions()
        : apply_loss_function(true) {
    }

    // If false, the loss function correction is not applied to the
    // residual blocks.
    bool apply_loss_function;
  };

  // Evaluate the cost function for the given state. Returns the cost,
  // residuals, and jacobian in the corresponding arguments. Both residuals and
  // jacobian are optional; to avoid computing them, pass NULL.
  //
  // If non-NULL, the Jacobian must have a suitable sparsity pattern; only the
  // values array of the jacobian is modified.
  //
  // state is an array of size NumParameters(), cost is a pointer to a single
  // double, and residuals is an array of doubles of size NumResiduals().
  virtual bool Evaluate(const EvaluateOptions& evaluate_options,
                        const double* state,
                        double* cost,
                        double* residuals,
                        double* gradient,
                        SparseMatrix* jacobian) = 0;

  // Variant of Evaluator::Evaluate where the user wishes to use the
  // default EvaluateOptions struct. This is mostly here as a
  // convenience method.
  bool Evaluate(const double* state,
                double* cost,
                double* residuals,
                double* gradient,
                SparseMatrix* jacobian) {
    return Evaluate(EvaluateOptions(),
                    state,
                    cost,
                    residuals,
                    gradient,
                    jacobian);
  }

  // Make a change delta (of size NumEffectiveParameters()) to state (of size
  // NumParameters()) and store the result in state_plus_delta.
  //
  // In the case that there are no parameterizations used, this is equivalent to
  //
  //   state_plus_delta[i] = state[i] + delta[i] ;
  //
  // however, the mapping is more complicated in the case of parameterizations
  // like quaternions. This is the same as the "Plus()" operation in
  // local_parameterization.h, but operating over the entire state vector for a
  // problem.
  virtual bool Plus(const double* state,
                    const double* delta,
                    double* state_plus_delta) const = 0;

  // The number of parameters in the optimization problem.
  virtual int NumParameters() const = 0;

  // This is the effective number of parameters that the optimizer may adjust.
  // This applies when there are parameterizations on some of the parameters.
  virtual int NumEffectiveParameters()  const = 0;

  // The number of residuals in the optimization problem.
  virtual int NumResiduals() const = 0;

  // The following two methods return copies instead of references so
  // that the base class implementation does not have to worry about
  // life time issues. Further, these calls are not expected to be
  // frequent or performance sensitive.
  virtual map<string, int> CallStatistics() const {
    return map<string, int>();
  }

  virtual map<string, double> TimeStatistics() const {
    return map<string, double>();
  }
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_EVALUATOR_H_
