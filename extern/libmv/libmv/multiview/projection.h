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

#ifndef LIBMV_MULTIVIEW_PROJECTION_H_
#define LIBMV_MULTIVIEW_PROJECTION_H_

#include "libmv/numeric/numeric.h"

namespace libmv {

void P_From_KRt(const Mat3 &K, const Mat3 &R, const Vec3 &t, Mat34 *P);
void KRt_From_P(const Mat34 &P, Mat3 *K, Mat3 *R, Vec3 *t);

// Applies a change of basis to the image coordinates of the projection matrix
// so that the principal point becomes principal_point_new.
void ProjectionShiftPrincipalPoint(const Mat34 &P,
                                   const Vec2 &principal_point,
                                   const Vec2 &principal_point_new,
                                   Mat34 *P_new);

// Applies a change of basis to the image coordinates of the projection matrix
// so that the aspect ratio becomes aspect_ratio_new.  This is done by
// stretching the y axis.  The aspect ratio is defined as the quotient between
// the focal length of the y and the x axis.
void ProjectionChangeAspectRatio(const Mat34 &P,
                                 const Vec2 &principal_point,
                                 double aspect_ratio,
                                 double aspect_ratio_new,
                                 Mat34 *P_new);

void HomogeneousToEuclidean(const Mat &H, Mat *X);
void HomogeneousToEuclidean(const Mat3X &h, Mat2X *e);
void HomogeneousToEuclidean(const Mat4X &h, Mat3X *e);
void HomogeneousToEuclidean(const Vec3 &H, Vec2 *X);
void HomogeneousToEuclidean(const Vec4 &H, Vec3 *X);
inline Vec2 HomogeneousToEuclidean(const Vec3 &h) {
  return h.head<2>() / h(2);
}
inline Vec3 HomogeneousToEuclidean(const Vec4 &h) {
  return h.head<3>() / h(3);
}
inline Mat2X HomogeneousToEuclidean(const Mat3X &h) {
  Mat2X e(2, h.cols());
  e.row(0) = h.row(0).array() / h.row(2).array();
  e.row(1) = h.row(1).array() / h.row(2).array();
  return e;
}

void EuclideanToHomogeneous(const Mat &X, Mat *H);
inline Mat3X EuclideanToHomogeneous(const Mat2X &x) {
  Mat3X h(3, x.cols());
  h.block(0, 0, 2, x.cols()) = x;
  h.row(2).setOnes();
  return h;
}
inline void EuclideanToHomogeneous(const Mat2X &x, Mat3X *h) {
  h->resize(3, x.cols());
  h->block(0, 0, 2, x.cols()) = x;
  h->row(2).setOnes();
}
inline Mat4X EuclideanToHomogeneous(const Mat3X &x) {
  Mat4X h(4, x.cols());
  h.block(0, 0, 3, x.cols()) = x;
  h.row(3).setOnes();
  return h;
}
inline void EuclideanToHomogeneous(const Mat3X &x, Mat4X *h) {
  h->resize(4, x.cols());
  h->block(0, 0, 3, x.cols()) = x;
  h->row(3).setOnes();
}
void EuclideanToHomogeneous(const Vec2 &X, Vec3 *H);
void EuclideanToHomogeneous(const Vec3 &X, Vec4 *H);
inline Vec3 EuclideanToHomogeneous(const Vec2 &x) {
  return Vec3(x(0), x(1), 1);
}
inline Vec4 EuclideanToHomogeneous(const Vec3 &x) {
  return Vec4(x(0), x(1), x(2), 1);
}
// Conversion from image coordinates to normalized camera coordinates
void EuclideanToNormalizedCamera(const Mat2X &x, const Mat3 &K, Mat2X *n);
void HomogeneousToNormalizedCamera(const Mat3X &x, const Mat3 &K, Mat2X *n);

inline Vec2 Project(const Mat34 &P, const Vec3 &X) {
  Vec4 HX;
  HX << X, 1.0;
  Vec3 hx = P * HX;
  return hx.head<2>() / hx(2);
}

inline void Project(const Mat34 &P, const Vec4 &X, Vec3 *x) {
  *x = P * X;
}

inline void Project(const Mat34 &P, const Vec4 &X, Vec2 *x) {
  Vec3 hx = P * X;
  *x = hx.head<2>() / hx(2);
}

inline void Project(const Mat34 &P, const Vec3 &X, Vec3 *x) {
  Vec4 HX;
  HX << X, 1.0;
  Project(P, HX, x);
}

inline void Project(const Mat34 &P, const Vec3 &X, Vec2 *x) {
  Vec3 hx;
  Project(P, X, x);
  *x = hx.head<2>() / hx(2);
}

inline void Project(const Mat34 &P, const Mat4X &X, Mat2X *x) {
  x->resize(2, X.cols());
  for (int c = 0; c < X.cols(); ++c) {
    Vec3 hx = P * X.col(c);
    x->col(c) = hx.head<2>() / hx(2);
  }
}

inline Mat2X Project(const Mat34 &P, const Mat4X &X) {
  Mat2X x;
  Project(P, X, &x);
  return x;
}

inline void Project(const Mat34 &P, const Mat3X &X, Mat2X *x) {
  x->resize(2, X.cols());
  for (int c = 0; c < X.cols(); ++c) {
    Vec4 HX;
    HX << X.col(c), 1.0;
    Vec3 hx = P * HX;
    x->col(c) = hx.head<2>() / hx(2);
  }
}

inline void Project(const Mat34 &P, const Mat3X &X, const Vecu &ids, Mat2X *x) {
  x->resize(2, ids.size());
  Vec4 HX;
  Vec3 hx;
  for (int c = 0; c < ids.size(); ++c) {
    HX << X.col(ids[c]), 1.0;
    hx = P * HX;
    x->col(c) = hx.head<2>() / hx(2);
  }
}

inline Mat2X Project(const Mat34 &P, const Mat3X &X) {
  Mat2X x(2, X.cols());
  Project(P, X, &x);
  return x;
}

inline Mat2X Project(const Mat34 &P, const Mat3X &X, const Vecu &ids) {
  Mat2X x(2, ids.size());
  Project(P, X, ids, &x);
  return x;
}

double Depth(const Mat3 &R, const Vec3 &t, const Vec3 &X);
double Depth(const Mat3 &R, const Vec3 &t, const Vec4 &X);

/**
* Returns true if the homogenious 3D point X is in front of
* the camera P.
*/
inline bool isInFrontOfCamera(const Mat34 &P, const Vec4 &X) {
  double condition_1 = P.row(2).dot(X) * X[3];
  double condition_2 = X[2] * X[3];
  if (condition_1 > 0 && condition_2 > 0)
    return true;
  else
    return false;
}

inline bool isInFrontOfCamera(const Mat34 &P, const Vec3 &X) {
  Vec4 X_homo;
  X_homo.segment<3>(0) = X;
  X_homo(3) = 1;
  return isInFrontOfCamera( P, X_homo);
}

/**
* Transforms a 2D point from pixel image coordinates to a 2D point in
* normalized image coordinates.
*/
inline Vec2 ImageToNormImageCoordinates(Mat3 &Kinverse, Vec2 &x) {
  Vec3 x_h = Kinverse*EuclideanToHomogeneous(x);
  return HomogeneousToEuclidean( x_h );
}

/// Estimates the root mean square error (2D)
inline double RootMeanSquareError(const Mat2X &x_image,
                                  const Mat4X &X_world,
                                  const Mat34 &P) {
  size_t num_points = x_image.cols();
  Mat2X dx = Project(P, X_world) - x_image;
  return dx.norm() / num_points;
}

/// Estimates the root mean square error (2D)
inline double RootMeanSquareError(const Mat2X &x_image,
                                  const Mat3X &X_world,
                                  const Mat3 &K,
                                  const Mat3 &R,
                                  const Vec3 &t) {
  Mat34 P;
  P_From_KRt(K, R, t, &P);
  size_t num_points = x_image.cols();
  Mat2X dx = Project(P, X_world) - x_image;
  return dx.norm() / num_points;
}
}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_PROJECTION_H_
