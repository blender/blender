// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: keir@google.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)
//
// Templated functions for manipulating rotations. The templated
// functions are useful when implementing functors for automatic
// differentiation.
//
// In the following, the Quaternions are laid out as 4-vectors, thus:
//
//   q[0]  scalar part.
//   q[1]  coefficient of i.
//   q[2]  coefficient of j.
//   q[3]  coefficient of k.
//
// where: i*i = j*j = k*k = -1 and i*j = k, j*k = i, k*i = j.

#ifndef CERES_PUBLIC_ROTATION_H_
#define CERES_PUBLIC_ROTATION_H_

#include <algorithm>
#include <cmath>

#include "ceres/constants.h"
#include "ceres/internal/euler_angles.h"
#include "glog/logging.h"

namespace ceres {

// Trivial wrapper to index linear arrays as matrices, given a fixed
// column and row stride. When an array "T* array" is wrapped by a
//
//   (const) MatrixAdapter<T, row_stride, col_stride> M"
//
// the expression  M(i, j) is equivalent to
//
//   array[i * row_stride + j * col_stride]
//
// Conversion functions to and from rotation matrices accept
// MatrixAdapters to permit using row-major and column-major layouts,
// and rotation matrices embedded in larger matrices (such as a 3x4
// projection matrix).
template <typename T, int row_stride, int col_stride>
struct MatrixAdapter;

// Convenience functions to create a MatrixAdapter that treats the
// array pointed to by "pointer" as a 3x3 (contiguous) column-major or
// row-major matrix.
template <typename T>
MatrixAdapter<T, 1, 3> ColumnMajorAdapter3x3(T* pointer);

template <typename T>
MatrixAdapter<T, 3, 1> RowMajorAdapter3x3(T* pointer);

// Convert a value in combined axis-angle representation to a quaternion.
// The value angle_axis is a triple whose norm is an angle in radians,
// and whose direction is aligned with the axis of rotation,
// and quaternion is a 4-tuple that will contain the resulting quaternion.
// The implementation may be used with auto-differentiation up to the first
// derivative, higher derivatives may have unexpected results near the origin.
template <typename T>
void AngleAxisToQuaternion(const T* angle_axis, T* quaternion);

// Convert a quaternion to the equivalent combined axis-angle representation.
// The value quaternion must be a unit quaternion - it is not normalized first,
// and angle_axis will be filled with a value whose norm is the angle of
// rotation in radians, and whose direction is the axis of rotation.
// The implementation may be used with auto-differentiation up to the first
// derivative, higher derivatives may have unexpected results near the origin.
template <typename T>
void QuaternionToAngleAxis(const T* quaternion, T* angle_axis);

// Conversions between 3x3 rotation matrix (in column major order) and
// quaternion rotation representations. Templated for use with
// autodifferentiation.
template <typename T>
void RotationMatrixToQuaternion(const T* R, T* quaternion);

template <typename T, int row_stride, int col_stride>
void RotationMatrixToQuaternion(
    const MatrixAdapter<const T, row_stride, col_stride>& R, T* quaternion);

// Conversions between 3x3 rotation matrix (in column major order) and
// axis-angle rotation representations. Templated for use with
// autodifferentiation.
template <typename T>
void RotationMatrixToAngleAxis(const T* R, T* angle_axis);

template <typename T, int row_stride, int col_stride>
void RotationMatrixToAngleAxis(
    const MatrixAdapter<const T, row_stride, col_stride>& R, T* angle_axis);

template <typename T>
void AngleAxisToRotationMatrix(const T* angle_axis, T* R);

template <typename T, int row_stride, int col_stride>
void AngleAxisToRotationMatrix(
    const T* angle_axis, const MatrixAdapter<T, row_stride, col_stride>& R);

// Conversions between 3x3 rotation matrix (in row major order) and
// Euler angle (in degrees) rotation representations.
//
// The {pitch,roll,yaw} Euler angles are rotations around the {x,y,z}
// axes, respectively.  They are applied in that same order, so the
// total rotation R is Rz * Ry * Rx.
template <typename T>
void EulerAnglesToRotationMatrix(const T* euler, int row_stride, T* R);

template <typename T, int row_stride, int col_stride>
void EulerAnglesToRotationMatrix(
    const T* euler, const MatrixAdapter<T, row_stride, col_stride>& R);

// Convert a generic Euler Angle sequence (in radians) to a 3x3 rotation matrix.
//
// Euler Angles define a sequence of 3 rotations about a sequence of axes,
// typically taken to be the X, Y, or Z axes. The last axis may be the same as
// the first axis (e.g. ZYZ) per Euler's original definition of his angles
// (proper Euler angles) or not (e.g. ZYX / yaw-pitch-roll), per common usage in
// the nautical and aerospace fields (Tait-Bryan angles). The three rotations
// may be in a global frame of reference (Extrinsic) or in a body fixed frame of
// reference (Intrinsic) that moves with the rotating object.
//
// Internally, Euler Axis sequences are classified by Ken Shoemake's scheme from
// "Euler angle conversion", Graphics Gems IV, where a choice of axis for the
// first rotation and 3 binary choices:
// 1. Parity of the axis permutation. The axis sequence has Even parity if the
// second axis of rotation is 'greater-than' the first axis of rotation
// according to the order X<Y<Z<X, otherwise it has Odd parity.
// 2. Proper Euler Angles v.s. Tait-Bryan Angles
// 3. Extrinsic Rotations v.s. Intrinsic Rotations
// compactly represent all 24 possible Euler Angle Conventions
//
// One template parameter: EulerSystem must be explicitly given. This parameter
// is a tag named by 'Extrinsic' or 'Intrinsic' followed by three characters in
// the set '[XYZ]', specifying the axis sequence, e.g. ceres::ExtrinsicYZY
// (robotic arms), ceres::IntrinsicZYX (for aerospace), etc.
//
// The order of elements in the input array 'euler' follows the axis sequence
template <typename EulerSystem, typename T>
inline void EulerAnglesToRotation(const T* euler, T* R);

template <typename EulerSystem, typename T, int row_stride, int col_stride>
void EulerAnglesToRotation(const T* euler,
                           const MatrixAdapter<T, row_stride, col_stride>& R);

// Convert a 3x3 rotation matrix to a generic Euler Angle sequence (in radians)
//
// Euler Angles define a sequence of 3 rotations about a sequence of axes,
// typically taken to be the X, Y, or Z axes. The last axis may be the same as
// the first axis (e.g. ZYZ) per Euler's original definition of his angles
// (proper Euler angles) or not (e.g. ZYX / yaw-pitch-roll), per common usage in
// the nautical and aerospace fields (Tait-Bryan angles). The three rotations
// may be in a global frame of reference (Extrinsic) or in a body fixed frame of
// reference (Intrinsic) that moves with the rotating object.
//
// Internally, Euler Axis sequences are classified by Ken Shoemake's scheme from
// "Euler angle conversion", Graphics Gems IV, where a choice of axis for the
// first rotation and 3 binary choices:
// 1. Oddness of the axis permutation, that defines whether the second axis is
// 'greater-than' the first axis according to the order X>Y>Z>X)
// 2. Proper Euler Angles v.s. Tait-Bryan Angles
// 3. Extrinsic Rotations v.s. Intrinsic Rotations
// compactly represent all 24 possible Euler Angle Conventions
//
// One template parameter: EulerSystem must be explicitly given. This parameter
// is a tag named by 'Extrinsic' or 'Intrinsic' followed by three characters in
// the set '[XYZ]', specifying the axis sequence, e.g. ceres::ExtrinsicYZY
// (robotic arms), ceres::IntrinsicZYX (for aerospace), etc.
//
// The order of elements in the output array 'euler' follows the axis sequence
template <typename EulerSystem, typename T>
inline void RotationMatrixToEulerAngles(const T* R, T* euler);

template <typename EulerSystem, typename T, int row_stride, int col_stride>
void RotationMatrixToEulerAngles(
    const MatrixAdapter<const T, row_stride, col_stride>& R, T* euler);

// Convert a 4-vector to a 3x3 scaled rotation matrix.
//
// The choice of rotation is such that the quaternion [1 0 0 0] goes to an
// identity matrix and for small a, b, c the quaternion [1 a b c] goes to
// the matrix
//
//         [  0 -c  b ]
//   I + 2 [  c  0 -a ] + higher order terms
//         [ -b  a  0 ]
//
// which corresponds to a Rodrigues approximation, the last matrix being
// the cross-product matrix of [a b c]. Together with the property that
// R(q1 * q2) = R(q1) * R(q2) this uniquely defines the mapping from q to R.
//
// No normalization of the quaternion is performed, i.e.
// R = ||q||^2 * Q, where Q is an orthonormal matrix
// such that det(Q) = 1 and Q*Q' = I
//
// WARNING: The rotation matrix is ROW MAJOR
template <typename T>
inline void QuaternionToScaledRotation(const T q[4], T R[3 * 3]);

template <typename T, int row_stride, int col_stride>
inline void QuaternionToScaledRotation(
    const T q[4], const MatrixAdapter<T, row_stride, col_stride>& R);

// Same as above except that the rotation matrix is normalized by the
// Frobenius norm, so that R * R' = I (and det(R) = 1).
//
// WARNING: The rotation matrix is ROW MAJOR
template <typename T>
inline void QuaternionToRotation(const T q[4], T R[3 * 3]);

template <typename T, int row_stride, int col_stride>
inline void QuaternionToRotation(
    const T q[4], const MatrixAdapter<T, row_stride, col_stride>& R);

// Rotates a point pt by a quaternion q:
//
//   result = R(q) * pt
//
// Assumes the quaternion is unit norm. This assumption allows us to
// write the transform as (something)*pt + pt, as is clear from the
// formula below. If you pass in a quaternion with |q|^2 = 2 then you
// WILL NOT get back 2 times the result you get for a unit quaternion.
//
// Inplace rotation is not supported. pt and result must point to different
// memory locations, otherwise the result will be undefined.
template <typename T>
inline void UnitQuaternionRotatePoint(const T q[4], const T pt[3], T result[3]);

// With this function you do not need to assume that q has unit norm.
// It does assume that the norm is non-zero.
//
// Inplace rotation is not supported. pt and result must point to different
// memory locations, otherwise the result will be undefined.
template <typename T>
inline void QuaternionRotatePoint(const T q[4], const T pt[3], T result[3]);

// zw = z * w, where * is the Quaternion product between 4 vectors.
//
// Inplace quaternion product is not supported. The resulting quaternion zw must
// not share the memory with the input quaternion z and w, otherwise the result
// will be undefined.
template <typename T>
inline void QuaternionProduct(const T z[4], const T w[4], T zw[4]);

// xy = x cross y;
//
// Inplace cross product is not supported. The resulting vector x_cross_y must
// not share the memory with the input vectors x and y, otherwise the result
// will be undefined.
template <typename T>
inline void CrossProduct(const T x[3], const T y[3], T x_cross_y[3]);

template <typename T>
inline T DotProduct(const T x[3], const T y[3]);

// y = R(angle_axis) * x;
//
// Inplace rotation is not supported. pt and result must point to different
// memory locations, otherwise the result will be undefined.
template <typename T>
inline void AngleAxisRotatePoint(const T angle_axis[3],
                                 const T pt[3],
                                 T result[3]);

// --- IMPLEMENTATION

template <typename T, int row_stride, int col_stride>
struct MatrixAdapter {
  T* pointer_;
  explicit MatrixAdapter(T* pointer) : pointer_(pointer) {}

