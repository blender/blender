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
//
// The Problem object is used to build and hold least squares problems.

#ifndef CERES_PUBLIC_PROBLEM_H_
#define CERES_PUBLIC_PROBLEM_H_

#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include <glog/logging.h>
#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {

class CostFunction;
class LossFunction;
class LocalParameterization;

namespace internal {
class Preprocessor;
class ProblemImpl;
class ParameterBlock;
class ResidualBlock;
class SolverImpl;
}  // namespace internal

// A ResidualBlockId is a handle clients can use to delete residual
// blocks after creating them. They are opaque for any purposes other
// than that.
typedef const internal::ResidualBlock* ResidualBlockId;

// A class to represent non-linear least squares problems. Such
// problems have a cost function that is a sum of error terms (known
// as "residuals"), where each residual is a function of some subset
// of the parameters. The cost function takes the form
//
//    N    1
//   SUM  --- loss( || r_i1, r_i2,..., r_ik ||^2  ),
//   i=1   2
//
// where
//
//   r_ij     is residual number i, component j; the residual is a
//            function of some subset of the parameters x1...xk. For
//            example, in a structure from motion problem a residual
//            might be the difference between a measured point in an
//            image and the reprojected position for the matching
//            camera, point pair. The residual would have two
//            components, error in x and error in y.
//
//   loss(y)  is the loss function; for example, squared error or
//            Huber L1 loss. If loss(y) = y, then the cost function is
//            non-robustified least squares.
//
// This class is specifically designed to address the important subset
// of "sparse" least squares problems, where each component of the
// residual depends only on a small number number of parameters, even
// though the total number of residuals and parameters may be very
// large. This property affords tremendous gains in scale, allowing
// efficient solving of large problems that are otherwise
// inaccessible.
//
// The canonical example of a sparse least squares problem is
// "structure-from-motion" (SFM), where the parameters are points and
// cameras, and residuals are reprojection errors. Typically a single
// residual will depend only on 9 parameters (3 for the point, 6 for
// the camera).
//
// To create a least squares problem, use the AddResidualBlock() and
// AddParameterBlock() methods, documented below. Here is an example least
// squares problem containing 3 parameter blocks of sizes 3, 4 and 5
// respectively and two residual terms of size 2 and 6:
//
//   double x1[] = { 1.0, 2.0, 3.0 };
//   double x2[] = { 1.0, 2.0, 3.0, 5.0 };
//   double x3[] = { 1.0, 2.0, 3.0, 6.0, 7.0 };
//
//   Problem problem;
//
//   problem.AddResidualBlock(new MyUnaryCostFunction(...), x1);
//   problem.AddResidualBlock(new MyBinaryCostFunction(...), x2, x3);
//
// Please see cost_function.h for details of the CostFunction object.
class Problem {
 public:
  struct Options {
    Options()
        : cost_function_ownership(TAKE_OWNERSHIP),
          loss_function_ownership(TAKE_OWNERSHIP),
          local_parameterization_ownership(TAKE_OWNERSHIP) {}

    // These flags control whether the Problem object owns the cost
    // functions, loss functions, and parameterizations passed into
    // the Problem. If set to TAKE_OWNERSHIP, then the problem object
    // will delete the corresponding cost or loss functions on
    // destruction. The destructor is careful to delete the pointers
    // only once, since sharing cost/loss/parameterizations is
    // allowed.
    Ownership cost_function_ownership;
    Ownership loss_function_ownership;
    Ownership local_parameterization_ownership;
  };

  // The default constructor is equivalent to the
  // invocation Problem(Problem::Options()).
  Problem();
  explicit Problem(const Options& options);

  ~Problem();

  // Add a residual block to the overall cost function. The cost
  // function carries with it information about the sizes of the
  // parameter blocks it expects. The function checks that these match
  // the sizes of the parameter blocks listed in parameter_blocks. The
  // program aborts if a mismatch is detected. loss_function can be
  // NULL, in which case the cost of the term is just the squared norm
  // of the residuals.
  //
  // The user has the option of explicitly adding the parameter blocks
  // using AddParameterBlock. This causes additional correctness
  // checking; however, AddResidualBlock implicitly adds the parameter
  // blocks if they are not present, so calling AddParameterBlock
  // explicitly is not required.
  //
  // The Problem object by default takes ownership of the
  // cost_function and loss_function pointers. These objects remain
  // live for the life of the Problem object. If the user wishes to
  // keep control over the destruction of these objects, then they can
  // do this by setting the corresponding enums in the Options struct.
  //
  // Note: Even though the Problem takes ownership of cost_function
  // and loss_function, it does not preclude the user from re-using
  // them in another residual block. The destructor takes care to call
  // delete on each cost_function or loss_function pointer only once,
  // regardless of how many residual blocks refer to them.
  //
  // Example usage:
  //
  //   double x1[] = {1.0, 2.0, 3.0};
  //   double x2[] = {1.0, 2.0, 5.0, 6.0};
  //   double x3[] = {3.0, 6.0, 2.0, 5.0, 1.0};
  //
  //   Problem problem;
  //
  //   problem.AddResidualBlock(new MyUnaryCostFunction(...), NULL, x1);
  //   problem.AddResidualBlock(new MyBinaryCostFunction(...), NULL, x2, x1);
  //
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   const vector<double*>& parameter_blocks);

  // Convenience methods for adding residuals with a small number of
  // parameters. This is the common case. Instead of specifying the
  // parameter block arguments as a vector, list them as pointers.
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4);
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0, double* x1, double* x2,
                                   double* x3, double* x4, double* x5);

  // Add a parameter block with appropriate size to the problem.
  // Repeated calls with the same arguments are ignored. Repeated
  // calls with the same double pointer but a different size results
  // in undefined behaviour.
  void AddParameterBlock(double* values, int size);

  // Add a parameter block with appropriate size and parameterization
  // to the problem. Repeated calls with the same arguments are
  // ignored. Repeated calls with the same double pointer but a
  // different size results in undefined behaviour.
  void AddParameterBlock(double* values,
                         int size,
                         LocalParameterization* local_parameterization);

  // Hold the indicated parameter block constant during optimization.
  void SetParameterBlockConstant(double* values);

  // Allow the indicated parameter to vary during optimization.
  void SetParameterBlockVariable(double* values);

  // Set the local parameterization for one of the parameter blocks.
  // The local_parameterization is owned by the Problem by default. It
  // is acceptable to set the same parameterization for multiple
  // parameters; the destructor is careful to delete local
  // parameterizations only once. The local parameterization can only
  // be set once per parameter, and cannot be changed once set.
  void SetParameterization(double* values,
                           LocalParameterization* local_parameterization);

  // Number of parameter blocks in the problem. Always equals
  // parameter_blocks().size() and parameter_block_sizes().size().
  int NumParameterBlocks() const;

  // The size of the parameter vector obtained by summing over the
  // sizes of all the parameter blocks.
  int NumParameters() const;

  // Number of residual blocks in the problem. Always equals
  // residual_blocks().size().
  int NumResidualBlocks() const;

  // The size of the residual vector obtained by summing over the
  // sizes of all of the residual blocks.
  int NumResiduals() const;

 private:
  friend class internal::SolverImpl;
  internal::scoped_ptr<internal::ProblemImpl> problem_impl_;
  DISALLOW_COPY_AND_ASSIGN(Problem);
};

}  // namespace ceres

#endif  // CERES_PUBLIC_PROBLEM_H_
