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
//
// The LossFunction interface is the way users describe how residuals
// are converted to cost terms for the overall problem cost function.
// For the exact manner in which loss functions are converted to the
// overall cost for a problem, see problem.h.
//
// For least squares problem where there are no outliers and standard
// squared loss is expected, it is not necessary to create a loss
// function; instead passing a NULL to the problem when adding
// residuals implies a standard squared loss.
//
// For least squares problems where the minimization may encounter
// input terms that contain outliers, that is, completely bogus
// measurements, it is important to use a loss function that reduces
// their associated penalty.
//
// Consider a structure from motion problem. The unknowns are 3D
// points and camera parameters, and the measurements are image
// coordinates describing the expected reprojected position for a
// point in a camera. For example, we want to model the geometry of a
// street scene with fire hydrants and cars, observed by a moving
// camera with unknown parameters, and the only 3D points we care
// about are the pointy tippy-tops of the fire hydrants. Our magic
// image processing algorithm, which is responsible for producing the
// measurements that are input to Ceres, has found and matched all
// such tippy-tops in all image frames, except that in one of the
// frame it mistook a car's headlight for a hydrant. If we didn't do
// anything special (i.e. if we used a basic quadratic loss), the
// residual for the erroneous measurement will result in extreme error
// due to the quadratic nature of squared loss. This results in the
// entire solution getting pulled away from the optimimum to reduce
// the large error that would otherwise be attributed to the wrong
// measurement.
//
// Using a robust loss function, the cost for large residuals is
// reduced. In the example above, this leads to outlier terms getting
// downweighted so they do not overly influence the final solution.
//
// What cost function is best?
//
// In general, there isn't a principled way to select a robust loss
// function. The authors suggest starting with a non-robust cost, then
// only experimenting with robust loss functions if standard squared
// loss doesn't work.

#ifndef CERES_PUBLIC_LOSS_FUNCTION_H_
#define CERES_PUBLIC_LOSS_FUNCTION_H_

#include "ceres/internal/macros.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

class LossFunction {
 public:
  virtual ~LossFunction() {}

  // For a residual vector with squared 2-norm 'sq_norm', this method
  // is required to fill in the value and derivatives of the loss
  // function (rho in this example):
  //
  //   out[0] = rho(sq_norm),
  //   out[1] = rho'(sq_norm),
  //   out[2] = rho''(sq_norm),
  //
  // Here the convention is that the contribution of a term to the
  // cost function is given by 1/2 rho(s),  where
  //
  //   s = ||residuals||^2.
  //
  // Calling the method with a negative value of 's' is an error and
  // the implementations are not required to handle that case.
  //
  // Most sane choices of rho() satisfy:
  //
  //   rho(0) = 0,
  //   rho'(0) = 1,
  //   rho'(s) < 1 in outlier region,
  //   rho''(s) < 0 in outlier region,
  //
  // so that they mimic the least squares cost for small residuals.
  virtual void Evaluate(double sq_norm, double out[3]) const = 0;
};

// Some common implementations follow below.
//
// Note: in the region of interest (i.e. s < 3) we have:
//   TrivialLoss >= HuberLoss >= SoftLOneLoss >= CauchyLoss


// This corresponds to no robustification.
//
//   rho(s) = s
//
// At s = 0: rho = [0, 1, 0].
//
// It is not normally necessary to use this, as passing NULL for the
// loss function when building the problem accomplishes the same
// thing.
class TrivialLoss : public LossFunction {
 public:
  virtual void Evaluate(double, double*) const;
};

// Scaling
// -------
// Given one robustifier
//   s -> rho(s)
// one can change the length scale at which robustification takes
// place, by adding a scale factor 'a' as follows:
//
//   s -> a^2 rho(s / a^2).
//
// The first and second derivatives are:
//
//   s -> rho'(s / a^2),
//   s -> (1 / a^2) rho''(s / a^2),
//
// but the behaviour near s = 0 is the same as the original function,
// i.e.
//
//   rho(s) = s + higher order terms,
//   a^2 rho(s / a^2) = s + higher order terms.
//
// The scalar 'a' should be positive.
//
// The reason for the appearance of squaring is that 'a' is in the
// units of the residual vector norm whereas 's' is a squared
// norm. For applications it is more convenient to specify 'a' than
// its square. The commonly used robustifiers below are described in
// un-scaled format (a = 1) but their implementations work for any
// non-zero value of 'a'.