  T& operator()(int r, int c) const {
    return pointer_[r * row_stride + c * col_stride];
  }
};

template <typename T>
MatrixAdapter<T, 1, 3> ColumnMajorAdapter3x3(T* pointer) {
  return MatrixAdapter<T, 1, 3>(pointer);
}

template <typename T>
MatrixAdapter<T, 3, 1> RowMajorAdapter3x3(T* pointer) {
  return MatrixAdapter<T, 3, 1>(pointer);
}

template <typename T>
inline void AngleAxisToQuaternion(const T* angle_axis, T* quaternion) {
  using std::fpclassify;
  using std::hypot;
  const T& a0 = angle_axis[0];
  const T& a1 = angle_axis[1];
  const T& a2 = angle_axis[2];
  const T theta = hypot(a0, a1, a2);

  // For points not at the origin, the full conversion is numerically stable.
  if (fpclassify(theta) != FP_ZERO) {
    const T half_theta = theta * T(0.5);
    const T k = sin(half_theta) / theta;
    quaternion[0] = cos(half_theta);
    quaternion[1] = a0 * k;
    quaternion[2] = a1 * k;
    quaternion[3] = a2 * k;
  } else {
    // At the origin, sqrt() will produce NaN in the derivative since
    // the argument is zero.  By approximating with a Taylor series,
    // and truncating at one term, the value and first derivatives will be
    // computed correctly when Jets are used.
    const T k(0.5);
    quaternion[0] = T(1.0);
    quaternion[1] = a0 * k;
    quaternion[2] = a1 * k;
    quaternion[3] = a2 * k;
  }
}

template <typename T>
inline void QuaternionToAngleAxis(const T* quaternion, T* angle_axis) {
  using std::fpclassify;
  using std::hypot;
  const T& q1 = quaternion[1];
  const T& q2 = quaternion[2];
  const T& q3 = quaternion[3];
  const T sin_theta = hypot(q1, q2, q3);

  // For quaternions representing non-zero rotation, the conversion
  // is numerically stable.
  if (fpclassify(sin_theta) != FP_ZERO) {
    const T& cos_theta = quaternion[0];

    // If cos_theta is negative, theta is greater than pi/2, which
    // means that angle for the angle_axis vector which is 2 * theta
    // would be greater than pi.
    //
    // While this will result in the correct rotation, it does not
    // result in a normalized angle-axis vector.
    //
    // In that case we observe that 2 * theta ~ 2 * theta - 2 * pi,
    // which is equivalent saying
    //
    //   theta - pi = atan(sin(theta - pi), cos(theta - pi))
    //              = atan(-sin(theta), -cos(theta))
    //
    const T two_theta =
        T(2.0) * ((cos_theta < T(0.0)) ? atan2(-sin_theta, -cos_theta)
                                       : atan2(sin_theta, cos_theta));
    const T k = two_theta / sin_theta;
    angle_axis[0] = q1 * k;
    angle_axis[1] = q2 * k;
    angle_axis[2] = q3 * k;
  } else {
    // For zero rotation, sqrt() will produce NaN in the derivative since
    // the argument is zero.  By approximating with a Taylor series,
    // and truncating at one term, the value and first derivatives will be
    // computed correctly when Jets are used.
    const T k(2.0);
    angle_axis[0] = q1 * k;
    angle_axis[1] = q2 * k;
    angle_axis[2] = q3 * k;
  }
}

template <typename T>
void RotationMatrixToQuaternion(const T* R, T* quaternion) {
  RotationMatrixToQuaternion(ColumnMajorAdapter3x3(R), quaternion);
}

// This algorithm comes from "Quaternion Calculus and Fast Animation",
// Ken Shoemake, 1987 SIGGRAPH course notes
template <typename T, int row_stride, int col_stride>
void RotationMatrixToQuaternion(
    const MatrixAdapter<const T, row_stride, col_stride>& R, T* quaternion) {
  const T trace = R(0, 0) + R(1, 1) + R(2, 2);
  if (trace >= 0.0) {
    T t = sqrt(trace + T(1.0));
    quaternion[0] = T(0.5) * t;
    t = T(0.5) / t;
    quaternion[1] = (R(2, 1) - R(1, 2)) * t;
    quaternion[2] = (R(0, 2) - R(2, 0)) * t;
    quaternion[3] = (R(1, 0) - R(0, 1)) * t;
  } else {
    int i = 0;
    if (R(1, 1) > R(0, 0)) {
      i = 1;
    }

    if (R(2, 2) > R(i, i)) {
      i = 2;
    }

    const int j = (i + 1) % 3;
    const int k = (j + 1) % 3;
    T t = sqrt(R(i, i) - R(j, j) - R(k, k) + T(1.0));
    quaternion[i + 1] = T(0.5) * t;
    t = T(0.5) / t;
    quaternion[0] = (R(k, j) - R(j, k)) * t;
    quaternion[j + 1] = (R(j, i) + R(i, j)) * t;
    quaternion[k + 1] = (R(k, i) + R(i, k)) * t;
  }
}

// The conversion of a rotation matrix to the angle-axis form is
// numerically problematic when then rotation angle is close to zero
// or to Pi. The following implementation detects when these two cases
// occurs and deals with them by taking code paths that are guaranteed
// to not perform division by a small number.
template <typename T>
inline void RotationMatrixToAngleAxis(const T* R, T* angle_axis) {
  RotationMatrixToAngleAxis(ColumnMajorAdapter3x3(R), angle_axis);
}

template <typename T, int row_stride, int col_stride>
void RotationMatrixToAngleAxis(
    const MatrixAdapter<const T, row_stride, col_stride>& R, T* angle_axis) {
  T quaternion[4];
  RotationMatrixToQuaternion(R, quaternion);
  QuaternionToAngleAxis(quaternion, angle_axis);
  return;
}

template <typename T>
inline void AngleAxisToRotationMatrix(const T* angle_axis, T* R) {
  AngleAxisToRotationMatrix(angle_axis, ColumnMajorAdapter3x3(R));
}

template <typename T, int row_stride, int col_stride>
void AngleAxisToRotationMatrix(
    const T* angle_axis, const MatrixAdapter<T, row_stride, col_stride>& R) {
  using std::fpclassify;
  using std::hypot;
  static const T kOne = T(1.0);
  const T theta = hypot(angle_axis[0], angle_axis[1], angle_axis[2]);
  if (fpclassify(theta) != FP_ZERO) {
    // We want to be careful to only evaluate the square root if the
    // norm of the angle_axis vector is greater than zero. Otherwise
    // we get a division by zero.
    const T wx = angle_axis[0] / theta;
    const T wy = angle_axis[1] / theta;
    const T wz = angle_axis[2] / theta;

    const T costheta = cos(theta);
    const T sintheta = sin(theta);

    // clang-format off
    R(0, 0) =     costheta   + wx*wx*(kOne -    costheta);
    R(1, 0) =  wz*sintheta   + wx*wy*(kOne -    costheta);
    R(2, 0) = -wy*sintheta   + wx*wz*(kOne -    costheta);
    R(0, 1) =  wx*wy*(kOne - costheta)     - wz*sintheta;
    R(1, 1) =     costheta   + wy*wy*(kOne -    costheta);
    R(2, 1) =  wx*sintheta   + wy*wz*(kOne -    costheta);
    R(0, 2) =  wy*sintheta   + wx*wz*(kOne -    costheta);
    R(1, 2) = -wx*sintheta   + wy*wz*(kOne -    costheta);
    R(2, 2) =     costheta   + wz*wz*(kOne -    costheta);
    // clang-format on
  } else {
    // At zero, we switch to using the first order Taylor expansion.
    R(0, 0) = kOne;
    R(1, 0) = angle_axis[2];
    R(2, 0) = -angle_axis[1];
    R(0, 1) = -angle_axis[2];
    R(1, 1) = kOne;
    R(2, 1) = angle_axis[0];
    R(0, 2) = angle_axis[1];
    R(1, 2) = -angle_axis[0];
    R(2, 2) = kOne;
  }
}

template <typename EulerSystem, typename T>
inline void EulerAnglesToRotation(const T* euler, T* R) {
  EulerAnglesToRotation<EulerSystem>(euler, RowMajorAdapter3x3(R));
}

template <typename EulerSystem, typename T, int row_stride, int col_stride>
void EulerAnglesToRotation(const T* euler,
                           const MatrixAdapter<T, row_stride, col_stride>& R) {
  using std::cos;
  using std::sin;

  const auto [i, j, k] = EulerSystem::kAxes;

  T ea[3];
  ea[1] = euler[1];
  if constexpr (EulerSystem::kIsIntrinsic) {
    ea[0] = euler[2];
    ea[2] = euler[0];
  } else {
    ea[0] = euler[0];
    ea[2] = euler[2];
  }
  if constexpr (EulerSystem::kIsParityOdd) {
    ea[0] = -ea[0];
    ea[1] = -ea[1];
    ea[2] = -ea[2];
  }

  const T ci = cos(ea[0]);
  const T cj = cos(ea[1]);
  const T ch = cos(ea[2]);
  const T si = sin(ea[0]);
  const T sj = sin(ea[1]);
  const T sh = sin(ea[2]);
  const T cc = ci * ch;
  const T cs = ci * sh;
  const T sc = si * ch;
  const T ss = si * sh;
  if constexpr (EulerSystem::kIsProperEuler) {
    R(i, i) = cj;
    R(i, j) = sj * si;
    R(i, k) = sj * ci;
    R(j, i) = sj * sh;
    R(j, j) = -cj * ss + cc;
    R(j, k) = -cj * cs - sc;
    R(k, i) = -sj * ch;
    R(k, j) = cj * sc + cs;
    R(k, k) = cj * cc - ss;
  } else {
    R(i, i) = cj * ch;
    R(i, j) = sj * sc - cs;
    R(i, k) = sj * cc + ss;
    R(j, i) = cj * sh;
    R(j, j) = sj * ss + cc;
    R(j, k) = sj * cs - sc;
    R(k, i) = -sj;
    R(k, j) = cj * si;
    R(k, k) = cj * ci;
  }
}

template <typename EulerSystem, typename T>
inline void RotationMatrixToEulerAngles(const T* R, T* euler) {
  RotationMatrixToEulerAngles<EulerSystem>(RowMajorAdapter3x3(R), euler);
}

template <typename EulerSystem, typename T, int row_stride, int col_stride>
void RotationMatrixToEulerAngles(
    const MatrixAdapter<const T, row_stride, col_stride>& R, T* euler) {
  using std::atan2;
  using std::fpclassify;
  using std::hypot;

  const auto [i, j, k] = EulerSystem::kAxes;

  T ea[3];
  if constexpr (EulerSystem::kIsProperEuler) {
    const T sy = hypot(R(i, j), R(i, k));
    if (fpclassify(sy) != FP_ZERO) {
      ea[0] = atan2(R(i, j), R(i, k));
      ea[1] = atan2(sy, R(i, i));
      ea[2] = atan2(R(j, i), -R(k, i));
    } else {
      ea[0] = atan2(-R(j, k), R(j, j));
      ea[1] = atan2(sy, R(i, i));
      ea[2] = T(0.0);
    }
  } else {
    const T cy = hypot(R(i, i), R(j, i));
    if (fpclassify(cy) != FP_ZERO) {
      ea[0] = atan2(R(k, j), R(k, k));
      ea[1] = atan2(-R(k, i), cy);
      ea[2] = atan2(R(j, i), R(i, i));
    } else {
      ea[0] = atan2(-R(j, k), R(j, j));
      ea[1] = atan2(-R(k, i), cy);
      ea[2] = T(0.0);
    }
  }
  if constexpr (EulerSystem::kIsParityOdd) {
    ea[0] = -ea[0];
    ea[1] = -ea[1];
    ea[2] = -ea[2];
  }
  euler[1] = ea[1];
  if constexpr (EulerSystem::kIsIntrinsic) {
    euler[0] = ea[2];
    euler[2] = ea[0];
  } else {
    euler[0] = ea[0];
    euler[2] = ea[2];
  }

  // Proper euler angles are defined for angles in
  //   [-pi, pi) x [0, pi / 2) x [-pi, pi)
  // which is enforced here
  if constexpr (EulerSystem::kIsProperEuler) {
    const T kPi(constants::pi);
    const T kTwoPi(2.0 * kPi);
    if (euler[1] < T(0.0) || ea[1] > kPi) {
      euler[0] += kPi;
      euler[1] = -euler[1];
      euler[2] -= kPi;
    }

    for (int i = 0; i < 3; ++i) {
      if (euler[i] < -kPi) {
        euler[i] += kTwoPi;
      } else if (euler[i] > kPi) {
        euler[i] -= kTwoPi;
      }
    }
  }
}

template <typename T>
inline void EulerAnglesToRotationMatrix(const T* euler,
                                        const int row_stride_parameter,
                                        T* R) {
  EulerAnglesToRotationMatrix(euler, RowMajorAdapter3x3(R));
}

template <typename T, int row_stride, int col_stride>
void EulerAnglesToRotationMatrix(
    const T* euler, const MatrixAdapter<T, row_stride, col_stride>& R) {
  const double kPi = 3.14159265358979323846;
  const T degrees_to_radians(kPi / 180.0);

  const T pitch(euler[0] * degrees_to_radians);
  const T roll(euler[1] * degrees_to_radians);
  const T yaw(euler[2] * degrees_to_radians);

  const T c1 = cos(yaw);
  const T s1 = sin(yaw);
  const T c2 = cos(roll);
  const T s2 = sin(roll);
  const T c3 = cos(pitch);
  const T s3 = sin(pitch);

  R(0, 0) = c1 * c2;
  R(0, 1) = -s1 * c3 + c1 * s2 * s3;
  R(0, 2) = s1 * s3 + c1 * s2 * c3;

  R(1, 0) = s1 * c2;
  R(1, 1) = c1 * c3 + s1 * s2 * s3;
  R(1, 2) = -c1 * s3 + s1 * s2 * c3;

  R(2, 0) = -s2;
  R(2, 1) = c2 * s3;
  R(2, 2) = c2 * c3;
}

template <typename T>
inline void QuaternionToScaledRotation(const T q[4], T R[3 * 3]) {
  QuaternionToScaledRotation(q, RowMajorAdapter3x3(R));
}

template <typename T, int row_stride, int col_stride>
inline void QuaternionToScaledRotation(
    const T q[4], const MatrixAdapter<T, row_stride, col_stride>& R) {
  // Make convenient names for elements of q.
  T a = q[0];
  T b = q[1];
  T c = q[2];
  T d = q[3];
  // This is not to eliminate common sub-expression, but to
  // make the lines shorter so that they fit in 80 columns!
  T aa = a * a;
  T ab = a * b;
  T ac = a * c;
  T ad = a * d;
  T bb = b * b;
  T bc = b * c;
  T bd = b * d;
  T cc = c * c;
  T cd = c * d;
  T dd = d * d;

  // clang-format off
  R(0, 0) = aa + bb - cc - dd; R(0, 1) = T(2) * (bc - ad);  R(0, 2) = T(2) * (ac + bd);
  R(1, 0) = T(2) * (ad + bc);  R(1, 1) = aa - bb + cc - dd; R(1, 2) = T(2) * (cd - ab);
  R(2, 0) = T(2) * (bd - ac);  R(2, 1) = T(2) * (ab + cd);  R(2, 2) = aa - bb - cc + dd;
  // clang-format on
}

template <typename T>
inline void QuaternionToRotation(const T q[4], T R[3 * 3]) {
  QuaternionToRotation(q, RowMajorAdapter3x3(R));
}

template <typename T, int row_stride, int col_stride>
inline void QuaternionToRotation(
    const T q[4], const MatrixAdapter<T, row_stride, col_stride>& R) {
  QuaternionToScaledRotation(q, R);

  T normalizer = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
  normalizer = T(1) / normalizer;

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      R(i, j) *= normalizer;
    }
  }
}

