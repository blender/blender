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
// Author: keir@google.com (Keir Mierle)

#include <glog/logging.h>
#include "ceres/evaluator.h"
#include "ceres/block_evaluate_preparer.h"
#include "ceres/block_jacobian_writer.h"
#include "ceres/compressed_row_jacobian_writer.h"
#include "ceres/scratch_evaluate_preparer.h"
#include "ceres/dense_jacobian_writer.h"
#include "ceres/program_evaluator.h"

namespace ceres {
namespace internal {

Evaluator::~Evaluator() {}

Evaluator* Evaluator::Create(const Evaluator::Options& options,
                             Program* program,
                             string* error) {
  switch (options.linear_solver_type) {
    case DENSE_QR:
      return new ProgramEvaluator<ScratchEvaluatePreparer,
                                  DenseJacobianWriter>(options,
                                                       program);
    case DENSE_SCHUR:
    case SPARSE_SCHUR:
    case ITERATIVE_SCHUR:
    case CGNR:
      return new ProgramEvaluator<BlockEvaluatePreparer,
                                  BlockJacobianWriter>(options,
                                                       program);
    case SPARSE_NORMAL_CHOLESKY:
      return new ProgramEvaluator<ScratchEvaluatePreparer,
                                  CompressedRowJacobianWriter>(options,
                                                               program);
    default:
      *error = "Invalid Linear Solver Type. Unable to create evaluator.";
      return NULL;
  }
}

}  // namespace internal
}  // namespace ceres
