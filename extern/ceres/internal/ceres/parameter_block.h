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
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_PARAMETER_BLOCK_H_
#define CERES_INTERNAL_PARAMETER_BLOCK_H_

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>
#include "ceres/array_utils.h"
#include "ceres/collections_port.h"
#include "ceres/integral_types.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/local_parameterization.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

class ProblemImpl;
class ResidualBlock;

// The parameter block encodes the location of the user's original value, and
// also the "current state" of the parameter. The evaluator uses whatever is in
// the current state of the parameter when evaluating. This is inlined since the
// methods are performance sensitive.
//
// The class is not thread-safe, unless only const methods are called. The
// parameter block may also hold a pointer to a local parameterization; the
// parameter block does not take ownership of this pointer, so the user is
// responsible for the proper disposal of the local parameterization.
class ParameterBlock {
 public:
  // TODO(keir): Decide what data structure is best here. Should this be a set?
  // Probably not, because sets are memory inefficient. However, if it's a
  // vector, you can get into pathological linear performance when removing a
  // residual block from a problem where all the residual blocks depend on one
  // parameter; for example, shared focal length in a bundle adjustment
  // problem. It might be worth making a custom structure that is just an array
  // when it is small, but transitions to a hash set when it has more elements.
  //
  // For now, use a hash set.
  typedef HashSet<ResidualBlock*> ResidualBlockSet;

  // Create a parameter block with the user state, size, and index specified.
  // The size is the size of the parameter block and the index is the position
  // of the parameter block inside a Program (if any).
  ParameterBlock(double* user_state, int size, int index) {
    Init(user_state, size, index, NULL);
  }

  ParameterBlock(double* user_state,
                 int size,
                 int index,
                 LocalParameterization* local_parameterization) {
    Init(user_state, size, index, local_parameterization);
  }

  // The size of the parameter block.
  int Size() const { return size_; }

  // Manipulate the parameter state.
  bool SetState(const double* x) {
    CHECK(x != NULL)
        << "Tried to set the state of constant parameter "
        << "with user location " << user_state_;
    CHECK(!is_constant_)
        << "Tried to set the state of constant parameter "
        << "with user location " << user_state_;

    state_ = x;
    return UpdateLocalParameterizationJacobian();
  }

  // Copy the current parameter state out to x. This is "GetState()" rather than
  // simply "state()" since it is actively copying the data into the passed
  // pointer.
  void GetState(double *x) const {
    if (x != state_) {
      memcpy(x, state_, sizeof(*state_) * size_);
    }
  }

  // Direct pointers to the current state.
  const double* state() const { return state_; }
  const double* user_state() const { return user_state_; }
  double* mutable_user_state() { return user_state_; }
  LocalParameterization* local_parameterization() const {
    return local_parameterization_;
  }
  LocalParameterization* mutable_local_parameterization() {
    return local_parameterization_;
  }

  // Set this parameter block to vary or not.
  void SetConstant() { is_constant_ = true; }
  void SetVarying() { is_constant_ = false; }
  bool IsConstant() const { return is_constant_; }

  // This parameter block's index in an array.
  int index() const { return index_; }
  void set_index(int index) { index_ = index; }

  // This parameter offset inside a larger state vector.
  int state_offset() const { return state_offset_; }
  void set_state_offset(int state_offset) { state_offset_ = state_offset; }

  // This parameter offset inside a larger delta vector.
  int delta_offset() const { return delta_offset_; }
  void set_delta_offset(int delta_offset) { delta_offset_ = delta_offset; }

  // Methods relating to the parameter block's parameterization.

  // The local to global jacobian. Returns NULL if there is no local
  // parameterization for this parameter block. The returned matrix is row-major
  // and has Size() rows and  LocalSize() columns.
  const double* LocalParameterizationJacobian() const {
    return local_parameterization_jacobian_.get();
  }

  int LocalSize() const {
    return (local_parameterization_ == NULL)
        ? size_
        : local_parameterization_->LocalSize();
  }

