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

#include "libmv/multiview/euclidean_resection.h"

#include <cmath>
#include <limits>

#include <Eigen/SVD>
#include <Eigen/Geometry>

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/projection.h"

namespace libmv {
namespace euclidean_resection {

typedef unsigned int uint;

bool EuclideanResection(const Mat2X &x_camera,
                        const Mat3X &X_world,
                        Mat3 *R, Vec3 *t,
                        ResectionMethod method) {
  switch (method) {
    case RESECTION_ANSAR_DANIILIDIS:
      EuclideanResectionAnsarDaniilidis(x_camera, X_world, R, t);
      break;
    case RESECTION_EPNP:
      return EuclideanResectionEPnP(x_camera, X_world, R, t);
      break;
    case RESECTION_PPNP:
      return EuclideanResectionPPnP(x_camera, X_world, R, t);
      break;
    default:
      LOG(FATAL) << "Unknown resection method.";
  }
  return false;
}

bool EuclideanResection(const Mat &x_image,
                        const Mat3X &X_world,
                        const Mat3 &K,
                        Mat3 *R, Vec3 *t,
                        ResectionMethod method) {
  CHECK(x_image.rows() == 2 || x_image.rows() == 3)
    << "Invalid size for x_image: "
    << x_image.rows() << "x" << x_image.cols();

  Mat2X x_camera;
  if (x_image.rows() == 2) {
    EuclideanToNormalizedCamera(x_image, K, &x_camera);
  } else if (x_image.rows() == 3) {
    HomogeneousToNormalizedCamera(x_image, K, &x_camera);
  }
  return EuclideanResection(x_camera, X_world, R, t, method);
}

void AbsoluteOrientation(const Mat3X &X,
                         const Mat3X &Xp,
                         Mat3 *R,
                         Vec3 *t) {
  int num_points = X.cols();
  Vec3 C  = X.rowwise().sum() / num_points;   // Centroid of X.
  Vec3 Cp = Xp.rowwise().sum() / num_points;  // Centroid of Xp.

  // Normalize the two point sets.
  Mat3X Xn(3, num_points), Xpn(3, num_points);
  for (int i = 0; i < num_points; ++i) {
    Xn.col(i)  = X.col(i) - C;
    Xpn.col(i) = Xp.col(i) - Cp;
  }

  // Construct the N matrix (pg. 635).
  double Sxx = Xn.row(0).dot(Xpn.row(0));
  double Syy = Xn.row(1).dot(Xpn.row(1));
  double Szz = Xn.row(2).dot(Xpn.row(2));
  double Sxy = Xn.row(0).dot(Xpn.row(1));
  double Syx = Xn.row(1).dot(Xpn.row(0));
  double Sxz = Xn.row(0).dot(Xpn.row(2));
  double Szx = Xn.row(2).dot(Xpn.row(0));
  double Syz = Xn.row(1).dot(Xpn.row(2));
  double Szy = Xn.row(2).dot(Xpn.row(1));

  Mat4 N;
  N << Sxx + Syy + Szz, Syz - Szy,        Szx - Sxz,        Sxy - Syx,
       Syz - Szy,       Sxx - Syy - Szz,  Sxy + Syx,        Szx + Sxz,
       Szx - Sxz,       Sxy + Syx,       -Sxx + Syy - Szz,  Syz + Szy,
       Sxy - Syx,       Szx + Sxz,        Syz + Szy,       -Sxx - Syy + Szz;

  // Find the unit quaternion q that maximizes qNq. It is the eigenvector
  // corresponding to the lagest eigenvalue.
  Vec4 q = N.jacobiSvd(Eigen::ComputeFullU).matrixU().col(0);

  // Retrieve the 3x3 rotation matrix.
  Vec4 qq = q.array() * q.array();
  double q0q1 = q(0) * q(1);
  double q0q2 = q(0) * q(2);
  double q0q3 = q(0) * q(3);
  double q1q2 = q(1) * q(2);
  double q1q3 = q(1) * q(3);
  double q2q3 = q(2) * q(3);

  (*R) << qq(0) + qq(1) - qq(2) - qq(3),
          2 * (q1q2 - q0q3),
          2 * (q1q3 + q0q2),
          2 * (q1q2+ q0q3),
          qq(0) - qq(1) + qq(2) - qq(3),
          2 * (q2q3 - q0q1),
          2 * (q1q3 - q0q2),
          2 * (q2q3 + q0q1),
          qq(0) - qq(1) - qq(2) + qq(3);

  // Fix the handedness of the R matrix.
  if (R->determinant() < 0) {
    R->row(2) = -R->row(2);
  }
  // Compute the final translation.
  *t = Cp - *R * C;
}

// Convert i and j indices of the original variables into their quadratic
// permutation single index. It follows that t_ij = t_ji.
static int IJToPointIndex(int i, int j, int num_points) {
  // Always make sure that j is bigger than i. This handles t_ij = t_ji.
  if (j < i) {
    std::swap(i, j);
  }
  int idx;
  int num_permutation_rows = num_points * (num_points - 1) / 2;

  // All t_ii's are located at the end of the t vector after all t_ij's.
  if (j == i) {
    idx = num_permutation_rows + i;
  } else {
    int offset = (num_points - i - 1) * (num_points - i) / 2;
    idx = (num_permutation_rows - offset + j - i - 1);
  }
  return idx;
};

// Convert i and j indexes of the solution for lambda to their linear indexes.
static int IJToIndex(int i, int j, int num_lambda) {
  if (j < i) {
    std::swap(i, j);
  }
  int A = num_lambda * (num_lambda + 1) / 2;
  int B = num_lambda - i;
  int C = B * (B + 1) / 2;
  int idx = A - C + j - i;
  return idx;
};

static int Sign(double value) {
  return (value < 0) ? -1 : 1;
};

// Organizes a square matrix into a single row constraint on the elements of
// Lambda to create the constraints in equation (5) in "Linear Pose Estimation
// from Points or Lines", by Ansar, A. and Daniilidis, PAMI 2003. vol. 25, no.
// 5.
static Vec MatrixToConstraint(const Mat &A,
                              int num_k_columns,
                              int num_lambda) {
  Vec C(num_k_columns);
  C.setZero();
  int idx = 0;
  for (int i = 0; i < num_lambda; ++i) {
    for (int j = i; j < num_lambda; ++j) {
      C(idx) = A(i, j);
      if (i != j) {
        C(idx) += A(j, i);
      }
      ++idx;
    }
  }
  return C;
}

// Normalizes the columns of vectors.
static void NormalizeColumnVectors(Mat3X *vectors) {
  int num_columns = vectors->cols();
  for (int i = 0; i < num_columns; ++i) {
    vectors->col(i).normalize();
  }
}

void EuclideanResectionAnsarDaniilidis(const Mat2X &x_camera,
                                       const Mat3X &X_world,
                                       Mat3 *R,
                                       Vec3 *t) {
  CHECK(x_camera.cols() == X_world.cols());
  CHECK(x_camera.cols() > 3);

  int num_points = x_camera.cols();

  // Copy the normalized camera coords into 3 vectors and normalize them so
  // that they are unit vectors from the camera center.
  Mat3X x_camera_unit(3, num_points);
  x_camera_unit.block(0, 0, 2, num_points) = x_camera;
  x_camera_unit.row(2).setOnes();
  NormalizeColumnVectors(&x_camera_unit);

  int num_m_rows = num_points * (num_points - 1) / 2;
  int num_tt_variables = num_points * (num_points + 1) / 2;
  int num_m_columns = num_tt_variables + 1;
  Mat M(num_m_columns, num_m_columns);
  M.setZero();
  Matu ij_index(num_tt_variables, 2);

  // Create the constraint equations for the t_ij variables (7) and arrange
  // them into the M matrix (8). Also store the initial (i, j) indices.
  int row = 0;
  for (int i = 0; i < num_points; ++i) {
    for (int j = i+1; j < num_points; ++j) {
      M(row, row) = -2 * x_camera_unit.col(i).dot(x_camera_unit.col(j));
      M(row, num_m_rows + i) = x_camera_unit.col(i).dot(x_camera_unit.col(i));
      M(row, num_m_rows + j) = x_camera_unit.col(j).dot(x_camera_unit.col(j));
      Vec3 Xdiff = X_world.col(i) - X_world.col(j);
      double center_to_point_distance = Xdiff.norm();
      M(row, num_m_columns - 1) =
          - center_to_point_distance * center_to_point_distance;
      ij_index(row, 0) = i;
      ij_index(row, 1) = j;
      ++row;
    }
    ij_index(i + num_m_rows, 0) = i;
    ij_index(i + num_m_rows, 1) = i;
  }

  int num_lambda = num_points + 1;  // Dimension of the null space of M.
  Mat V = M.jacobiSvd(Eigen::ComputeFullV).matrixV().block(0,
                                                           num_m_rows,
                                                           num_m_columns,
                                                           num_lambda);

  // TODO(vess): The number of constraint equations in K (num_k_rows) must be
  // (num_points + 1) * (num_points + 2)/2. This creates a performance issue
  // for more than 4 points. It is fine for 4 points at the moment with 18
  // instead of 15 equations.
  int num_k_rows = num_m_rows + num_points *
                   (num_points*(num_points-1)/2 - num_points+1);
  int num_k_columns = num_lambda * (num_lambda + 1) / 2;
  Mat K(num_k_rows, num_k_columns);
  K.setZero();

  // Construct the first part of the K matrix corresponding to (t_ii, t_jk) for
  // i != j.
  int counter_k_row = 0;
  for (int idx1 = num_m_rows; idx1 < num_tt_variables; ++idx1) {
    for (int idx2 = 0; idx2 < num_m_rows; ++idx2) {
      unsigned int i = ij_index(idx1, 0);
      unsigned int j = ij_index(idx2, 0);
      unsigned int k = ij_index(idx2, 1);

      if (i != j && i != k) {
        int idx3 = IJToPointIndex(i, j, num_points);
        int idx4 = IJToPointIndex(i, k, num_points);

        K.row(counter_k_row) =
            MatrixToConstraint(V.row(idx1).transpose() * V.row(idx2)-
                               V.row(idx3).transpose() * V.row(idx4),
                               num_k_columns,
                               num_lambda);
        ++counter_k_row;
      }
    }
  }

  // Construct the second part of the K matrix corresponding to (t_ii,t_jk) for
  // j==k.
  for (int idx1 = num_m_rows; idx1 < num_tt_variables; ++idx1) {
    for (int idx2 = idx1 + 1; idx2 < num_tt_variables; ++idx2) {
      unsigned int i = ij_index(idx1, 0);
      unsigned int j = ij_index(idx2, 0);
      unsigned int k = ij_index(idx2, 1);

      int idx3 = IJToPointIndex(i, j, num_points);
      int idx4 = IJToPointIndex(i, k, num_points);

      K.row(counter_k_row) =
          MatrixToConstraint(V.row(idx1).transpose() * V.row(idx2)-
                             V.row(idx3).transpose() * V.row(idx4),
                             num_k_columns,
                             num_lambda);
      ++counter_k_row;
    }
  }
  Vec L_sq = K.jacobiSvd(Eigen::ComputeFullV).matrixV().col(num_k_columns - 1);

  // Pivot on the largest element for numerical stability. Afterwards recover
  // the sign of the lambda solution.
  double max_L_sq_value = fabs(L_sq(IJToIndex(0, 0, num_lambda)));
  int max_L_sq_index = 1;
  for (int i = 1; i < num_lambda; ++i) {
    double abs_sq_value = fabs(L_sq(IJToIndex(i, i, num_lambda)));
    if (max_L_sq_value < abs_sq_value) {
      max_L_sq_value = abs_sq_value;
      max_L_sq_index = i;
    }
  }
  // Ensure positiveness of the largest value corresponding to lambda_ii.
  L_sq = L_sq * Sign(L_sq(IJToIndex(max_L_sq_index,
                                    max_L_sq_index,
                                    num_lambda)));

  Vec L(num_lambda);
  L(max_L_sq_index) = sqrt(L_sq(IJToIndex(max_L_sq_index,
                                          max_L_sq_index,
                                          num_lambda)));

  for (int i = 0; i < num_lambda; ++i) {
    if (i != max_L_sq_index) {
      L(i) = L_sq(IJToIndex(max_L_sq_index, i, num_lambda)) / L(max_L_sq_index);
    }
  }

  // Correct the scale using the fact that the last constraint is equal to 1.
  L = L / (V.row(num_m_columns - 1).dot(L));
  Vec X = V * L;

  // Recover the distances from the camera center to the 3D points Q.
  Vec d(num_points);
  d.setZero();
  for (int c_point = num_m_rows; c_point < num_tt_variables; ++c_point) {
    d(c_point - num_m_rows) = sqrt(X(c_point));
  }

  // Create the 3D points in the camera system.
  Mat X_cam(3, num_points);
  for (int c_point = 0; c_point < num_points; ++c_point) {
    X_cam.col(c_point) = d(c_point) * x_camera_unit.col(c_point);
  }
  // Recover the camera translation and rotation.
  AbsoluteOrientation(X_world, X_cam, R, t);
}

// Selects 4 virtual control points using mean and PCA.
static void SelectControlPoints(const Mat3X &X_world,
                                Mat *X_centered,
                                Mat34 *X_control_points) {
  size_t num_points = X_world.cols();

  // The first virtual control point, C0, is the centroid.
  Vec mean, variance;
  MeanAndVarianceAlongRows(X_world, &mean, &variance);
  X_control_points->col(0) = mean;

  // Computes PCA
  X_centered->resize(3, num_points);
  for (size_t c = 0; c < num_points; c++) {
    X_centered->col(c) = X_world.col(c) - mean;
  }
  Mat3 X_centered_sq = (*X_centered) * X_centered->transpose();
  Eigen::JacobiSVD<Mat3> X_centered_sq_svd(X_centered_sq, Eigen::ComputeFullU);
  Vec3 w = X_centered_sq_svd.singularValues();
  Mat3 u = X_centered_sq_svd.matrixU();
  for (size_t c = 0; c < 3; c++) {
    double k = sqrt(w(c) / num_points);
    X_control_points->col(c + 1) = mean + k * u.col(c);
  }
}

// Computes the barycentric coordinates for all real points
static void ComputeBarycentricCoordinates(const Mat3X &X_world_centered,
                                          const Mat34 &X_control_points,
                                          Mat4X *alphas) {
  size_t num_points = X_world_centered.cols();
  Mat3 C2;
  for (size_t c = 1; c < 4; c++) {
    C2.col(c-1) = X_control_points.col(c) - X_control_points.col(0);
  }

  Mat3 C2inv = C2.inverse();
  Mat3X a = C2inv * X_world_centered;

  alphas->resize(4, num_points);
  alphas->setZero();
  alphas->block(1, 0, 3, num_points) = a;
  for (size_t c = 0; c < num_points; c++) {
    (*alphas)(0, c) = 1.0 - alphas->col(c).sum();
  }
}

// Estimates the coordinates of all real points in the camera coordinate frame
static void ComputePointsCoordinatesInCameraFrame(
    const Mat4X &alphas,
    const Vec4 &betas,
    const Eigen::Matrix<double, 12, 12> &U,
    Mat3X *X_camera) {
  size_t num_points = alphas.cols();

  // Estimates the control points in the camera reference frame.
  Mat34 C2b; C2b.setZero();
  for (size_t cu = 0; cu < 4; cu++) {
    for (size_t c = 0; c < 4; c++) {
      C2b.col(c) += betas(cu) * U.block(11 - cu, c * 3, 1, 3).transpose();
    }
  }

  // Estimates the 3D points in the camera reference frame
  X_camera->resize(3, num_points);
  for (size_t c = 0; c < num_points; c++) {
    X_camera->col(c) = C2b * alphas.col(c);
  }

  // Check the sign of the z coordinate of the points (should be positive)
  uint num_z_neg = 0;
  for (size_t i = 0; i < X_camera->cols(); ++i) {
    if ((*X_camera)(2, i) < 0) {
      num_z_neg++;
    }
  }

  // If more than 50% of z are negative, we change the signs
  if (num_z_neg > 0.5 * X_camera->cols()) {
    C2b = -C2b;
    *X_camera = -(*X_camera);
  }
}

bool EuclideanResectionEPnP(const Mat2X &x_camera,
                            const Mat3X &X_world,
                            Mat3 *R, Vec3 *t) {
  CHECK(x_camera.cols() == X_world.cols());
  CHECK(x_camera.cols() > 3);
  size_t num_points = X_world.cols();

  // Select the control points.
  Mat34 X_control_points;
  Mat X_centered;
  SelectControlPoints(X_world, &X_centered, &X_control_points);

  // Compute the barycentric coordinates.
  Mat4X alphas(4, num_points);
  ComputeBarycentricCoordinates(X_centered, X_control_points, &alphas);

  // Estimates the M matrix with the barycentric coordinates
  Mat M(2 * num_points, 12);
  Eigen::Matrix<double, 2, 12> sub_M;
  for (size_t c = 0; c < num_points; c++) {
    double a0 = alphas(0, c);
    double a1 = alphas(1, c);
    double a2 = alphas(2, c);
    double a3 = alphas(3, c);
    double ui = x_camera(0, c);
    double vi = x_camera(1, c);
    M.block(2*c, 0, 2, 12) << a0, 0,
                              a0*(-ui), a1, 0,
                              a1*(-ui), a2, 0,
                              a2*(-ui), a3, 0,
                              a3*(-ui), 0,
                              a0, a0*(-vi), 0,
                              a1, a1*(-vi), 0,
                              a2, a2*(-vi), 0,
                              a3, a3*(-vi);
  }

  // TODO(julien): Avoid the transpose by rewriting the u2.block() calls.
  Eigen::JacobiSVD<Mat> MtMsvd(M.transpose()*M, Eigen::ComputeFullU);
  Eigen::Matrix<double, 12, 12> u2 = MtMsvd.matrixU().transpose();

  // Estimate the L matrix.
  Eigen::Matrix<double, 6, 3> dv1;
  Eigen::Matrix<double, 6, 3> dv2;
  Eigen::Matrix<double, 6, 3> dv3;
  Eigen::Matrix<double, 6, 3> dv4;

  dv1.row(0) = u2.block(11, 0, 1, 3) - u2.block(11, 3, 1, 3);
  dv1.row(1) = u2.block(11, 0, 1, 3) - u2.block(11, 6, 1, 3);
  dv1.row(2) = u2.block(11, 0, 1, 3) - u2.block(11, 9, 1, 3);
  dv1.row(3) = u2.block(11, 3, 1, 3) - u2.block(11, 6, 1, 3);
  dv1.row(4) = u2.block(11, 3, 1, 3) - u2.block(11, 9, 1, 3);
  dv1.row(5) = u2.block(11, 6, 1, 3) - u2.block(11, 9, 1, 3);
  dv2.row(0) = u2.block(10, 0, 1, 3) - u2.block(10, 3, 1, 3);
  dv2.row(1) = u2.block(10, 0, 1, 3) - u2.block(10, 6, 1, 3);
  dv2.row(2) = u2.block(10, 0, 1, 3) - u2.block(10, 9, 1, 3);
  dv2.row(3) = u2.block(10, 3, 1, 3) - u2.block(10, 6, 1, 3);
  dv2.row(4) = u2.block(10, 3, 1, 3) - u2.block(10, 9, 1, 3);
  dv2.row(5) = u2.block(10, 6, 1, 3) - u2.block(10, 9, 1, 3);
  dv3.row(0) = u2.block(9,  0, 1, 3) - u2.block(9,  3, 1, 3);
  dv3.row(1) = u2.block(9,  0, 1, 3) - u2.block(9,  6, 1, 3);
  dv3.row(2) = u2.block(9,  0, 1, 3) - u2.block(9,  9, 1, 3);
  dv3.row(3) = u2.block(9,  3, 1, 3) - u2.block(9,  6, 1, 3);
  dv3.row(4) = u2.block(9,  3, 1, 3) - u2.block(9,  9, 1, 3);
  dv3.row(5) = u2.block(9,  6, 1, 3) - u2.block(9,  9, 1, 3);
  dv4.row(0) = u2.block(8,  0, 1, 3) - u2.block(8,  3, 1, 3);
  dv4.row(1) = u2.block(8,  0, 1, 3) - u2.block(8,  6, 1, 3);
  dv4.row(2) = u2.block(8,  0, 1, 3) - u2.block(8,  9, 1, 3);
  dv4.row(3) = u2.block(8,  3, 1, 3) - u2.block(8,  6, 1, 3);
  dv4.row(4) = u2.block(8,  3, 1, 3) - u2.block(8,  9, 1, 3);
  dv4.row(5) = u2.block(8,  6, 1, 3) - u2.block(8,  9, 1, 3);

  Eigen::Matrix<double, 6, 10> L;
  for (size_t r = 0; r < 6; r++) {
    L.row(r) << dv1.row(r).dot(dv1.row(r)),
          2.0 * dv1.row(r).dot(dv2.row(r)),
                dv2.row(r).dot(dv2.row(r)),
          2.0 * dv1.row(r).dot(dv3.row(r)),
          2.0 * dv2.row(r).dot(dv3.row(r)),
                dv3.row(r).dot(dv3.row(r)),
          2.0 * dv1.row(r).dot(dv4.row(r)),
          2.0 * dv2.row(r).dot(dv4.row(r)),
          2.0 * dv3.row(r).dot(dv4.row(r)),
                dv4.row(r).dot(dv4.row(r));
  }
  Vec6 rho;
  rho << (X_control_points.col(0) - X_control_points.col(1)).squaredNorm(),
         (X_control_points.col(0) - X_control_points.col(2)).squaredNorm(),
         (X_control_points.col(0) - X_control_points.col(3)).squaredNorm(),
         (X_control_points.col(1) - X_control_points.col(2)).squaredNorm(),
         (X_control_points.col(1) - X_control_points.col(3)).squaredNorm(),
         (X_control_points.col(2) - X_control_points.col(3)).squaredNorm();

  // There are three possible solutions based on the three approximations of L
  // (betas). Below, each one is solved for then the best one is chosen.
  Mat3X X_camera;
  Mat3 K; K.setIdentity();
  vector<Mat3> Rs(3);
  vector<Vec3> ts(3);
  Vec rmse(3);

  // At one point this threshold was 1e-3, and caused no end of problems in
  // Blender by causing frames to not resect when they would have worked fine.
  // When the resect failed, the projective followup is run leading to worse
  // results, and often the dreaded "flipping" where objects get flipped
  // between frames. Instead, disable the check for now, always succeeding. The
  // ultimate check is always reprojection error anyway.
  //
  // TODO(keir): Decide if setting this to infinity, effectively disabling the
  // check, is the right approach. So far this seems the case.
   double kSuccessThreshold = std::numeric_limits<double>::max();

  // Find the first possible solution for R, t corresponding to:
  // Betas          = [b00 b01 b11 b02 b12 b22 b03 b13 b23 b33]
  // Betas_approx_1 = [b00 b01     b02         b03]
  Vec4 betas = Vec4::Zero();
  Eigen::Matrix<double, 6, 4> l_6x4;
  for (size_t r = 0; r < 6; r++) {
    l_6x4.row(r) << L(r, 0), L(r, 1), L(r, 3), L(r, 6);
  }
  Eigen::JacobiSVD<Mat> svd_of_l4(l_6x4,
                                  Eigen::ComputeFullU | Eigen::ComputeFullV);
  Vec4 b4 = svd_of_l4.solve(rho);
  if ((l_6x4 * b4).isApprox(rho, kSuccessThreshold)) {
    if (b4(0) < 0) {
      b4 = -b4;
    }
    b4(0) =  std::sqrt(b4(0));
    betas << b4(0), b4(1) / b4(0), b4(2) / b4(0), b4(3) / b4(0);
    ComputePointsCoordinatesInCameraFrame(alphas, betas, u2, &X_camera);
    AbsoluteOrientation(X_world, X_camera, &Rs[0], &ts[0]);
    rmse(0) = RootMeanSquareError(x_camera, X_world, K, Rs[0], ts[0]);
  } else {
    LOG(ERROR) << "First approximation of beta not good enough.";
    ts[0].setZero();
    rmse(0) = std::numeric_limits<double>::max();
  }

  // Find the second possible solution for R, t corresponding to:
  // Betas          = [b00 b01 b11 b02 b12 b22 b03 b13 b23 b33]
  // Betas_approx_2 = [b00 b01 b11]
  betas.setZero();
  Eigen::Matrix<double, 6, 3> l_6x3;
  l_6x3 = L.block(0, 0, 6, 3);
  Eigen::JacobiSVD<Mat> svdOfL3(l_6x3,
                                Eigen::ComputeFullU | Eigen::ComputeFullV);
  Vec3 b3 = svdOfL3.solve(rho);
  VLOG(2) << " rho = " << rho;
  VLOG(2) << " l_6x3 * b3 = " << l_6x3 * b3;
  if ((l_6x3 * b3).isApprox(rho, kSuccessThreshold)) {
    if (b3(0) < 0) {
      betas(0) = std::sqrt(-b3(0));
      betas(1) = (b3(2) < 0) ? std::sqrt(-b3(2)) : 0;
    } else {
      betas(0) = std::sqrt(b3(0));
      betas(1) = (b3(2) > 0) ? std::sqrt(b3(2)) : 0;
    }
    if (b3(1) < 0) {
      betas(0) = -betas(0);
    }
    betas(2) = 0;
    betas(3) = 0;
    ComputePointsCoordinatesInCameraFrame(alphas, betas, u2, &X_camera);
    AbsoluteOrientation(X_world, X_camera, &Rs[1], &ts[1]);
    rmse(1) = RootMeanSquareError(x_camera, X_world, K, Rs[1], ts[1]);
  } else {
    LOG(ERROR) << "Second approximation of beta not good enough.";
    ts[1].setZero();
    rmse(1) = std::numeric_limits<double>::max();
  }

  // Find the third possible solution for R, t corresponding to:
  // Betas          = [b00 b01 b11 b02 b12 b22 b03 b13 b23 b33]
  // Betas_approx_3 = [b00 b01 b11 b02 b12]
  betas.setZero();
  Eigen::Matrix<double, 6, 5> l_6x5;
  l_6x5 = L.block(0, 0, 6, 5);
  Eigen::JacobiSVD<Mat> svdOfL5(l_6x5,
                                Eigen::ComputeFullU | Eigen::ComputeFullV);
  Vec5 b5 = svdOfL5.solve(rho);
  if ((l_6x5 * b5).isApprox(rho, kSuccessThreshold)) {
    if (b5(0) < 0) {
      betas(0) = std::sqrt(-b5(0));
      if (b5(2) < 0) {
        betas(1) = std::sqrt(-b5(2));
      } else {
        b5(2) = 0;
      }
    } else {
      betas(0) = std::sqrt(b5(0));
      if (b5(2) > 0) {
        betas(1) = std::sqrt(b5(2));
      } else {
        b5(2) = 0;
      }
    }
    if (b5(1) < 0) {
      betas(0) = -betas(0);
    }
    betas(2) = b5(3) / betas(0);
    betas(3) = 0;
    ComputePointsCoordinatesInCameraFrame(alphas, betas, u2, &X_camera);
    AbsoluteOrientation(X_world, X_camera, &Rs[2], &ts[2]);
    rmse(2) = RootMeanSquareError(x_camera, X_world, K, Rs[2], ts[2]);
  } else {
    LOG(ERROR) << "Third approximation of beta not good enough.";
    ts[2].setZero();
    rmse(2) = std::numeric_limits<double>::max();
  }

  // Finally, with all three solutions, select the (R, t) with the best RMSE.
  VLOG(2) << "RMSE for solution 0: " << rmse(0);
  VLOG(2) << "RMSE for solution 1: " << rmse(1);
  VLOG(2) << "RMSE for solution 2: " << rmse(2);
  size_t n = 0;
  if (rmse(1) < rmse(0)) {
    n = 1;
  }
  if (rmse(2) < rmse(n)) {
    n = 2;
  }
  if (rmse(n) == std::numeric_limits<double>::max()) {
    LOG(ERROR) << "All three possibilities failed. Reporting failure.";
    return false;
  }

  VLOG(1) << "RMSE for best solution #" << n << ": " << rmse(n);
  *R = Rs[n];
  *t = ts[n];

  // TODO(julien): Improve the solutions with non-linear refinement.
  return true;
}
  
/*
 
 Straight from the paper:
 http://www.diegm.uniud.it/fusiello/papers/3dimpvt12-b.pdf
 
 function [R T] = ppnp(P,S,tol)
 % input
 % P  : matrix (nx3) image coordinates in camera reference [u v 1]
 % S  : matrix (nx3) coordinates in world reference [X Y Z]
 % tol: exit threshold
 %
 % output
 % R : matrix (3x3) rotation (world-to-camera)
 % T : vector (3x1) translation (world-to-camera)
 %
 n = size(P,1);
 Z = zeros(n);
 e = ones(n,1);
 A = eye(n)-((e*e’)./n);
 II = e./n;
 err = +Inf;
 E_old = 1000*ones(n,3);
 while err>tol
   [U,˜,V] = svd(P’*Z*A*S);
   VT = V’;
   R=U*[1 0 0; 0 1 0; 0 0 det(U*VT)]*VT;
   PR = P*R;
   c = (S-Z*PR)’*II;
   Y = S-e*c’;
   Zmindiag = diag(PR*Y’)./(sum(P.*P,2));
   Zmindiag(Zmindiag<0)=0;
   Z = diag(Zmindiag);
   E = Y-Z*PR;
   err = norm(E-E_old,’fro’);
   E_old = E;
 end
 T = -R*c;
 end
 
 */
// TODO(keir): Re-do all the variable names and add comments matching the paper.
// This implementation has too much of the terseness of the original. On the
// other hand, it did work on the first try.
bool EuclideanResectionPPnP(const Mat2X &x_camera,
                            const Mat3X &X_world,
                            Mat3 *R, Vec3 *t) {
  int n = x_camera.cols();
  Mat Z = Mat::Zero(n, n);
  Vec e = Vec::Ones(n);
  Mat A = Mat::Identity(n, n) - (e * e.transpose() / n);
  Vec II = e / n;
  
  Mat P(n, 3);
  P.col(0) = x_camera.row(0);
  P.col(1) = x_camera.row(1);
  P.col(2).setConstant(1.0);
  
  Mat S = X_world.transpose();
  
  double error = std::numeric_limits<double>::infinity();
  Mat E_old = 1000 * Mat::Ones(n, 3);
  
  Vec3 c;
  Mat E(n, 3);
  
  int iteration = 0;
  double tolerance = 1e-5;
  // TODO(keir): The limit of 100 can probably be reduced, but this will require
  // some investigation.
  while (error > tolerance && iteration < 100) {
    Mat3 tmp = P.transpose() * Z * A * S;
    Eigen::JacobiSVD<Mat3> svd(tmp, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Mat3 U = svd.matrixU();
    Mat3 VT = svd.matrixV().transpose();
    Vec3 s;
    s << 1, 1, (U * VT).determinant();
    *R = U * s.asDiagonal() * VT;
    Mat PR = P * *R;  // n x 3
    c = (S - Z*PR).transpose() * II;
    Mat Y = S - e*c.transpose();  // n x 3
    Vec Zmindiag = (PR * Y.transpose()).diagonal()
        .cwiseQuotient(P.rowwise().squaredNorm());
    for (int i = 0; i < n; ++i) {
      Zmindiag[i] = std::max(Zmindiag[i], 0.0);
    }
    Z = Zmindiag.asDiagonal();
    E = Y - Z*PR;
    error = (E - E_old).norm();
    LOG(INFO) << "PPnP error(" << (iteration++) << "): " << error;
    E_old = E;
  }
  *t = -*R*c;

  // TODO(keir): Figure out what the failure cases are. Is it too many
  // iterations? Spend some time going through the math figuring out if there
  // is some way to detect that the algorithm is going crazy, and return false.
  return true;
}


}  // namespace resection
}  // namespace libmv
