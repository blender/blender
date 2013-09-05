// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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
// Author: mierle@gmail.com (Keir Mierle)
//
// An incomplete C API for Ceres.
//
// TODO(keir): Figure out why logging does not seem to work.

#include "ceres/c_api.h"

#include <vector>
#include <iostream>
#include <string>
#include "ceres/cost_function.h"
#include "ceres/loss_function.h"
#include "ceres/problem.h"
#include "ceres/solver.h"
#include "ceres/types.h"  // for std
#include "glog/logging.h"

using ceres::Problem;

void ceres_init() {
  // This is not ideal, but it's not clear what to do if there is no gflags and
  // no access to command line arguments.
  char message[] = "<unknown>";
  google::InitGoogleLogging(message);
}

ceres_problem_t* ceres_create_problem() {
  return reinterpret_cast<ceres_problem_t*>(new Problem);
}

void ceres_free_problem(ceres_problem_t* problem) {
  delete reinterpret_cast<Problem*>(problem);
}

// This cost function wraps a C-level function pointer from the user, to bridge
// between C and C++.
class CallbackCostFunction : public ceres::CostFunction {
 public:
  CallbackCostFunction(ceres_cost_function_t cost_function,
                       void* user_data,
                       int num_residuals,
                       int num_parameter_blocks,
                       int* parameter_block_sizes)
      : cost_function_(cost_function),
        user_data_(user_data) {
    set_num_residuals(num_residuals);
    for (int i = 0; i < num_parameter_blocks; ++i) {
      mutable_parameter_block_sizes()->push_back(parameter_block_sizes[i]);
    }
  }

  virtual ~CallbackCostFunction() {}

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const {
    return (*cost_function_)(user_data_,
                             const_cast<double**>(parameters),
                             residuals,
                             jacobians);
  }

 private:
  ceres_cost_function_t cost_function_;
  void* user_data_;
};

// This loss function wraps a C-level function pointer from the user, to bridge
// between C and C++.
class CallbackLossFunction : public ceres::LossFunction {
 public:
  explicit CallbackLossFunction(ceres_loss_function_t loss_function,
                                void* user_data)
    : loss_function_(loss_function), user_data_(user_data) {}
  virtual void Evaluate(double sq_norm, double* rho) const {
    (*loss_function_)(user_data_, sq_norm, rho);
  }

 private:
  ceres_loss_function_t loss_function_;
  void* user_data_;
};

// Wrappers for the stock loss functions.
void* ceres_create_huber_loss_function_data(double a) {
  return new ceres::HuberLoss(a);
}
void* ceres_create_softl1_loss_function_data(double a) {
  return new ceres::SoftLOneLoss(a);
}
void* ceres_create_cauchy_loss_function_data(double a) {
  return new ceres::CauchyLoss(a);
}
void* ceres_create_arctan_loss_function_data(double a) {
  return new ceres::ArctanLoss(a);
}
void* ceres_create_tolerant_loss_function_data(double a, double b) {
  return new ceres::TolerantLoss(a, b);
}

void ceres_free_stock_loss_function_data(void* loss_function_data) {
  delete reinterpret_cast<ceres::LossFunction*>(loss_function_data);
}

void ceres_stock_loss_function(void* user_data,
                               double squared_norm,
                               double out[3]) {
  reinterpret_cast<ceres::LossFunction*>(user_data)
      ->Evaluate(squared_norm, out);
}

ceres_residual_block_id_t* ceres_problem_add_residual_block(
    ceres_problem_t* problem,
    ceres_cost_function_t cost_function,
    void* cost_function_data,
    ceres_loss_function_t loss_function,
    void* loss_function_data,
    int num_residuals,
    int num_parameter_blocks,
    int* parameter_block_sizes,
    double** parameters) {
  Problem* ceres_problem = reinterpret_cast<Problem*>(problem);

  ceres::CostFunction* callback_cost_function =
      new CallbackCostFunction(cost_function,
                               cost_function_data,
                               num_residuals,
                               num_parameter_blocks,
                               parameter_block_sizes);

  ceres::LossFunction* callback_loss_function = NULL;
  if (loss_function != NULL) {
    callback_loss_function = new CallbackLossFunction(loss_function,
                                                      loss_function_data);
  }

  std::vector<double*> parameter_blocks(parameters,
                                        parameters + num_parameter_blocks);
  return reinterpret_cast<ceres_residual_block_id_t*>(
      ceres_problem->AddResidualBlock(callback_cost_function,
                                      callback_loss_function,
                                      parameter_blocks));
}

void ceres_solve(ceres_problem_t* c_problem) {
  Problem* problem = reinterpret_cast<Problem*>(c_problem);

  // TODO(keir): Obviously, this way of setting options won't scale or last.
  // Instead, figure out a way to specify some of the options without
  // duplicating everything.
  ceres::Solver::Options options;
  options.max_num_iterations = 100;
  options.linear_solver_type = ceres::DENSE_QR;
  options.minimizer_progress_to_stdout = true;

  ceres::Solver::Summary summary;
  ceres::Solve(options, problem, &summary);
  std::cout << summary.FullReport() << "\n";
}
