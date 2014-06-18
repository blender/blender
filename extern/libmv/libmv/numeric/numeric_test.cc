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
#include "testing/testing.h"

using namespace libmv;

namespace {

TEST(Numeric, DynamicSizedNullspace) {
  Mat A(3, 4);
  A << 0.76026643, 0.01799744, 0.55192142, 0.8699745,
       0.42016166, 0.97863392, 0.33711682, 0.14479271,
       0.51016811, 0.66528302, 0.54395496, 0.57794893;
  Vec x;
  double s = Nullspace(&A, &x);
  EXPECT_NEAR(0.0, s, 1e-15);
  EXPECT_NEAR(0.0, (A * x).norm(), 1e-15);
  EXPECT_NEAR(1.0, x.norm(), 1e-15);
}

TEST(Numeric, FixedSizeMatrixNullspace) {
  Mat34 A;
  A << 0.76026643, 0.01799744, 0.55192142, 0.8699745,
       0.42016166, 0.97863392, 0.33711682, 0.14479271,
       0.51016811, 0.66528302, 0.54395496, 0.57794893;
  Vec x;
  double s = Nullspace(&A, &x);
  EXPECT_NEAR(0.0, s, 1e-15);
  EXPECT_NEAR(0.0, (A * x).norm(), 1e-15);
  EXPECT_NEAR(1.0, x.norm(), 1e-15);
}

TEST(Numeric, NullspaceMatchesLapackSVD) {
  Mat43 A;
  A << 0.76026643, 0.01799744, 0.55192142,
       0.8699745,  0.42016166, 0.97863392,
       0.33711682, 0.14479271, 0.51016811,
       0.66528302, 0.54395496, 0.57794893;
  Vec x;
  double s = Nullspace(&A, &x);
  EXPECT_NEAR(1.0, x.norm(), 1e-15);
  EXPECT_NEAR(0.206694992663, s, 1e-9);
  EXPECT_NEAR(0.206694992663, (A * x).norm(), 1e-9);

  EXPECT_NEAR(-0.64999717, x(0), 1e-8);
  EXPECT_NEAR(-0.18452646, x(1), 1e-8);
  EXPECT_NEAR(0.7371931, x(2), 1e-8);
}

TEST(Numeric, Nullspace2) {
  Mat43 A;
  A << 0.76026643, 0.01799744, 0.55192142,
       0.8699745,  0.42016166, 0.97863392,
       0.33711682, 0.14479271, 0.51016811,
       0.66528302, 0.54395496, 0.57794893;
  Vec3 x1, x2;
  double s = Nullspace2(&A, &x1, &x2);
  EXPECT_NEAR(1.0, x1.norm(), 1e-15);
  EXPECT_NEAR(0.206694992663, s, 1e-9);
  EXPECT_NEAR(0.206694992663, (A * x1).norm(), 1e-9);

  EXPECT_NEAR(-0.64999717, x1(0), 1e-8);
  EXPECT_NEAR(-0.18452646, x1(1), 1e-8);
  EXPECT_NEAR( 0.7371931,  x1(2), 1e-8);

  if (x2(0) < 0) {
    x2 *= -1;
  }
  EXPECT_NEAR( 0.34679618, x2(0), 1e-8);
  EXPECT_NEAR(-0.93519689, x2(1), 1e-8);
  EXPECT_NEAR( 0.07168809, x2(2), 1e-8);
}

TEST(Numeric, TinyMatrixSquareTranspose) {
  Mat2 A;
  A << 1.0, 2.0, 3.0, 4.0;
  libmv::TransposeInPlace(&A);
  EXPECT_EQ(1.0, A(0, 0));
  EXPECT_EQ(3.0, A(0, 1));
  EXPECT_EQ(2.0, A(1, 0));
  EXPECT_EQ(4.0, A(1, 1));
}

TEST(Numeric, NormalizeL1) {
  Vec2 x;
  x << 1, 2;
  double l1 = NormalizeL1(&x);
  EXPECT_DOUBLE_EQ(3., l1);
  EXPECT_DOUBLE_EQ(1./3., x(0));
  EXPECT_DOUBLE_EQ(2./3., x(1));
}

TEST(Numeric, NormalizeL2) {
  Vec2 x;
  x << 1, 2;
  double l2 = NormalizeL2(&x);
  EXPECT_DOUBLE_EQ(sqrt(5.0), l2);
  EXPECT_DOUBLE_EQ(1./sqrt(5.), x(0));
  EXPECT_DOUBLE_EQ(2./sqrt(5.), x(1));
}

TEST(Numeric, Diag) {
  Vec x(2);
  x << 1, 2;
  Mat D = Diag(x);
  EXPECT_EQ(1, D(0, 0));
  EXPECT_EQ(0, D(0, 1));
  EXPECT_EQ(0, D(1, 0));
  EXPECT_EQ(2, D(1, 1));
}

TEST(Numeric, Determinant) {
  Mat A(2, 2);
  A <<  1, 2,
       -1, 3;
  double detA = A.determinant();
  EXPECT_NEAR(5, detA, 1e-8);

  Mat B(4, 4);
  B <<  0,  1,  2,  3,
        4,  5,  6,  7,
        8,  9, 10, 11,
       12, 13, 14, 15;
  double detB = B.determinant();
  EXPECT_NEAR(0, detB, 1e-8);

  Mat3 C;
  C <<  0, 1, 2,
        3, 4, 5,
        6, 7, 1;
  double detC = C.determinant();
  EXPECT_NEAR(21, detC, 1e-8);
}

TEST(Numeric, Inverse) {
  Mat A(2, 2), A1;
  A <<  1, 2,
       -1, 3;
  Mat I = A * A.inverse();

  EXPECT_NEAR(1, I(0, 0), 1e-8);
  EXPECT_NEAR(0, I(0, 1), 1e-8);
  EXPECT_NEAR(0, I(1, 0), 1e-8);
  EXPECT_NEAR(1, I(1, 1), 1e-8);

  Mat B(4, 4), B1;
  B <<  0,  1,  2,  3,
        4,  5,  6,  7,
        8,  9,  2, 11,
       12, 13, 14,  4;
  Mat I2 = B * B.inverse();
  EXPECT_NEAR(1, I2(0, 0), 1e-8);
  EXPECT_NEAR(0, I2(0, 1), 1e-8);
  EXPECT_NEAR(0, I2(0, 2), 1e-8);
  EXPECT_NEAR(0, I2(1, 0), 1e-8);
  EXPECT_NEAR(1, I2(1, 1), 1e-8);
  EXPECT_NEAR(0, I2(1, 2), 1e-8);
  EXPECT_NEAR(0, I2(2, 0), 1e-8);
  EXPECT_NEAR(0, I2(2, 1), 1e-8);
  EXPECT_NEAR(1, I2(2, 2), 1e-8);
}

TEST(Numeric, MeanAndVarianceAlongRows) {
  int n = 4;
  Mat points(2, n);
  points << 0, 0, 1, 1,
            0, 2, 1, 3;

  Vec mean, variance;
  MeanAndVarianceAlongRows(points, &mean, &variance);

  EXPECT_NEAR(0.5, mean(0), 1e-8);
  EXPECT_NEAR(1.5, mean(1), 1e-8);
  EXPECT_NEAR(0.25, variance(0), 1e-8);
  EXPECT_NEAR(1.25, variance(1), 1e-8);
}

TEST(Numeric, HorizontalStack) {
  Mat x(2, 1), y(2, 1), z;
  x << 1, 2;
  y << 3, 4;

  HorizontalStack(x, y, &z);

  EXPECT_EQ(2, z.cols());
  EXPECT_EQ(2, z.rows());
  EXPECT_EQ(1, z(0, 0));
  EXPECT_EQ(2, z(1, 0));
  EXPECT_EQ(3, z(0, 1));
  EXPECT_EQ(4, z(1, 1));
}

TEST(Numeric, HStack) {
  Mat x(2, 1), y(2, 1), z(2, 2);
  x << 1, 2;
  y << 3, 4;
  z << 1, 3,
       2, 4;
  Vec2 xC = x, yC = y;

  Mat2 xy = HStack(x,  y);
  EXPECT_MATRIX_EQ(z, xy);

  EXPECT_MATRIX_EQ(z, HStack(x,  y));
  EXPECT_MATRIX_EQ(z, HStack(x,  yC));
  EXPECT_MATRIX_EQ(z, HStack(xC, y));
  EXPECT_MATRIX_EQ(z, HStack(xC, yC));
}

// TODO(keir): Need some way of verifying that the compile time types of the
// resulting stacked matrices properly propagate the fixed dimensions.
TEST(Numeric, VStack) {
  Mat x(2, 2), y(2, 2), z(4, 2);
  x << 1, 2,
       3, 4;
  y << 10, 20,
       30, 40;
  z <<  1,  2,
        3,  4,
       10, 20,
       30, 40;
  Mat2 xC = x, yC = y;

  Mat xy = VStack(x,  y);
  EXPECT_MATRIX_EQ(z, xy);

  EXPECT_MATRIX_EQ(z, VStack(x,  y));
  EXPECT_MATRIX_EQ(z, VStack(x,  yC));
  EXPECT_MATRIX_EQ(z, VStack(xC, y));
  EXPECT_MATRIX_EQ(z, VStack(xC, yC));
}

TEST(Numeric, VerticalStack) {
  Mat x(1, 2), y(1, 2), z;
  x << 1, 2;
  y << 3, 4;
  VerticalStack(x, y, &z);

  EXPECT_EQ(2, z.cols());
  EXPECT_EQ(2, z.rows());
  EXPECT_EQ(1, z(0, 0));
  EXPECT_EQ(2, z(0, 1));
  EXPECT_EQ(3, z(1, 0));
  EXPECT_EQ(4, z(1, 1));
}

TEST(Numeric, CrossProduct) {
  Vec3 x, y, z;
  x << 1, 0, 0;
  y << 0, 1, 0;
  z << 0, 0, 1;
  Vec3 xy = CrossProduct(x, y);
  Vec3 yz = CrossProduct(y, z);
  Vec3 zx = CrossProduct(z, x);
  EXPECT_NEAR(0, DistanceLInfinity(xy, z), 1e-8);
  EXPECT_NEAR(0, DistanceLInfinity(yz, x), 1e-8);
  EXPECT_NEAR(0, DistanceLInfinity(zx, y), 1e-8);
}

TEST(Numeric, CrossProductMatrix) {
  Vec3 x, y;
  x << 1, 2, 3;
  y << 2, 3, 4;
  Vec3 xy = CrossProduct(x, y);
  Vec3 yx = CrossProduct(y, x);
  Mat3 X = CrossProductMatrix(x);
  Vec3 Xy, Xty;
  Xy = X * y;
  Xty = X.transpose() * y;
  EXPECT_NEAR(0, DistanceLInfinity(xy, Xy), 1e-8);
  EXPECT_NEAR(0, DistanceLInfinity(yx, Xty), 1e-8);
}

TEST(Numeric, MatrixColumn) {
  Mat A2(2, 3);
  Vec2 v2;
  A2 << 1, 2, 3,
        4, 5, 6;
  MatrixColumn(A2, 1, &v2);
  EXPECT_EQ(2, v2(0));
  EXPECT_EQ(5, v2(1));

  Mat A3(3, 3);
  Vec3 v3;
  A3 << 1, 2, 3,
        4, 5, 6,
        7, 8, 9;
  MatrixColumn(A3, 1, &v3);
  EXPECT_EQ(2, v3(0));
  EXPECT_EQ(5, v3(1));
  EXPECT_EQ(8, v3(2));

  Mat A4(4, 3);
  Vec4 v4;
  A4 <<  1,  2,  3,
         4,  5,  6,
         7,  8,  9,
        10, 11, 12;
  MatrixColumn(A4, 1, &v4);
  EXPECT_EQ( 2, v4(0));
  EXPECT_EQ( 5, v4(1));
  EXPECT_EQ( 8, v4(2));
  EXPECT_EQ(11, v4(3));
}

// This used to give a compile error with FLENS.
TEST(Numeric, TinyMatrixView) {
  Mat34 P;
  Mat K = P.block(0, 0, 3, 3);
}

// This gives a compile error.
TEST(Numeric, Mat3MatProduct) {
  Mat3 A;
  Mat3 B;
  Mat C = A * B;
}

// This gives a compile error.
TEST(Numeric, Vec3Negative) {
  Vec3 y; y << 1, 2, 3;
  Vec3 x = -y;
  EXPECT_EQ(-1, x(0));
  EXPECT_EQ(-2, x(1));
  EXPECT_EQ(-3, x(2));
}

// This gives a compile error.
TEST(Numeric, Vec3VecInteroperability) {
  Vec y(3);
  y << 1, 2, 3;
  Vec3 x = y + y;
  EXPECT_EQ(2, x(0));
  EXPECT_EQ(4, x(1));
  EXPECT_EQ(6, x(2));
}

// This segfaults inside lapack.
TEST(Numeric, DeterminantLU7) {
  Mat A(5, 5);
  A <<  1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 0, 1;
  EXPECT_NEAR(1, A.determinant(), 1e-8);
}

// This segfaults inside lapack.
TEST(Numeric, DeterminantLU) {
  Mat A(2, 2);
  A <<  1, 2,
       -1, 3;
  EXPECT_NEAR(5, A.determinant(), 1e-8);
}

// This does unexpected things.
// Keir: Not with eigen2!
TEST(Numeric, InplaceProduct) {
  Mat2 K, S;
  K << 1, 0,
       0, 1;
  S << 1, 0,
       0, 1;
  K = K * S;
  EXPECT_MATRIX_NEAR(Mat2::Identity(), K, 1e-8);
}

TEST(Numeric, ExtractColumns) {
  Mat2X A(2, 5);
  A << 1, 2, 3, 4, 5,
       6, 7, 8, 9, 10;
  Vec2i columns; columns << 0, 2;
  Mat2X extracted = ExtractColumns(A, columns);
  EXPECT_NEAR(1, extracted(0, 0), 1e-15);
  EXPECT_NEAR(3, extracted(0, 1), 1e-15);
  EXPECT_NEAR(6, extracted(1, 0), 1e-15);
  EXPECT_NEAR(8, extracted(1, 1), 1e-15);
}

TEST(Numeric, RotationRodrigues) {
  Vec3 x, y, z;
  x << 1, 0, 0;
  y << 0, 1, 0;
  z << 0, 0, 1;

  Mat3 rodrigues_x = RotationRodrigues(x);
  Mat3 rodrigues_y = RotationRodrigues(y);
  Mat3 rodrigues_z = RotationRodrigues(z);

  Mat3 Rx = RotationAroundX(1);
  Mat3 Ry = RotationAroundY(1);
  Mat3 Rz = RotationAroundZ(1);

  EXPECT_MATRIX_NEAR(Rx, rodrigues_x, 1e-15);
  EXPECT_MATRIX_NEAR(Ry, rodrigues_y, 1e-15);
  EXPECT_MATRIX_NEAR(Rz, rodrigues_z, 1e-15);
}

TEST(Numeric, LookAt) {
  // Simple orthogonality check.
  Vec3 e; e << 1, 2, 3;
  Mat3 R = LookAt(e), I = Mat3::Identity();
  Mat3 RRT = R*R.transpose();
  Mat3 RTR = R.transpose()*R;

  EXPECT_MATRIX_NEAR(I, RRT, 1e-15);
  EXPECT_MATRIX_NEAR(I, RTR, 1e-15);
}

TEST(Numeric, Reshape) {
  Vec4 x; x << 1, 2, 3, 4;
  Mat2 M, M_expected;
  reshape(x, 2, 2, &M);
  M_expected << 1, 2,
                3, 4;
  EXPECT_MATRIX_NEAR(M_expected, M, 1e-15);
}

}  // namespace
