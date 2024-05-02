/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_matrix.hh"

#include "BLI_math_rotation.hh"
#include "BLI_simd.hh"

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

/* -------------------------------------------------------------------- */
/** \name Matrix multiplication
 * \{ */

namespace blender {

template<> float4x4 operator*(const float4x4 &a, const float4x4 &b)
{
  using namespace math;
  float4x4 result;

#if BLI_HAVE_SSE2
  __m128 A0 = _mm_load_ps(a[0]);
  __m128 A1 = _mm_load_ps(a[1]);
  __m128 A2 = _mm_load_ps(a[2]);
  __m128 A3 = _mm_load_ps(a[3]);

  for (int i = 0; i < 4; i++) {
    __m128 B0 = _mm_set1_ps(b[i][0]);
    __m128 B1 = _mm_set1_ps(b[i][1]);
    __m128 B2 = _mm_set1_ps(b[i][2]);
    __m128 B3 = _mm_set1_ps(b[i][3]);

    __m128 sum = _mm_add_ps(_mm_add_ps(_mm_mul_ps(B0, A0), _mm_mul_ps(B1, A1)),
                            _mm_add_ps(_mm_mul_ps(B2, A2), _mm_mul_ps(B3, A3)));

    _mm_store_ps(result[i], sum);
  }
#else
  result[0][0] = b[0][0] * a[0][0] + b[0][1] * a[1][0] + b[0][2] * a[2][0] + b[0][3] * a[3][0];
  result[0][1] = b[0][0] * a[0][1] + b[0][1] * a[1][1] + b[0][2] * a[2][1] + b[0][3] * a[3][1];
  result[0][2] = b[0][0] * a[0][2] + b[0][1] * a[1][2] + b[0][2] * a[2][2] + b[0][3] * a[3][2];
  result[0][3] = b[0][0] * a[0][3] + b[0][1] * a[1][3] + b[0][2] * a[2][3] + b[0][3] * a[3][3];

  result[1][0] = b[1][0] * a[0][0] + b[1][1] * a[1][0] + b[1][2] * a[2][0] + b[1][3] * a[3][0];
  result[1][1] = b[1][0] * a[0][1] + b[1][1] * a[1][1] + b[1][2] * a[2][1] + b[1][3] * a[3][1];
  result[1][2] = b[1][0] * a[0][2] + b[1][1] * a[1][2] + b[1][2] * a[2][2] + b[1][3] * a[3][2];
  result[1][3] = b[1][0] * a[0][3] + b[1][1] * a[1][3] + b[1][2] * a[2][3] + b[1][3] * a[3][3];

  result[2][0] = b[2][0] * a[0][0] + b[2][1] * a[1][0] + b[2][2] * a[2][0] + b[2][3] * a[3][0];
  result[2][1] = b[2][0] * a[0][1] + b[2][1] * a[1][1] + b[2][2] * a[2][1] + b[2][3] * a[3][1];
  result[2][2] = b[2][0] * a[0][2] + b[2][1] * a[1][2] + b[2][2] * a[2][2] + b[2][3] * a[3][2];
  result[2][3] = b[2][0] * a[0][3] + b[2][1] * a[1][3] + b[2][2] * a[2][3] + b[2][3] * a[3][3];

  result[3][0] = b[3][0] * a[0][0] + b[3][1] * a[1][0] + b[3][2] * a[2][0] + b[3][3] * a[3][0];
  result[3][1] = b[3][0] * a[0][1] + b[3][1] * a[1][1] + b[3][2] * a[2][1] + b[3][3] * a[3][1];
  result[3][2] = b[3][0] * a[0][2] + b[3][1] * a[1][2] + b[3][2] * a[2][2] + b[3][3] * a[3][2];
  result[3][3] = b[3][0] * a[0][3] + b[3][1] * a[1][3] + b[3][2] * a[2][3] + b[3][3] * a[3][3];
#endif

  return result;
}

template<> float3x3 operator*(const float3x3 &a, const float3x3 &b)
{
  using namespace math;
  float3x3 result;

#if 0 /* 1.2 times slower. Could be used as reference for aligned version. */
  __m128 A0 = _mm_set_ps(0, a[0][2], a[0][1], a[0][0]);
  __m128 A1 = _mm_set_ps(0, a[1][2], a[1][1], a[1][0]);
  __m128 A2 = _mm_set_ps(0, a[2][2], a[2][1], a[2][0]);

  for (int i = 0; i < 2; i++) {
    __m128 B0 = _mm_set1_ps(b[i][0]);
    __m128 B1 = _mm_set1_ps(b[i][1]);
    __m128 B2 = _mm_set1_ps(b[i][2]);
    __m128 sum = _mm_add_ps(_mm_add_ps(_mm_mul_ps(B0, A0), _mm_mul_ps(B1, A1)),
                            _mm_mul_ps(B2, A2));
    _mm_storeu_ps(result[i], sum);
  }

  _mm_storeu_ps(result[1], sum[1]);
  /* Manual per component store to avoid segfault. */
  _mm_store_ss(&result[2][0], sum[2]);
  sum[2] = _mm_shuffle_ps(sum[2], sum[2], _MM_SHUFFLE(3, 2, 1, 1));
  _mm_store_ss(&result[2][1], sum[2]);
  sum[2] = _mm_shuffle_ps(sum[2], sum[2], _MM_SHUFFLE(3, 2, 1, 2));
  _mm_store_ss(&result[2][2], sum[2]);

#else
  /** Manual unrolling since MSVC doesn't seem to unroll properly. */
  result[0][0] = b[0][0] * a[0][0] + b[0][1] * a[1][0] + b[0][2] * a[2][0];
  result[0][1] = b[0][0] * a[0][1] + b[0][1] * a[1][1] + b[0][2] * a[2][1];
  result[0][2] = b[0][0] * a[0][2] + b[0][1] * a[1][2] + b[0][2] * a[2][2];

  result[1][0] = b[1][0] * a[0][0] + b[1][1] * a[1][0] + b[1][2] * a[2][0];
  result[1][1] = b[1][0] * a[0][1] + b[1][1] * a[1][1] + b[1][2] * a[2][1];
  result[1][2] = b[1][0] * a[0][2] + b[1][1] * a[1][2] + b[1][2] * a[2][2];

  result[2][0] = b[2][0] * a[0][0] + b[2][1] * a[1][0] + b[2][2] * a[2][0];
  result[2][1] = b[2][0] * a[0][1] + b[2][1] * a[1][1] + b[2][2] * a[2][1];
  result[2][2] = b[2][0] * a[0][2] + b[2][1] * a[1][2] + b[2][2] * a[2][2];
#endif
  return result;
}

template float2x2 operator*(const float2x2 &a, const float2x2 &b);
template double2x2 operator*(const double2x2 &a, const double2x2 &b);
template double3x3 operator*(const double3x3 &a, const double3x3 &b);
template double4x4 operator*(const double4x4 &a, const double4x4 &b);

}  // namespace blender