template <typename T>
inline void UnitQuaternionRotatePoint(const T q[4],
                                      const T pt[3],
                                      T result[3]) {
  DCHECK_NE(pt, result) << "Inplace rotation is not supported.";

  // clang-format off
  T uv0 = q[2] * pt[2] - q[3] * pt[1];
  T uv1 = q[3] * pt[0] - q[1] * pt[2];
  T uv2 = q[1] * pt[1] - q[2] * pt[0];
  uv0 += uv0;
  uv1 += uv1;
  uv2 += uv2;
  result[0] = pt[0] + q[0] * uv0;
  result[1] = pt[1] + q[0] * uv1;
  result[2] = pt[2] + q[0] * uv2;
  result[0] += q[2] * uv2 - q[3] * uv1;
  result[1] += q[3] * uv0 - q[1] * uv2;
  result[2] += q[1] * uv1 - q[2] * uv0;
  // clang-format on
}

template <typename T>
inline void QuaternionRotatePoint(const T q[4], const T pt[3], T result[3]) {
  DCHECK_NE(pt, result) << "Inplace rotation is not supported.";

  // 'scale' is 1 / norm(q).
  const T scale =
      T(1) / sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);

  // Make unit-norm version of q.
  const T unit[4] = {
      scale * q[0],
      scale * q[1],
      scale * q[2],
      scale * q[3],
  };

  UnitQuaternionRotatePoint(unit, pt, result);
}

