/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct MeshVertex {
  float3 lP;
  /* TODO(fclem): Add explicit attribute loading for mesh through vertex fetch. */
};

struct PointCloudPoint {
  int point_id;
};

struct CurvesPoint {
  int curve_id;
  int point_id;
  int curve_segment;
};

struct WorldPoint {
  float3 lP;
};

struct VolumePoint {
  float3 lP;
};
