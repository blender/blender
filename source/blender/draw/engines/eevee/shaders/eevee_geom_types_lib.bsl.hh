/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_shader_shared.hh"
#include "gpu_shader_compat.hh"

struct MeshVertex {
  float3 lP;
  /* Needed for tangent transformation. */
  float3x3 world_to_object;
  /* TODO(fclem): Add explicit attribute loading for mesh through vertex fetch. */

  /* Default derived from local position. */
  float3 orco_default;
};

struct PointCloudPoint {
  float3 lP;
  int point_id;
  /* Default derived from local position. */
  float3 orco_default;
};

struct CurvesPoint {
  int curve_id;
  int point_id;
  int curve_segment;
  /* One bit per attribute (up to 16). */
  uint is_point_attribute;
  /* Default derived from local position. */
  float3 orco_default;
};

struct WorldPoint {
  float3 lP;
};

struct VolumePoint {
  float3 lP;
  /* Default derived from local position. */
  float3 orco_default;

  float3 grid_co[DRW_GRID_PER_VOLUME_MAX];
};