// Huber.
//
//   rho(s) = s               for s <= 1,
//   rho(s) = 2 sqrt(s) - 1   for s >= 1.
//
// At s = 0: rho = [0, 1, 0].
//
// The scaling parameter 'a' corresponds to 'delta' on this page:
//   http://en.wikipedia.org/wiki/Huber_Loss_Function
class HuberLoss : public LossFunction {
 public:
  explicit HuberLoss(double a) : a_(a), b_(a * a) { }
  virtual void Evaluate(double, double*) const;

 private:
  const double a_;
  // b = a^2.
  const double b_;
};

// Soft L1, similar to Huber but smooth.
//
//   rho(s) = 2 (sqrt(1 + s) - 1).
//
// At s = 0: rho = [0, 1, -1/2].
class SoftLOneLoss : public LossFunction {
 public:
  explicit SoftLOneLoss(double a) : b_(a * a), c_(1 / b_) { }
  virtual void Evaluate(double, double*) const;

 private:
  // b = a^2.
  const double b_;
  // c = 1 / a^2.
  const double c_;
};

// Inspired by the Cauchy distribution
//
//   rho(s) = log(1 + s).
//
// At s = 0: rho = [0, 1, -1].
class CauchyLoss : public LossFunction {
 public:
  explicit CauchyLoss(double a) : b_(a * a), c_(1 / b_) { }
  virtual void Evaluate(double, double*) const;

 private:
  // b = a^2.
  const double b_;
  // c = 1 / a^2.
  const double c_;
};

// Loss that is capped beyond a certain level using the arc-tangent function.
// The scaling parameter 'a' determines the level where falloff occurs.
// For costs much smaller than 'a', the loss function is linear and behaves like
// TrivialLoss, and for values much larger than 'a' the value asymptotically
// approaches the constant value of a * PI / 2.
//
//   rho(s) = a atan(s / a).
//
// At s = 0: rho = [0, 1, 0].
class ArctanLoss : public LossFunction {
 public:
  explicit ArctanLoss(double a) : a_(a), b_(1 / (a * a)) { }
  virtual void Evaluate(double, double*) const;

 private:
  const double a_;
  // b = 1 / a^2.
  const double b_;
};

// Loss function that maps to approximately zero cost in a range around the
// origin, and reverts to linear in error (quadratic in cost) beyond this range.
// The tolerance parameter 'a' sets the nominal point at which the
// transition occurs, and the transition size parameter 'b' sets the nominal
// distance over which most of the transition occurs. Both a and b must be
// greater than zero, and typically b will be set to a fraction of a.
// The slope rho'[s] varies smoothly from about 0 at s <= a - b to
// about 1 at s >= a + b.
//
// The term is computed as:
//
//   rho(s) = b log(1 + exp((s - a) / b)) - c0.
//
// where c0 is chosen so that rho(0) == 0
//
//   c0 = b log(1 + exp(-a / b)
//
// This has the following useful properties:
//
//   rho(s) == 0               for s = 0
//   rho'(s) ~= 0              for s << a - b
//   rho'(s) ~= 1              for s >> a + b
//   rho''(s) > 0              for all s
//
// In addition, all derivatives are continuous, and the curvature is
// concentrated in the range a - b to a + b.
//
// At s = 0: rho = [0, ~0, ~0].
class TolerantLoss : public LossFunction {
 public:
  explicit TolerantLoss(double a, double b);
  virtual void Evaluate(double, double*) const;

 private:
  const double a_, b_, c_;
};

