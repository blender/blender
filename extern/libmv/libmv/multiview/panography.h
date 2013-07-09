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

#ifndef LIBMV_MULTIVIEW_PANOGRAPHY_H
#define LIBMV_MULTIVIEW_PANOGRAPHY_H

#include "libmv/numeric/numeric.h"
#include "libmv/numeric/poly.h"
#include "libmv/base/vector.h"

namespace libmv {

// This implements a minimal solution (2 points) for panoramic stitching:
//
//   http://www.cs.ubc.ca/~mbrown/minimal/minimal.html
//
//   [1] M. Brown and R. Hartley and D. Nister. Minimal Solutions for Panoramic
//       Stitching. CVPR07.
//
// The 2-point algorithm solves for the rotation of the camera with a single
// focal length (4 degrees of freedom).
//
// Compute from 1 to 3 possible focal lenght for 2 point correspondences.
// Suppose that the cameras share the same optical center and focal lengths:
//
//   Image 1 => H*x = x'  =>  Image 2
//   x (u1j)                  x' (u2j)
//   a (u11)                  a' (u21)
//   b (u12)                  b' (u22)
//
// The return values are 1 to 3 possible values for the focal lengths such
// that:
//
//       [f 0 0]
//   K = [0 f 0]
//       [0 0 1]
//
void F_FromCorrespondance_2points(const Mat &x1, const Mat &x2,
                                  vector<double> *fs);

// Compute the 3x3 rotation matrix that fits two 3D point clouds in the least
// square sense. The method is from:
//
//   K. Arun,T. Huand and D. Blostein. Least-squares fitting of 2 3-D point
//   sets.  IEEE Transactions on Pattern Analysis and Machine Intelligence,
//   9:698-700, 1987.
//
// Given the calibration matrices K1, K2 solve for the rotation from
// corresponding image rays.
//
//   R = min || X2 - R * x1 ||.
//
// In case of panography, which is for a camera that shares the same camera
// center,
//
//   H = K2 * R * K1.inverse();
//
// For the full explanation, see Section 8, Solving for Rotation from [1].
//
// Parameters:
//
//   x1 : Point cloud A (3D coords)
//   x2 : Point cloud B (3D coords)
//
//        [f 0 0]
//   K1 = [0 f 0]
//        [0 0 1]
//
//   K2 (the same form as K1, but may have different f)
//
// Returns: A rotation matrix that minimizes
//
//   R = arg min || X2 - R * x1 ||
//
void GetR_FixedCameraCenter(const Mat &x1, const Mat &x2,
                            const double focal,
                            Mat3 *R);

}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_PANOGRAPHY_H
