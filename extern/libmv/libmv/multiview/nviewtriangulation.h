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
// Compute a 3D position of a point from several images of it. In particular,
// compute the projective point X in R^4 such that x = PX.
//
// Algorithm is the standard DLT; for derivation see appendix of Keir's thesis.

#ifndef LIBMV_MULTIVIEW_NVIEWTRIANGULATION_H
#define LIBMV_MULTIVIEW_NVIEWTRIANGULATION_H

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

// x's are 2D coordinates (x,y,1) in each image; Ps are projective cameras. The
// output, X, is a homogeneous four vectors.
template<typename T>
void NViewTriangulate(const Matrix<T, 2, Dynamic> &x,
                      const vector<Matrix<T, 3, 4> > &Ps,
                      Matrix<T, 4, 1> *X) {
  int nviews = x.cols();
  assert(nviews == Ps.size());

  Matrix<T, Dynamic, Dynamic> design(3*nviews, 4 + nviews);
  design.setConstant(0.0);
  for (int i = 0; i < nviews; i++) {
    design.template block<3, 4>(3*i, 0) = -Ps[i];
    design(3*i + 0, 4 + i) = x(0, i);
    design(3*i + 1, 4 + i) = x(1, i);
    design(3*i + 2, 4 + i) = 1.0;
  }
  Matrix<T, Dynamic, 1>  X_and_alphas;
  Nullspace(&design, &X_and_alphas);
  X->resize(4);
  *X = X_and_alphas.head(4);
}

// x's are 2D coordinates (x,y,1) in each image; Ps are projective cameras. The
// output, X, is a homogeneous four vectors.
// This method uses the algebraic distance approximation.
// Note that this method works better when the 2D points are normalized
// with an isotopic normalization.
template<typename T>
void NViewTriangulateAlgebraic(const Matrix<T, 2, Dynamic> &x,
                               const vector<Matrix<T, 3, 4> > &Ps,
                               Matrix<T, 4, 1> *X) {
  int nviews = x.cols();
  assert(nviews == Ps.size());

  Matrix<T, Dynamic, 4> design(2*nviews, 4);
  for (int i = 0; i < nviews; i++) {
    design.template block<2, 4>(2*i, 0) = SkewMatMinimal(x.col(i)) * Ps[i];
  }
  X->resize(4);
  Nullspace(&design, X);
}

}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_RESECTION_H
