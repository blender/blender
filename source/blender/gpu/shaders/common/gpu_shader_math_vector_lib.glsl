/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_base_lib.glsl"

/* WORKAROUND: to guard against double include in EEVEE. */
#ifndef GPU_SHADER_MATH_VECTOR_LIB_GLSL
#  define GPU_SHADER_MATH_VECTOR_LIB_GLSL

/* Metal does not need prototypes. */
#  ifndef GPU_METAL

/**
 * Return true if all components is equal to zero.
 */
bool is_zero(float2 vec);
bool is_zero(float3 vec);
bool is_zero(float4 vec);

/**
 * Return true if any component is equal to zero.
 */
bool is_any_zero(float2 vec);
bool is_any_zero(float3 vec);
bool is_any_zero(float4 vec);

/**
 * Return true if the deference between`a` and `b` is below the `epsilon` value.
 * Epsilon value is scaled by magnitude of `a` before comparison.
 */
bool almost_equal_relative(float2 a, float2 b, float epsilon_factor);
bool almost_equal_relative(float3 a, float3 b, float epsilon_factor);
bool almost_equal_relative(float4 a, float4 b, float epsilon_factor);

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
float2 safe_mod(float2 a, float2 b);
float3 safe_mod(float3 a, float3 b);
float4 safe_mod(float4 a, float4 b);
float2 safe_mod(float2 a, float b);
float3 safe_mod(float3 a, float b);
float4 safe_mod(float4 a, float b);

/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float2 compatible_mod(float2 a, float2 b);
float3 compatible_mod(float3 a, float3 b);
float4 compatible_mod(float4 a, float4 b);
float2 compatible_mod(float2 a, float b);
float3 compatible_mod(float3 a, float b);
float4 compatible_mod(float4 a, float b);

/**
 * Wrap the given value a to fall within the range [b, c].
 */
float2 wrap(float2 a, float2 b, float2 c);
float3 wrap(float3 a, float3 b, float3 c);
float4 wrap(float4 a, float4 b, float4 c);

/**
 * Returns \a a if it is a multiple of \a b or the next multiple or \a b after \b a .
 * In other words, it is equivalent to `divide_ceil(a, b) * b`.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
int2 ceil_to_multiple(int2 a, int2 b);
int3 ceil_to_multiple(int3 a, int3 b);
int4 ceil_to_multiple(int4 a, int4 b);
uint2 ceil_to_multiple(uint2 a, uint2 b);
uint3 ceil_to_multiple(uint3 a, uint3 b);
uint4 ceil_to_multiple(uint4 a, uint4 b);

/**
 * Integer division that returns the ceiling, instead of flooring like normal C division.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
int2 divide_ceil(int2 a, int2 b);
int3 divide_ceil(int3 a, int3 b);
int4 divide_ceil(int4 a, int4 b);
uint2 divide_ceil(uint2 a, uint2 b);
uint3 divide_ceil(uint3 a, uint3 b);
uint4 divide_ceil(uint4 a, uint4 b);

/**
 * Component wise, use vector to replace min if it is smaller and max if bigger.
 */
void min_max(float2 vector, inout float2 min, inout float2 max);
void min_max(float3 vector, inout float3 min, inout float3 max);
void min_max(float4 vector, inout float4 min, inout float4 max);

/**
 * Safe divide `a` by `b`.
 * If `b` equal 0 the result will be 0.
 */
float2 safe_divide(float2 a, float2 b);
float3 safe_divide(float3 a, float3 b);
float4 safe_divide(float4 a, float4 b);
float2 safe_divide(float2 a, float b);
float3 safe_divide(float3 a, float b);
float4 safe_divide(float4 a, float b);

/**
 * Return the manhattan length of `a`.
 * This is also the sum of the absolute value of all components.
 */
float length_manhattan(float2 a);
float length_manhattan(float3 a);
float length_manhattan(float4 a);

/**
 * Return the length squared of `a`.
 */
float length_squared(float2 a);
float length_squared(float3 a);
float length_squared(float4 a);

/**
 * Return the manhattan distance between `a` and `b`.
 */
float distance_manhattan(float2 a, float2 b);
float distance_manhattan(float3 a, float3 b);
float distance_manhattan(float4 a, float4 b);

/**
 * Return the squared distance between `a` and `b`.
 */
float distance_squared(float2 a, float2 b);
float distance_squared(float3 a, float3 b);
float distance_squared(float4 a, float4 b);

/**
 * Return the projection of `p` onto `v_proj`.
 */
float3 project(float3 p, float3 v_proj);

/**
 * Return normalized version of the `vector` and its length.
 */
