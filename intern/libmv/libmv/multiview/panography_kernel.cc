// Copyright (c) 2008, 2009 libmv authors.
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

#include "libmv/multiview/panography_kernel.h"
#include "libmv/multiview/panography.h"

namespace libmv {
namespace panography {
namespace kernel {

void TwoPointSolver::Solve(const Mat &x1, const Mat &x2, vector<Mat3> *Hs) {
  // Solve for the focal lengths.
  vector<double> fs;
  F_FromCorrespondance_2points(x1, x2, &fs);

  // Then solve for the rotations and homographies.
  Mat x1h, x2h;
  EuclideanToHomogeneous(x1, &x1h);
  EuclideanToHomogeneous(x2, &x2h);
  for (int i = 0; i < fs.size(); ++i)  {
    Mat3 K1 = Mat3::Identity() * fs[i];
    K1(2, 2) = 1.0;

    Mat3 R;
    GetR_FixedCameraCenter(x1h, x2h, fs[i], &R);
    R /= R(2, 2);

    (*Hs).push_back(K1 * R * K1.inverse());
  }
}

}  // namespace kernel
}  // namespace panography
}  // namespace libmv
