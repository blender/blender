// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2021 Google Inc. All rights reserved.
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
//         keir@google.com (Keir Mierle)
//
// The Problem object is used to build and hold least squares problems.

#ifndef CERES_PUBLIC_PROBLEM_H_
#define CERES_PUBLIC_PROBLEM_H_

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ceres/context.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/internal/port.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

class CostFunction;
class EvaluationCallback;
class LossFunction;
class LocalParameterization;
class Manifold;
class Solver;
struct CRSMatrix;

namespace internal {
class Preprocessor;
class ProblemImpl;
class ParameterBlock;
class ResidualBlock;
}  // namespace internal

// A ResidualBlockId is an opaque handle clients can use to remove residual
// blocks from a Problem after adding them.
using ResidualBlockId = internal::ResidualBlock*;

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
//   r_ij is residual number i, component j; the residual is a function of some
//        subset of the parameters x1...xk. For example, in a structure from
//        motion problem a residual might be the difference between a measured
//        point in an image and the reprojected position for the matching
//        camera, point pair. The residual would have two components, error in x
//        and error in y.
//
//   loss(y) is the loss function; for example, squared error or Huber L1
//           loss. If loss(y) = y, then the cost function is non-robustified
//           least squares.
//
// This class is specifically designed to address the important subset of
// "sparse" least squares problems, where each component of the residual depends
// only on a small number number of parameters, even though the total number of
// residuals and parameters may be very large. This property affords tremendous
// gains in scale, allowing efficient solving of large problems that are
// otherwise inaccessible.
//
// The canonical example of a sparse least squares problem is
// "structure-from-motion" (SFM), where the parameters are points and cameras,
// and residuals are reprojection errors. Typically a single residual will
// depend only on 9 parameters (3 for the point, 6 for the camera).
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
//   problem.AddResidualBlock(new MyUnaryCostFunction(...), nullptr, x1);
//   problem.AddResidualBlock(new MyBinaryCostFunction(...), nullptr, x2, x3);
//
// Please see cost_function.h for details of the CostFunction object.
//
// NOTE: We are currently in the process of transitioning from
// LocalParameterization to Manifolds in the Ceres API. During this period,
// Problem will support using both Manifold and LocalParameterization objects
// interchangably. In particular, adding a LocalParameterization to a parameter
// block is the same as adding a Manifold to that parameter block. For methods
// in the API affected by this change, see their documentation below.
class CERES_EXPORT Problem {
 public:
  struct CERES_EXPORT Options {
    // These flags control whether the Problem object owns the CostFunctions,
    // LossFunctions, LocalParameterizations, and Manifolds passed into the
    // Problem.
    //
    // If set to TAKE_OWNERSHIP, then the problem object will delete the
    // corresponding object on destruction. The destructor is careful to delete
    // the pointers only once, since sharing objects is allowed.
    Ownership cost_function_ownership = TAKE_OWNERSHIP;
    Ownership loss_function_ownership = TAKE_OWNERSHIP;
    CERES_DEPRECATED_WITH_MSG(
        "Local Parameterizations are deprecated. Use Manifold and "
        "manifold_ownership instead.")
    Ownership local_parameterization_ownership = TAKE_OWNERSHIP;
    Ownership manifold_ownership = TAKE_OWNERSHIP;

    // If true, trades memory for faster RemoveResidualBlock() and
    // RemoveParameterBlock() operations.
    //
    // By default, RemoveParameterBlock() and RemoveResidualBlock() take time
    // proportional to the size of the entire problem. If you only ever remove
    // parameters or residuals from the problem occasionally, this might be
    // acceptable. However, if you have memory to spare, enable this option to
    // make RemoveParameterBlock() take time proportional to the number of
    // residual blocks that depend on it, and RemoveResidualBlock() take (on
    // average) constant time.
    //
    // The increase in memory usage is two-fold: an additional hash set per
    // parameter block containing all the residuals that depend on the parameter
    // block; and a hash set in the problem containing all residuals.
    bool enable_fast_removal = false;

    // By default, Ceres performs a variety of safety checks when constructing
    // the problem. There is a small but measurable performance penalty to these
    // checks, typically around 5% of construction time. If you are sure your
    // problem construction is correct, and 5% of the problem construction time
    // is truly an overhead you want to avoid, then you can set
    // disable_all_safety_checks to true.
    //
    // WARNING: Do not set this to true, unless you are absolutely sure of what
    // you are doing.
    bool disable_all_safety_checks = false;

