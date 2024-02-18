/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
#  define GPU_SHADER_MATH_VECTOR_LIB_GLSL

/* Metal does not need prototypes. */
#  ifndef GPU_METAL

/**
 * Return true if all components is equal to zero.
 */
bool is_zero(vec2 vec);
bool is_zero(vec3 vec);
bool is_zero(vec4 vec);

/**
 * Return true if any component is equal to zero.
 */
bool is_any_zero(vec2 vec);
bool is_any_zero(vec3 vec);
bool is_any_zero(vec4 vec);

/**
 * Return true if the deference between`a` and `b` is below the `epsilon` value.
 * Epsilon value is scaled by magnitude of `a` before comparison.
 */
bool almost_equal_relative(vec2 a, vec2 b, const float epsilon_factor);
bool almost_equal_relative(vec3 a, vec3 b, const float epsilon_factor);
bool almost_equal_relative(vec4 a, vec4 b, const float epsilon_factor);

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
vec2 safe_mod(vec2 a, vec2 b);
vec3 safe_mod(vec3 a, vec3 b);
vec4 safe_mod(vec4 a, vec4 b);
vec2 safe_mod(vec2 a, float b);
vec3 safe_mod(vec3 a, float b);
vec4 safe_mod(vec4 a, float b);

/**
 * Returns \a a if it is a multiple of \a b or the next multiple or \a b after \b a .
 * In other words, it is equivalent to `divide_ceil(a, b) * b`.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
ivec2 ceil_to_multiple(ivec2 a, ivec2 b);
ivec3 ceil_to_multiple(ivec3 a, ivec3 b);
ivec4 ceil_to_multiple(ivec4 a, ivec4 b);
uvec2 ceil_to_multiple(uvec2 a, uvec2 b);
uvec3 ceil_to_multiple(uvec3 a, uvec3 b);
uvec4 ceil_to_multiple(uvec4 a, uvec4 b);

/**
 * Integer division that returns the ceiling, instead of flooring like normal C division.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
ivec2 divide_ceil(ivec2 a, ivec2 b);
ivec3 divide_ceil(ivec3 a, ivec3 b);
ivec4 divide_ceil(ivec4 a, ivec4 b);
uvec2 divide_ceil(uvec2 a, uvec2 b);
uvec3 divide_ceil(uvec3 a, uvec3 b);
uvec4 divide_ceil(uvec4 a, uvec4 b);

/**
 * Component wise, use vector to replace min if it is smaller and max if bigger.
 */
void min_max(vec2 vector, inout vec2 min, inout vec2 max);
void min_max(vec3 vector, inout vec3 min, inout vec3 max);
void min_max(vec4 vector, inout vec4 min, inout vec4 max);

/**
 * Safe divide `a` by `b`.
 * If `b` equal 0 the result will be 0.
 */
vec2 safe_divide(vec2 a, vec2 b);
vec3 safe_divide(vec3 a, vec3 b);
vec4 safe_divide(vec4 a, vec4 b);
vec2 safe_divide(vec2 a, float b);
vec3 safe_divide(vec3 a, float b);
vec4 safe_divide(vec4 a, float b);

/**
 * Return the manhattan length of `a`.
 * This is also the sum of the absolute value of all components.
 */
float length_manhattan(vec2 a);
float length_manhattan(vec3 a);
float length_manhattan(vec4 a);

/**
 * Return the length squared of `a`.
 */
float length_squared(vec2 a);
float length_squared(vec3 a);
float length_squared(vec4 a);

/**
 * Return the manhattan distance between `a` and `b`.
 */
float distance_manhattan(vec2 a, vec2 b);
float distance_manhattan(vec3 a, vec3 b);
float distance_manhattan(vec4 a, vec4 b);

/**
 * Return the squared distance between `a` and `b`.
 */
float distance_squared(vec2 a, vec2 b);
float distance_squared(vec3 a, vec3 b);
float distance_squared(vec4 a, vec4 b);

/**
 * Return the projection of `p` onto `v_proj`.
 */
vec3 project(vec3 p, vec3 v_proj);

