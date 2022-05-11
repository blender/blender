// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2019 Google Inc. All rights reserved.
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

#ifndef CERES_PUBLIC_CRS_MATRIX_H_
#define CERES_PUBLIC_CRS_MATRIX_H_

#include <vector>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"

namespace ceres {

// A compressed row sparse matrix used primarily for communicating the
// Jacobian matrix to the user.
struct CERES_EXPORT CRSMatrix {
  CRSMatrix() = default;

  int num_rows{0};
  int num_cols{0};

  // A compressed row matrix stores its contents in three arrays,
  // rows, cols and values.
  //
  // rows is a num_rows + 1 sized array that points into the cols and
  // values array. For each row i:
  //
  // cols[rows[i]] ... cols[rows[i + 1] - 1] are the indices of the
  // non-zero columns of row i.
  //
  // values[rows[i]] .. values[rows[i + 1] - 1] are the values of the
  // corresponding entries.
  //
  // cols and values contain as many entries as there are non-zeros in
  // the matrix.
  //
  // e.g, consider the 3x4 sparse matrix
  //
  //  [ 0 10  0  4 ]
  //  [ 0  2 -3  2 ]
  //  [ 1  2  0  0 ]
  //
  // The three arrays will be:
  //
  //
  //            -row0-  ---row1---  -row2-
  //  rows   = [ 0,      2,          5,     7]
  //  cols   = [ 1,  3,  1,  2,  3,  0,  1]
  //  values = [10,  4,  2, -3,  2,  1,  2]

  std::vector<int> cols;
  std::vector<int> rows;
  std::vector<double> values;
};

}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_PUBLIC_CRS_MATRIX_H_
