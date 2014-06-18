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

#ifndef LIBMV_MULTIVIEW_HOMOGRAPHY_ERRORS_H_
#define LIBMV_MULTIVIEW_HOMOGRAPHY_ERRORS_H_

#include "libmv/multiview/projection.h"

namespace libmv {
namespace homography {
namespace homography2D {

 /**
   * Structure for estimating the asymmetric error between a vector x2 and the 
   * transformed x1 such that 
   *   Error = ||x2 - Psi(H * x1)||^2
   * where Psi is the function that transforms homogeneous to euclidean coords.
   * \note It should be distributed as Chi-squared with k = 2.
   */
struct AsymmetricError {
  /**
   * Computes the asymmetric residuals between a set of 2D points x2 and the 
   * transformed 2D point set x1 such that
   *   Residuals_i = x2_i - Psi(H * x1_i) 
   * where Psi is the function that transforms homogeneous to euclidean coords.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1  A set of 2D points (2xN or 3xN matrix of column vectors).
   * \param[in]  x2  A set of 2D points (2xN or 3xN matrix of column vectors).
   * \param[out] dx  A 2xN matrix of column vectors of residuals errors
   */
  static void Residuals(const Mat &H, const Mat &x1,
                        const Mat &x2, Mat2X *dx) {
    dx->resize(2, x1.cols());
    Mat3X x2h_est;
    if (x1.rows() == 2)
      x2h_est = H * EuclideanToHomogeneous(static_cast<Mat2X>(x1));
    else
      x2h_est = H * x1;
    dx->row(0) = x2h_est.row(0).array() / x2h_est.row(2).array();
    dx->row(1) = x2h_est.row(1).array() / x2h_est.row(2).array();
    if (x2.rows() == 2)
      *dx = x2 - *dx;
    else
      *dx = HomogeneousToEuclidean(static_cast<Mat3X>(x2)) - *dx;
  }
  /**
   * Computes the asymmetric residuals between a 2D point x2 and the transformed 
   * 2D point x1 such that
   *   Residuals = x2 - Psi(H * x1) 
   * where Psi is the function that transforms homogeneous to euclidean coords.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[in]  x2 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[out] dx  A vector of size 2 of the residual error
   */
  static void Residuals(const Mat &H, const Vec &x1,
                        const Vec &x2, Vec2 *dx) {
    Vec3 x2h_est;
    if (x1.rows() == 2)
      x2h_est = H * EuclideanToHomogeneous(static_cast<Vec2>(x1));
    else
      x2h_est = H * x1;
    if (x2.rows() == 2)
      *dx = x2 - x2h_est.head<2>() / x2h_est[2];
    else
      *dx = HomogeneousToEuclidean(static_cast<Vec3>(x2)) -
              x2h_est.head<2>() / x2h_est[2];
  }
  /**
   * Computes the squared norm of the residuals between a set of 2D points x2 
   * and the transformed 2D point set x1 such that
   *   Error = || x2 - Psi(H * x1) ||^2
   * where Psi is the function that transforms homogeneous to euclidean coords.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1  A set of 2D points (2xN or 3xN matrix of column vectors).
   * \param[in]  x2  A set of 2D points (2xN or 3xN matrix of column vectors).
   * \return  The squared norm of the asymmetric residuals errors
   */
  static double Error(const Mat &H, const Mat &x1, const Mat &x2) {
    Mat2X dx;
    Residuals(H, x1, x2, &dx);
    return dx.squaredNorm();
  }
  /**
   * Computes the squared norm of the residuals between a 2D point x2 and the 
   * transformed 2D point x1 such that  rms = || x2 - Psi(H * x1) ||^2 
   * where Psi is the function that transforms homogeneous to euclidean coords.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[in]  x2 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \return  The squared norm of the asymmetric residual error
   */
  static double Error(const Mat &H, const Vec &x1, const Vec &x2) {
    Vec2 dx;
    Residuals(H, x1, x2, &dx);
    return dx.squaredNorm();
  }
};

