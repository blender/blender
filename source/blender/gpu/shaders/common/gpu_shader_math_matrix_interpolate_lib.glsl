/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_quaternion_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"
#include "gpu_shader_math_rotation_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Interpolate
 * \{ */

/**
 * Naive interpolation implementation, faster than polar decomposition
 *
 * \note This code is about five times faster than the polar decomposition.
 * However, it gives un-expected results even with non-uniformly scaled matrices,
 * see #46418 for an example.
 *
 * \param A: Input matrix which is totally effective with `t = 0.0`.
 * \param B: Input matrix which is totally effective with `t = 1.0`.
 * \param t: Interpolation factor.
 */
/* TODO(fclem): Implement */
// float3x3 interpolate_fast(float3x3 a, float3x3 b, float t);

/**
 * Naive transform matrix interpolation,
 * based on naive-decomposition-based interpolation from #interpolate_fast<T, 3, 3>.
 */
float4x4 interpolate_fast(float4x4 a, float4x4 b, float t)
{
  float3 a_loc, b_loc;
  float3 a_scale, b_scale;
  Quaternion a_quat, b_quat;
  to_loc_rot_scale(a, a_loc, a_quat, a_scale);
  to_loc_rot_scale(b, b_loc, b_quat, b_scale);

  float3 location = interpolate(a_loc, b_loc, t);
  float3 scale = interpolate(a_scale, b_scale, t);
  Quaternion rotation = interpolate(a_quat, b_quat, t);
  return from_loc_rot_scale(location, rotation, scale);
}

/**
 * Compute Moore-Penrose pseudo inverse of matrix.
 * Singular values below epsilon are ignored for stability (truncated SVD).
 */
/* TODO(fclem): Implement */
// mat4x4 pseudo_invert(mat4x4 mat, float epsilon); /* Not implemented. Too complex to port. */

/** \} */