float2 normalize_and_get_length(float2 vector, out float out_length);
float3 normalize_and_get_length(float3 vector, out float out_length);
float4 normalize_and_get_length(float4 vector, out float out_length);

/**
 * Return normalized version of the `vector` or a default normalized vector if `vector` is invalid.
 */
float2 safe_normalize(float2 vector);
float3 safe_normalize(float3 vector);
float4 safe_normalize(float4 vector);

/**
 * Safe reciprocal function. Returns `1/a`.
 * If `a` equal 0 the result will be 0.
 */
float2 safe_rcp(float2 a);
float3 safe_rcp(float3 a);
float4 safe_rcp(float4 a);

/**
 * A version of pow that returns a fallback value if the computation is undefined. From the spec:
 * The result is undefined if x < 0 or if x = 0 and y is less than or equal 0.
 */
float2 fallback_pow(float2 a, float b, float2 fallback);
float3 fallback_pow(float3 a, float b, float3 fallback);
float4 fallback_pow(float4 a, float b, float4 fallback);

/**
 * Per component linear interpolation.
 */
float2 interpolate(float2 a, float2 b, float t);
float3 interpolate(float3 a, float3 b, float t);
float4 interpolate(float4 a, float4 b, float t);

/**
 * Return half-way point between `a` and  `b`.
 */
float2 midpoint(float2 a, float2 b);
float3 midpoint(float3 a, float3 b);
float4 midpoint(float4 a, float4 b);

/**
 * Return `vector` if `incident` and `reference` are pointing in the same direction.
 */
// float2 faceforward(float2 vector, float2 incident, float2 reference); /* Built-in GLSL. */

/**
 * Return the index of the component with the greatest absolute value.
 */
int dominant_axis(float3 a);

/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector can be in any perpendicular direction.
 * \note Returned vector might not the same length as \a v.
 */
float3 orthogonal(float3 v);
/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector is always rotated 90 degrees counter clock wise.
 */
float2 orthogonal(float2 v);
int2 orthogonal(int2 v);

/**
 * Return true if the difference between`a` and `b` is below the `epsilon` value.
 */
bool is_equal(float2 a, float2 b, float epsilon);
bool is_equal(float3 a, float3 b, float epsilon);
bool is_equal(float4 a, float4 b, float epsilon);

/**
 * Return the maximum component of a vector.
 */
float reduce_max(float2 a);
float reduce_max(float3 a);
float reduce_max(float4 a);
int reduce_max(int2 a);
int reduce_max(int3 a);
int reduce_max(int4 a);

/**
 * Return the minimum component of a vector.
 */
float reduce_min(float2 a);
float reduce_min(float3 a);
float reduce_min(float4 a);
int reduce_min(int2 a);
int reduce_min(int3 a);
int reduce_min(int4 a);

/**
 * Return the sum of the components of a vector.
 */
float reduce_add(float2 a);
float reduce_add(float3 a);
float reduce_add(float4 a);
int reduce_add(int2 a);
int reduce_add(int3 a);
int reduce_add(int4 a);

/**
 * Return the product of the components of a vector.
 */
float reduce_mul(float2 a);
float reduce_mul(float3 a);
float reduce_mul(float4 a);
int reduce_mul(int2 a);
int reduce_mul(int3 a);
int reduce_mul(int4 a);

/**
 * Return the average of the components of a vector.
 */
float average(float2 a);
float average(float3 a);
float average(float4 a);

#  endif /* !GPU_METAL */

/* ---------------------------------------------------------------------- */
/** \name Implementation
 * \{ */

template<typename VecT> bool is_zero(VecT vec)
{
  return all(equal(vec, VecT(0.0f)));
}
template bool is_zero<float2>(float2);
template bool is_zero<float3>(float3);
template bool is_zero<float4>(float4);

template<typename VecT> bool is_any_zero(VecT vec)
{
  return any(equal(vec, VecT(0.0f)));
}
template bool is_any_zero<float2>(float2);
template bool is_any_zero<float3>(float3);
template bool is_any_zero<float4>(float4);

template<typename VecT, int dim>
bool almost_equal_relative(VecT a, VecT b, const float epsilon_factor)
{
  for (int i = 0; i < dim; i++) {
    if (abs(a[i] - b[i]) > epsilon_factor * abs(a[i])) {
      return false;
    }
  }
  return true;
}
template bool almost_equal_relative<float2, 2>(float2 a, float2 b, const float epsilon_factor);
template bool almost_equal_relative<float3, 3>(float3 a, float3 b, const float epsilon_factor);
template bool almost_equal_relative<float4, 4>(float4 a, float4 b, const float epsilon_factor);