 /**
   * Structure for estimating the symmetric error 
   * between a vector x2 and the transformed x1 such that 
   *   Error = ||x2 - Psi(H * x1)||^2 + ||x1 - Psi(H^-1 * x2)||^2
   * where Psi is the function that transforms homogeneous to euclidean coords.
   * \note It should be distributed as Chi-squared with k = 4.
   */
struct SymmetricError {
  /**
   * Computes the squared norm of the residuals between x2 and the 
   * transformed x1 such that  
   *   Error = ||x2 - Psi(H * x1)||^2 + ||x1 - Psi(H^-1 * x2)||^2
   * where Psi is the function that transforms homogeneous to euclidean coords.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[in]  x2 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \return  The squared norm of the symmetric residuals errors
   */
  static double Error(const Mat &H, const Vec &x1, const Vec &x2) {
    // TODO(keir): This is awesomely inefficient because it does a 3x3
    // inversion for each evaluation.
    Mat3 Hinv = H.inverse();
    return AsymmetricError::Error(H,    x1, x2) +
           AsymmetricError::Error(Hinv, x2, x1);
  }
  // TODO(julien) Add residuals function \see AsymmetricError
};
 /**
   * Structure for estimating the algebraic error (cross product) 
   * between a vector x2 and the transformed x1 such that 
   *   Error = ||[x2] * H * x1||^^2
   * where [x2] is the skew matrix of x2.
   */
struct AlgebraicError {
  // TODO(julien) Make an AlgebraicError2Rows and AlgebraicError3Rows

  /**
   * Computes the algebraic residuals (cross product) between a set of 2D 
   * points x2 and the transformed 2D point set x1 such that 
   *   [x2] * H * x1  where [x2] is the skew matrix of x2.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1  A set of 2D points (2xN or 3xN matrix of column vectors).
   * \param[in]  x2  A set of 2D points (2xN or 3xN matrix of column vectors).
   * \param[out] dx  A 3xN matrix of column vectors of residuals errors
   */
  static void Residuals(const Mat &H, const Mat &x1,
                        const Mat &x2, Mat3X *dx) {
    dx->resize(3, x1.cols());
    Vec3 col;
    for (int i = 0; i < x1.cols(); ++i) {
      Residuals(H, x1.col(i), x2.col(i), &col);
      dx->col(i) = col;
    }
  }
  /**
   * Computes the algebraic residuals (cross product) between a 2D point x2 
   * and the transformed 2D point x1 such that 
   *   [x2] * H * x1  where [x2] is the skew matrix of x2.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[in]  x2 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[out] dx  A vector of size 3 of the residual error
   */
  static void Residuals(const Mat &H, const Vec &x1,
                        const Vec &x2, Vec3 *dx) {
    Vec3 x2h_est;
    if (x1.rows() == 2)
      x2h_est = H * EuclideanToHomogeneous(static_cast<Vec2>(x1));
    else
      x2h_est = H * x1;
    if (x2.rows() == 2)
      *dx = SkewMat(EuclideanToHomogeneous(static_cast<Vec2>(x2))) * x2h_est;
    else
      *dx = SkewMat(x2) * x2h_est;
    // TODO(julien) This is inefficient since it creates an
    // identical 3x3 skew matrix for each evaluation.
  }
  /**
   * Computes the squared norm of the algebraic residuals between a set of 2D 
   * points x2 and the transformed 2D point set x1 such that 
   *   [x2] * H * x1  where [x2] is the skew matrix of x2.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1 A set of 2D points (2xN or 3xN matrix of column vectors).
   * \param[in]  x2 A set of 2D points (2xN or 3xN matrix of column vectors).
   * \return  The squared norm of the asymmetric residuals errors
   */
  static double Error(const Mat &H, const Mat &x1, const Mat &x2) {
    Mat3X dx;
    Residuals(H, x1, x2, &dx);
    return dx.squaredNorm();
  }
  /**
   * Computes the squared norm of the algebraic residuals between a 2D point x2 
   * and the transformed 2D point x1 such that 
   * [x2] * H * x1  where [x2] is the skew matrix of x2.
   *
   * \param[in]  H The 3x3 homography matrix.
   * The estimated homography should approximatelly hold the condition y = H x.
   * \param[in]  x1 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \param[in]  x2 A 2D point (vector of size 2 or 3 (euclidean/homogeneous)) 
   * \return  The squared norm of the asymmetric residual error
   */
  static double Error(const Mat &H, const Vec &x1, const Vec &x2) {
    Vec3 dx;
    Residuals(H, x1, x2, &dx);
    return dx.squaredNorm();
  }
};
// TODO(keir): Add error based on ideal points.

}  // namespace homography2D
// TODO(julien) add homography3D errors
}  // namespace homography
}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_HOMOGRAPHY_ERRORS_H_