  // Set the parameterization. The parameterization can be set exactly once;
  // multiple calls to set the parameterization to different values will crash.
  // It is an error to pass NULL for the parameterization. The parameter block
  // does not take ownership of the parameterization.
  void SetParameterization(LocalParameterization* new_parameterization) {
    CHECK(new_parameterization != NULL) << "NULL parameterization invalid.";
    // Nothing to do if the new parameterization is the same as the
    // old parameterization.
    if (new_parameterization == local_parameterization_) {
      return;
    }

    CHECK(local_parameterization_ == NULL)
        << "Can't re-set the local parameterization; it leads to "
        << "ambiguous ownership. Current local parameterization is: "
        << local_parameterization_;

    CHECK(new_parameterization->GlobalSize() == size_)
        << "Invalid parameterization for parameter block. The parameter block "
        << "has size " << size_ << " while the parameterization has a global "
        << "size of " << new_parameterization->GlobalSize() << ". Did you "
        << "accidentally use the wrong parameter block or parameterization?";

    CHECK_GT(new_parameterization->LocalSize(), 0)
        << "Invalid parameterization. Parameterizations must have a positive "
        << "dimensional tangent space.";

    local_parameterization_ = new_parameterization;
    local_parameterization_jacobian_.reset(
        new double[local_parameterization_->GlobalSize() *
                   local_parameterization_->LocalSize()]);
    CHECK(UpdateLocalParameterizationJacobian())
        << "Local parameterization Jacobian computation failed for x: "
        << ConstVectorRef(state_, Size()).transpose();
  }

  void SetUpperBound(int index, double upper_bound) {
    CHECK_LT(index, size_);

    if (upper_bounds_.get() == NULL) {
      upper_bounds_.reset(new double[size_]);
      std::fill(upper_bounds_.get(),
                upper_bounds_.get() + size_,
                std::numeric_limits<double>::max());
    }

    upper_bounds_[index] = upper_bound;
  }

  void SetLowerBound(int index, double lower_bound) {
    CHECK_LT(index, size_);

    if (lower_bounds_.get() == NULL) {
      lower_bounds_.reset(new double[size_]);
      std::fill(lower_bounds_.get(),
                lower_bounds_.get() + size_,
                -std::numeric_limits<double>::max());
    }

    lower_bounds_[index] = lower_bound;
  }

  // Generalization of the addition operation. This is the same as
  // LocalParameterization::Plus() followed by projection onto the
  // hyper cube implied by the bounds constraints.
  bool Plus(const double *x, const double* delta, double* x_plus_delta) {
    if (local_parameterization_ != NULL) {
      if (!local_parameterization_->Plus(x, delta, x_plus_delta)) {
        return false;
      }
    } else {
      VectorRef(x_plus_delta, size_) = ConstVectorRef(x, size_) +
                                       ConstVectorRef(delta,  size_);
    }

    // Project onto the box constraints.
    if (lower_bounds_.get() != NULL) {
      for (int i = 0; i < size_; ++i) {
        x_plus_delta[i] = std::max(x_plus_delta[i], lower_bounds_[i]);
      }
    }

    if (upper_bounds_.get() != NULL) {
      for (int i = 0; i < size_; ++i) {
        x_plus_delta[i] = std::min(x_plus_delta[i], upper_bounds_[i]);
      }
    }

    return true;
  }

  std::string ToString() const {
    return StringPrintf("{ this=%p, user_state=%p, state=%p, size=%d, "
                        "constant=%d, index=%d, state_offset=%d, "
                        "delta_offset=%d }",
                        this,
                        user_state_,
                        state_,
                        size_,
                        is_constant_,
                        index_,
                        state_offset_,
                        delta_offset_);
  }

  void EnableResidualBlockDependencies() {
    CHECK(residual_blocks_.get() == NULL)
        << "Ceres bug: There is already a residual block collection "
        << "for parameter block: " << ToString();
    residual_blocks_.reset(new ResidualBlockSet);
  }

  void AddResidualBlock(ResidualBlock* residual_block) {
    CHECK(residual_blocks_.get() != NULL)
        << "Ceres bug: The residual block collection is null for parameter "
        << "block: " << ToString();
    residual_blocks_->insert(residual_block);
  }

