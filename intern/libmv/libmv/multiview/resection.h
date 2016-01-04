// Copyright (c) 2009 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Compute the projection matrix from a set of 3D points X and their
// projections x = PX in 2D. This is useful if a point cloud is reconstructed.
//
// Algorithm is the standard DLT as described in Hartley & Zisserman, page 179.

#ifndef LIBMV_MULTIVIEW_RESECTION_H
#define LIBMV_MULTIVIEW_RESECTION_H

#include "libmv/logging/logging.h"
#include "libmv/numeric/numeric.h"

namespace libmv {
namespace resection {

// x's are 2D image coordinates, (x,y,1), and X's are homogeneous four vectors.
template<typename T>
void Resection(const Matrix<T, 2, Dynamic> &x,
               const Matrix<T, 4, Dynamic> &X,
               Matrix<T, 3, 4> *P) {
  int N = x.cols();
  assert(X.cols() == N);

  Matrix<T, Dynamic, 12> design(2*N, 12);
  design.setZero();
  for (int i = 0; i < N; i++) {
    T xi = x(0, i);
    T yi = x(1, i);
    // See equation (7.2) on page 179 of H&Z.
    design.template block<1, 4>(2*i,     4) =    -X.col(i).transpose();
    design.template block<1, 4>(2*i,     8) =  yi*X.col(i).transpose();
    design.template block<1, 4>(2*i + 1, 0) =     X.col(i).transpose();
    design.template block<1, 4>(2*i + 1, 8) = -xi*X.col(i).transpose();
  }
  Matrix<T, 12, 1> p;
  Nullspace(&design, &p);
  reshape(p, 3, 4, P);
}

}  // namespace resection
}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_RESECTION_H
