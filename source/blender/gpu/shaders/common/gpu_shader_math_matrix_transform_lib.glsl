/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* -------------------------------------------------------------------- */
/** \name Transform function.
 * \{ */

/**
 * Transform a 3d point using a 3x3 matrix (rotation & scale).
 */
float3 transform_point(float3x3 mat, float3 point)
{
  return mat * point;
}

/**
 * Transform a 3d point using a 4x4 matrix (location & rotation & scale).
 */
float3 transform_point(float4x4 mat, float3 point)
{
  return (mat * float4(point, 1.0f)).xyz;
}

/**
 * Transform a 3d direction vector using a 3x3 matrix (rotation & scale).
 */
float3 transform_direction(float3x3 mat, float3 direction)
{
  return mat * direction;
}

/**
 * Transform a 3d direction vector using a 4x4 matrix (rotation & scale).
 */
float3 transform_direction(float4x4 mat, float3 direction)
{
  return to_float3x3(mat) * direction;
}

/**
 * Project a point using a matrix (location & rotation & scale & perspective divide).
 */
float2 project_point(float3x3 mat, float2 point)
{
  float3 tmp = mat * float3(point, 1.0f);
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return tmp.xy / abs(tmp.z);
}
/**
 * Project a point using a matrix (location & rotation & scale & perspective divide).
 */
float3 project_point(float4x4 mat, float3 point)
{
  float4 tmp = mat * float4(point, 1.0f);
  /* Absolute value to not flip the frustum upside down behind the camera. */
  return tmp.xyz / abs(tmp.w);
}

/** \} */