/**
 * Return normalized version of the `vector` and its length.
 */
vec2 normalize_and_get_length(vec2 vector, out float out_length);
vec3 normalize_and_get_length(vec3 vector, out float out_length);
vec4 normalize_and_get_length(vec4 vector, out float out_length);

/**
 * Return normalized version of the `vector` or a default normalized vector if `vector` is invalid.
 */
vec2 safe_normalize(vec2 vector);
vec3 safe_normalize(vec3 vector);
vec4 safe_normalize(vec4 vector);

/**
 * Safe reciprocal function. Returns `1/a`.
 * If `a` equal 0 the result will be 0.
 */
vec2 safe_rcp(vec2 a);
vec3 safe_rcp(vec3 a);
vec4 safe_rcp(vec4 a);

/**
 * Per component linear interpolation.
 */
vec2 interpolate(vec2 a, vec2 b, float t);
vec3 interpolate(vec3 a, vec3 b, float t);
vec4 interpolate(vec4 a, vec4 b, float t);

/**
 * Return half-way point between `a` and  `b`.
 */
vec2 midpoint(vec2 a, vec2 b);
vec3 midpoint(vec3 a, vec3 b);
vec4 midpoint(vec4 a, vec4 b);

/**
 * Return `vector` if `incident` and `reference` are pointing in the same direction.
 */
// vec2 faceforward(vec2 vector, vec2 incident, vec2 reference); /* Built-in GLSL. */

/**
 * Return the index of the component with the greatest absolute value.
 */
int dominant_axis(vec3 a);

/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector can be in any perpendicular direction.
 * \note Returned vector might not the same length as \a v.
 */
vec3 orthogonal(vec3 v);
/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector is always rotated 90 degrees counter clock wise.
 */
vec2 orthogonal(vec2 v);

/**
 * Return true if the difference between`a` and `b` is below the `epsilon` value.
 */
bool is_equal(vec2 a, vec2 b, const float epsilon);
bool is_equal(vec3 a, vec3 b, const float epsilon);
bool is_equal(vec4 a, vec4 b, const float epsilon);

/**
 * Return the maximum component of a vector.
 */
float reduce_max(vec2 a);
float reduce_max(vec3 a);
float reduce_max(vec4 a);
int reduce_max(ivec2 a);
int reduce_max(ivec3 a);
int reduce_max(ivec4 a);

/**
 * Return the minimum component of a vector.
 */
float reduce_min(vec2 a);
float reduce_min(vec3 a);
float reduce_min(vec4 a);
int reduce_min(ivec2 a);
int reduce_min(ivec3 a);
int reduce_min(ivec4 a);

/**
 * Return the sum of the components of a vector.
 */
float reduce_add(vec2 a);
float reduce_add(vec3 a);
float reduce_add(vec4 a);
int reduce_add(ivec2 a);
int reduce_add(ivec3 a);
int reduce_add(ivec4 a);

/**
 * Return the average of the components of a vector.
 */
float average(vec2 a);
float average(vec3 a);
float average(vec4 a);

#  endif /* !GPU_METAL */

/* ---------------------------------------------------------------------- */
/** \name Implementation
 * \{ */

#  ifdef GPU_METAL /* Already defined in shader_defines.msl/glsl to move here. */
bool is_zero(vec2 vec)
{
  return all(equal(vec, vec2(0.0)));
}
bool is_zero(vec3 vec)
{
  return all(equal(vec, vec3(0.0)));
}
bool is_zero(vec4 vec)
{
  return all(equal(vec, vec4(0.0)));
}
#  endif /* GPU_METAL */

bool is_any_zero(vec2 vec)
{
  return any(equal(vec, vec2(0.0)));
}
bool is_any_zero(vec3 vec)
{
  return any(equal(vec, vec3(0.0)));
}
bool is_any_zero(vec4 vec)
{
  return any(equal(vec, vec4(0.0)));
}

