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

static bool Build_Minimal2Point_PolynomialFactor(
                                          const Mat & x1, const Mat & x2,
                                          double * P) //P must be a double[4]
{
  assert(2 == x1.rows());
  assert(2 == x1.cols());
  assert(x1.rows() == x2.rows());
  assert(x1.cols() == x2.cols());

  // Setup the variable of the input problem:
  Vec xx1 = (x1.col(0)).transpose();
  Vec yx1 = (x1.col(1)).transpose();

  double a12 = xx1.dot(yx1);
  Vec xx2 = (x2.col(0)).transpose();
  Vec yx2 = (x2.col(1)).transpose();
  double b12 = xx2.dot(yx2);

  double a1  = xx1.squaredNorm();
  double a2  = yx1.squaredNorm();

  double b1  = xx2.squaredNorm();
  double b2  = yx2.squaredNorm();

  // Build the 3rd degre polynomial in F^2.
  //
  //   f^6 * p + f^4 * q + f^2* r + s = 0;
  //
  // Coefficients in ascending powers of alpha, i.e. P[N]*x^N.
  // Run panography_coeffs.py to get the below coefficients.
  P[0] = b1*b2*a12*a12-a1*a2*b12*b12;
  P[1] = -2*a1*a2*b12+2*a12*b1*b2+b1*a12*a12+b2*a12*a12-a1*b12*b12-a2*b12*b12;
  P[2] = b1*b2-a1*a2-2*a1*b12-2*a2*b12+2*a12*b1+2*a12*b2+a12*a12-b12*b12;
  P[3] = b1+b2-2*b12-a1-a2+2*a12;

  // If P[3] equal to 0 we get ill conditionned data
  return (P[3] != 0.0);
}

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
static void F_FromCorrespondance_2points(const Mat &x1, const Mat &x2,
                                         vector<double> *fs) {

  // Build Polynomial factor to get squared focal value.
  double P[4];
  Build_Minimal2Point_PolynomialFactor(x1, x2, &P[0]);

  // Solve it by using F = f^2 and a Cubic polynomial solver
  //
  //   F^3 * p + F^2 * q + F^1 * r + s = 0
  //
  double roots[3];
  int num_roots = SolveCubicPolynomial(P, roots);
  for (int i = 0; i < num_roots; ++i)  {
    if (roots[i] > 0.0)  {
      fs->push_back(sqrt(roots[i]));
    }
  }
}

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
static void GetR_FixedCameraCenter(const Mat &x1, const Mat &x2,
                                   const double focal,
                                   Mat3 *R)  {
  assert(3 == x1.rows());
  assert(2 <= x1.cols());
  assert(x1.rows() == x2.rows());
  assert(x1.cols() == x2.cols());

  // Build simplified K matrix
  Mat3 K( Mat3::Identity() * 1.0/focal );
  K(2,2)= 1.0;

  // Build the correlation matrix; equation (22) in [1].
  Mat3 C = Mat3::Zero();
  for(int i = 0; i < x1.cols(); ++i) {
    Mat r1i = (K * x1.col(i)).normalized();
    Mat r2i = (K * x2.col(i)).normalized();
    C += r2i * r1i.transpose();
  }

  // Solve for rotation. Equations (24) and (25) in [1].
  Eigen::JacobiSVD<Mat> svd(C, Eigen::ComputeThinU | Eigen::ComputeThinV);
  Mat3 scale = Mat3::Identity();
  scale(2,2) = ((svd.matrixU() * svd.matrixV().transpose()).determinant() > 0.0)
             ?  1.0
             : -1.0;

  (*R) = svd.matrixU() * scale * svd.matrixV().transpose();
}

}  // namespace libmv

#endif // LIBMV_MULTIVIEW_PANOGRAPHY_H