template<typename VecT> VecT safe_mod(VecT a, VecT b)
{
  return select(VecT(0), mod(a, b), notEqual(b, VecT(0)));
}
template float2 safe_mod<float2>(float2, float2);
template float3 safe_mod<float3>(float3, float3);
template float4 safe_mod<float4>(float4, float4);

/* NOTE: Cannot overload templates. */
float2 safe_mod(float2 a, float b)
{
  return (b != 0.0f) ? mod(a, float2(b)) : float2(0);
}
float3 safe_mod(float3 a, float b)
{
  return (b != 0.0f) ? mod(a, float3(b)) : float3(0);
}
float4 safe_mod(float4 a, float b)
{
  return (b != 0.0f) ? mod(a, float4(b)) : float4(0);
}

float2 compatible_mod(float2 a, float b)
{
  return float2(compatible_mod(a.x, b), compatible_mod(a.y, b));
}
float3 compatible_mod(float3 a, float b)
{
  return float3(compatible_mod(a.x, b), compatible_mod(a.y, b), compatible_mod(a.z, b));
}
float4 compatible_mod(float4 a, float b)
{
  return float4(compatible_mod(a.x, b),
                compatible_mod(a.y, b),
                compatible_mod(a.z, b),
                compatible_mod(a.w, b));
}

float2 compatible_mod(float2 a, float2 b)
{
  return float2(compatible_mod(a.x, b.x), compatible_mod(a.y, b.y));
}
float3 compatible_mod(float3 a, float3 b)
{
  return float3(compatible_mod(a.x, b.x), compatible_mod(a.y, b.y), compatible_mod(a.z, b.z));
}
float4 compatible_mod(float4 a, float4 b)
{
  return float4(compatible_mod(a.x, b.x),
                compatible_mod(a.y, b.y),
                compatible_mod(a.z, b.z),
                compatible_mod(a.w, b.w));
}

float2 wrap(float2 a, float2 b, float2 c)
{
  return float2(wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y));
}
float3 wrap(float3 a, float3 b, float3 c)
{
  return float3(wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y), wrap(a.z, b.z, c.z));
}
float4 wrap(float4 a, float4 b, float4 c)
{
  return float4(
      wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y), wrap(a.z, b.z, c.z), wrap(a.w, b.w, c.w));
}

template<typename VecT> VecT ceil_to_multiple(VecT a, VecT b)
{
  return ((a + b - VecT(1)) / b) * b;
}
template int2 ceil_to_multiple<int2>(int2, int2);
template int3 ceil_to_multiple<int3>(int3, int3);
template int4 ceil_to_multiple<int4>(int4, int4);
template uint2 ceil_to_multiple<uint2>(uint2, uint2);
template uint3 ceil_to_multiple<uint3>(uint3, uint3);
template uint4 ceil_to_multiple<uint4>(uint4, uint4);

template<typename VecT> VecT divide_ceil(VecT a, VecT b)
{
  return (a + b - VecT(1)) / b;
}
template int2 divide_ceil<int2>(int2, int2);
template int3 divide_ceil<int3>(int3, int3);
template int4 divide_ceil<int4>(int4, int4);
template uint2 divide_ceil<uint2>(uint2, uint2);
template uint3 divide_ceil<uint3>(uint3, uint3);
template uint4 divide_ceil<uint4>(uint4, uint4);

template<typename VecT> void min_max(VecT vector, inout VecT min_v, inout VecT max_v)
{
  min_v = min(vector, min_v);
  max_v = max(vector, max_v);
}
template void min_max<float2>(float2, inout float2, inout float2);
template void min_max<float3>(float3, inout float3, inout float3);
template void min_max<float4>(float4, inout float4, inout float4);

template<typename VecT> VecT safe_divide(VecT a, VecT b)
{
  return select(VecT(0), a / b, notEqual(b, VecT(0)));
}
template float2 safe_divide<float2>(float2, float2);
template float3 safe_divide<float3>(float3, float3);
template float4 safe_divide<float4>(float4, float4);

/* NOTE: Cannot overload templates. */
float2 safe_divide(float2 a, float b)
{
  return (b != 0.0f) ? (a / b) : float2(0);
}
float3 safe_divide(float3 a, float b)
{
  return (b != 0.0f) ? (a / b) : float3(0);
}
float4 safe_divide(float4 a, float b)
{
  return (b != 0.0f) ? (a / b) : float4(0);
}

template<typename VecT> float length_manhattan(VecT a)
{
  return dot(abs(a), VecT(1));
}
template float length_manhattan<float2>(float2);
template float length_manhattan<float3>(float3);
template float length_manhattan<float4>(float4);

