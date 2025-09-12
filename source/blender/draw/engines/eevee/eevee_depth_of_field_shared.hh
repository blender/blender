/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "GPU_shader_shared_utils.hh"

#include "eevee_camera_shared.hh"
#include "eevee_defines.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/* 5% error threshold. */
#define DOF_FAST_GATHER_COC_ERROR 0.05
#define DOF_GATHER_RING_COUNT 5
#define DOF_DILATE_RING_COUNT 3

struct DepthOfFieldData {
  /** Size of the render targets for gather & scatter passes. */
  int2 extent;
  /** Size of a pixel in uv space (1.0 / extent). */
  float2 texel_size;
  /** Scale factor for anisotropic bokeh. */
  float2 bokeh_anisotropic_scale;
  float2 bokeh_anisotropic_scale_inv;
  /* Correction factor to align main target pixels with the filtered mipmap chain texture. */
  float2 gather_uv_fac;
  /** Scatter parameters. */
  float scatter_coc_threshold;
  float scatter_color_threshold;
  float scatter_neighbor_max_color;
  int scatter_sprite_per_row;
  /** Number of side the bokeh shape has. */
  float bokeh_blades;
  /** Rotation of the bokeh shape. */
  float bokeh_rotation;
  /** Multiplier and bias to apply to linear depth to Circle of confusion (CoC). */
  float coc_mul, coc_bias;
  /** Maximum absolute allowed Circle of confusion (CoC). Min of computed max and user max. */
  float coc_abs_max;
  /** Copy of camera type. */
  eCameraType camera_type;
  /** Weights of spatial filtering in stabilize pass. Not array to avoid alignment restriction. */
  float4 filter_samples_weight;
  float filter_center_weight;
  /** Max number of sprite in the scatter pass for each ground. */
  uint scatter_max_rect;

  int _pad0, _pad1;
};
BLI_STATIC_ASSERT_ALIGN(DepthOfFieldData, 16)

struct ScatterRect {
  /** Color and CoC of the 4 pixels the scatter sprite represents. */
  float4 color_and_coc[4];
  /** Rect center position in half pixel space. */
  float2 offset;
  /** Rect half extent in half pixel space. */
  float2 half_extent;
};
BLI_STATIC_ASSERT_ALIGN(ScatterRect, 16)

static inline float coc_radius_from_camera_depth(DepthOfFieldData dof, float depth)
{
  depth = (dof.camera_type != CAMERA_ORTHO) ? 1.0f / depth : depth;
  return dof.coc_mul * depth + dof.coc_bias;
}

static inline float regular_polygon_side_length(float sides_count)
{
  return 2.0f * sinf(EEVEE_PI / sides_count);
}

/* Returns intersection ratio between the radius edge at theta and the regular polygon edge.
 * Start first corners at theta == 0. */
static inline float circle_to_polygon_radius(float sides_count, float theta)
{
  /* From Graphics Gems from CryENGINE 3 (SIGGRAPH 2013) by Tiago Sousa (slide 36). */
  float side_angle = (2.0f * EEVEE_PI) / sides_count;
  return cosf(side_angle * 0.5f) /
         cosf(theta - side_angle * floorf((sides_count * theta + EEVEE_PI) / (2.0f * EEVEE_PI)));
}

/* Remap input angle to have homogenous spacing of points along a polygon edge.
 * Expects theta to be in [0..2pi] range. */
static inline float circle_to_polygon_angle(float sides_count, float theta)
{
  float side_angle = (2.0f * EEVEE_PI) / sides_count;
  float halfside_angle = side_angle * 0.5f;
  float side = floorf(theta / side_angle);
  /* Length of segment from center to the middle of polygon side. */
  float adjacent = circle_to_polygon_radius(sides_count, 0.0f);

  /* This is the relative position of the sample on the polygon half side. */
  float local_theta = theta - side * side_angle;
  float ratio = (local_theta - halfside_angle) / halfside_angle;

  float halfside_len = regular_polygon_side_length(sides_count) * 0.5f;
  float opposite = ratio * halfside_len;

  /* NOTE: atan(y_over_x) has output range [-pi/2..pi/2]. */
  float final_local_theta = atanf(opposite / adjacent);

  return side * side_angle + final_local_theta;
}

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
