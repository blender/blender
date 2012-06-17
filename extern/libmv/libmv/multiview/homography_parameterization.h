// Copyright (c) 2011 libmv authors.
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

#ifndef LIBMV_MULTIVIEW_HOMOGRAPHY_PARAMETERIZATION_H_
#define LIBMV_MULTIVIEW_HOMOGRAPHY_PARAMETERIZATION_H_

#include "libmv/numeric/numeric.h"

namespace libmv {

/** A parameterization of the 2D homography matrix that uses 8 parameters so 
 * that the matrix is normalized (H(2,2) == 1).
 * The homography matrix H is built from a list of 8 parameters (a, b,...g, h)
 * as follows
 *         |a b c| 
 *     H = |d e f|
 *         |g h 1| 
 */
template<typename T = double>
class Homography2DNormalizedParameterization {
 public:
  typedef Eigen::Matrix<T, 8, 1> Parameters;     // a, b, ... g, h
  typedef Eigen::Matrix<T, 3, 3> Parameterized;  // H

  /// Convert from the 8 parameters to a H matrix.
  static void To(const Parameters &p, Parameterized *h) {    
    *h << p(0), p(1), p(2),
          p(3), p(4), p(5),
          p(6), p(7), 1.0;
  }

  /// Convert from a H matrix to the 8 parameters.
  static void From(const Parameterized &h, Parameters *p) {
    *p << h(0, 0), h(0, 1), h(0, 2),
          h(1, 0), h(1, 1), h(1, 2),
          h(2, 0), h(2, 1);
  }
};

/** A parameterization of the 2D homography matrix that uses 15 parameters so 
 * that the matrix is normalized (H(3,3) == 1).
 * The homography matrix H is built from a list of 15 parameters (a, b,...n, o)
 * as follows
 *          |a b c d| 
 *      H = |e f g h|
 *          |i j k l|
 *          |m n o 1| 
 */
template<typename T = double>
class Homography3DNormalizedParameterization {
 public:
  typedef Eigen::Matrix<T, 15, 1> Parameters;     // a, b, ... n, o
  typedef Eigen::Matrix<T, 4, 4>  Parameterized;  // H

  /// Convert from the 15 parameters to a H matrix.
  static void To(const Parameters &p, Parameterized *h) {   
    *h << p(0), p(1), p(2), p(3),
          p(4), p(5), p(6), p(7),
          p(8), p(9), p(10), p(11),
          p(12), p(13), p(14), 1.0;
  }

  /// Convert from a H matrix to the 15 parameters.
  static void From(const Parameterized &h, Parameters *p) {
    *p << h(0, 0), h(0, 1), h(0, 2), h(0, 3),
          h(1, 0), h(1, 1), h(1, 2), h(1, 3),
          h(2, 0), h(2, 1), h(2, 2), h(2, 3),
          h(3, 0), h(3, 1), h(3, 2);
  }
};

} // namespace libmv

#endif  // LIBMV_MULTIVIEW_HOMOGRAPHY_PARAMETERIZATION_H_
