// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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

namespace ceres {

// Convert a value in combined axis-angle representation to a quaternion.
// The value angle_axis is a triple whose norm is an angle in radians,
// and whose direction is aligned with the axis of rotation,
// and quaternion is a 4-tuple that will contain the resulting quaternion.
// The implementation may be used with auto-differentiation up to the first
// derivative, higher derivatives may have unexpected results near the origin.
template<typename T>
void AngleAxisToQuaternion(T const* angle_axis, T* quaternion);

// Convert a quaternion to the equivalent combined axis-angle representation.
// The value quaternion must be a unit quaternion - it is not normalized first,
// and angle_axis will be filled with a value whose norm is the angle of
// rotation in radians, and whose direction is the axis of rotation.
// The implemention may be used with auto-differentiation up to the first
// derivative, higher derivatives may have unexpected results near the origin.
template<typename T>
void QuaternionToAngleAxis(T const* quaternion, T* angle_axis);

// Conversions between 3x3 rotation matrix (in column major order) and
// axis-angle rotation representations.  Templated for use with
// autodifferentiation.
template <typename T>
void RotationMatrixToAngleAxis(T const * R, T * angle_axis);
template <typename T>
void AngleAxisToRotationMatrix(T const * angle_axis, T * R);

// Conversions between 3x3 rotation matrix (in row major order) and
// Euler angle (in degrees) rotation representations.
//
// The {pitch,roll,yaw} Euler angles are rotations around the {x,y,z}
// axes, respectively.  They are applied in that same order, so the
// total rotation R is Rz * Ry * Rx.
template <typename T>
void EulerAnglesToRotationMatrix(const T* euler, int row_stride, T* R);

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
// The rotation matrix is row-major.
//
// No normalization of the quaternion is performed, i.e.
// R = ||q||^2 * Q, where Q is an orthonormal matrix
// such that det(Q) = 1 and Q*Q' = I
template <typename T> inline
void QuaternionToScaledRotation(const T q[4], T R[3 * 3]);

// Same as above except that the rotation matrix is normalized by the
// Frobenius norm, so that R * R' = I (and det(R) = 1).
template <typename T> inline
void QuaternionToRotation(const T q[4], T R[3 * 3]);

// Rotates a point pt by a quaternion q:
//
//   result = R(q) * pt
//
// Assumes the quaternion is unit norm. This assumption allows us to
// write the transform as (something)*pt + pt, as is clear from the
// formula below. If you pass in a quaternion with |q|^2 = 2 then you
// WILL NOT get back 2 times the result you get for a unit quaternion.
template <typename T> inline
void UnitQuaternionRotatePoint(const T q[4], const T pt[3], T result[3]);

// With this function you do not need to assume that q has unit norm.
// It does assume that the norm is non-zero.
template <typename T> inline
void QuaternionRotatePoint(const T q[4], const T pt[3], T result[3]);

// zw = z * w, where * is the Quaternion product between 4 vectors.
template<typename T> inline
void QuaternionProduct(const T z[4], const T w[4], T zw[4]);

// xy = x cross y;
template<typename T> inline
void CrossProduct(const T x[3], const T y[3], T x_cross_y[3]);

template<typename T> inline
T DotProduct(const T x[3], const T y[3]);

// y = R(angle_axis) * x;
template<typename T> inline
void AngleAxisRotatePoint(const T angle_axis[3], const T pt[3], T result[3]);

// --- IMPLEMENTATION

// Duplicate rather than decorate every use of cmath with _USE_MATH_CONSTANTS.
// Necessitated by Windows.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#define CERES_NEED_M_PI_UNDEF
#endif

template<typename T>
inline void AngleAxisToQuaternion(const T* angle_axis, T* quaternion) {
  const T &a0 = angle_axis[0];
  const T &a1 = angle_axis[1];
  const T &a2 = angle_axis[2];
  const T theta_squared = a0 * a0 + a1 * a1 + a2 * a2;

  // For points not at the origin, the full conversion is numerically stable.
  if (theta_squared > T(0.0)) {
    const T theta = sqrt(theta_squared);
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

template<typename T>
inline void QuaternionToAngleAxis(const T* quaternion, T* angle_axis) {
  const T &q1 = quaternion[1];
  const T &q2 = quaternion[2];
  const T &q3 = quaternion[3];
  const T sin_squared = q1 * q1 + q2 * q2 + q3 * q3;

  // For quaternions representing non-zero rotation, the conversion
  // is numerically stable.
  if (sin_squared > T(0.0)) {
    const T sin_theta = sqrt(sin_squared);
    const T k = T(2.0) * atan2(sin_theta, quaternion[0]) / sin_theta;
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

// The conversion of a rotation matrix to the angle-axis form is
// numerically problematic when then rotation angle is close to zero
// or to Pi. The following implementation detects when these two cases
// occurs and deals with them by taking code paths that are guaranteed
// to not perform division by a small number.
template <typename T>
inline void RotationMatrixToAngleAxis(const T * R, T * angle_axis) {
  // x = k * 2 * sin(theta), where k is the axis of rotation.
  angle_axis[0] = R[5] - R[7];
  angle_axis[1] = R[6] - R[2];
  angle_axis[2] = R[1] - R[3];

  static const T kOne = T(1.0);
  static const T kTwo = T(2.0);

  // Since the right hand side may give numbers just above 1.0 or
  // below -1.0 leading to atan misbehaving, we threshold.
  T costheta = std::min(std::max((R[0] + R[4] + R[8] - kOne) / kTwo,
                                 T(-1.0)),
                        kOne);

  // sqrt is guaranteed to give non-negative results, so we only
  // threshold above.
  T sintheta = std::min(sqrt(angle_axis[0] * angle_axis[0] +
                             angle_axis[1] * angle_axis[1] +
                             angle_axis[2] * angle_axis[2]) / kTwo,
                        kOne);

  // Use the arctan2 to get the right sign on theta
  const T theta = atan2(sintheta, costheta);

  // Case 1: sin(theta) is large enough, so dividing by it is not a
  // problem. We do not use abs here, because while jets.h imports
  // std::abs into the namespace, here in this file, abs resolves to
  // the int version of the function, which returns zero always.
  //
  // We use a threshold much larger then the machine epsilon, because
  // if sin(theta) is small, not only do we risk overflow but even if
  // that does not occur, just dividing by a small number will result
  // in numerical garbage. So we play it safe.
  static const double kThreshold = 1e-12;
  if ((sintheta > kThreshold) || (sintheta < -kThreshold)) {
    const T r = theta / (kTwo * sintheta);
    for (int i = 0; i < 3; ++i) {
      angle_axis[i] *= r;
    }
    return;
  }

  // Case 2: theta ~ 0, means sin(theta) ~ theta to a good
  // approximation.
  if (costheta > 0) {
    const T kHalf = T(0.5);
    for (int i = 0; i < 3; ++i) {
      angle_axis[i] *= kHalf;
    }
    return;
  }

  // Case 3: theta ~ pi, this is the hard case. Since theta is large,
  // and sin(theta) is small. Dividing by theta by sin(theta) will
  // either give an overflow or worse still numerically meaningless
  // results. Thus we use an alternate more complicated formula
  // here.

  // Since cos(theta) is negative, division by (1-cos(theta)) cannot
  // overflow.
  const T inv_one_minus_costheta = kOne / (kOne - costheta);

  // We now compute the absolute value of coordinates of the axis
  // vector using the diagonal entries of R. To resolve the sign of
  // these entries, we compare the sign of angle_axis[i]*sin(theta)
  // with the sign of sin(theta). If they are the same, then
  // angle_axis[i] should be positive, otherwise negative.
  for (int i = 0; i < 3; ++i) {
    angle_axis[i] = theta * sqrt((R[i*4] - costheta) * inv_one_minus_costheta);
    if (((sintheta < 0) && (angle_axis[i] > 0)) ||
        ((sintheta > 0) && (angle_axis[i] < 0))) {
      angle_axis[i] = -angle_axis[i];
    }
  }
}

template <typename T>
inline void AngleAxisToRotationMatrix(const T * angle_axis, T * R) {
  static const T kOne = T(1.0);
  const T theta2 = DotProduct(angle_axis, angle_axis);
  if (theta2 > 0.0) {
    // We want to be careful to only evaluate the square root if the
    // norm of the angle_axis vector is greater than zero. Otherwise
    // we get a division by zero.
    const T theta = sqrt(theta2);
    const T wx = angle_axis[0] / theta;
    const T wy = angle_axis[1] / theta;
    const T wz = angle_axis[2] / theta;

    const T costheta = cos(theta);
    const T sintheta = sin(theta);

    R[0] =     costheta   + wx*wx*(kOne -    costheta);
    R[1] =  wz*sintheta   + wx*wy*(kOne -    costheta);
    R[2] = -wy*sintheta   + wx*wz*(kOne -    costheta);
    R[3] =  wx*wy*(kOne - costheta)     - wz*sintheta;
    R[4] =     costheta   + wy*wy*(kOne -    costheta);
    R[5] =  wx*sintheta   + wy*wz*(kOne -    costheta);
    R[6] =  wy*sintheta   + wx*wz*(kOne -    costheta);
    R[7] = -wx*sintheta   + wy*wz*(kOne -    costheta);
    R[8] =     costheta   + wz*wz*(kOne -    costheta);
  } else {
    // At zero, we switch to using the first order Taylor expansion.
    R[0] =  kOne;
    R[1] = -angle_axis[2];
    R[2] =  angle_axis[1];
    R[3] =  angle_axis[2];
    R[4] =  kOne;
    R[5] = -angle_axis[0];
    R[6] = -angle_axis[1];
    R[7] =  angle_axis[0];
    R[8] = kOne;
  }
}

template <typename T>
inline void EulerAnglesToRotationMatrix(const T* euler,
                                        const int row_stride,
                                        T* R) {
  const T degrees_to_radians(M_PI / 180.0);

  const T pitch(euler[0] * degrees_to_radians);
  const T roll(euler[1] * degrees_to_radians);
  const T yaw(euler[2] * degrees_to_radians);

  const T c1 = cos(yaw);
  const T s1 = sin(yaw);
  const T c2 = cos(roll);
  const T s2 = sin(roll);
  const T c3 = cos(pitch);
  const T s3 = sin(pitch);

  // Rows of the rotation matrix.
  T* R1 = R;
  T* R2 = R1 + row_stride;
  T* R3 = R2 + row_stride;

  R1[0] = c1*c2;
  R1[1] = -s1*c3 + c1*s2*s3;
  R1[2] = s1*s3 + c1*s2*c3;

  R2[0] = s1*c2;
  R2[1] = c1*c3 + s1*s2*s3;
  R2[2] = -c1*s3 + s1*s2*c3;

  R3[0] = -s2;
  R3[1] = c2*s3;
  R3[2] = c2*c3;
}

template <typename T> inline
void QuaternionToScaledRotation(const T q[4], T R[3 * 3]) {
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

  R[0] =  aa + bb - cc - dd; R[1] = T(2) * (bc - ad); R[2] = T(2) * (ac + bd);  // NOLINT
  R[3] = T(2) * (ad + bc); R[4] =  aa - bb + cc - dd; R[5] = T(2) * (cd - ab);  // NOLINT
  R[6] = T(2) * (bd - ac); R[7] = T(2) * (ab + cd); R[8] =  aa - bb - cc + dd;  // NOLINT
}

template <typename T> inline
void QuaternionToRotation(const T q[4], T R[3 * 3]) {
  QuaternionToScaledRotation(q, R);

  T normalizer = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
  CHECK_NE(normalizer, T(0));
  normalizer = T(1) / normalizer;

  for (int i = 0; i < 9; ++i) {
    R[i] *= normalizer;
  }
}

template <typename T> inline
void UnitQuaternionRotatePoint(const T q[4], const T pt[3], T result[3]) {
  const T t2 =  q[0] * q[1];
  const T t3 =  q[0] * q[2];
  const T t4 =  q[0] * q[3];
  const T t5 = -q[1] * q[1];
  const T t6 =  q[1] * q[2];
  const T t7 =  q[1] * q[3];
  const T t8 = -q[2] * q[2];
  const T t9 =  q[2] * q[3];
  const T t1 = -q[3] * q[3];
  result[0] = T(2) * ((t8 + t1) * pt[0] + (t6 - t4) * pt[1] + (t3 + t7) * pt[2]) + pt[0];  // NOLINT
  result[1] = T(2) * ((t4 + t6) * pt[0] + (t5 + t1) * pt[1] + (t9 - t2) * pt[2]) + pt[1];  // NOLINT
  result[2] = T(2) * ((t7 - t3) * pt[0] + (t2 + t9) * pt[1] + (t5 + t8) * pt[2]) + pt[2];  // NOLINT
}


template <typename T> inline
void QuaternionRotatePoint(const T q[4], const T pt[3], T result[3]) {
  // 'scale' is 1 / norm(q).
  const T scale = T(1) / sqrt(q[0] * q[0] +
                              q[1] * q[1] +
                              q[2] * q[2] +
                              q[3] * q[3]);

  // Make unit-norm version of q.
  const T unit[4] = {
    scale * q[0],
    scale * q[1],
    scale * q[2],
    scale * q[3],
  };

  UnitQuaternionRotatePoint(unit, pt, result);
}

template<typename T> inline
void QuaternionProduct(const T z[4], const T w[4], T zw[4]) {
  zw[0] = z[0] * w[0] - z[1] * w[1] - z[2] * w[2] - z[3] * w[3];
  zw[1] = z[0] * w[1] + z[1] * w[0] + z[2] * w[3] - z[3] * w[2];
  zw[2] = z[0] * w[2] - z[1] * w[3] + z[2] * w[0] + z[3] * w[1];
  zw[3] = z[0] * w[3] + z[1] * w[2] - z[2] * w[1] + z[3] * w[0];
}

// xy = x cross y;
template<typename T> inline
void CrossProduct(const T x[3], const T y[3], T x_cross_y[3]) {
  x_cross_y[0] = x[1] * y[2] - x[2] * y[1];
  x_cross_y[1] = x[2] * y[0] - x[0] * y[2];
  x_cross_y[2] = x[0] * y[1] - x[1] * y[0];
}

template<typename T> inline
T DotProduct(const T x[3], const T y[3]) {
  return (x[0] * y[0] + x[1] * y[1] + x[2] * y[2]);
}

template<typename T> inline
void AngleAxisRotatePoint(const T angle_axis[3], const T pt[3], T result[3]) {
  T w[3];
  T sintheta;
  T costheta;

  const T theta2 = DotProduct(angle_axis, angle_axis);
  if (theta2 > 0.0) {
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
    const T theta = sqrt(theta2);
    w[0] = angle_axis[0] / theta;
    w[1] = angle_axis[1] / theta;
    w[2] = angle_axis[2] / theta;
    costheta = cos(theta);
    sintheta = sin(theta);
    T w_cross_pt[3];
    CrossProduct(w, pt, w_cross_pt);
    T w_dot_pt = DotProduct(w, pt);
    for (int i = 0; i < 3; ++i) {
      result[i] = pt[i] * costheta +
          w_cross_pt[i] * sintheta +
          w[i] * (T(1.0) - costheta) * w_dot_pt;
    }
  } else {
    // Near zero, the first order Taylor approximation of the rotation
    // matrix R corresponding to a vector w and angle w is
    //
    //   R = I + hat(w) * sin(theta)
    //
    // But sintheta ~ theta and theta * w = angle_axis, which gives us
    //
    //  R = I + hat(w)
    //
    // and actually performing multiplication with the point pt, gives us
    // R * pt = pt + w x pt.
    //
    // Switching to the Taylor expansion at zero helps avoid all sorts
    // of numerical nastiness.
    T w_cross_pt[3];
    CrossProduct(angle_axis, pt, w_cross_pt);
    for (int i = 0; i < 3; ++i) {
      result[i] = pt[i] + w_cross_pt[i];
    }
  }
}

}  // namespace ceres

// Clean define pollution.
#ifdef CERES_NEED_M_PI_UNDEF
#undef CERES_NEED_M_PI_UNDEF
#undef M_PI
#endif

#endif  // CERES_PUBLIC_ROTATION_H_