/** \} */

namespace blender::math {

/* -------------------------------------------------------------------- */
/** \name Determinant
 * \{ */

template<typename T, int Size> T determinant(const MatBase<T, Size, Size> &mat)
{
  return Eigen::Map<const Eigen::Matrix<T, Size, Size>>(mat.base_ptr()).determinant();
}

template float determinant(const float2x2 &mat);
template float determinant(const float3x3 &mat);
template float determinant(const float4x4 &mat);
template double determinant(const double2x2 &mat);
template double determinant(const double3x3 &mat);
template double determinant(const double4x4 &mat);

template<typename T> bool is_negative(const MatBase<T, 4, 4> &mat)
{
  return Eigen::Map<const Eigen::Matrix<T, 3, 3>, 0, Eigen::Stride<4, 1>>(mat.base_ptr())
             .determinant() < T(0);
}

template bool is_negative(const float4x4 &mat);
template bool is_negative(const double4x4 &mat);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Adjoint
 * \{ */

template<typename T, int Size> MatBase<T, Size, Size> adjoint(const MatBase<T, Size, Size> &mat)
{
  MatBase<T, Size, Size> adj;
  unroll<Size>([&](auto c) {
    unroll<Size>([&](auto r) {
      /* Copy other cells except the "cross" to compute the determinant. */
      MatBase<T, Size - 1, Size - 1> tmp;
      unroll<Size>([&](auto m_c) {
        unroll<Size>([&](auto m_r) {
          if (m_c != c && m_r != r) {
            int d_c = (m_c < c) ? m_c : (m_c - 1);
            int d_r = (m_r < r) ? m_r : (m_r - 1);
            tmp[d_c][d_r] = mat[m_c][m_r];
          }
        });
      });
      T minor = determinant(tmp);
      /* Transpose directly to get the adjugate. Swap destination row and col. */
      adj[r][c] = ((c + r) & 1) ? -minor : minor;
    });
  });
  return adj;
}

template float2x2 adjoint(const float2x2 &mat);
template float3x3 adjoint(const float3x3 &mat);
template float4x4 adjoint(const float4x4 &mat);
template double2x2 adjoint(const double2x2 &mat);
template double3x3 adjoint(const double3x3 &mat);
template double4x4 adjoint(const double4x4 &mat);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inverse
 * \{ */

template<typename T, int Size>
MatBase<T, Size, Size> invert(const MatBase<T, Size, Size> &mat, bool &r_success)
{
  MatBase<T, Size, Size> result;
  Eigen::Map<const Eigen::Matrix<T, Size, Size>> M(mat.base_ptr());
  Eigen::Map<Eigen::Matrix<T, Size, Size>> R(result.base_ptr());
  M.computeInverseWithCheck(R, r_success, 0.0f);
  if (!r_success) {
    R = R.Zero();
  }
  return result;
}

template float2x2 invert(const float2x2 &mat, bool &r_success);
template float3x3 invert(const float3x3 &mat, bool &r_success);
template float4x4 invert(const float4x4 &mat, bool &r_success);
template double2x2 invert(const double2x2 &mat, bool &r_success);
template double3x3 invert(const double3x3 &mat, bool &r_success);
template double4x4 invert(const double4x4 &mat, bool &r_success);

template<typename T, int Size>
MatBase<T, Size, Size> pseudo_invert(const MatBase<T, Size, Size> &mat, T epsilon)
{
  /* Start by trying normal inversion first. */
  bool success;
  MatBase<T, Size, Size> inv = invert<T, Size>(mat, success);
  if (success) {
    return inv;
  }

  /**
   * Compute the Single Value Decomposition of an arbitrary matrix A
   * That is compute the 3 matrices U,W,V with U column orthogonal (m,n)
   * ,W a diagonal matrix and V an orthogonal square matrix `s.t.A = U.W.Vt`.
   * From this decomposition it is trivial to compute the (pseudo-inverse)
   * of `A` as `Ainv = V.Winv.transpose(U)`.
   */
  MatBase<T, Size, Size> U, W, V;
  VecBase<T, Size> S_val;

  {
    using namespace Eigen;
    using MatrixT = Eigen::Matrix<T, Size, Size>;
    using VectorT = Eigen::Matrix<T, Size, 1>;
    /* Blender and Eigen matrices are both column-major by default.
     * Since our matrix is squared, we can use thinU/V. */
    /**
     * WORKAROUND:
     * (ComputeThinU | ComputeThinV) must be set as runtime parameters in Eigen < 3.4.0.
     * But this requires the matrix type to be dynamic to avoid an assert.
     */
    using MatrixDynamicT = Eigen::Matrix<T, Dynamic, Dynamic>;
    JacobiSVD<MatrixDynamicT, NoQRPreconditioner> svd(
        Eigen::Map<const MatrixDynamicT>(mat.base_ptr(), Size, Size), ComputeThinU | ComputeThinV);

    Eigen::Map<MatrixT>(U.base_ptr()) = svd.matrixU();
    (Eigen::Map<VectorT>(S_val)) = svd.singularValues();
    Eigen::Map<MatrixT>(V.base_ptr()) = svd.matrixV();
  }

  /* Invert or nullify component based on epsilon comparison. */
  unroll<Size>([&](auto i) { S_val[i] = (S_val[i] < epsilon) ? T(0) : (T(1) / S_val[i]); });

  W = from_scale<MatBase<T, Size, Size>>(S_val);
  return (V * W) * transpose(U);
}

template float2x2 pseudo_invert(const float2x2 &mat, float epsilon);
template float3x3 pseudo_invert(const float3x3 &mat, float epsilon);
template float4x4 pseudo_invert(const float4x4 &mat, float epsilon);
template double2x2 pseudo_invert(const double2x2 &mat, double epsilon);
template double3x3 pseudo_invert(const double3x3 &mat, double epsilon);
template double4x4 pseudo_invert(const double4x4 &mat, double epsilon);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Polar Decomposition
 * \{ */

/**
 * Right polar decomposition:
 *     M = UP
 *
 * U is the 'rotation'-like component, the closest orthogonal matrix to M.
 * P is the 'scaling'-like component, defined in U space.
 *
 * See https://en.wikipedia.org/wiki/Polar_decomposition for more.
 */
template<typename T>
static void polar_decompose(const MatBase<T, 3, 3> &mat3,
                            MatBase<T, 3, 3> &r_U,
                            MatBase<T, 3, 3> &r_P)
{
  /* From svd decomposition (M = WSV*), we have:
   *     U = WV*
   *     P = VSV*
   */
  MatBase<T, 3, 3> W, V;
  VecBase<T, 3> S_val;

  {
    using namespace Eigen;
    using MatrixT = Eigen::Matrix<T, 3, 3>;
    using VectorT = Eigen::Matrix<T, 3, 1>;
    /* Blender and Eigen matrices are both column-major by default.
     * Since our matrix is squared, we can use thinU/V. */
    /**
     * WORKAROUND: (ComputeThinU | ComputeThinV) must be set as runtime parameters in
     * Eigen < 3.4.0. But this requires the matrix type to be dynamic to avoid an assert.
     */
    using MatrixDynamicT = Eigen::Matrix<T, Dynamic, Dynamic>;
    JacobiSVD<MatrixDynamicT, NoQRPreconditioner> svd(
        Eigen::Map<const MatrixDynamicT>(mat3.base_ptr(), 3, 3), ComputeThinU | ComputeThinV);

    Eigen::Map<MatrixT>(W.base_ptr()) = svd.matrixU();
    (Eigen::Map<VectorT>(S_val)) = svd.singularValues();
    Map<MatrixT>(V.base_ptr()) = svd.matrixV();
  }

  MatBase<T, 3, 3> S = from_scale<MatBase<T, 3, 3>>(S_val);
  MatBase<T, 3, 3> Vt = transpose(V);

  r_U = W * Vt;
  r_P = (V * S) * Vt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolate
 * \{ */

template<typename T>
MatBase<T, 3, 3> interpolate(const MatBase<T, 3, 3> &A, const MatBase<T, 3, 3> &B, T t)
{
  using Mat3T = MatBase<T, 3, 3>;
  /* 'Rotation' component ('U' part of polar decomposition,
   * the closest orthogonal matrix to M3 rot/scale
   * transformation matrix), spherically interpolated. */
  Mat3T U_A, U_B;
  /* 'Scaling' component ('P' part of polar decomposition, i.e. scaling in U-defined space),
   * linearly interpolated. */
  Mat3T P_A, P_B;

  polar_decompose(A, U_A, P_A);
  polar_decompose(B, U_B, P_B);

  /* Quaternions cannot represent an axis flip. If such a singularity is detected, choose a
   * different decomposition of the matrix that still satisfies A = U_A * P_A but which has a
   * positive determinant and thus no axis flips. This resolves #77154.
   *
   * Note that a flip of two axes is just a rotation of 180 degrees around the third axis, and
   * three flipped axes are just an 180 degree rotation + a single axis flip. It is thus sufficient
   * to solve this problem for single axis flips. */
  if (is_negative(U_A)) {
    U_A = -U_A;
    P_A = -P_A;
  }
  if (is_negative(U_B)) {
    U_B = -U_B;
    P_B = -P_B;
  }

  QuaternionBase<T> quat_A = math::to_quaternion(normalize(U_A));
  QuaternionBase<T> quat_B = math::to_quaternion(normalize(U_B));
  QuaternionBase<T> quat = math::interpolate(quat_A, quat_B, t);
  Mat3T U = from_rotation<Mat3T>(quat);

  Mat3T P = interpolate_linear(P_A, P_B, t);
  /* And we reconstruct rot/scale matrix from interpolated polar components */
  return U * P;
}

template float3x3 interpolate(const float3x3 &a, const float3x3 &b, float t);
template double3x3 interpolate(const double3x3 &a, const double3x3 &b, double t);

template<typename T>
MatBase<T, 4, 4> interpolate(const MatBase<T, 4, 4> &A, const MatBase<T, 4, 4> &B, T t)
{
  MatBase<T, 4, 4> result = MatBase<T, 4, 4>(
      interpolate(MatBase<T, 3, 3>(A), MatBase<T, 3, 3>(B), t));

  /* Location component, linearly interpolated. */
  const auto &loc_a = static_cast<const MatBase<T, 4, 4> &>(A).location();
  const auto &loc_b = static_cast<const MatBase<T, 4, 4> &>(B).location();
  result.location() = interpolate(loc_a, loc_b, t);

  return result;
}

template float4x4 interpolate(const float4x4 &a, const float4x4 &b, float t);
template double4x4 interpolate(const double4x4 &a, const double4x4 &b, double t);

template<typename T>
MatBase<T, 3, 3> interpolate_fast(const MatBase<T, 3, 3> &a, const MatBase<T, 3, 3> &b, T t)
{
  using QuaternionT = QuaternionBase<T>;
  using Vec3T = typename MatBase<T, 3, 3>::vec3_type;

  Vec3T a_scale, b_scale;
  QuaternionT a_quat, b_quat;
  to_rot_scale<true>(a, a_quat, a_scale);
  to_rot_scale<true>(b, b_quat, b_scale);

  const Vec3T scale = interpolate(a_scale, b_scale, t);
  const QuaternionT rotation = interpolate(a_quat, b_quat, t);
  return from_rot_scale<MatBase<T, 3, 3>>(rotation, scale);
}

template float3x3 interpolate_fast(const float3x3 &a, const float3x3 &b, float t);
template double3x3 interpolate_fast(const double3x3 &a, const double3x3 &b, double t);

template<typename T>
MatBase<T, 4, 4> interpolate_fast(const MatBase<T, 4, 4> &a, const MatBase<T, 4, 4> &b, T t)
{
  using QuaternionT = QuaternionBase<T>;
  using Vec3T = typename MatBase<T, 3, 3>::vec3_type;

  Vec3T a_loc, b_loc;
  Vec3T a_scale, b_scale;
  QuaternionT a_quat, b_quat;
  to_loc_rot_scale<true>(a, a_loc, a_quat, a_scale);
  to_loc_rot_scale<true>(b, b_loc, b_quat, b_scale);

  const Vec3T location = interpolate(a_loc, b_loc, t);
  const Vec3T scale = interpolate(a_scale, b_scale, t);
  const QuaternionT rotation = interpolate(a_quat, b_quat, t);
  return from_loc_rot_scale<MatBase<T, 4, 4>>(location, rotation, scale);
}

template float4x4 interpolate_fast(const float4x4 &a, const float4x4 &b, float t);
template double4x4 interpolate_fast(const double4x4 &a, const double4x4 &b, double t);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Legacy
 * \{ */

Quaternion to_quaternion_legacy(const float3x3 &mat)
{
  float3x3 n_mat = normalize(mat);
  /* Rotate z-axis of matrix to z-axis. */
  float3 z_axis = n_mat.z_axis();

  /* Cross product with (0,0,1). */
  float3 nor = normalize(float3(z_axis.y, -z_axis.x, 0.0f));

  float ha = 0.5f * math::safe_acos(z_axis.z);
  /* `nor` negative here, but why? */
  Quaternion q1(math::cos(ha), -nor * math::sin(ha));

  /* Rotate back x-axis from mat, using inverse q1. */
  float3 x_axis = transform_point(conjugate(q1), n_mat.x_axis());

  /* And align x-axes. */
  float ha2 = 0.5f * math::atan2(x_axis.y, x_axis.x);
  Quaternion q2(math::cos(ha2), 0.0f, 0.0f, math::sin(ha2));

  return q1 * q2;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Template instantiation
 * \{ */

namespace detail {

template void normalized_to_eul2(const float3x3 &mat,
                                 Euler3Base<float> &eul1,
                                 Euler3Base<float> &eul2);
template void normalized_to_eul2(const float3x3 &mat,
                                 EulerXYZBase<float> &eul1,
                                 EulerXYZBase<float> &eul2);
template void normalized_to_eul2(const double3x3 &mat,
                                 EulerXYZBase<double> &eul1,
                                 EulerXYZBase<double> &eul2);

template QuaternionBase<float> normalized_to_quat_with_checks(const float3x3 &mat);
template QuaternionBase<double> normalized_to_quat_with_checks(const double3x3 &mat);

template MatBase<float, 2, 2> from_rotation(const AngleRadian &rotation);
template MatBase<float, 3, 3> from_rotation(const AngleRadian &rotation);
template MatBase<float, 3, 3> from_rotation(const EulerXYZ &rotation);
template MatBase<float, 4, 4> from_rotation(const EulerXYZ &rotation);
template MatBase<float, 3, 3> from_rotation(const Euler3 &rotation);
template MatBase<float, 4, 4> from_rotation(const Euler3 &rotation);
template MatBase<float, 3, 3> from_rotation(const Quaternion &rotation);
template MatBase<float, 4, 4> from_rotation(const Quaternion &rotation);
template MatBase<float, 3, 3> from_rotation(const AxisAngle &rotation);
template MatBase<float, 4, 4> from_rotation(const AxisAngle &rotation);
template MatBase<float, 3, 3> from_rotation(const AxisAngleCartesian &rotation);
template MatBase<float, 4, 4> from_rotation(const AxisAngleCartesian &rotation);

}  // namespace detail

template float3 transform_point(const float3x3 &mat, const float3 &point);
template float3 transform_point(const float4x4 &mat, const float3 &point);
template float3 transform_direction(const float3x3 &mat, const float3 &direction);
template float3 transform_direction(const float4x4 &mat, const float3 &direction);
template float3 project_point(const float4x4 &mat, const float3 &point);
template float2 project_point(const float3x3 &mat, const float2 &point);

namespace projection {

template float4x4 orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip);
template float4x4 perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip);
template float4x4 perspective_infinite(
    float left, float right, float bottom, float top, float near_clip);

}  // namespace projection

/** \} */

}  // namespace blender::math