bool almost_equal_relative(vec2 a, vec2 b, const float epsilon_factor)
{
  for (int i = 0; i < 2; i++) {
    if (abs(a[i] - b[i]) > epsilon_factor * abs(a[i])) {
      return false;
    }
  }
  return true;
}
bool almost_equal_relative(vec3 a, vec3 b, const float epsilon_factor)
{
  for (int i = 0; i < 3; i++) {
    if (abs(a[i] - b[i]) > epsilon_factor * abs(a[i])) {
      return false;
    }
  }
  return true;
}
bool almost_equal_relative(vec4 a, vec4 b, const float epsilon_factor)
{
  for (int i = 0; i < 4; i++) {
    if (abs(a[i] - b[i]) > epsilon_factor * abs(a[i])) {
      return false;
    }
  }
  return true;
}

vec2 safe_mod(vec2 a, vec2 b)
{
  return select(vec2(0), mod(a, b), notEqual(b, vec2(0)));
}
vec3 safe_mod(vec3 a, vec3 b)
{
  return select(vec3(0), mod(a, b), notEqual(b, vec3(0)));
}
vec4 safe_mod(vec4 a, vec4 b)
{
  return select(vec4(0), mod(a, b), notEqual(b, vec4(0)));
}

vec2 safe_mod(vec2 a, float b)
{
  return (b != 0.0) ? mod(a, vec2(b)) : vec2(0);
}
vec3 safe_mod(vec3 a, float b)
{
  return (b != 0.0) ? mod(a, vec3(b)) : vec3(0);
}
vec4 safe_mod(vec4 a, float b)
{
  return (b != 0.0) ? mod(a, vec4(b)) : vec4(0);
}

ivec2 ceil_to_multiple(ivec2 a, ivec2 b)
{
  return ((a + b - 1) / b) * b;
}
ivec3 ceil_to_multiple(ivec3 a, ivec3 b)
{
  return ((a + b - 1) / b) * b;
}
ivec4 ceil_to_multiple(ivec4 a, ivec4 b)
{
  return ((a + b - 1) / b) * b;
}
uvec2 ceil_to_multiple(uvec2 a, uvec2 b)
{
  return ((a + b - 1u) / b) * b;
}
uvec3 ceil_to_multiple(uvec3 a, uvec3 b)
{
  return ((a + b - 1u) / b) * b;
}
uvec4 ceil_to_multiple(uvec4 a, uvec4 b)
{
  return ((a + b - 1u) / b) * b;
}

ivec2 divide_ceil(ivec2 a, ivec2 b)
{
  return (a + b - 1) / b;
}
ivec3 divide_ceil(ivec3 a, ivec3 b)
{
  return (a + b - 1) / b;
}
ivec4 divide_ceil(ivec4 a, ivec4 b)
{
  return (a + b - 1) / b;
}
uvec2 divide_ceil(uvec2 a, uvec2 b)
{
  return (a + b - 1u) / b;
}
uvec3 divide_ceil(uvec3 a, uvec3 b)
{
  return (a + b - 1u) / b;
}
uvec4 divide_ceil(uvec4 a, uvec4 b)
{
  return (a + b - 1u) / b;
}

void min_max(vec2 vector, inout vec2 min_v, inout vec2 max_v)
{
  min_v = min(vector, min_v);
  max_v = max(vector, max_v);
}
void min_max(vec3 vector, inout vec3 min_v, inout vec3 max_v)
{
  min_v = min(vector, min_v);
  max_v = max(vector, max_v);
}
void min_max(vec4 vector, inout vec4 min_v, inout vec4 max_v)
{
  min_v = min(vector, min_v);
  max_v = max(vector, max_v);
}

vec2 safe_divide(vec2 a, vec2 b)
{
  return select(vec2(0), a / b, notEqual(b, vec2(0)));
}
vec3 safe_divide(vec3 a, vec3 b)
{
  return select(vec3(0), a / b, notEqual(b, vec3(0)));
}
vec4 safe_divide(vec4 a, vec4 b)
{
  return select(vec4(0), a / b, notEqual(b, vec4(0)));
}

vec2 safe_divide(vec2 a, float b)
{
  return (b != 0.0) ? (a / b) : vec2(0);
}
vec3 safe_divide(vec3 a, float b)
{
  return (b != 0.0) ? (a / b) : vec3(0);
}
vec4 safe_divide(vec4 a, float b)
{
  return (b != 0.0) ? (a / b) : vec4(0);
}

