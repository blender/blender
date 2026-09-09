/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* -------------------------------------------------------------------- */
/** \name Projection Matrices.
 * \{ */

/**
 * \brief Create an orthographic projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * The resulting matrix can be used with either #project_point or #transform_point.
 */
float4x4 projection_orthographic(
    float left, float right, float bottom, float top, float near_clip, float far_clip)
{
  float x_delta = right - left;
  float y_delta = top - bottom;
  float z_delta = far_clip - near_clip;

  float4x4 mat = float4x4(1.0f);
  if (x_delta != 0.0f && y_delta != 0.0f && z_delta != 0.0f) {
    mat[0][0] = 2.0f / x_delta;
    mat[3][0] = -(right + left) / x_delta;
    mat[1][1] = 2.0f / y_delta;
    mat[3][1] = -(top + bottom) / y_delta;
    mat[2][2] = -2.0f / z_delta; /* NOTE: negate Z. */
    mat[3][2] = -(far_clip + near_clip) / z_delta;
  }
  return mat;
}

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * `left`, `right`, `bottom`, `top` are frustum side distances at `z=near_clip`.
 * The resulting matrix can be used with #project_point.
 */
float4x4 projection_perspective(
    float left, float right, float bottom, float top, float near_clip, float far_clip)
{
  float x_delta = right - left;
  float y_delta = top - bottom;
  float z_delta = far_clip - near_clip;

  float4x4 mat = float4x4(1.0f);
  if (x_delta != 0.0f && y_delta != 0.0f && z_delta != 0.0f) {
    mat[0][0] = near_clip * 2.0f / x_delta;
    mat[1][1] = near_clip * 2.0f / y_delta;
    mat[2][0] = (right + left) / x_delta; /* NOTE: negate Z. */
    mat[2][1] = (top + bottom) / y_delta;
    mat[2][2] = -(far_clip + near_clip) / z_delta;
    mat[2][3] = -1.0f;
    mat[3][2] = (-2.0f * near_clip * far_clip) / z_delta;
    mat[3][3] = 0.0f;
  }
  return mat;
}

/**
 * \brief Create a perspective projection matrix using OpenGL coordinate convention:
 * Maps each axis range to [-1..1] range for all axes.
 * Uses field of view angles instead of plane distances.
 * The resulting matrix can be used with #project_point.
 */
float4x4 projection_perspective_fov(float angle_left,
                                    float angle_right,
                                    float angle_bottom,
                                    float angle_top,
                                    float near_clip,
                                    float far_clip)
{
  float4x4 mat = projection_perspective(
      tan(angle_left), tan(angle_right), tan(angle_bottom), tan(angle_top), near_clip, far_clip);
  mat[0][0] /= near_clip;
  mat[1][1] /= near_clip;
  return mat;
}

/** \} */
