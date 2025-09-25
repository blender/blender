/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_angle_lib.glsl"
#include "gpu_shader_math_axis_angle_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_euler_lib.glsl"
#include "gpu_shader_math_quaternion_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Static constructors
 * \{ */

float2x2 mat2x2_diagonal(float v)
{
  return float2x2(float2(v, 0.0f), float2(0.0f, v));
}
float3x3 mat3x3_diagonal(float v)
{
  return float3x3(float3(v, 0.0f, 0.0f), float3(0.0f, v, 0.0f), float3(0.0f, 0.0f, v));
}
float4x4 mat4x4_diagonal(float v)
{
  return float4x4(float4(v, 0.0f, 0.0f, 0.0f),
                  float4(0.0f, v, 0.0f, 0.0f),
                  float4(0.0f, 0.0f, v, 0.0f),
                  float4(0.0f, 0.0f, 0.0f, v));
}

float2x2 mat2x2_all(float v)
{
  return float2x2(float2(v), float2(v));
}
float3x3 mat3x3_all(float v)
{
  return float3x3(float3(v), float3(v), float3(v));
}
float4x4 mat4x4_all(float v)
{
  return float4x4(float4(v), float4(v), float4(v), float4(v));
}

float2x2 mat2x2_zero()
{
  return mat2x2_all(0.0f);
}
float3x3 mat3x3_zero()
{
  return mat3x3_all(0.0f);
}
float4x4 mat4x4_zero()
{
  return mat4x4_all(0.0f);
}

float2x2 mat2x2_identity()
{
  return mat2x2_diagonal(1.0f);
}
float3x3 mat3x3_identity()
{
  return mat3x3_diagonal(1.0f);
}
float4x4 mat4x4_identity()
{
  return mat4x4_diagonal(1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Init helpers.
 * \{ */

/**
 * Create a translation only matrix. Matrix dimensions should be at least 4 col x 3 row.
 */
float4x4 from_location(float3 location)
{
  float4x4 ret = float4x4(1.0f);
  ret[3].xyz = location;
  return ret;
}

/**
 * Create a matrix whose diagonal is defined by the given scale vector.
 */
float2x2 from_scale(float2 scale)
{
  float2x2 ret = float2x2(0.0f);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  return ret;
}
/**
 * Create a matrix whose diagonal is defined by the given scale vector.
 */
float3x3 from_scale(float3 scale)
{
  float3x3 ret = float3x3(0.0f);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  ret[2][2] = scale[2];
  return ret;
}
/**
 * Create a matrix whose diagonal is defined by the given scale vector.
 */
float4x4 from_scale(float4 scale)
{
  float4x4 ret = float4x4(0.0f);
  ret[0][0] = scale[0];
  ret[1][1] = scale[1];
  ret[2][2] = scale[2];
  ret[3][3] = scale[3];
  return ret;
}

/**
 * Create a rotation only matrix.
 */
float2x2 from_rotation(AngleRadian rotation)
{
  float c = cos(rotation.angle);
  float s = sin(rotation.angle);
  return float2x2(c, -s, s, c);
}
/**
 * Create a rotation only matrix.
 */
float3x3 from_rotation(EulerXYZ rotation)
{
  float ci = cos(rotation.x);
  float cj = cos(rotation.y);
  float ch = cos(rotation.z);
  float si = sin(rotation.x);
  float sj = sin(rotation.y);
  float sh = sin(rotation.z);
  float cc = ci * ch;
  float cs = ci * sh;
  float sc = si * ch;
  float ss = si * sh;

  float3x3 mat;
  mat[0][0] = cj * ch;
  mat[1][0] = sj * sc - cs;
  mat[2][0] = sj * cc + ss;

  mat[0][1] = cj * sh;
  mat[1][1] = sj * ss + cc;
  mat[2][1] = sj * cs - sc;

  mat[0][2] = -sj;
  mat[1][2] = cj * si;
  mat[2][2] = cj * ci;
  return mat;
}
/**
 * Create a rotation only matrix.
 */
float3x3 from_rotation(Quaternion rotation)
{
  /* NOTE: Should be double but support isn't native on most GPUs. */
  float q0 = M_SQRT2 * float(rotation.x);
  float q1 = M_SQRT2 * float(rotation.y);
  float q2 = M_SQRT2 * float(rotation.z);
  float q3 = M_SQRT2 * float(rotation.w);

  float qda = q0 * q1;
  float qdb = q0 * q2;
  float qdc = q0 * q3;
  float qaa = q1 * q1;
  float qab = q1 * q2;
  float qac = q1 * q3;
  float qbb = q2 * q2;
  float qbc = q2 * q3;
  float qcc = q3 * q3;

  float3x3 mat;
  mat[0][0] = float(1.0f - qbb - qcc);
  mat[0][1] = float(qdc + qab);
  mat[0][2] = float(-qdb + qac);

  mat[1][0] = float(-qdc + qab);
  mat[1][1] = float(1.0f - qaa - qcc);
  mat[1][2] = float(qda + qbc);

  mat[2][0] = float(qdb + qac);
  mat[2][1] = float(-qda + qbc);
  mat[2][2] = float(1.0f - qaa - qbb);
  return mat;
}
/**
 * Create a rotation only matrix.
 */
float3x3 from_rotation(AxisAngle rotation)
{
  float angle_sin = sin(rotation.angle);
  float angle_cos = cos(rotation.angle);
  float3 axis = rotation.axis;

  float ico = (float(1) - angle_cos);
  float3 nsi = axis * angle_sin;

  float3 n012 = (axis * axis) * ico;
  float n_01 = (axis[0] * axis[1]) * ico;
  float n_02 = (axis[0] * axis[2]) * ico;
  float n_12 = (axis[1] * axis[2]) * ico;

  float3x3 mat = from_scale(n012 + angle_cos);
  mat[0][1] = n_01 + nsi[2];
  mat[0][2] = n_02 - nsi[1];
  mat[1][0] = n_01 - nsi[2];
  mat[1][2] = n_12 + nsi[0];
  mat[2][0] = n_02 + nsi[1];
  mat[2][1] = n_12 - nsi[0];
  return mat;
}

/**
 * Create a transform matrix with rotation and scale applied in this order.
 */
float3x3 from_rot_scale(EulerXYZ rotation, float3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}
/**
 * Create a transform matrix with rotation and scale applied in this order.
 */
float3x3 from_rot_scale(Quaternion rotation, float3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}
/**
 * Create a transform matrix with rotation and scale applied in this order.
 */
float3x3 from_rot_scale(AxisAngle rotation, float3 scale)
{
  return from_rotation(rotation) * from_scale(scale);
}

/**
 * Create a transform matrix with translation and rotation applied in this order.
 */
float4x4 from_loc_rot(float3 location, EulerXYZ rotation)
{
  float4x4 ret = to_float4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}
/**
 * Create a transform matrix with translation and rotation applied in this order.
 */
float4x4 from_loc_rot(float3 location, Quaternion rotation)
{
  float4x4 ret = to_float4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}
/**
 * Create a transform matrix with translation and rotation applied in this order.
 */
float4x4 from_loc_rot(float3 location, AxisAngle rotation)
{
  float4x4 ret = to_float4x4(from_rotation(rotation));
  ret[3].xyz = location;
  return ret;
}

/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
float4x4 from_loc_rot_scale(float3 location, EulerXYZ rotation, float3 scale)
{
  float4x4 ret = to_float4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}
/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
float4x4 from_loc_rot_scale(float3 location, Quaternion rotation, float3 scale)
{
  float4x4 ret = to_float4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}
/**
 * Create a transform matrix with translation, rotation and scale applied in this order.
 */
float4x4 from_loc_rot_scale(float3 location, AxisAngle rotation, float3 scale)
{
  float4x4 ret = to_float4x4(from_rot_scale(rotation, scale));
  ret[3].xyz = location;
  return ret;
}

/**
 * Creates a 2D rotation matrix with the angle that the given direction makes with the x axis.
 * Assumes the direction vector is normalized.
 */
float2x2 from_direction(float2 direction)
{
  float cos_angle = direction.x;
  float sin_angle = direction.y;
  return float2x2(cos_angle, sin_angle, -sin_angle, cos_angle);
}

/**
 * Create a rotation matrix from 2 basis vectors.
 * The matrix determinant is given to be positive and it can be converted to other rotation types.
 * \note `forward` and `up` must be normalized.
 */
// mat3x3 from_normalized_axis_data(vec3 forward, vec3 up); /* TODO. */

/**
 * Create a transform matrix with translation and rotation from 2 basis vectors and a translation.
 * \note `forward` and `up` must be normalized.
 */
// mat4x4 from_normalized_axis_data(vec3 location, vec3 forward, vec3 up); /* TODO. */

/**
 * Create a rotation matrix from only one \a up axis.
 * The other axes are chosen to always be orthogonal. The resulting matrix is a basis matrix.
 * \note `up` must be normalized.
 * \note This can be used to create a tangent basis from a normal vector.
 * \note The output of this function is not given to be same across blender version. Prefer using
 * `from_orthonormal_axes` for more stable output.
 */
float3x3 from_up_axis(float3 up)
{
  /* Duff, Tom, et al. "Building an orthonormal basis, revisited." JCGT 6.1 (2017). */
  float z_sign = up.z >= 0.0f ? 1.0f : -1.0f;
  float a = -1.0f / (z_sign + up.z);
  float b = up.x * up.y * a;

  float3x3 basis;
  basis[0] = float3(1.0f + z_sign * square(up.x) * a, z_sign * b, -z_sign * up.x);
  basis[1] = float3(b, z_sign + square(up.y) * a, -up.y);
  basis[2] = up;
  return basis;
}
/** \} */
