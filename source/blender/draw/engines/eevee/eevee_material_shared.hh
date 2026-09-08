/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shared code between host and client code-bases.
 */

#pragma once

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/**
 * Defines the specific rendering pass or shading strategy for a material.
 * This determines which shader variants are generated.
 */
enum eMaterialPipeline {
  /* G-Buffer pass, lighting is calculated in a separate pass. */
  MAT_PIPE_DEFERRED = 0,
  /* Main shading pass where lighting is calculated per-pixel during geometry submission. */
  MAT_PIPE_FORWARD,

  /**
   * Pre-pass shaders: These are used to populate the Depth buffer and Motion Vectors before
   * the main shading pass.
   */

  /* Standard depth-only pass for the deferred pipeline. */
  MAT_PIPE_PREPASS_DEFERRED,
  MAT_PIPE_PREPASS_DEFERRED_VELOCITY,
  /* Standard depth-only pass for the forward pipeline (opaque only). */
  MAT_PIPE_PREPASS_FORWARD,
  MAT_PIPE_PREPASS_FORWARD_VELOCITY,
  /* Per object prepass to handle the transparency overlap option. */
  MAT_PIPE_PREPASS_OVERLAP,
  /* Depth pre-pass specifically for planar reflection probes. */
  MAT_PIPE_PREPASS_PLANAR,

  /* Pipeline for baking meshes volume occupancy to the froxel grid. */
  MAT_PIPE_VOLUME_OCCUPANCY,
  /* Pipeline for baking volume material properties to the froxel grid. */
  MAT_PIPE_VOLUME_MATERIAL,

  /* Pipeline for shadow map rendering. */
  MAT_PIPE_SHADOW,

  /* Pipeline for surfel capture. */
  MAT_PIPE_CAPTURE,
};

/**
 * Defines the geometric primitive type the shader is intended to run on.
 * This affects attribute fetching and attribute interpolation.
 */
enum eMaterialGeometry {
  /* These maps directly to object types. */
  MAT_GEOM_MESH = 0,
  MAT_GEOM_POINTCLOUD,
  MAT_GEOM_CURVES,
  MAT_GEOM_VOLUME,

  /* Special case: The world background / HDRI environment shader. */
  MAT_GEOM_WORLD,
};

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
