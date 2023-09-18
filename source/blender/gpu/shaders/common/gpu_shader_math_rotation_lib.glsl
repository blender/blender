/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_ROTATION_LIB_GLSL
#  define GPU_SHADER_MATH_ROTATION_LIB_GLSL

/* -------------------------------------------------------------------- */
/** \name Rotation Types
 * \{ */

struct Angle {
  /* Angle in radian. */
  float angle;

#  ifdef GPU_METAL
  Angle() = default;
  Angle(float angle_) : angle(angle_){};
#  endif
};

struct AxisAngle {
  vec3 axis;
  float angle;

#  ifdef GPU_METAL
  AxisAngle() = default;
  AxisAngle(vec3 axis_, float angle_) : axis(axis_), angle(angle_){};
#  endif
};

AxisAngle AxisAngle_identity()
{
  return AxisAngle(vec3(0, 1, 0), 0);
}

struct Quaternion {
  float x, y, z, w;
#  ifdef GPU_METAL
  Quaternion() = default;
  Quaternion(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_){};
#  endif
};

vec4 as_vec4(Quaternion quat)
{
  return vec4(quat.x, quat.y, quat.z, quat.w);
}

Quaternion Quaternion_identity()
{
  return Quaternion(1, 0, 0, 0);
}

struct EulerXYZ {
  float x, y, z;
#  ifdef GPU_METAL
  EulerXYZ() = default;
  EulerXYZ(float x_, float y_, float z_) : x(x_), y(y_), z(z_){};
#  endif
};

vec3 as_vec3(EulerXYZ eul)
{
  return vec3(eul.x, eul.y, eul.z);
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
vec2 interpolate_dot_slerp(float t, float cosom)
{
  vec2 w = vec2(1.0 - t, t);
  /* Within [-1..1] range, avoid aligned axis. */
  const float eps = 1e-4;
  if (abs(cosom) < 1.0 - eps) {
    float omega = acos(cosom);
    w = sin(w * omega) / sin(omega);
  }
  return w;
}

Quaternion interpolate(Quaternion a, Quaternion b, float t)
{
  vec4 quat = as_vec4(a);
  float cosom = dot(as_vec4(a), as_vec4(b));
  /* Rotate around shortest angle. */
  if (cosom < 0.0) {
    cosom = -cosom;
    quat = -quat;
  }
  vec2 w = interpolate_dot_slerp(t, cosom);
  quat = w.x * quat + w.y * as_vec4(b);
  return Quaternion(UNPACK4(quat));
}

Quaternion to_quaternion(EulerXYZ eul)
{
  float ti = eul.x * 0.5;
  float tj = eul.y * 0.5;
  float th = eul.z * 0.5;
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
  float sine = sqrt(0.5 - angle_cos * 0.5);
  float cosine = sqrt(0.5 + angle_cos * 0.5);

  /* TODO(fclem): Optimize. */
  float angle_sin = sin(axis_angle.angle);
  if (angle_sin < 0.0) {
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
  if (abs(si) < 0.0005) {
    si = 1.0;
  }

  vec3 axis = vec3(quat.y, quat.z, quat.w) / si;
  if (is_zero(axis)) {
    axis[1] = 1.0;
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
