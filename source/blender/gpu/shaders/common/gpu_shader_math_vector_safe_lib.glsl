/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_safe_lib.glsl"

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
template<typename VecT> VecT safe_mod(VecT a, VecT b)
{
  return select(VecT(0), mod(a, b), notEqual(b, VecT(0)));
}
template float2 safe_mod<float2>(float2, float2);
template float3 safe_mod<float3>(float3, float3);
template float4 safe_mod<float4>(float4, float4);

/**
 * Safe `a` modulo `b`.
 * If `b` equal 0 the result will be 0.
 */
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

/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float2 compatible_mod(float2 a, float b)
{
  return float2(compatible_mod(a.x, b), compatible_mod(a.y, b));
}
/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float3 compatible_mod(float3 a, float b)
{
  return float3(compatible_mod(a.x, b), compatible_mod(a.y, b), compatible_mod(a.z, b));
}
/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float4 compatible_mod(float4 a, float b)
{
  return float4(compatible_mod(a.x, b),
                compatible_mod(a.y, b),
                compatible_mod(a.z, b),
                compatible_mod(a.w, b));
}

/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float2 compatible_mod(float2 a, float2 b)
{
  return float2(compatible_mod(a.x, b.x), compatible_mod(a.y, b.y));
}
/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float3 compatible_mod(float3 a, float3 b)
{
  return float3(compatible_mod(a.x, b.x), compatible_mod(a.y, b.y), compatible_mod(a.z, b.z));
}
/**
 * A version of mod that behaves similar to C++ `std::modf`, and is safe such that it returns 0
 * when b is also 0.
 */
float4 compatible_mod(float4 a, float4 b)
{
  return float4(compatible_mod(a.x, b.x),
                compatible_mod(a.y, b.y),
                compatible_mod(a.z, b.z),
                compatible_mod(a.w, b.w));
}

/**
 * Safe divide `a` by `b`.
 * If `b` equal 0 the result will be 0.
 */
template<typename VecT> VecT safe_divide(VecT a, VecT b)
{
  return select(VecT(0), a / b, notEqual(b, VecT(0)));
}
template float2 safe_divide<float2>(float2, float2);
template float3 safe_divide<float3>(float3, float3);
template float4 safe_divide<float4>(float4, float4);

/* NOTE: Cannot overload templates. */

/**
 * Safe divide `a` by `b`.
 * If `b` equal 0 the result will be 0.
 */
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

/**
 * Return normalized version of the `vector` or a default normalized vector if `vector` is invalid.
 */
template<typename VecT> VecT safe_normalize_and_get_length(VecT vector, out float out_length)
{
  float length_squared = dot(vector, vector);
  constexpr float threshold = 1e-35f;
  if (length_squared > threshold) {
    out_length = sqrt(length_squared);
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

/**
 * Return normalized version of the `vector` or a default normalized vector if `vector` is invalid.
 */
template<typename VecT> VecT safe_normalize(VecT vector)
{
  float unused_length = 0.0f;
  return safe_normalize_and_get_length(vector, unused_length);
}
template float2 safe_normalize<float2>(float2);
template float3 safe_normalize<float3>(float3);
template float4 safe_normalize<float4>(float4);

/**
 * Safe reciprocal function. Returns `1/a`.
 * If `a` equal 0 the result will be 0.
 */
template<typename VecT> VecT safe_rcp(VecT a)
{
  return select(VecT(0.0f), (1.0f / a), notEqual(a, VecT(0.0f)));
}
template float2 safe_rcp<float2>(float2);
template float3 safe_rcp<float3>(float3);
template float4 safe_rcp<float4>(float4);

/**
 * A version of pow that returns a fallback value if the computation is undefined. From the spec:
 * The result is undefined if x < 0 or if x = 0 and y is less than or equal 0.
 */
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

/**
 * Wrap the given value a to fall within the range [b, c].
 */
float2 wrap(float2 a, float2 b, float2 c)
{
  return float2(wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y));
}
/**
 * Wrap the given value a to fall within the range [b, c].
 */
float3 wrap(float3 a, float3 b, float3 c)
{
  return float3(wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y), wrap(a.z, b.z, c.z));
}
/**
 * Wrap the given value a to fall within the range [b, c].
 */
float4 wrap(float4 a, float4 b, float4 c)
{
  return float4(
      wrap(a.x, b.x, c.x), wrap(a.y, b.y, c.y), wrap(a.z, b.z, c.z), wrap(a.w, b.w, c.w));
}