float length_manhattan(vec2 a)
{
  return dot(abs(a), vec2(1));
}
float length_manhattan(vec3 a)
{
  return dot(abs(a), vec3(1));
}
float length_manhattan(vec4 a)
{
  return dot(abs(a), vec4(1));
}

float length_squared(vec2 a)
{
  return dot(a, a);
}
float length_squared(vec3 a)
{
  return dot(a, a);
}
float length_squared(vec4 a)
{
  return dot(a, a);
}

float distance_manhattan(vec2 a, vec2 b)
{
  return length_manhattan(a - b);
}
float distance_manhattan(vec3 a, vec3 b)
{
  return length_manhattan(a - b);
}
float distance_manhattan(vec4 a, vec4 b)
{
  return length_manhattan(a - b);
}

float distance_squared(vec2 a, vec2 b)
{
  return length_squared(a - b);
}
float distance_squared(vec3 a, vec3 b)
{
  return length_squared(a - b);
}
float distance_squared(vec4 a, vec4 b)
{
  return length_squared(a - b);
}

vec3 project(vec3 p, vec3 v_proj)
{
  if (is_zero(v_proj)) {
    return vec3(0);
  }
  return v_proj * (dot(p, v_proj) / dot(v_proj, v_proj));
}

vec2 normalize_and_get_length(vec2 vector, out float out_length)
{
  out_length = length_squared(vector);
  const float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0;
  return vec2(0.0);
}
vec3 normalize_and_get_length(vec3 vector, out float out_length)
{
  out_length = length_squared(vector);
  const float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0;
  return vec3(0.0);
}
vec4 normalize_and_get_length(vec4 vector, out float out_length)
{
  out_length = length_squared(vector);
  const float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0;
  return vec4(0.0);
}

vec2 safe_normalize_and_get_length(vec2 vector, out float out_length)
{
  out_length = length_squared(vector);
  const float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0;
  return vec2(1.0, 0.0);
}
vec3 safe_normalize_and_get_length(vec3 vector, out float out_length)
{
  out_length = length_squared(vector);
  const float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0;
  return vec3(1.0, 0.0, 0.0);
}
vec4 safe_normalize_and_get_length(vec4 vector, out float out_length)
{
  out_length = length_squared(vector);
  const float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0;
  return vec4(1.0, 0.0, 0.0, 0.0);
}

vec2 safe_normalize(vec2 vector)
{
  float unused_length;
  return safe_normalize_and_get_length(vector, unused_length);
}
vec3 safe_normalize(vec3 vector)
{
  float unused_length;
  return safe_normalize_and_get_length(vector, unused_length);
}
vec4 safe_normalize(vec4 vector)
{
  float unused_length;
  return safe_normalize_and_get_length(vector, unused_length);
}

vec2 safe_rcp(vec2 a)
{
  return select(vec2(0.0), (1.0 / a), notEqual(a, vec2(0.0)));
}
vec3 safe_rcp(vec3 a)
{
  return select(vec3(0.0), (1.0 / a), notEqual(a, vec3(0.0)));
}
vec4 safe_rcp(vec4 a)
{
  return select(vec4(0.0), (1.0 / a), notEqual(a, vec4(0.0)));
}

vec2 interpolate(vec2 a, vec2 b, float t)
{
  return mix(a, b, t);
}
vec3 interpolate(vec3 a, vec3 b, float t)
{
  return mix(a, b, t);
}
vec4 interpolate(vec4 a, vec4 b, float t)
{
  return mix(a, b, t);
}

vec2 midpoint(vec2 a, vec2 b)
{
  return (a + b) * 0.5;
}
vec3 midpoint(vec3 a, vec3 b)
{
  return (a + b) * 0.5;
}
vec4 midpoint(vec4 a, vec4 b)
{
  return (a + b) * 0.5;
}