  void RemoveResidualBlock(ResidualBlock* residual_block) {
    CHECK(residual_blocks_.get() != NULL)
        << "Ceres bug: The residual block collection is null for parameter "
        << "block: " << ToString();
    CHECK(residual_blocks_->find(residual_block) != residual_blocks_->end())
        << "Ceres bug: Missing residual for parameter block: " << ToString();
    residual_blocks_->erase(residual_block);
  }

  // This is only intended for iterating; perhaps this should only expose
  // .begin() and .end().
  ResidualBlockSet* mutable_residual_blocks() {
    return residual_blocks_.get();
  }

  double LowerBoundForParameter(int index) const {
    if (lower_bounds_.get() == NULL) {
      return -std::numeric_limits<double>::max();
    } else {
      return lower_bounds_[index];
    }
  }

  double UpperBoundForParameter(int index) const {
    if (upper_bounds_.get() == NULL) {
      return std::numeric_limits<double>::max();
    } else {
      return upper_bounds_[index];
    }
  }

 private:
  void Init(double* user_state,
            int size,
            int index,
            LocalParameterization* local_parameterization) {
    user_state_ = user_state;
    size_ = size;
    index_ = index;
    is_constant_ = false;
    state_ = user_state_;

    local_parameterization_ = NULL;
    if (local_parameterization != NULL) {
      SetParameterization(local_parameterization);
    }

    state_offset_ = -1;
    delta_offset_ = -1;
  }

  bool UpdateLocalParameterizationJacobian() {
    if (local_parameterization_ == NULL) {
      return true;
    }

    // Update the local to global Jacobian. In some cases this is
    // wasted effort; if this is a bottleneck, we will find a solution
    // at that time.

    const int jacobian_size = Size() * LocalSize();
    InvalidateArray(jacobian_size,
                    local_parameterization_jacobian_.get());
    if (!local_parameterization_->ComputeJacobian(
            state_,
            local_parameterization_jacobian_.get())) {
      LOG(WARNING) << "Local parameterization Jacobian computation failed"
          "for x: " << ConstVectorRef(state_, Size()).transpose();
      return false;
    }

    if (!IsArrayValid(jacobian_size, local_parameterization_jacobian_.get())) {
      LOG(WARNING) << "Local parameterization Jacobian computation returned"
                   << "an invalid matrix for x: "
                   << ConstVectorRef(state_, Size()).transpose()
                   << "\n Jacobian matrix : "
                   << ConstMatrixRef(local_parameterization_jacobian_.get(),
                                     Size(),
                                     LocalSize());
      return false;
    }
    return true;
  }

  double* user_state_;
  int size_;
  bool is_constant_;
  LocalParameterization* local_parameterization_;

  // The "state" of the parameter. These fields are only needed while the
  // solver is running. While at first glance using mutable is a bad idea, this
  // ends up simplifying the internals of Ceres enough to justify the potential
  // pitfalls of using "mutable."
  mutable const double* state_;
  mutable scoped_array<double> local_parameterization_jacobian_;

  // The index of the parameter. This is used by various other parts of Ceres to
  // permit switching from a ParameterBlock* to an index in another array.
  int32 index_;

  // The offset of this parameter block inside a larger state vector.
  int32 state_offset_;

  // The offset of this parameter block inside a larger delta vector.
  int32 delta_offset_;

  // If non-null, contains the residual blocks this parameter block is in.
  scoped_ptr<ResidualBlockSet> residual_blocks_;

  // Upper and lower bounds for the parameter block.  SetUpperBound
  // and SetLowerBound lazily initialize the upper_bounds_ and
  // lower_bounds_ arrays. If they are never called, then memory for
  // these arrays is never allocated. Thus for problems where there
  // are no bounds, or only one sided bounds we do not pay the cost of
  // allocating memory for the inactive bounds constraints.
  //
  // Upon initialization these arrays are initialized to
  // std::numeric_limits<double>::max() and
  // -std::numeric_limits<double>::max() respectively which correspond
  // to the parameter block being unconstrained.
  scoped_array<double> upper_bounds_;
  scoped_array<double> lower_bounds_;

  // Necessary so ProblemImpl can clean up the parameterizations.
  friend class ProblemImpl;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PARAMETER_BLOCK_H_
