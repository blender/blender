/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

enum eMaterialPipeline {
  MAT_PIPE_DEFERRED = 0,
  MAT_PIPE_FORWARD,
  /* These all map to the depth shader. */
  MAT_PIPE_PREPASS_DEFERRED,
  MAT_PIPE_PREPASS_DEFERRED_VELOCITY,
  MAT_PIPE_PREPASS_OVERLAP,
  MAT_PIPE_PREPASS_FORWARD,
  MAT_PIPE_PREPASS_FORWARD_VELOCITY,
  MAT_PIPE_PREPASS_PLANAR,

  MAT_PIPE_VOLUME_MATERIAL,
  MAT_PIPE_VOLUME_OCCUPANCY,
  MAT_PIPE_SHADOW,
  MAT_PIPE_CAPTURE,
};

enum eMaterialGeometry {
  /* These maps directly to object types. */
  MAT_GEOM_MESH = 0,
  MAT_GEOM_POINTCLOUD,
  MAT_GEOM_CURVES,
  MAT_GEOM_VOLUME,

  /* These maps to special shader. */
  MAT_GEOM_WORLD,
};

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