int dominant_axis(vec3 a)
{
  vec3 b = abs(a);
  return ((b.x > b.y) ? ((b.x > b.z) ? 0 : 2) : ((b.y > b.z) ? 1 : 2));
}

vec3 orthogonal(vec3 v)
{
  switch (dominant_axis(v)) {
    default:
    case 0:
      return vec3(-v.y - v.z, v.x, v.x);
    case 1:
      return vec3(v.y, -v.x - v.z, v.y);
    case 2:
      return vec3(v.z, v.z, -v.x - v.y);
  }
}

vec2 orthogonal(vec2 v)
{
  return vec2(-v.y, v.x);
}

bool is_equal(vec2 a, vec2 b, const float epsilon)
{
  return all(lessThanEqual(abs(a - b), vec2(epsilon)));
}
bool is_equal(vec3 a, vec3 b, const float epsilon)
{
  return all(lessThanEqual(abs(a - b), vec3(epsilon)));
}
bool is_equal(vec4 a, vec4 b, const float epsilon)
{
  return all(lessThanEqual(abs(a - b), vec4(epsilon)));
}

float reduce_max(vec2 a)
{
  return max(a.x, a.y);
}
float reduce_max(vec3 a)
{
  return max(a.x, max(a.y, a.z));
}
float reduce_max(vec4 a)
{
  return max(max(a.x, a.y), max(a.z, a.w));
}
int reduce_max(ivec2 a)
{
  return max(a.x, a.y);
}
int reduce_max(ivec3 a)
{
  return max(a.x, max(a.y, a.z));
}
int reduce_max(ivec4 a)
{
  return max(max(a.x, a.y), max(a.z, a.w));
}

float reduce_min(vec2 a)
{
  return min(a.x, a.y);
}
float reduce_min(vec3 a)
{
  return min(a.x, min(a.y, a.z));
}
float reduce_min(vec4 a)
{
  return min(min(a.x, a.y), min(a.z, a.w));
}
int reduce_min(ivec2 a)
{
  return min(a.x, a.y);
}
int reduce_min(ivec3 a)
{
  return min(a.x, min(a.y, a.z));
}
int reduce_min(ivec4 a)
{
  return min(min(a.x, a.y), min(a.z, a.w));
}

float reduce_add(vec2 a)
{
  return a.x + a.y;
}
float reduce_add(vec3 a)
{
  return a.x + a.y + a.z;
}
float reduce_add(vec4 a)
{
  return a.x + a.y + a.z + a.w;
}
int reduce_add(ivec2 a)
{
  return a.x + a.y;
}
int reduce_add(ivec3 a)
{
  return a.x + a.y + a.z;
}
int reduce_add(ivec4 a)
{
  return a.x + a.y + a.z + a.w;
}

float average(vec2 a)
{
  return reduce_add(a) * (1.0 / 2.0);
}
float average(vec3 a)
{
  return reduce_add(a) * (1.0 / 3.0);
}
float average(vec4 a)
{
  return reduce_add(a) * (1.0 / 4.0);
}

#  define ASSERT_UNIT_EPSILON 0.0002

/* Checks are flipped so NAN doesn't assert because we're making sure the value was
 * normalized and in the case we don't want NAN to be raising asserts since there
 * is nothing to be done in that case. */
bool is_unit_scale(vec2 v)
{
  float test_unit = length_squared(v);
  return (!(abs(test_unit - 1.0) >= ASSERT_UNIT_EPSILON) ||
          !(abs(test_unit) >= ASSERT_UNIT_EPSILON));
}
bool is_unit_scale(vec3 v)
{
  float test_unit = length_squared(v);
  return (!(abs(test_unit - 1.0) >= ASSERT_UNIT_EPSILON) ||
          !(abs(test_unit) >= ASSERT_UNIT_EPSILON));
}
bool is_unit_scale(vec4 v)
{
  float test_unit = length_squared(v);
  return (!(abs(test_unit - 1.0) >= ASSERT_UNIT_EPSILON) ||
          !(abs(test_unit) >= ASSERT_UNIT_EPSILON));
}

/** \} */

#endif /* GPU_SHADER_MATH_VECTOR_LIB_GLSL */
