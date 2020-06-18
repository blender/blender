// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
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
// Author: wjr@google.com (William Rucklidge)

#include "ceres/parallel_utils.h"

namespace ceres {
namespace internal {

void LinearIndexToUpperTriangularIndex(int k, int n, int* i, int* j) {
  // This works by unfolding a rectangle into a triangle.
  // Say n is even. 4 is a nice even number. The 10 i,j pairs that we
  // want to produce are:
  // 0,0 0,1 0,2 0,3
  //     1,1 1,2 1,3
  //         2,2 2,3
  //             3,3
  // This triangle can be folded into a 5x2 rectangle:
  // 3,3 0,0 0,1 0,2 0,3
  // 2,2 2,3 1,1 1,2 1,3

  // If N is odd, say 5, then the 15 i,j pairs are:
  // 0,0 0,1 0,2 0,3 0,4
  //     1,1 1,2 1,3 1,4
  //         2,2 2,3 2,3
  //             3,3 3,4
  //                 4,4
  // which folds to a 5x3 rectangle:
  // 0,0 0,1 0,2 0,3 0,4
  // 4,4 1,1 1,2 1,3 1,4
  // 3,3 3,4 2,2 2,3 2,4

  // All this function does is map the linear iteration position to a
  // location in the rectangle and work out the appropriate (i, j) for that
  // location.
  if (n & 1) {
    // Odd n. The tip of the triangle is on row 1.
    int w = n;  // Width of the rectangle to unfold
    int i0 = k / w;
    int j0 = k % w;
    if (j0 >= i0) {
      *i = i0;
      *j = j0;
    } else {
      *i = n - i0;
      *j = *i + j0;
    }
  } else {
    // Even n. The tip of the triangle is on row 0, making it one wider.
    int w = n + 1;
    int i0 = k / w;
    int j0 = k % w;
    if (j0 > i0) {
      *i = i0;
      *j = j0 - 1;
    } else {
      *i = n - 1 - i0;
      *j = *i + j0;
    }
  }
}

}  // namespace internal
}  // namespace ceres
