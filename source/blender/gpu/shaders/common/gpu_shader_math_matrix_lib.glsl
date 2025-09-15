/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Flip the matrix across its diagonal. Also flips dimensions for non square matrices.
 */
// float3x3 transpose(float3x3 mat); /* Built-In using shading languages. */

/**
 * Returns the determinant of the matrix.
 * It can be interpreted as the signed volume (or area) of the unit cube after transformation.
 */
// float determinant(float3x3 mat); /* Built-In using shading languages. */

/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float2x2 invert(float2x2 mat)
{
  return inverse(mat);
}
/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float3x3 invert(float3x3 mat)
{
  return inverse(mat);
}
/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float4x4 invert(float4x4 mat)
{
  return inverse(mat);
}

/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float2x2 invert(float2x2 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0f;
  return r_success ? inverse(mat) : float2x2(0.0f);
}
/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float3x3 invert(float3x3 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0f;
  return r_success ? inverse(mat) : float3x3(0.0f);
}
/**
 * Returns the inverse of a square matrix or zero matrix on failure.
 * \a r_success is optional and set to true if the matrix was inverted successfully.
 */
float4x4 invert(float4x4 mat, out bool r_success)
{
  r_success = determinant(mat) != 0.0f;
  return r_success ? inverse(mat) : float4x4(0.0f);
}

/**
 * Equivalent to `mat * from_location(translation)` but with fewer operation.
 */
float4x4 translate(float4x4 mat, float3 translation)
{
  mat[3].xyz += translation[0] * mat[0].xyz;
  mat[3].xyz += translation[1] * mat[1].xyz;
  mat[3].xyz += translation[2] * mat[2].xyz;
  return mat;
}
/**
 * Equivalent to `mat * from_location(translation)` but with fewer operation.
 */
float4x4 translate(float4x4 mat, float2 translation)
{
  mat[3].xyz += translation[0] * mat[0].xyz;
  mat[3].xyz += translation[1] * mat[1].xyz;
  return mat;
}

/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
float3x3 scale(float3x3 mat, float2 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  return mat;
}
/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
float3x3 scale(float3x3 mat, float3 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  mat[2] *= scale[2];
  return mat;
}
/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
float4x4 scale(float4x4 mat, float2 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  return mat;
}
/**
 * Equivalent to `mat * from_scale(scale)` but with fewer operation.
 */
float4x4 scale(float4x4 mat, float3 scale)
{
  mat[0] *= scale[0];
  mat[1] *= scale[1];
  mat[2] *= scale[2];
  return mat;
}

/** \} */
