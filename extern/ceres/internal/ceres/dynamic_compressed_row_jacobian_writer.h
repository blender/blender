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
// Author: richie.stebbing@gmail.com (Richard Stebbing)
//
// A jacobian writer that directly writes to dynamic compressed row sparse
// matrices.

#ifndef CERES_INTERNAL_DYNAMIC_COMPRESSED_ROW_JACOBIAN_WRITER_H_
#define CERES_INTERNAL_DYNAMIC_COMPRESSED_ROW_JACOBIAN_WRITER_H_

#include <memory>

#include "ceres/evaluator.h"
#include "ceres/internal/export.h"
#include "ceres/scratch_evaluate_preparer.h"

namespace ceres::internal {

class Program;
class SparseMatrix;

class CERES_NO_EXPORT DynamicCompressedRowJacobianWriter {
 public:
  DynamicCompressedRowJacobianWriter(Evaluator::Options /* ignored */,
                                     Program* program)
      : program_(program) {}

  // JacobianWriter interface.

  // The compressed row matrix has different layout than that assumed by
  // the cost functions. The scratch space is therefore used to store
  // the jacobians (including zeros) temporarily before only the non-zero
  // entries are copied over to the larger jacobian in `Write`.
  std::unique_ptr<ScratchEvaluatePreparer[]> CreateEvaluatePreparers(
      int num_threads);

  // Return a `DynamicCompressedRowSparseMatrix` which is filled by
  // `Write`. Note that `Finalize` must be called to make the
  // `CompressedRowSparseMatrix` interface valid.
  std::unique_ptr<SparseMatrix> CreateJacobian() const;

  // Write only the non-zero jacobian entries for a residual block
  // (specified by `residual_id`) into `base_jacobian`, starting at the row
  // specified by `residual_offset`.
  //
  // This method is thread-safe over residual blocks (each `residual_id`).
  void Write(int residual_id,
             int residual_offset,
             double** jacobians,
             SparseMatrix* base_jacobian);

 private:
  Program* program_;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_DYNAMIC_COMPRESSED_ROW_JACOBIAN_WRITER_H_