template <typename T>
inline void QuaternionProduct(const T z[4], const T w[4], T zw[4]) {
  DCHECK_NE(z, zw) << "Inplace quaternion product is not supported.";
  DCHECK_NE(w, zw) << "Inplace quaternion product is not supported.";

  // clang-format off
  zw[0] = z[0] * w[0] - z[1] * w[1] - z[2] * w[2] - z[3] * w[3];
  zw[1] = z[0] * w[1] + z[1] * w[0] + z[2] * w[3] - z[3] * w[2];
  zw[2] = z[0] * w[2] - z[1] * w[3] + z[2] * w[0] + z[3] * w[1];
  zw[3] = z[0] * w[3] + z[1] * w[2] - z[2] * w[1] + z[3] * w[0];
  // clang-format on
}

// xy = x cross y;
template <typename T>
inline void CrossProduct(const T x[3], const T y[3], T x_cross_y[3]) {
  DCHECK_NE(x, x_cross_y) << "Inplace cross product is not supported.";
  DCHECK_NE(y, x_cross_y) << "Inplace cross product is not supported.";

  x_cross_y[0] = x[1] * y[2] - x[2] * y[1];
  x_cross_y[1] = x[2] * y[0] - x[0] * y[2];
  x_cross_y[2] = x[0] * y[1] - x[1] * y[0];
}

