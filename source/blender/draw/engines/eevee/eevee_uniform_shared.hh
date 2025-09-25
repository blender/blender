/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared code between host and client codebases.
 */

#pragma once

#include "eevee_camera_shared.hh"
#include "eevee_film_shared.hh"
#include "eevee_hizbuffer_shared.hh"
#include "eevee_raytrace_shared.hh"
#include "eevee_renderbuffers_shared.hh"
#include "eevee_subsurface_shared.hh"
#include "eevee_volume_shared.hh"

#ifndef GPU_SHADER
namespace blender::eevee {
#endif

/* This should be inside "eevee_light_shared.hh" but it would pull a huge header that is not
 * essential for most shaders. This could be moved back if including "eevee_bxdf_lib.glsl" is used
 * only for shading shaders. */
enum LightingType : uint32_t {
  LIGHT_DIFFUSE = 0u,
  LIGHT_SPECULAR = 1u,
  LIGHT_TRANSMISSION = 2u,
  LIGHT_VOLUME = 3u,
  /* WORKAROUND: Special value used to tag translucent BSDF with thickness.
   * Fall back to LIGHT_DIFFUSE. */
  LIGHT_TRANSLUCENT_WITH_THICKNESS = 4u,
};

struct ShadowSceneData {
  /* Number of shadow rays to shoot for each light. */
  int ray_count;
  /* Number of shadow samples to take for each shadow ray. */
  int step_count;
  /* Bounding radius for a film pixel at 1 unit from the camera. */
  float film_pixel_radius;
  /* Global switch for jittered shadows. */
  bool32_t use_jitter;
};
BLI_STATIC_ASSERT_ALIGN(ShadowSceneData, 16)

/* Light Clamping. */
struct ClampData {
  float sun_threshold;
  float surface_direct;
  float surface_indirect;
  float volume_direct;
  float volume_indirect;
  float _pad0;
  float _pad1;
  float _pad2;
};
BLI_STATIC_ASSERT_ALIGN(ClampData, 16)

struct PipelineInfoData {
  float alpha_hash_scale;
  bool32_t is_sphere_probe;
  /* WORKAROUND: Usually we would use imageSize to get the number of layers and get this id.
   * However, some implementation return the number of layers from the base texture instead of the
   * texture view (see #146132). So we always pass the correct layer index manually to avoid any
   * platform inconsistency. */
  int gbuffer_additional_data_layer_id;
  float _pad2;
};
BLI_STATIC_ASSERT_ALIGN(PipelineInfoData, 16)

/* Combines data from several modules to avoid wasting binding slots. */
struct UniformData {
  AOData ao;
  CameraData camera;
  ClampData clamp;
  FilmData film;
  HiZData hiz;
  RayTraceData raytrace;
  RenderBuffersInfoData render_pass;
  ShadowSceneData shadow;
  SubsurfaceData subsurface;
  VolumesInfoData volumes;
  PipelineInfoData pipeline;
};
BLI_STATIC_ASSERT_ALIGN(UniformData, 16)

/**
 * World space clip plane equation. Used to render planar light-probes.
 * Moved here to avoid dependencies to light-probe just for this. */
struct ClipPlaneData {
  float4 plane;
};
BLI_STATIC_ASSERT_ALIGN(ClipPlaneData, 16)

#ifndef GPU_SHADER
}  // namespace blender::eevee
#endif
