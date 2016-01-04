// Copyright (c) 2007, 2008 libmv authors.
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


#include "libmv/numeric/numeric.h"

namespace libmv {

Mat3 RotationAroundX(double angle) {
  double c, s;
  sincos(angle, &s, &c);
  Mat3 R;
  R << 1,  0,  0,
       0,  c, -s,
       0,  s,  c;
  return R;
}

Mat3 RotationAroundY(double angle) {
  double c, s;
  sincos(angle, &s, &c);
  Mat3 R;
  R <<  c, 0, s,
        0, 1, 0,
       -s, 0, c;
  return R;
}

Mat3 RotationAroundZ(double angle) {
  double c, s;
  sincos(angle, &s, &c);
  Mat3 R;
  R << c, -s,  0,
       s,  c,  0,
       0,  0,  1;
  return R;
}


Mat3 RotationRodrigues(const Vec3 &axis) {
  double theta = axis.norm();
  Vec3 w = axis / theta;
  Mat3 W = CrossProductMatrix(w);

  return Mat3::Identity() + sin(theta) * W + (1 - cos(theta)) * W * W;
}


Mat3 LookAt(Vec3 center) {
  Vec3 zc = center.normalized();
  Vec3 xc = Vec3::UnitY().cross(zc).normalized();
  Vec3 yc = zc.cross(xc);
  Mat3 R;
  R.row(0) = xc;
  R.row(1) = yc;
  R.row(2) = zc;
  return R;
}

Mat3 CrossProductMatrix(const Vec3 &x) {
  Mat3 X;
  X <<     0, -x(2),  x(1),
        x(2),     0, -x(0),
       -x(1),  x(0),     0;
  return X;
}

void MeanAndVarianceAlongRows(const Mat &A,
                              Vec *mean_pointer,
                              Vec *variance_pointer) {
  Vec &mean = *mean_pointer;
  Vec &variance = *variance_pointer;
  int n = A.rows();
  int m = A.cols();
  mean.resize(n);
  variance.resize(n);

  for (int i = 0; i < n; ++i) {
    mean(i) = 0;
    variance(i) = 0;
    for (int j = 0; j < m; ++j) {
      double x = A(i, j);
      mean(i) += x;
      variance(i) += x * x;
    }
  }

  mean /= m;
  for (int i = 0; i < n; ++i) {
    variance(i) = variance(i) / m - Square(mean(i));
  }
}

void HorizontalStack(const Mat &left, const Mat &right, Mat *stacked) {
  assert(left.rows() == left.rows());
  int n = left.rows();
  int m1 = left.cols();
  int m2 = right.cols();

  stacked->resize(n, m1 + m2);
  stacked->block(0, 0,  n, m1) = left;
  stacked->block(0, m1, n, m2) = right;
}

void MatrixColumn(const Mat &A, int i, Vec2 *v) {
  assert(A.rows() == 2);
  *v << A(0, i), A(1, i);
}
void MatrixColumn(const Mat &A, int i, Vec3 *v) {
  assert(A.rows() == 3);
  *v << A(0, i), A(1, i), A(2, i);
}
void MatrixColumn(const Mat &A, int i, Vec4 *v) {
  assert(A.rows() == 4);
  *v << A(0, i), A(1, i), A(2, i), A(3, i);
}

}  // namespace libmv