// Composition of two loss functions.  The error is the result of first
// evaluating g followed by f to yield the composition f(g(s)).
// The loss functions must not be NULL.
class ComposedLoss : public LossFunction {
 public:
  explicit ComposedLoss(const LossFunction* f, Ownership ownership_f,
                        const LossFunction* g, Ownership ownership_g);
  virtual ~ComposedLoss();
  virtual void Evaluate(double, double*) const;

 private:
  internal::scoped_ptr<const LossFunction> f_, g_;
  const Ownership ownership_f_, ownership_g_;
};

// The discussion above has to do with length scaling: it affects the space
// in which s is measured. Sometimes you want to simply scale the output
// value of the robustifier. For example, you might want to weight
// different error terms differently (e.g., weight pixel reprojection
// errors differently from terrain errors).
//
// If rho is the wrapped robustifier, then this simply outputs
// s -> a * rho(s)
//
// The first and second derivatives are, not surprisingly
// s -> a * rho'(s)
// s -> a * rho''(s)
//
// Since we treat the a NULL Loss function as the Identity loss
// function, rho = NULL is a valid input and will result in the input
// being scaled by a. This provides a simple way of implementing a
// scaled ResidualBlock.
class ScaledLoss : public LossFunction {
 public:
  // Constructs a ScaledLoss wrapping another loss function. Takes
  // ownership of the wrapped loss function or not depending on the
  // ownership parameter.
  ScaledLoss(const LossFunction* rho, double a, Ownership ownership) :
      rho_(rho), a_(a), ownership_(ownership) { }

  virtual ~ScaledLoss() {
    if (ownership_ == DO_NOT_TAKE_OWNERSHIP) {
      rho_.release();
    }
  }
  virtual void Evaluate(double, double*) const;

 private:
  internal::scoped_ptr<const LossFunction> rho_;
  const double a_;
  const Ownership ownership_;
  CERES_DISALLOW_COPY_AND_ASSIGN(ScaledLoss);
};

// Sometimes after the optimization problem has been constructed, we
// wish to mutate the scale of the loss function. For example, when
// performing estimation from data which has substantial outliers,
// convergence can be improved by starting out with a large scale,
// optimizing the problem and then reducing the scale. This can have
// better convergence behaviour than just using a loss function with a
// small scale.
//
// This templated class allows the user to implement a loss function
// whose scale can be mutated after an optimization problem has been
// constructed.
//
// Example usage
//
//  Problem problem;
//
//  // Add parameter blocks
//
//  CostFunction* cost_function =
//    new AutoDiffCostFunction < UW_Camera_Mapper, 2, 9, 3>(
//      new UW_Camera_Mapper(feature_x, feature_y));
//
//  LossFunctionWrapper* loss_function(new HuberLoss(1.0), TAKE_OWNERSHIP);
//
//  problem.AddResidualBlock(cost_function, loss_function, parameters);
//
//  Solver::Options options;
//  Solger::Summary summary;
//
//  Solve(options, &problem, &summary)
//
//  loss_function->Reset(new HuberLoss(1.0), TAKE_OWNERSHIP);
//
//  Solve(options, &problem, &summary)
//
class LossFunctionWrapper : public LossFunction {
 public:
  LossFunctionWrapper(LossFunction* rho, Ownership ownership)
      : rho_(rho), ownership_(ownership) {
  }

  virtual ~LossFunctionWrapper() {
    if (ownership_ == DO_NOT_TAKE_OWNERSHIP) {
      rho_.release();
    }
  }

  virtual void Evaluate(double sq_norm, double out[3]) const {
    CHECK_NOTNULL(rho_.get());
    rho_->Evaluate(sq_norm, out);
  }

  void Reset(LossFunction* rho, Ownership ownership) {
    if (ownership_ == DO_NOT_TAKE_OWNERSHIP) {
      rho_.release();
    }
    rho_.reset(rho);
    ownership_ = ownership;
  }

 private:
  internal::scoped_ptr<const LossFunction> rho_;
  Ownership ownership_;
  CERES_DISALLOW_COPY_AND_ASSIGN(LossFunctionWrapper);
};

}  // namespace ceres

#endif  // CERES_PUBLIC_LOSS_FUNCTION_H_
