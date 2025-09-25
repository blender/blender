/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Normalize
 * \{ */

/* Needs to be defined for correct overloading. */
#if defined(GPU_OPENGL) || defined(GPU_METAL)
float2 normalize(float2 a)
{
  return a * inversesqrt(length_squared(a));
}
float3 normalize(float3 a)
{
  return a * inversesqrt(length_squared(a));
}
float4 normalize(float4 a)
{
  return a * inversesqrt(length_squared(a));
}
#endif

/**
 * Normalize each column of the matrix individually.
 */
#if 0 /* Remove unused variants as they are slow down compilation. */
float2x2 normalize(float2x2 mat)
{
  float2x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  return ret;
}
float2x3 normalize(float2x3 mat)
{
  float2x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  return ret;
}
float2x4 normalize(float2x4 mat)
{
  float2x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  return ret;
}
float3x2 normalize(float3x2 mat)
{
  float3x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  ret[2] = normalize(mat[2].xy);
  return ret;
}
#endif
float3x3 normalize(float3x3 mat)
{
  float3x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  ret[2] = normalize(mat[2].xyz);
  return ret;
}
#if 0 /* Remove unused variants as they are slow down compilation. */
float3x4 normalize(float3x4 mat)
{
  float3x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  ret[2] = normalize(mat[2].xyzw);
  return ret;
}
float4x2 normalize(float4x2 mat)
{
  float4x2 ret;
  ret[0] = normalize(mat[0].xy);
  ret[1] = normalize(mat[1].xy);
  ret[2] = normalize(mat[2].xy);
  ret[3] = normalize(mat[3].xy);
  return ret;
}
float4x3 normalize(float4x3 mat)
{
  float4x3 ret;
  ret[0] = normalize(mat[0].xyz);
  ret[1] = normalize(mat[1].xyz);
  ret[2] = normalize(mat[2].xyz);
  ret[3] = normalize(mat[3].xyz);
  return ret;
}
#endif
float4x4 normalize(float4x4 mat)
{
  float4x4 ret;
  ret[0] = normalize(mat[0].xyzw);
  ret[1] = normalize(mat[1].xyzw);
  ret[2] = normalize(mat[2].xyzw);
  ret[3] = normalize(mat[3].xyzw);
  return ret;
}

/**
 * Normalize each column of the matrix individually.
 * Return the length of each column vector.
 */
#if 0 /* Remove unused variants as they are slow down compilation. */
float2x2 normalize_and_get_size(float2x2 mat, out float2 r_size)
{
  float size_x = 0.0f, size_y = 0.0f;
  float2x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = float2(size_x, size_y);
  return ret;
}
float2x3 normalize_and_get_size(float2x3 mat, out float2 r_size)
{
  float size_x = 0.0f, size_y = 0.0f;
  float2x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = float2(size_x, size_y);
  return ret;
}
float2x4 normalize_and_get_size(float2x4 mat, out float2 r_size)
{
  float size_x = 0.0f, size_y = 0.0f;
  float2x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  r_size = float2(size_x, size_y);
  return ret;
}
float3x2 normalize_and_get_size(float3x2 mat, out float3 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f;
  float3x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = float3(size_x, size_y, size_z);
  return ret;
}
#endif
float3x3 normalize_and_get_size(float3x3 mat, out float3 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f;
  float3x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = float3(size_x, size_y, size_z);
  return ret;
}
#if 0 /* Remove unused variants as they are slow down compilation. */
float3x4 normalize_and_get_size(float3x4 mat, out float3 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f;
  float3x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  r_size = float3(size_x, size_y, size_z);
  return ret;
}
float4x2 normalize_and_get_size(float4x2 mat, out float4 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f, size_w = 0.0f;
  float4x2 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = float4(size_x, size_y, size_z, size_w);
  return ret;
}
float4x3 normalize_and_get_size(float4x3 mat, out float4 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f, size_w = 0.0f;
  float4x3 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = float4(size_x, size_y, size_z, size_w);
  return ret;
}
#endif
float4x4 normalize_and_get_size(float4x4 mat, out float4 r_size)
{
  float size_x = 0.0f, size_y = 0.0f, size_z = 0.0f, size_w = 0.0f;
  float4x4 ret;
  ret[0] = normalize_and_get_length(mat[0], size_x);
  ret[1] = normalize_and_get_length(mat[1], size_y);
  ret[2] = normalize_and_get_length(mat[2], size_z);
  ret[3] = normalize_and_get_length(mat[3], size_w);
  r_size = float4(size_x, size_y, size_z, size_w);
  return ret;
}

/** \} */