template<typename VecT> float length_squared(VecT a)
{
  return dot(a, a);
}
template float length_squared<float2>(float2);
template float length_squared<float3>(float3);
template float length_squared<float4>(float4);

template<typename VecT> float distance_manhattan(VecT a, VecT b)
{
  return length_manhattan(a - b);
}
template float distance_manhattan<float2>(float2, float2);
template float distance_manhattan<float3>(float3, float3);
template float distance_manhattan<float4>(float4, float4);

template<typename VecT> float distance_squared(VecT a, VecT b)
{
  return length_squared(a - b);
}
template float distance_squared<float2>(float2, float2);
template float distance_squared<float3>(float3, float3);
template float distance_squared<float4>(float4, float4);

float3 project(float3 p, float3 v_proj)
{
  if (is_zero(v_proj)) {
    return float3(0);
  }
  return v_proj * (dot(p, v_proj) / dot(v_proj, v_proj));
}

template<typename VecT> VecT normalize_and_get_length(VecT vector, out float out_length)
{
  out_length = length_squared(vector);
  constexpr float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0f;
  return VecT(0.0f);
}
template float2 normalize_and_get_length<float2>(float2, out float);
template float3 normalize_and_get_length<float3>(float3, out float);
template float4 normalize_and_get_length<float4>(float4, out float);

template<typename VecT> VecT safe_normalize_and_get_length(VecT vector, out float out_length)
{
  out_length = length_squared(vector);
  constexpr float threshold = 1e-35f;
  if (out_length > threshold) {
    out_length = sqrt(out_length);
    return vector / out_length;
  }
  /* Either the vector is small or one of its values contained `nan`. */
  out_length = 0.0f;
  VecT result = VecT(0.0f);
  result[0] = 1.0f;
  return result;
}
template float2 safe_normalize_and_get_length<float2>(float2, out float);
template float3 safe_normalize_and_get_length<float3>(float3, out float);
template float4 safe_normalize_and_get_length<float4>(float4, out float);

template<typename VecT> VecT safe_normalize(VecT vector)
{
  float unused_length = 0.0f;
  return safe_normalize_and_get_length(vector, unused_length);
}
template float2 safe_normalize<float2>(float2);
template float3 safe_normalize<float3>(float3);
template float4 safe_normalize<float4>(float4);

template<typename VecT> VecT safe_rcp(VecT a)
{
  return select(VecT(0.0f), (1.0f / a), notEqual(a, VecT(0.0f)));
}
template float2 safe_rcp<float2>(float2);
template float3 safe_rcp<float3>(float3);
template float4 safe_rcp<float4>(float4);

float2 fallback_pow(float2 a, float b, float2 fallback)
{
  return float2(fallback_pow(a.x, b, fallback.x), fallback_pow(a.y, b, fallback.y));
}
float3 fallback_pow(float3 a, float b, float3 fallback)
{
  return float3(fallback_pow(a.x, b, fallback.x),
                fallback_pow(a.y, b, fallback.y),
                fallback_pow(a.z, b, fallback.z));
}
float4 fallback_pow(float4 a, float b, float4 fallback)
{
  return float4(fallback_pow(a.x, b, fallback.x),
                fallback_pow(a.y, b, fallback.y),
                fallback_pow(a.z, b, fallback.z),
                fallback_pow(a.w, b, fallback.w));
}

float2 compatible_pow(float2 a, float2 b)
{
  return float2(compatible_pow(a.x, b.x), compatible_pow(a.y, b.y));
}

float3 compatible_pow(float3 a, float3 b)
{
  return float3(compatible_pow(a.x, b.x), compatible_pow(a.y, b.y), compatible_pow(a.z, b.z));
}

float4 compatible_pow(float4 a, float4 b)
{
  return float4(compatible_pow(a.x, b.x),
                compatible_pow(a.y, b.y),
                compatible_pow(a.z, b.z),
                compatible_pow(a.w, b.w));
}

template<typename VecT> VecT interpolate(VecT a, VecT b, float t)
{
  return mix(a, b, t);
}
template float2 interpolate<float2>(float2, float2, float);
template float3 interpolate<float3>(float3, float3, float);
template float4 interpolate<float4>(float4, float4, float);

template<typename VecT> VecT midpoint(VecT a, VecT b)
{
  return (a + b) * 0.5f;
}
template float2 midpoint<float2>(float2, float2);
template float3 midpoint<float3>(float3, float3);
template float4 midpoint<float4>(float4, float4);

