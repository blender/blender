/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Float Math */

/* WORKAROUND: To be removed once we port all code to use `gpu_shader_math_base_lib.glsl`. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL

float safe_divide(float a, float b)
{
  return (b != 0.0) ? a / b : 0.0;
}

#endif

/* fmod function compatible with OSL (copy from OSL/dual.h) */
float compatible_fmod(float a, float b)
{
  if (b != 0.0) {
    int N = int(a / b);
    return a - N * b;
  }
  return 0.0;
}

float compatible_pow(float x, float y)
{
  if (y == 0.0) { /* x^0 -> 1, including 0^0 */
    return 1.0;
  }

  /* GLSL pow doesn't accept negative x. */
  if (x < 0.0) {
    if (mod(-y, 2.0) == 0.0) {
      return pow(-x, y);
    }
    else {
      return -pow(-x, y);
    }
  }
  else if (x == 0.0) {
    return 0.0;
  }

  return pow(x, y);
}

/* A version of pow that returns a fallback value if the computation is undefined. From the spec:
 * The result is undefined if x < 0 or if x = 0 and y is less than or equal 0. */
float fallback_pow(float x, float y, float fallback)
{
  if (x < 0.0 || (x == 0.0 && y <= 0.0)) {
    return fallback;
  }

  return pow(x, y);
}

float wrap(float a, float b, float c)
{
  float range = b - c;
  return (range != 0.0) ? a - (range * floor((a - c) / range)) : c;
}

vec3 wrap(vec3 a, vec3 b, vec3 c)
{
  return vec3(wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y), wrap(a.z, b.z, c.z));
}

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL

float hypot(float x, float y)
{
  return sqrt(x * x + y * y);
}

#endif

int floor_to_int(float x)
{
  return int(floor(x));
}

int quick_floor(float x)
{
  return int(x) - ((x < 0) ? 1 : 0);
}

/* Vector Math */

/* WORKAROUND: To be removed once we port all code to use gpu_shader_math_base_lib.glsl. */
#ifndef GPU_SHADER_MATH_BASE_LIB_GLSL

vec2 safe_divide(vec2 a, vec2 b)
{
  return vec2(safe_divide(a.x, b.x), safe_divide(a.y, b.y));
}

vec3 safe_divide(vec3 a, vec3 b)
{
  return vec3(safe_divide(a.x, b.x), safe_divide(a.y, b.y), safe_divide(a.z, b.z));
}

vec4 safe_divide(vec4 a, vec4 b)
{
  return vec4(
      safe_divide(a.x, b.x), safe_divide(a.y, b.y), safe_divide(a.z, b.z), safe_divide(a.w, b.w));
}

vec2 safe_divide(vec2 a, float b)
{
  return (b != 0.0) ? a / b : vec2(0.0);
}

vec3 safe_divide(vec3 a, float b)
{
  return (b != 0.0) ? a / b : vec3(0.0);
}

vec4 safe_divide(vec4 a, float b)
{
  return (b != 0.0) ? a / b : vec4(0.0);
}

#endif

vec3 compatible_fmod(vec3 a, vec3 b)
{
  return vec3(compatible_fmod(a.x, b.x), compatible_fmod(a.y, b.y), compatible_fmod(a.z, b.z));
}

void invert_z(vec3 v, out vec3 outv)
{
  v.z = -v.z;
  outv = v;
}

void vector_normalize(vec3 normal, out vec3 outnormal)
{
  outnormal = normalize(normal);
}

void vector_copy(vec3 normal, out vec3 outnormal)
{
  outnormal = normal;
}

vec3 fallback_pow(vec3 a, float b, vec3 fallback)
{
  return vec3(fallback_pow(a.x, b, fallback.x),
              fallback_pow(a.y, b, fallback.y),
              fallback_pow(a.z, b, fallback.z));
}

/* Matrix Math */

/* Return a 2D rotation matrix with the angle that the input 2D vector makes with the x axis. */
mat2 vector_to_rotation_matrix(vec2 vector)
{
  vec2 normalized_vector = normalize(vector);
  float cos_angle = normalized_vector.x;
  float sin_angle = normalized_vector.y;
  return mat2(cos_angle, sin_angle, -sin_angle, cos_angle);
}

mat3 euler_to_mat3(vec3 euler)
{
  float cx = cos(euler.x);
  float cy = cos(euler.y);
  float cz = cos(euler.z);
  float sx = sin(euler.x);
  float sy = sin(euler.y);
  float sz = sin(euler.z);

  mat3 mat;
  mat[0][0] = cy * cz;
  mat[0][1] = cy * sz;
  mat[0][2] = -sy;

  mat[1][0] = sy * sx * cz - cx * sz;
  mat[1][1] = sy * sx * sz + cx * cz;
  mat[1][2] = cy * sx;

  mat[2][0] = sy * cx * cz + sx * sz;
  mat[2][1] = sy * cx * sz - sx * cz;
  mat[2][2] = cy * cx;
  return mat;
}
