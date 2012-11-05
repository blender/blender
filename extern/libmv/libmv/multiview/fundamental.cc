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

#include "libmv/logging/logging.h"
#include "libmv/numeric/numeric.h"
#include "libmv/numeric/poly.h"
#include "libmv/multiview/conditioning.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/triangulation.h"
#include "libmv/multiview/fundamental.h"

namespace libmv {

static void EliminateRow(const Mat34 &P, int row, Mat *X) {
  X->resize(2, 4);

  int first_row = (row + 1) % 3;
  int second_row = (row + 2) % 3;

  for (int i = 0; i < 4; ++i) {
    (*X)(0, i) = P(first_row, i);
    (*X)(1, i) = P(second_row, i);
  }
}

void ProjectionsFromFundamental(const Mat3 &F, Mat34 *P1, Mat34 *P2) {
  *P1 << Mat3::Identity(), Vec3::Zero();
  Vec3 e2;
  Mat3 Ft = F.transpose();
  Nullspace(&Ft, &e2);
  *P2 << CrossProductMatrix(e2) * F, e2;
}

// Addapted from vgg_F_from_P.
void FundamentalFromProjections(const Mat34 &P1, const Mat34 &P2, Mat3 *F) {
  Mat X[3];
  Mat Y[3];
  Mat XY;

  for (int i = 0; i < 3; ++i) {
    EliminateRow(P1, i, X + i);
    EliminateRow(P2, i, Y + i);
  }

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      VerticalStack(X[j], Y[i], &XY);
      (*F)(i, j) = XY.determinant();
    }
  }
}

// HZ 11.1 pag.279 (x1 = x, x2 = x')
// http://www.cs.unc.edu/~marc/tutorial/node54.html
static double EightPointSolver(const Mat &x1, const Mat &x2, Mat3 *F) {
  DCHECK_EQ(x1.rows(), 2);
  DCHECK_GE(x1.cols(), 8);
  DCHECK_EQ(x1.rows(), x2.rows());
  DCHECK_EQ(x1.cols(), x2.cols());

  int n = x1.cols();
  Mat A(n, 9);
  for (int i = 0; i < n; ++i) {
    A(i, 0) = x2(0, i) * x1(0, i);
    A(i, 1) = x2(0, i) * x1(1, i);
    A(i, 2) = x2(0, i);
    A(i, 3) = x2(1, i) * x1(0, i);
    A(i, 4) = x2(1, i) * x1(1, i);
    A(i, 5) = x2(1, i);
    A(i, 6) = x1(0, i);
    A(i, 7) = x1(1, i);
    A(i, 8) = 1;
  }

  Vec9 f;
  double smaller_singular_value = Nullspace(&A, &f);
  *F = Map<RMat3>(f.data());
  return smaller_singular_value;
}

