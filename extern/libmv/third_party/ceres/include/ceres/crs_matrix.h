// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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

#ifndef CERES_PUBLIC_CRS_MATRIX_H_
#define CERES_PUBLIC_CRS_MATRIX_H_

#include <vector>
#include "ceres/internal/port.h"

namespace ceres {

// A compressed row sparse matrix used primarily for communicating the
// Jacobian matrix to the user.
struct CRSMatrix {
  CRSMatrix() : num_rows(0), num_cols(0) {}

  int num_rows;
  int num_cols;

  // A compressed row matrix stores its contents in three arrays.
  // The non-zero pattern of the i^th row is given by
  //
  //   rows[cols[i] ... cols[i + 1]]
  //
  // and the corresponding values by
  //
  //   values[cols[i] ... cols[i + 1]]
  //
  // Thus, cols is a vector of size num_cols + 1, and rows and values
  // have as many entries as number of non-zeros in the matrix.
  vector<int> cols;
  vector<int> rows;
  vector<double> values;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_CRS_MATRIX_H_