    // A Ceres global context to use for solving this problem. This may help to
    // reduce computation time as Ceres can reuse expensive objects to create.
    // The context object can be nullptr, in which case Ceres may create one.
    //
    // Ceres does NOT take ownership of the pointer.
    Context* context = nullptr;

    // Using this callback interface, Ceres can notify you when it is about to
    // evaluate the residuals or jacobians. With the callback, you can share
    // computation between residual blocks by doing the shared computation in
    // EvaluationCallback::PrepareForEvaluation() before Ceres calls
    // CostFunction::Evaluate(). It also enables caching results between a pure
    // residual evaluation and a residual & jacobian evaluation.
    //
    // Problem DOES NOT take ownership of the callback.
    //
    // NOTE: Evaluation callbacks are incompatible with inner iterations. So
    // calling Solve with Solver::Options::use_inner_iterations = true on a
    // Problem with a non-null evaluation callback is an error.
    EvaluationCallback* evaluation_callback = nullptr;
  };

  // The default constructor is equivalent to the invocation
  // Problem(Problem::Options()).
  Problem();
  explicit Problem(const Options& options);
  Problem(Problem&&);
  Problem& operator=(Problem&&);

  Problem(const Problem&) = delete;
  Problem& operator=(const Problem&) = delete;

  ~Problem();

