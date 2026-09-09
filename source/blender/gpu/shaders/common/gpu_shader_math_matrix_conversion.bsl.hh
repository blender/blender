/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_euler.bsl.hh"
#include "gpu_shader_math_matrix_compare.bsl.hh"
#include "gpu_shader_math_quaternion.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Conversion function.
 * \{ */

/**
 * Extract the absolute 3d scale from a transform matrix.
 */
float3 to_scale(float3x3 mat)
{
  return float3(length(mat[0]), length(mat[1]), length(mat[2]));
}
/**
 * Extract the absolute 3d scale from a transform matrix.
 */
float3 to_scale(float4x4 mat)
{
  return to_scale(to_float3x3(mat));
}
/**
 * Extract the absolute 3d scale from a transform matrix.
 */
template<typename MatT, bool allow_negative_scale> float3 to_scale(MatT mat)
{
  float3 result = to_scale(mat);
  if (allow_negative_scale) {
    if (is_negative(mat)) {
      result = -result;
    }
  }
  return result;
}
template float3 to_scale<float3x3, true>(float3x3 mat);
template float3 to_scale<float3x3, false>(float3x3 mat);
template float3 to_scale<float4x4, true>(float4x4 mat);
template float3 to_scale<float4x4, false>(float4x4 mat);

/** \} */
