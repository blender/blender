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

#ifndef LIBMV_MULTIVIEW_PANOGRAPHY_KERNEL_H
#define LIBMV_MULTIVIEW_PANOGRAPHY_KERNEL_H

#include "libmv/base/vector.h"
#include "libmv/multiview/conditioning.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/two_view_kernel.h"
#include "libmv/multiview/homography_error.h"
#include "libmv/numeric/numeric.h"

namespace libmv {
namespace panography {
namespace kernel {

struct TwoPointSolver {
  enum { MINIMUM_SAMPLES = 2 };
  static void Solve(const Mat &x1, const Mat &x2, vector<Mat3> *Hs);
};

typedef two_view::kernel::Kernel<
    TwoPointSolver, homography::homography2D::AsymmetricError, Mat3>
  UnnormalizedKernel;

typedef two_view::kernel::Kernel<
        two_view::kernel::NormalizedSolver<TwoPointSolver, UnnormalizerI>,
        homography::homography2D::AsymmetricError,
        Mat3>
  Kernel;

}  // namespace kernel
}  // namespace panography
}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_PANOGRAPHY_KERNEL_H