int dominant_axis(float3 a)
{
  float3 b = abs(a);
  return ((b.x > b.y) ? ((b.x > b.z) ? 0 : 2) : ((b.y > b.z) ? 1 : 2));
}

float3 orthogonal(float3 v)
{
  switch (dominant_axis(v)) {
    default:
    case 0:
      return float3(-v.y - v.z, v.x, v.x);
    case 1:
      return float3(v.y, -v.x - v.z, v.y);
    case 2:
      return float3(v.z, v.z, -v.x - v.y);
  }
}

float2 orthogonal(float2 v)
{
  return float2(-v.y, v.x);
}
int2 orthogonal(int2 v)
{
  return int2(-v.y, v.x);
}

bool is_equal(float2 a, float2 b, float epsilon)
{
  return all(lessThanEqual(abs(a - b), float2(epsilon)));
}
bool is_equal(float3 a, float3 b, float epsilon)
{
  return all(lessThanEqual(abs(a - b), float3(epsilon)));
}
bool is_equal(float4 a, float4 b, float epsilon)
{
  return all(lessThanEqual(abs(a - b), float4(epsilon)));
}

float reduce_max(float2 a)
{
  return max(a.x, a.y);
}
float reduce_max(float3 a)
{
  return max(a.x, max(a.y, a.z));
}
float reduce_max(float4 a)
{
  return max(max(a.x, a.y), max(a.z, a.w));
}
int reduce_max(int2 a)
{
  return max(a.x, a.y);
}
int reduce_max(int3 a)
{
  return max(a.x, max(a.y, a.z));
}
int reduce_max(int4 a)
{
  return max(max(a.x, a.y), max(a.z, a.w));
}

float reduce_min(float2 a)
{
  return min(a.x, a.y);
}
float reduce_min(float3 a)
{
  return min(a.x, min(a.y, a.z));
}
float reduce_min(float4 a)
{
  return min(min(a.x, a.y), min(a.z, a.w));
}
int reduce_min(int2 a)
{
  return min(a.x, a.y);
}
int reduce_min(int3 a)
{
  return min(a.x, min(a.y, a.z));
}
int reduce_min(int4 a)
{
  return min(min(a.x, a.y), min(a.z, a.w));
}

float reduce_add(float2 a)
{
  return a.x + a.y;
}
float reduce_add(float3 a)
{
  return a.x + a.y + a.z;
}
float reduce_add(float4 a)
{
  return a.x + a.y + a.z + a.w;
}
int reduce_add(int2 a)
{
  return a.x + a.y;
}
int reduce_add(int3 a)
{
  return a.x + a.y + a.z;
}
int reduce_add(int4 a)
{
  return a.x + a.y + a.z + a.w;
}

float reduce_mul(float2 a)
{
  return a.x * a.y;
}
float reduce_mul(float3 a)
{
  return a.x * a.y * a.z;
}
float reduce_mul(float4 a)
{
  return a.x * a.y * a.z * a.w;
}
int reduce_mul(int2 a)
{
  return a.x * a.y;
}
int reduce_mul(int3 a)
{
  return a.x * a.y * a.z;
}
int reduce_mul(int4 a)
{
  return a.x * a.y * a.z * a.w;
}

float average(float2 a)
{
  return reduce_add(a) * (1.0f / 2.0f);
}
float average(float3 a)
{
  return reduce_add(a) * (1.0f / 3.0f);
}
float average(float4 a)
{
  return reduce_add(a) * (1.0f / 4.0f);
}

#  define ASSERT_UNIT_EPSILON 0.0002f

/* Checks are flipped so NAN doesn't assert because we're making sure the value was
 * normalized and in the case we don't want NAN to be raising asserts since there
 * is nothing to be done in that case. */
bool is_unit_scale(float2 v)
{
  float test_unit = length_squared(v);
  return (!(abs(test_unit - 1.0f) >= ASSERT_UNIT_EPSILON) ||
          !(abs(test_unit) >= ASSERT_UNIT_EPSILON));
}
bool is_unit_scale(float3 v)
{
  float test_unit = length_squared(v);
  return (!(abs(test_unit - 1.0f) >= ASSERT_UNIT_EPSILON) ||
          !(abs(test_unit) >= ASSERT_UNIT_EPSILON));
}
bool is_unit_scale(float4 v)
{
  float test_unit = length_squared(v);
  return (!(abs(test_unit - 1.0f) >= ASSERT_UNIT_EPSILON) ||
          !(abs(test_unit) >= ASSERT_UNIT_EPSILON));
}

/** \} */

#endif /* GPU_SHADER_MATH_VECTOR_LIB_GLSL */
