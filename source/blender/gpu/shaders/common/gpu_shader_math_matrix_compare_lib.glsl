/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_vector_compare_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Compare / Test
 * \{ */

/**
 * Returns true if all of the matrices components are strictly equal to 0.
 */
bool is_zero(float3x3 a)
{
  if (is_zero(a[0])) {
    if (is_zero(a[1])) {
      if (is_zero(a[2])) {
        return true;
      }
    }
  }
  return false;
}
/**
 * Returns true if all of the matrices components are strictly equal to 0.
 */
bool is_zero(float4x4 a)
{
  if (is_zero(a[0])) {
    if (is_zero(a[1])) {
      if (is_zero(a[2])) {
        if (is_zero(a[3])) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
 * Returns true if matrix has inverted handedness.
 */
bool is_negative(float3x3 mat)
{
  return determinant(mat) < 0.0f;
}
/**
 * Returns true if matrix has inverted handedness.
 *
 * \note It doesn't use determinant(mat4x4) as only the 3x3 components are needed
 * when the matrix is used as a transformation to represent location/scale/rotation.
 */
bool is_negative(float4x4 mat)
{
  return is_negative(to_float3x3(mat));
}

/**
 * Returns true if matrices are equal within the given epsilon.
 */
bool is_equal(float2x2 a, float2x2 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      return true;
    }
  }
  return false;
}
/**
 * Returns true if matrices are equal within the given epsilon.
 */
bool is_equal(float3x3 a, float3x3 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      if (is_equal(a[2], b[2], epsilon)) {
        return true;
      }
    }
  }
  return false;
}
/**
 * Returns true if matrices are equal within the given epsilon.
 */
bool is_equal(float4x4 a, float4x4 b, float epsilon)
{
  if (is_equal(a[0], b[0], epsilon)) {
    if (is_equal(a[1], b[1], epsilon)) {
      if (is_equal(a[2], b[2], epsilon)) {
        if (is_equal(a[3], b[3], epsilon)) {
          return true;
        }
      }
    }
  }
  return false;
}
/**
 * Test if the X, Y and Z axes are perpendicular with each other.
 */
bool is_orthogonal(float3x3 mat)
{
  if (abs(dot(mat[0], mat[1])) > 1e-5f) {
    return false;
  }
  if (abs(dot(mat[1], mat[2])) > 1e-5f) {
    return false;
  }
  if (abs(dot(mat[2], mat[0])) > 1e-5f) {
    return false;
  }
  return true;
}
/**
 * Test if the X, Y and Z axes are perpendicular with each other.
 */
bool is_orthogonal(float4x4 mat)
{
  return is_orthogonal(to_float3x3(mat));
}

/**
 * Test if the X, Y and Z axes are perpendicular with each other and unit length.
 */
bool is_orthonormal(float3x3 mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  if (abs(dot(mat[0], mat[0]) - 1.0f) > 1e-5f) {
    return false;
  }
  if (abs(dot(mat[1], mat[1]) - 1.0f) > 1e-5f) {
    return false;
  }
  if (abs(dot(mat[2], mat[2]) - 1.0f) > 1e-5f) {
    return false;
  }
  return true;
}
/**
 * Test if the X, Y and Z axes are perpendicular with each other and unit length.
 */
bool is_orthonormal(float4x4 mat)
{
  return is_orthonormal(to_float3x3(mat));
}

/**
 * Test if the X, Y and Z axes are perpendicular with each other and the same length.
 */
bool is_uniformly_scaled(float3x3 mat)
{
  if (!is_orthogonal(mat)) {
    return false;
  }
  constexpr float eps = 1e-7f;
  float x = dot(mat[0], mat[0]);
  float y = dot(mat[1], mat[1]);
  float z = dot(mat[2], mat[2]);
  return (abs(x - y) < eps) && abs(x - z) < eps;
}
/**
 * Test if the X, Y and Z axes are perpendicular with each other and the same length.
 */
bool is_uniformly_scaled(float4x4 mat)
{
  return is_uniformly_scaled(to_float3x3(mat));
}

/* Returns true if each individual columns are unit scaled. Mainly for assert usage. */
bool is_unit_scale(float4x4 m)
{
  if (is_unit_scale(m[0])) {
    if (is_unit_scale(m[1])) {
      if (is_unit_scale(m[2])) {
        if (is_unit_scale(m[3])) {
          return true;
        }
      }
    }
  }
  return false;
}
/* Returns true if each individual columns are unit scaled. Mainly for assert usage. */
bool is_unit_scale(float3x3 m)
{
  if (is_unit_scale(m[0])) {
    if (is_unit_scale(m[1])) {
      if (is_unit_scale(m[2])) {
        return true;
      }
    }
  }
  return false;
}
/* Returns true if each individual columns are unit scaled. Mainly for assert usage. */
bool is_unit_scale(float2x2 m)
{
  if (is_unit_scale(m[0])) {
    if (is_unit_scale(m[1])) {
      return true;
    }
  }
  return false;
}

/** \} */