template <typename T>
inline T DotProduct(const T x[3], const T y[3]) {
  return (x[0] * y[0] + x[1] * y[1] + x[2] * y[2]);
}

template <typename T>
inline void AngleAxisRotatePoint(const T angle_axis[3],
                                 const T pt[3],
                                 T result[3]) {
  DCHECK_NE(pt, result) << "Inplace rotation is not supported.";
  using std::fpclassify;
  using std::hypot;

  const T theta = hypot(angle_axis[0], angle_axis[1], angle_axis[2]);

  if (fpclassify(theta) != FP_ZERO) {
    // Away from zero, use the rodriguez formula
    //
    //   result = pt costheta +
    //            (w x pt) * sintheta +
    //            w (w . pt) (1 - costheta)
    //
    // We want to be careful to only evaluate the square root if the
    // norm of the angle_axis vector is greater than zero. Otherwise
    // we get a division by zero.
    //
    const T costheta = cos(theta);
    const T sintheta = sin(theta);
    const T theta_inverse = T(1.0) / theta;

    const T w[3] = {angle_axis[0] * theta_inverse,
                    angle_axis[1] * theta_inverse,
                    angle_axis[2] * theta_inverse};

    // Explicitly inlined evaluation of the cross product for
    // performance reasons.
    const T w_cross_pt[3] = {w[1] * pt[2] - w[2] * pt[1],
                             w[2] * pt[0] - w[0] * pt[2],
                             w[0] * pt[1] - w[1] * pt[0]};
    const T tmp =
        (w[0] * pt[0] + w[1] * pt[1] + w[2] * pt[2]) * (T(1.0) - costheta);

    result[0] = pt[0] * costheta + w_cross_pt[0] * sintheta + w[0] * tmp;
    result[1] = pt[1] * costheta + w_cross_pt[1] * sintheta + w[1] * tmp;
    result[2] = pt[2] * costheta + w_cross_pt[2] * sintheta + w[2] * tmp;
  } else {
    // At zero, the first order Taylor approximation of the rotation
    // matrix R corresponding to a vector w and angle theta is
    //
    //   R = I + hat(w) * sin(theta)
    //
    // But sintheta ~ theta and theta * w = angle_axis, which gives us
    //
    //  R = I + hat(angle_axis)
    //
    // and actually performing multiplication with the point pt, gives us
    // R * pt = pt + angle_axis x pt.
    //
    // Switching to the Taylor expansion at zero provides meaningful
    // derivatives when evaluated using Jets.
    //
    // Explicitly inlined evaluation of the cross product for
    // performance reasons.
    const T w_cross_pt[3] = {angle_axis[1] * pt[2] - angle_axis[2] * pt[1],
                             angle_axis[2] * pt[0] - angle_axis[0] * pt[2],
                             angle_axis[0] * pt[1] - angle_axis[1] * pt[0]};

    result[0] = pt[0] + w_cross_pt[0];
    result[1] = pt[1] + w_cross_pt[1];
    result[2] = pt[2] + w_cross_pt[2];
  }
}

}  // namespace ceres

#endif  // CERES_PUBLIC_ROTATION_H_