// HZ 11.1.1 pag.280
void EnforceFundamentalRank2Constraint(Mat3 *F) {
  Eigen::JacobiSVD<Mat3> USV(*F, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Vec3 d = USV.singularValues();
  d(2) = 0.0;
  *F = USV.matrixU() * d.asDiagonal() * USV.matrixV().transpose();
}

// HZ 11.2 pag.281 (x1 = x, x2 = x')
double NormalizedEightPointSolver(const Mat &x1,
                                  const Mat &x2,
                                  Mat3 *F) {
  DCHECK_EQ(x1.rows(), 2);
  DCHECK_GE(x1.cols(), 8);
  DCHECK_EQ(x1.rows(), x2.rows());
  DCHECK_EQ(x1.cols(), x2.cols());

  // Normalize the data.
  Mat3 T1, T2;
  PreconditionerFromPoints(x1, &T1);
  PreconditionerFromPoints(x2, &T2);
  Mat x1_normalized, x2_normalized;
  ApplyTransformationToPoints(x1, T1, &x1_normalized);
  ApplyTransformationToPoints(x2, T2, &x2_normalized);

  // Estimate the fundamental matrix.
  double smaller_singular_value =
      EightPointSolver(x1_normalized, x2_normalized, F);
  EnforceFundamentalRank2Constraint(F);

  // Denormalize the fundamental matrix.
  *F = T2.transpose() * (*F) * T1;

  return smaller_singular_value;
}

// Seven-point algorithm.
// http://www.cs.unc.edu/~marc/tutorial/node55.html
double FundamentalFrom7CorrespondencesLinear(const Mat &x1,
                                             const Mat &x2,
                                             std::vector<Mat3> *F) {
  DCHECK_EQ(x1.rows(), 2);
  DCHECK_EQ(x1.cols(), 7);
  DCHECK_EQ(x1.rows(), x2.rows());
  DCHECK_EQ(x2.cols(), x2.cols());

  // Build a 9 x n matrix from point matches, where each row is equivalent to
  // the equation x'T*F*x = 0 for a single correspondence pair (x', x). The
  // domain of the matrix is a 9 element vector corresponding to F. The
  // nullspace should be rank two; the two dimensions correspond to the set of
  // F matrices satisfying the epipolar geometry.
  Matrix<double, 7, 9> A;
  for (int ii = 0; ii < 7; ++ii) {
    A(ii, 0) = x1(0, ii) * x2(0, ii);  // 0 represents x coords,
    A(ii, 1) = x1(1, ii) * x2(0, ii);  // 1 represents y coords.
    A(ii, 2) = x2(0, ii);
    A(ii, 3) = x1(0, ii) * x2(1, ii);
    A(ii, 4) = x1(1, ii) * x2(1, ii);
    A(ii, 5) = x2(1, ii);
    A(ii, 6) = x1(0, ii);
    A(ii, 7) = x1(1, ii);
    A(ii, 8) = 1.0;
  }

  // Find the two F matrices in the nullspace of A.
  Vec9 f1, f2;
  double s = Nullspace2(&A, &f1, &f2);
  Mat3 F1 = Map<RMat3>(f1.data());
  Mat3 F2 = Map<RMat3>(f2.data());

  // Then, use the condition det(F) = 0 to determine F. In other words, solve
  // det(F1 + a*F2) = 0 for a.
  double a = F1(0, 0), j = F2(0, 0),
         b = F1(0, 1), k = F2(0, 1),
         c = F1(0, 2), l = F2(0, 2),
         d = F1(1, 0), m = F2(1, 0),
         e = F1(1, 1), n = F2(1, 1),
         f = F1(1, 2), o = F2(1, 2),
         g = F1(2, 0), p = F2(2, 0),
         h = F1(2, 1), q = F2(2, 1),
         i = F1(2, 2), r = F2(2, 2);

  // Run fundamental_7point_coeffs.py to get the below coefficients.
  // The coefficients are in ascending powers of alpha, i.e. P[N]*x^N.
  double P[4] = {
    a*e*i + b*f*g + c*d*h - a*f*h - b*d*i - c*e*g,
    a*e*r + a*i*n + b*f*p + b*g*o + c*d*q + c*h*m + d*h*l + e*i*j + f*g*k -
    a*f*q - a*h*o - b*d*r - b*i*m - c*e*p - c*g*n - d*i*k - e*g*l - f*h*j,
    a*n*r + b*o*p + c*m*q + d*l*q + e*j*r + f*k*p + g*k*o + h*l*m + i*j*n -
    a*o*q - b*m*r - c*n*p - d*k*r - e*l*p - f*j*q - g*l*n - h*j*o - i*k*m,
    j*n*r + k*o*p + l*m*q - j*o*q - k*m*r - l*n*p,
  };

  // Solve for the roots of P[3]*x^3 + P[2]*x^2 + P[1]*x + P[0] = 0.
  double roots[3];
  int num_roots = SolveCubicPolynomial(P, roots);

  // Build the fundamental matrix for each solution.
  for (int kk = 0; kk < num_roots; ++kk)  {
    F->push_back(F1 + roots[kk] * F2);
  }
  return s;
}

double FundamentalFromCorrespondences7Point(const Mat &x1,
                                            const Mat &x2,
                                            std::vector<Mat3> *F) {
  DCHECK_EQ(x1.rows(), 2);
  DCHECK_GE(x1.cols(), 7);
  DCHECK_EQ(x1.rows(), x2.rows());
  DCHECK_EQ(x1.cols(), x2.cols());

  // Normalize the data.
  Mat3 T1, T2;
  PreconditionerFromPoints(x1, &T1);
  PreconditionerFromPoints(x2, &T2);
  Mat x1_normalized, x2_normalized;
  ApplyTransformationToPoints(x1, T1, &x1_normalized);
  ApplyTransformationToPoints(x2, T2, &x2_normalized);

  // Estimate the fundamental matrix.
  double smaller_singular_value =
    FundamentalFrom7CorrespondencesLinear(x1_normalized, x2_normalized, &(*F));

  for (int k = 0; k < F->size(); ++k) {
    Mat3 & Fmat = (*F)[k];
    // Denormalize the fundamental matrix.
    Fmat = T2.transpose() * Fmat * T1;
  }
  return smaller_singular_value;
}

void NormalizeFundamental(const Mat3 &F, Mat3 *F_normalized) {
  *F_normalized = F / FrobeniusNorm(F);
  if ((*F_normalized)(2, 2) < 0) {
    *F_normalized *= -1;
  }
}

double SampsonDistance(const Mat &F, const Vec2 &x1, const Vec2 &x2) {
  Vec3 x(x1(0), x1(1), 1.0);
  Vec3 y(x2(0), x2(1), 1.0);

  Vec3 F_x = F * x;
  Vec3 Ft_y = F.transpose() * y;
  double y_F_x = y.dot(F_x);

  return Square(y_F_x) / (  F_x.head<2>().squaredNorm()
                          + Ft_y.head<2>().squaredNorm());
}

double SymmetricEpipolarDistance(const Mat &F, const Vec2 &x1, const Vec2 &x2) {
  Vec3 x(x1(0), x1(1), 1.0);
  Vec3 y(x2(0), x2(1), 1.0);

  Vec3 F_x = F * x;
  Vec3 Ft_y = F.transpose() * y;
  double y_F_x = y.dot(F_x);

  return Square(y_F_x) * (  1 / F_x.head<2>().squaredNorm()
                          + 1 / Ft_y.head<2>().squaredNorm());
}

// HZ 9.6 pag 257 (formula 9.12)
void EssentialFromFundamental(const Mat3 &F,
                              const Mat3 &K1,
                              const Mat3 &K2,
                              Mat3 *E) {
  *E = K2.transpose() * F * K1;
}

// HZ 9.6 pag 257 (formula 9.12)
// Or http://ai.stanford.edu/~birch/projective/node20.html
void FundamentalFromEssential(const Mat3 &E,
                              const Mat3 &K1,
                              const Mat3 &K2,
                              Mat3 *F)  {
  *F = K2.inverse().transpose() * E * K1.inverse();
}

void RelativeCameraMotion(const Mat3 &R1,
                          const Vec3 &t1,
                          const Mat3 &R2,
                          const Vec3 &t2,
                          Mat3 *R,
                          Vec3 *t) {
  *R = R2 * R1.transpose();
  *t = t2 - (*R) * t1;
}

// HZ 9.6 pag 257
void EssentialFromRt(const Mat3 &R1,
                     const Vec3 &t1,
                     const Mat3 &R2,
                     const Vec3 &t2,
                     Mat3 *E) {
  Mat3 R;
  Vec3 t;
  RelativeCameraMotion(R1, t1, R2, t2, &R, &t);
  Mat3 Tx = CrossProductMatrix(t);
  *E = Tx * R;
}

// HZ 9.6 pag 259 (Result 9.19)
void MotionFromEssential(const Mat3 &E,
                         std::vector<Mat3> *Rs,
                         std::vector<Vec3> *ts) {
  Eigen::JacobiSVD<Mat3> USV(E, Eigen::ComputeFullU | Eigen::ComputeFullV);
  Mat3 U =  USV.matrixU();
  Vec3 d =  USV.singularValues();
  Mat3 Vt = USV.matrixV().transpose();

  // Last column of U is undetermined since d = (a a 0).
  if (U.determinant() < 0) {
    U.col(2) *= -1;
  }
  // Last row of Vt is undetermined since d = (a a 0).
  if (Vt.determinant() < 0) {
    Vt.row(2) *= -1;
  }

  Mat3 W;
  W << 0, -1,  0,
       1,  0,  0,
       0,  0,  1;

  Mat3 U_W_Vt = U * W * Vt;
  Mat3 U_Wt_Vt = U * W.transpose() * Vt;

  Rs->resize(4);
  (*Rs)[0] = U_W_Vt;
  (*Rs)[1] = U_W_Vt;
  (*Rs)[2] = U_Wt_Vt;
  (*Rs)[3] = U_Wt_Vt;

  ts->resize(4);
  (*ts)[0] =  U.col(2);
  (*ts)[1] = -U.col(2);
  (*ts)[2] =  U.col(2);
  (*ts)[3] = -U.col(2);
}

int MotionFromEssentialChooseSolution(const std::vector<Mat3> &Rs,
                                      const std::vector<Vec3> &ts,
                                      const Mat3 &K1,
                                      const Vec2 &x1,
                                      const Mat3 &K2,
                                      const Vec2 &x2) {
  DCHECK_EQ(4, Rs.size());
  DCHECK_EQ(4, ts.size());

  Mat34 P1, P2;
  Mat3 R1;
  Vec3 t1;
  R1.setIdentity();
  t1.setZero();
  P_From_KRt(K1, R1, t1, &P1);
  for (int i = 0; i < 4; ++i) {
    const Mat3 &R2 = Rs[i];
    const Vec3 &t2 = ts[i];
    P_From_KRt(K2, R2, t2, &P2);
    Vec3 X;
    TriangulateDLT(P1, x1, P2, x2, &X);
    double d1 = Depth(R1, t1, X);
    double d2 = Depth(R2, t2, X);
    // Test if point is front to the two cameras.
    if (d1 > 0 && d2 > 0) {
      return i;
    }
  }
  return -1;
}

bool MotionFromEssentialAndCorrespondence(const Mat3 &E,
                                          const Mat3 &K1,
                                          const Vec2 &x1,
                                          const Mat3 &K2,
                                          const Vec2 &x2,
                                          Mat3 *R,
                                          Vec3 *t) {
  std::vector<Mat3> Rs;
  std::vector<Vec3> ts;
  MotionFromEssential(E, &Rs, &ts);
  int solution = MotionFromEssentialChooseSolution(Rs, ts, K1, x1, K2, x2);
  if (solution >= 0) {
    *R = Rs[solution];
    *t = ts[solution];
    return true;
  } else {
    return false;
  }
}

}  // namespace libmv
