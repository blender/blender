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

#ifndef CERES_INTERNAL_PARAMETER_BLOCK_ORDERING_H_
#define CERES_INTERNAL_PARAMETER_BLOCK_ORDERING_H_

#include <vector>
#include "ceres/ordered_groups.h"
#include "ceres/graph.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class Program;
class ParameterBlock;

// Uses an approximate independent set ordering to order the parameter
// blocks of a problem so that it is suitable for use with Schur
// complement based solvers. The output variable ordering contains an
// ordering of the parameter blocks and the return value is size of
// the independent set or the number of e_blocks (see
// schur_complement_solver.h for an explanation). Constant parameters
// are added to the end.
//
// The ordering vector has the structure
//
// ordering = [independent set,
//             complement of the independent set,
//             fixed blocks]
int ComputeSchurOrdering(const Program& program,
                         vector<ParameterBlock* >* ordering);

// Same as above, except that ties while computing the independent set
// ordering are resolved in favour of the order in which the parameter
// blocks occur in the program.
int ComputeStableSchurOrdering(const Program& program,
                               vector<ParameterBlock* >* ordering);

// Use an approximate independent set ordering to decompose the
// parameter blocks of a problem in a sequence of independent
// sets. The ordering covers all the non-constant parameter blocks in
// the program.
void ComputeRecursiveIndependentSetOrdering(const Program& program,
                                            ParameterBlockOrdering* ordering);

// Builds a graph on the parameter blocks of a Problem, whose
// structure reflects the sparsity structure of the Hessian. Each
// vertex corresponds to a parameter block in the Problem except for
// parameter blocks that are marked constant. An edge connects two
// parameter blocks, if they co-occur in a residual block.
Graph<ParameterBlock*>* CreateHessianGraph(const Program& program);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PARAMETER_BLOCK_ORDERING_H_