  // Add a residual block to the overall cost function. The cost function
  // carries with its information about the sizes of the parameter blocks it
  // expects. The function checks that these match the sizes of the parameter
  // blocks listed in parameter_blocks. The program aborts if a mismatch is
  // detected. loss_function can be nullptr, in which case the cost of the term
  // is just the squared norm of the residuals.
  //
  // The user has the option of explicitly adding the parameter blocks using
  // AddParameterBlock. This causes additional correctness checking; however,
  // AddResidualBlock implicitly adds the parameter blocks if they are not
  // present, so calling AddParameterBlock explicitly is not required.
  //
  // The Problem object by default takes ownership of the cost_function and
  // loss_function pointers (See Problem::Options to override this behaviour).
  // These objects remain live for the life of the Problem object. If the user
  // wishes to keep control over the destruction of these objects, then they can
  // do this by setting the corresponding enums in the Options struct.
  //
  // Note: Even though the Problem takes ownership of cost_function and
  // loss_function, it does not preclude the user from re-using them in another
  // residual block. The destructor takes care to call delete on each
  // cost_function or loss_function pointer only once, regardless of how many
  // residual blocks refer to them.
  //
  // Example usage:
  //
  //   double x1[] = {1.0, 2.0, 3.0};
  //   double x2[] = {1.0, 2.0, 5.0, 6.0};
  //   double x3[] = {3.0, 6.0, 2.0, 5.0, 1.0};
  //
  //   Problem problem;
  //
  //   problem.AddResidualBlock(new MyUnaryCostFunction(...), nullptr, x1);
  //   problem.AddResidualBlock(new MyBinaryCostFunction(...), nullptr, x2, x1);
  //
  // Add a residual block by listing the parameter block pointers directly
  // instead of wapping them in a container.
  template <typename... Ts>
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* x0,
                                   Ts*... xs) {
    const std::array<double*, sizeof...(Ts) + 1> parameter_blocks{{x0, xs...}};
    return AddResidualBlock(cost_function,
                            loss_function,
                            parameter_blocks.data(),
                            static_cast<int>(parameter_blocks.size()));
  }

  // Add a residual block by providing a vector of parameter blocks.
  ResidualBlockId AddResidualBlock(
      CostFunction* cost_function,
      LossFunction* loss_function,
      const std::vector<double*>& parameter_blocks);

  // Add a residual block by providing a pointer to the parameter block array
  // and the number of parameter blocks.
  ResidualBlockId AddResidualBlock(CostFunction* cost_function,
                                   LossFunction* loss_function,
                                   double* const* const parameter_blocks,
                                   int num_parameter_blocks);

  // Add a parameter block with appropriate size to the problem. Repeated calls
  // with the same arguments are ignored. Repeated calls with the same double
  // pointer but a different size will result in a crash.
  void AddParameterBlock(double* values, int size);

  // Add a parameter block with appropriate size and parameterization to the
  // problem. It is okay for local_parameterization to be nullptr.
  //
  // Repeated calls with the same arguments are ignored. Repeated calls
  // with the same double pointer but a different size results in a crash
  // (unless Solver::Options::diable_all_safety_checks is set to true).
  //
  // Repeated calls with the same double pointer and size but different
  // LocalParameterization is equivalent to calling
  // SetParameterization(local_parameterization), i.e., any previously
  // associated LocalParameterization or Manifold object will be replaced with
  // the local_parameterization.
  //
  // NOTE:
  // ----
  //
  // This method is deprecated and will be removed in the next public
  // release of Ceres Solver. Please move to using the Manifold based version of
  // AddParameterBlock.
  //
  // During the transition from LocalParameterization to Manifold, internally
  // the LocalParameterization is treated as a Manifold by wrapping it using a
  // ManifoldAdapter object. So HasManifold() will return true, GetManifold()
  // will return the wrapped object and ParameterBlockTangentSize() will return
  // the LocalSize of the LocalParameterization.
  CERES_DEPRECATED_WITH_MSG(
      "LocalParameterizations are deprecated. Use the version with Manifolds "
      "instead.")
  void AddParameterBlock(double* values,
                         int size,
                         LocalParameterization* local_parameterization);

  // Add a parameter block with appropriate size and Manifold to the
  // problem. It is okay for manifold to be nullptr.
  //
  // Repeated calls with the same arguments are ignored. Repeated calls
  // with the same double pointer but a different size results in a crash
  // (unless Solver::Options::diable_all_safety_checks is set to true).
  //
  // Repeated calls with the same double pointer and size but different Manifold
  // is equivalent to calling SetManifold(manifold), i.e., any previously
  // associated LocalParameterization or Manifold object will be replaced with
  // the manifold.
  //
  // Note:
  // ----
  //
  // During the transition from LocalParameterization to Manifold, calling
  // AddParameterBlock with a Manifold when a LocalParameterization is already
  // associated with the parameter block is okay. It is equivalent to calling
  // SetManifold(manifold), i.e., any previously associated
  // LocalParameterization or Manifold object will be replaced with the
  // manifold.
  void AddParameterBlock(double* values, int size, Manifold* manifold);

  // Remove a parameter block from the problem. The LocalParameterization or
  // Manifold of the parameter block, if it exists, will persist until the
  // deletion of the problem (similar to cost/loss functions in residual block
  // removal). Any residual blocks that depend on the parameter are also
  // removed, as described above in RemoveResidualBlock().
  //
  // If Problem::Options::enable_fast_removal is true, then the removal is fast
  // (almost constant time). Otherwise, removing a parameter block will incur a
  // scan of the entire Problem object.
  //
  // WARNING: Removing a residual or parameter block will destroy the implicit
  // ordering, rendering the jacobian or residuals returned from the solver
  // uninterpretable. If you depend on the evaluated jacobian, do not use
  // remove! This may change in a future release.
  void RemoveParameterBlock(const double* values);

  // Remove a residual block from the problem. Any parameters that the residual
  // block depends on are not removed. The cost and loss functions for the
  // residual block will not get deleted immediately; won't happen until the
  // problem itself is deleted.
  //
  // WARNING: Removing a residual or parameter block will destroy the implicit
  // ordering, rendering the jacobian or residuals returned from the solver
  // uninterpretable. If you depend on the evaluated jacobian, do not use
  // remove! This may change in a future release.
  void RemoveResidualBlock(ResidualBlockId residual_block);

  // Hold the indicated parameter block constant during optimization.
  void SetParameterBlockConstant(const double* values);

  // Allow the indicated parameter block to vary during optimization.
  void SetParameterBlockVariable(double* values);

  // Returns true if a parameter block is set constant, and false otherwise. A
  // parameter block may be set constant in two ways: either by calling
  // SetParameterBlockConstant or by associating a LocalParameterization or
  // Manifold with a zero dimensional tangent space with it.
  bool IsParameterBlockConstant(const double* values) const;

  // Set the LocalParameterization for the parameter block. Calling
  // SetParameterization with nullptr will clear any previously set
  // LocalParameterization or Manifold for the parameter block.
  //
  // Repeated calls will cause any previously associated LocalParameterization
  // or Manifold object to be replaced with the local_parameterization.
  //
  // The local_parameterization is owned by the Problem by default (See
  // Problem::Options to override this behaviour).
  //
  // It is acceptable to set the same LocalParameterization for multiple
  // parameter blocks; the destructor is careful to delete
  // LocalParamaterizations only once.
  //
  // NOTE:
  // ----
  //
  // This method is deprecated and will be removed in the next public
  // release of Ceres Solver. Please move to using the SetManifold instead.
  //
  // During the transition from LocalParameterization to Manifold, internally
  // the LocalParameterization is treated as a Manifold by wrapping it using a
  // ManifoldAdapter object. So HasManifold() will return true, GetManifold()
  // will return the wrapped object and ParameterBlockTangentSize will return
  // the same value of ParameterBlockLocalSize.
  CERES_DEPRECATED_WITH_MSG(
      "LocalParameterizations are deprecated. Use SetManifold instead.")
  void SetParameterization(double* values,
                           LocalParameterization* local_parameterization);

  // Get the LocalParameterization object associated with this parameter block.
  // If there is no LocalParameterization associated then nullptr is returned.
  //
  // NOTE: This method is deprecated and will be removed in the next public
  // release of Ceres Solver. Use GetManifold instead.
  //
  // Note also that if a LocalParameterization is associated with a parameter
  // block, HasManifold will return true and GetManifold will return the
  // LocalParameterization wrapped in a ManifoldAdapter.
  //
  // The converse is NOT true, i.e., if a Manifold is associated with a
  // parameter block, HasParameterization will return false and
  // GetParameterization will return a nullptr.
  CERES_DEPRECATED_WITH_MSG(
      "LocalParameterizations are deprecated. Use GetManifold "
      "instead.")
  const LocalParameterization* GetParameterization(const double* values) const;

  // Returns true if a LocalParameterization is associated with this parameter
  // block, false otherwise.
  //
  // NOTE: This method is deprecated and will be removed in the next public
  // release of Ceres Solver. Use HasManifold instead.
  //
  // Note also that if a Manifold is associated with the parameter block, this
  // method will return false.
  CERES_DEPRECATED_WITH_MSG(
      "LocalParameterizations are deprecated. Use HasManifold instead.")
  bool HasParameterization(const double* values) const;

  // Set the Manifold for the parameter block. Calling SetManifold with nullptr
  // will clear any previously set LocalParameterization or Manifold for the
  // parameter block.
  //
  // Repeated calls will result in any previously associated
  // LocalParameterization or Manifold object to be replaced with the manifold.
  //
  // The manifold is owned by the Problem by default (See Problem::Options to
  // override this behaviour).
  //
  // It is acceptable to set the same Manifold for multiple parameter blocks.
  void SetManifold(double* values, Manifold* manifold);

  // Get the Manifold object associated with this parameter block.
  //
  // If there is no Manifold Or LocalParameterization object associated then
  // nullptr is returned.
  //
  // NOTE: During the transition from LocalParameterization to Manifold,
  // internally the LocalParameterization is treated as a Manifold by wrapping
  // it using a ManifoldAdapter object. So calling GetManifold on a parameter
  // block with a LocalParameterization associated with it will return the
  // LocalParameterization wrapped in a ManifoldAdapter
  const Manifold* GetManifold(const double* values) const;

  // Returns true if a Manifold or a LocalParameterization is associated with
  // this parameter block, false otherwise.
  bool HasManifold(const double* values) const;

  // Set the lower/upper bound for the parameter at position "index".
  void SetParameterLowerBound(double* values, int index, double lower_bound);
  void SetParameterUpperBound(double* values, int index, double upper_bound);

  // Get the lower/upper bound for the parameter at position "index". If the
  // parameter is not bounded by the user, then its lower bound is
  // -std::numeric_limits<double>::max() and upper bound is
  // std::numeric_limits<double>::max().
  double GetParameterLowerBound(const double* values, int index) const;
  double GetParameterUpperBound(const double* values, int index) const;

  // Number of parameter blocks in the problem. Always equals
  // parameter_blocks().size() and parameter_block_sizes().size().
  int NumParameterBlocks() const;

  // The size of the parameter vector obtained by summing over the sizes of all
  // the parameter blocks.
  int NumParameters() const;

  // Number of residual blocks in the problem. Always equals
  // residual_blocks().size().
  int NumResidualBlocks() const;

  // The size of the residual vector obtained by summing over the sizes of all
  // of the residual blocks.
  int NumResiduals() const;

  // The size of the parameter block.
  int ParameterBlockSize(const double* values) const;

  // The dimension of the tangent space of the LocalParameterization or Manifold
  // for the parameter block. If there is no LocalParameterization or Manifold
  // associated with this parameter block, then ParameterBlockLocalSize =
  // ParameterBlockSize.
  CERES_DEPRECATED_WITH_MSG(
      "LocalParameterizations are deprecated. Use ParameterBlockTangentSize "
      "instead.")
  int ParameterBlockLocalSize(const double* values) const;

  // The dimenion of the tangent space of the LocalParameterization or Manifold
  // for the parameter block. If there is no LocalParameterization or Manifold
  // associated with this parameter block, then ParameterBlockTangentSize =
  // ParameterBlockSize.
  int ParameterBlockTangentSize(const double* values) const;

  // Is the given parameter block present in this problem or not?
  bool HasParameterBlock(const double* values) const;

  // Fills the passed parameter_blocks vector with pointers to the parameter
  // blocks currently in the problem. After this call, parameter_block.size() ==
  // NumParameterBlocks.
  void GetParameterBlocks(std::vector<double*>* parameter_blocks) const;

  // Fills the passed residual_blocks vector with pointers to the residual
  // blocks currently in the problem. After this call, residual_blocks.size() ==
  // NumResidualBlocks.
  void GetResidualBlocks(std::vector<ResidualBlockId>* residual_blocks) const;

  // Get all the parameter blocks that depend on the given residual block.
  void GetParameterBlocksForResidualBlock(
      const ResidualBlockId residual_block,
      std::vector<double*>* parameter_blocks) const;

  // Get the CostFunction for the given residual block.
  const CostFunction* GetCostFunctionForResidualBlock(
      const ResidualBlockId residual_block) const;

  // Get the LossFunction for the given residual block. Returns nullptr
  // if no loss function is associated with this residual block.
  const LossFunction* GetLossFunctionForResidualBlock(
      const ResidualBlockId residual_block) const;

  // Get all the residual blocks that depend on the given parameter block.
  //
  // If Problem::Options::enable_fast_removal is true, then getting the residual
  // blocks is fast and depends only on the number of residual
  // blocks. Otherwise, getting the residual blocks for a parameter block will
  // incur a scan of the entire Problem object.
  void GetResidualBlocksForParameterBlock(
      const double* values,
      std::vector<ResidualBlockId>* residual_blocks) const;

  // Options struct to control Problem::Evaluate.
  struct EvaluateOptions {
    // The set of parameter blocks for which evaluation should be
    // performed. This vector determines the order that parameter blocks occur
    // in the gradient vector and in the columns of the jacobian matrix. If
    // parameter_blocks is empty, then it is assumed to be equal to vector
    // containing ALL the parameter blocks. Generally speaking the parameter
    // blocks will occur in the order in which they were added to the
    // problem. But, this may change if the user removes any parameter blocks
    // from the problem.
    //
    // NOTE: This vector should contain the same pointers as the ones used to
    // add parameter blocks to the Problem. These parameter block should NOT
    // point to new memory locations. Bad things will happen otherwise.
    std::vector<double*> parameter_blocks;

    // The set of residual blocks to evaluate. This vector determines the order
    // in which the residuals occur, and how the rows of the jacobian are
    // ordered. If residual_blocks is empty, then it is assumed to be equal to
    // the vector containing ALL the residual blocks. Generally speaking the
    // residual blocks will occur in the order in which they were added to the
    // problem. But, this may change if the user removes any residual blocks
    // from the problem.
    std::vector<ResidualBlockId> residual_blocks;

    // Even though the residual blocks in the problem may contain loss
    // functions, setting apply_loss_function to false will turn off the
    // application of the loss function to the output of the cost function. This
    // is of use for example if the user wishes to analyse the solution quality
    // by studying the distribution of residuals before and after the solve.
    bool apply_loss_function = true;

    int num_threads = 1;
  };

  // Evaluate Problem. Any of the output pointers can be nullptr. Which residual
  // blocks and parameter blocks are used is controlled by the EvaluateOptions
  // struct above.
  //
  // Note 1: The evaluation will use the values stored in the memory locations
  // pointed to by the parameter block pointers used at the time of the
  // construction of the problem. i.e.,
  //
  //   Problem problem;
  //   double x = 1;
  //   problem.AddResidualBlock(new MyCostFunction, nullptr, &x);
  //
  //   double cost = 0.0;
  //   problem.Evaluate(Problem::EvaluateOptions(), &cost,
  //                    nullptr, nullptr, nullptr);
  //
  // The cost is evaluated at x = 1. If you wish to evaluate the problem at x =
  // 2, then
  //
  //   x = 2;
  //   problem.Evaluate(Problem::EvaluateOptions(), &cost,
  //                    nullptr, nullptr, nullptr);
  //
  // is the way to do so.
  //
  // Note 2: If no LocalParameterizations or Manifolds are used, then the size
  // of the gradient vector (and the number of columns in the jacobian) is the
  // sum of the sizes of all the parameter blocks. If a parameter block has a
  // LocalParameterization or Manifold, then it contributes "TangentSize"
  // entries to the gradient vector (and the number of columns in the jacobian).
  //
  // Note 3: This function cannot be called while the problem is being solved,
  // for example it cannot be called from an IterationCallback at the end of an
  // iteration during a solve.
  //
  // Note 4: If an EvaluationCallback is associated with the problem, then its
  // PrepareForEvaluation method will be called every time this method is called
  // with new_point = true.
  bool Evaluate(const EvaluateOptions& options,
                double* cost,
                std::vector<double>* residuals,
                std::vector<double>* gradient,
                CRSMatrix* jacobian);

  // Evaluates the residual block, storing the scalar cost in *cost, the
  // residual components in *residuals, and the jacobians between the parameters
  // and residuals in jacobians[i], in row-major order.
  //
  // If residuals is nullptr, the residuals are not computed.
  //
  // If jacobians is nullptr, no Jacobians are computed. If jacobians[i] is
  // nullptr, then the Jacobian for that parameter block is not computed.
  //
  // It is not okay to request the Jacobian w.r.t a parameter block that is
  // constant.
  //
  // The return value indicates the success or failure. Even if the function
  // returns false, the caller should expect the output memory locations to have
  // been modified.
  //
  // The returned cost and jacobians have had robustification and
  // LocalParameterization/Manifold applied already; for example, the jacobian
  // for a 4-dimensional quaternion parameter using the
  // "QuaternionParameterization" is num_residuals by 3 instead of num_residuals
  // by 4.
  //
  // apply_loss_function as the name implies allows the user to switch the
  // application of the loss function on and off.
  //
  // If an EvaluationCallback is associated with the problem, then its
  // PrepareForEvaluation method will be called every time this method is called
  // with new_point = true. This conservatively assumes that the user may have
  // changed the parameter values since the previous call to evaluate / solve.
  // For improved efficiency, and only if you know that the parameter values
  // have not changed between calls, see
  // EvaluateResidualBlockAssumingParametersUnchanged().
  bool EvaluateResidualBlock(ResidualBlockId residual_block_id,
                             bool apply_loss_function,
                             double* cost,
                             double* residuals,
                             double** jacobians) const;

  // Same as EvaluateResidualBlock except that if an EvaluationCallback is
  // associated with the problem, then its PrepareForEvaluation method will be
  // called every time this method is called with new_point = false.
  //
  // This means, if an EvaluationCallback is associated with the problem then it
  // is the user's responsibility to call PrepareForEvaluation before calling
  // this method if necessary, i.e. iff the parameter values have been changed
  // since the last call to evaluate / solve.'
  //
  // This is because, as the name implies, we assume that the parameter blocks
  // did not change since the last time PrepareForEvaluation was called (via
  // Solve, Evaluate or EvaluateResidualBlock).
  bool EvaluateResidualBlockAssumingParametersUnchanged(
      ResidualBlockId residual_block_id,
      bool apply_loss_function,
      double* cost,
      double* residuals,
      double** jacobians) const;

 private:
  friend class Solver;
  friend class Covariance;
  std::unique_ptr<internal::ProblemImpl> impl_;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_PROBLEM_H_
