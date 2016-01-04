// Copyright (c) 2014 libmv authors.
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
// Author: mierle@gmail.com (Keir Mierle)

#ifndef LIBMV_AUTOTRACK_QUAD_H_
#define LIBMV_AUTOTRACK_QUAD_H_

#include <Eigen/Core>

namespace mv {

template<typename T, int D>
struct Quad {
  // A quad is 4 points; generally in 2D or 3D.
  //
  //    +----------> x
  //    |\.
  //    | \.
  //    |  z (z goes into screen)
  //    |   
  //    |     r0----->r1
  //    |      ^       |
  //    |      |   .   |
  //    |      |       V
  //    |     r3<-----r2
  //    |              \.
  //    |               \.
  //    v                normal goes away (right handed).
  //    y   
  //
  // Each row is one of the corners coordinates; either (x, y) or (x, y, z).
  Eigen::Matrix<T, 4, D> coordinates;
};

typedef Quad<float, 2> Quad2Df;

}  // namespace mv

#endif  // LIBMV_AUTOTRACK_QUAD_H_
