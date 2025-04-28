/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_ROTATION_LIB_GLSL
#  define GPU_SHADER_MATH_ROTATION_LIB_GLSL

/* -------------------------------------------------------------------- */
/** \name Rotation Types
 * \{ */

struct Angle {
  /* Angle in radian. */
  float angle;

  METAL_CONSTRUCTOR_1(Angle, float, angle)
};

struct AxisAngle {
  float3 axis;
  float angle;

  METAL_CONSTRUCTOR_2(AxisAngle, float3, axis, float, angle)
};

AxisAngle AxisAngle_identity()
{
  return AxisAngle(float3(0, 1, 0), 0);
}

struct Quaternion {
  float x, y, z, w;
  METAL_CONSTRUCTOR_4(Quaternion, float, x, float, y, float, z, float, w)
};

float4 as_vec4(Quaternion quat)
{
  return float4(quat.x, quat.y, quat.z, quat.w);
}

Quaternion Quaternion_identity()
{
  return Quaternion(1, 0, 0, 0);
}

struct EulerXYZ {
  float x, y, z;
  METAL_CONSTRUCTOR_3(EulerXYZ, float, x, float, y, float, z)
};

float3 as_vec3(EulerXYZ eul)
{
  return float3(eul.x, eul.y, eul.z);
}

EulerXYZ as_EulerXYZ(float3 eul)
{
  return EulerXYZ(eul.x, eul.y, eul.z);
}

EulerXYZ EulerXYZ_identity()
{
  return EulerXYZ(0, 0, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rotation Functions
 * \{ */

/**
 * Generic function for implementing slerp
 * (quaternions and spherical vector coords).
 *
 * \param t: factor in [0..1]
 * \param cosom: dot product from normalized vectors/quaternions.
 * \param r_w: calculated weights.
 */
float2 interpolate_dot_slerp(float t, float cosom)
{
  float2 w = float2(1.0f - t, t);
  /* Within [-1..1] range, avoid aligned axis. */
  constexpr float eps = 1e-4f;
  if (abs(cosom) < 1.0f - eps) {
    float omega = acos(cosom);
    w = sin(w * omega) / sin(omega);
  }
  return w;
}

Quaternion interpolate(Quaternion a, Quaternion b, float t)
{
  float4 quat = as_vec4(a);
  float cosom = dot(as_vec4(a), as_vec4(b));
  /* Rotate around shortest angle. */
  if (cosom < 0.0f) {
    cosom = -cosom;
    quat = -quat;
  }
  float2 w = interpolate_dot_slerp(t, cosom);
  quat = w.x * quat + w.y * as_vec4(b);
  return Quaternion(UNPACK4(quat));
}

Quaternion to_quaternion(EulerXYZ eul)
{
  float ti = eul.x * 0.5f;
  float tj = eul.y * 0.5f;
  float th = eul.z * 0.5f;
  float ci = cos(ti);
  float cj = cos(tj);
  float ch = cos(th);
  float si = sin(ti);
  float sj = sin(tj);
  float sh = sin(th);
  float cc = ci * ch;
  float cs = ci * sh;
  float sc = si * ch;
  float ss = si * sh;

  Quaternion quat;
  quat.x = cj * cc + sj * ss;
  quat.y = cj * sc - sj * cs;
  quat.z = cj * ss + sj * cc;
  quat.w = cj * cs - sj * sc;
  return quat;
}

Quaternion to_axis_angle(AxisAngle axis_angle)
{
  float angle_cos = cos(axis_angle.angle);
  /** Using half angle identities: sin(angle / 2) = sqrt((1 - angle_cos) / 2) */
  float sine = sqrt(0.5f - angle_cos * 0.5f);
  float cosine = sqrt(0.5f + angle_cos * 0.5f);

  /* TODO(fclem): Optimize. */
  float angle_sin = sin(axis_angle.angle);
  if (angle_sin < 0.0f) {
    sine = -sine;
  }

  Quaternion quat;
  quat.x = cosine;
  quat.y = axis_angle.axis.x * sine;
  quat.z = axis_angle.axis.y * sine;
  quat.w = axis_angle.axis.z * sine;
  return quat;
}

AxisAngle to_axis_angle(Quaternion quat)
{
  /* Calculate angle/2, and sin(angle/2). */
  float ha = acos(quat.x);
  float si = sin(ha);

  /* From half-angle to angle. */
  float angle = ha * 2;
  /* Prevent division by zero for axis conversion. */
  if (abs(si) < 0.0005f) {
    si = 1.0f;
  }

  float3 axis = float3(quat.y, quat.z, quat.w) / si;
  if (is_zero(axis)) {
    axis[1] = 1.0f;
  }
  return AxisAngle(axis, angle);
}

AxisAngle to_axis_angle(EulerXYZ eul)
{
  /* Use quaternions as intermediate representation for now... */
  return to_axis_angle(to_quaternion(eul));
}

/** \} */

#endif /* GPU_SHADER_MATH_ROTATION_LIB_GLSL */
