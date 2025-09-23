/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

struct MeshVertex {
  int _pad; /* TODO(fclem): Add explicit attribute loading for mesh. */
  METAL_CONSTRUCTOR_1(MeshVertex, int, _pad)
};

struct PointCloudPoint {
  int _pad; /* TODO(fclem): Add explicit attribute loading for mesh. */
  METAL_CONSTRUCTOR_1(PointCloudPoint, int, _pad)
};

struct CurvesPoint {
  int curve_id;
  int point_id;
  int curve_segment;

  METAL_CONSTRUCTOR_3(CurvesPoint, int, curve_id, int, point_id, int, curve_segment)
};

struct WorldPoint {
  int _pad;
  METAL_CONSTRUCTOR_1(WorldPoint, int, _pad)
};

struct VolumePoint {
  int _pad; /* TODO(fclem): Add explicit attribute loading for volumes. */
  METAL_CONSTRUCTOR_1(VolumePoint, int, _pad)
};

struct GPencilPoint {
  int _pad;
  METAL_CONSTRUCTOR_1(GPencilPoint, int, _pad)
};
