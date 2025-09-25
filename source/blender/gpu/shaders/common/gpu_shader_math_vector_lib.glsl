/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Returns \a a if it is a multiple of \a b or the next multiple or \a b after \b a .
 * In other words, it is equivalent to `divide_ceil(a, b) * b`.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
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

/**
 * Integer division that returns the ceiling, instead of flooring like normal C division.
 * It is undefined if \a a is negative or \b b is not strictly positive.
 */
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

/**
 * Component wise, use vector to replace min if it is smaller and max if bigger.
 */
template<typename VecT> void min_max(VecT vector, inout VecT min_v, inout VecT max_v)
{
  min_v = min(vector, min_v);
  max_v = max(vector, max_v);
}
template void min_max<float2>(float2, inout float2, inout float2);
template void min_max<float3>(float3, inout float3, inout float3);
template void min_max<float4>(float4, inout float4, inout float4);

/**
 * Return the manhattan length of `a`.
 * This is also the sum of the absolute value of all components.
 */
template<typename VecT> float length_manhattan(VecT a)
{
  return dot(abs(a), VecT(1));
}
template float length_manhattan<float2>(float2);
template float length_manhattan<float3>(float3);
template float length_manhattan<float4>(float4);

/**
 * Return the length squared of `a`.
 */
template<typename VecT> float length_squared(VecT a)
{
  return dot(a, a);
}
template float length_squared<float2>(float2);
template float length_squared<float3>(float3);
template float length_squared<float4>(float4);

/**
 * Return the manhattan distance between `a` and `b`.
 */
template<typename VecT> float distance_manhattan(VecT a, VecT b)
{
  return length_manhattan(a - b);
}
template float distance_manhattan<float2>(float2, float2);
template float distance_manhattan<float3>(float3, float3);
template float distance_manhattan<float4>(float4, float4);

/**
 * Return the squared distance between `a` and `b`.
 */
template<typename VecT> float distance_squared(VecT a, VecT b)
{
  return length_squared(a - b);
}
template float distance_squared<float2>(float2, float2);
template float distance_squared<float3>(float3, float3);
template float distance_squared<float4>(float4, float4);

/**
 * Return normalized version of the `vector` and its length.
 */
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

/**
 * Per component linear interpolation.
 */
template<typename VecT> VecT interpolate(VecT a, VecT b, float t)
{
  return mix(a, b, t);
}
template float2 interpolate<float2>(float2, float2, float);
template float3 interpolate<float3>(float3, float3, float);
template float4 interpolate<float4>(float4, float4, float);

/**
 * Return half-way point between `a` and  `b`.
 */
template<typename VecT> VecT midpoint(VecT a, VecT b)
{
  return (a + b) * 0.5f;
}
template float2 midpoint<float2>(float2, float2);
template float3 midpoint<float3>(float3, float3);
template float4 midpoint<float4>(float4, float4);

/**
 * Return the index of the component with the greatest absolute value.
 */
int dominant_axis(float3 a)
{
  float3 b = abs(a);
  return ((b.x > b.y) ? ((b.x > b.z) ? 0 : 2) : ((b.y > b.z) ? 1 : 2));
}

/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector is always rotated 90 degrees counter clock wise.
 */
template<typename VecT> VecT orthogonal(VecT v)
{
  return VecT(-v.y, v.x);
}
template int2 orthogonal<int2>(int2);
template float2 orthogonal<float2>(float2);

/**
 * Calculates a perpendicular vector to \a v.
 * \note Returned vector can be in any perpendicular direction.
 * \note Returned vector might not the same length as \a v.
 */
template<> float3 orthogonal<float3>(float3 v)
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

/**
 * Return `vector` if `incident` and `reference` are pointing in the same direction.
 */
// float2 faceforward(float2 vector, float2 incident, float2 reference); /* Built-in GLSL. */
