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

#ifndef CERES_INTERNAL_DETECT_STRUCTURE_H_
#define CERES_INTERNAL_DETECT_STRUCTURE_H_

#include "ceres/block_structure.h"

namespace ceres {
namespace internal {

// Detect static blocks in the problem sparsity. For rows containing
// e_blocks, we are interested in detecting if the size of the row
// blocks, e_blocks and the f_blocks remain constant. If they do, then
// we can use template specialization to improve the performance of
// the block level linear algebra operations used by the
// SchurEliminator.
//
// If a block size is not constant, we return Eigen::Dynamic as the
// value. This just means that the eliminator uses dynamically sized
// linear algebra operations rather than static operations whose size
// is known as compile time.
//
// For more details about e_blocks and f_blocks, see
// schur_eliminator.h. This information is used to initialized an
// appropriate template specialization of SchurEliminator.
void DetectStructure(const CompressedRowBlockStructure& bs,
                     const int num_eliminate_blocks,
                     int* row_block_size,
                     int* e_block_size,
                     int* f_block_size);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_DETECT_STRUCTURE_H_
