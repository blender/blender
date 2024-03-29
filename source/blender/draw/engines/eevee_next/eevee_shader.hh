/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shader module that manage shader libraries, deferred compilation,
 * and static shader usage.
 */

#pragma once

#include <array>
#include <string>

#include "BLI_string_ref.hh"
#include "DRW_render.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"

#include "eevee_material.hh"
#include "eevee_sync.hh"

namespace blender::eevee {

/* Keep alphabetical order and clean prefix. */
enum eShaderType {
  AMBIENT_OCCLUSION_PASS = 0,

  FILM_FRAG,
  FILM_COMP,
  FILM_CRYPTOMATTE_POST,

  DEFERRED_CAPTURE_EVAL,
  DEFERRED_COMBINE,
  DEFERRED_LIGHT_SINGLE,
  DEFERRED_LIGHT_DOUBLE,
  DEFERRED_LIGHT_TRIPLE,
  DEFERRED_PLANAR_EVAL,
  DEFERRED_TILE_CLASSIFY,
  DEFERRED_TILE_COMPACT,
  DEFERRED_TILE_STENCIL,

  DEBUG_GBUFFER,
  DEBUG_SURFELS,
  DEBUG_IRRADIANCE_GRID,

  DISPLAY_PROBE_GRID,
  DISPLAY_PROBE_REFLECTION,
  DISPLAY_PROBE_PLANAR,

  DOF_BOKEH_LUT,
  DOF_DOWNSAMPLE,
  DOF_FILTER,
  DOF_GATHER_BACKGROUND_LUT,
  DOF_GATHER_BACKGROUND,
  DOF_GATHER_FOREGROUND_LUT,
  DOF_GATHER_FOREGROUND,
  DOF_GATHER_HOLE_FILL,
  DOF_REDUCE,
  DOF_RESOLVE_LUT,
  DOF_RESOLVE,
  DOF_SCATTER,
  DOF_SETUP,
  DOF_STABILIZE,
  DOF_TILES_DILATE_MINABS,
  DOF_TILES_DILATE_MINMAX,
  DOF_TILES_FLATTEN,

  HIZ_UPDATE,
  HIZ_UPDATE_LAYER,
  HIZ_DEBUG,

  HORIZON_DENOISE,
  HORIZON_RESOLVE,
  HORIZON_SCAN,
  HORIZON_SETUP,

  LIGHT_CULLING_DEBUG,
  LIGHT_CULLING_SELECT,
  LIGHT_CULLING_SORT,
  LIGHT_CULLING_TILE,
  LIGHT_CULLING_ZBIN,

  LIGHTPROBE_IRRADIANCE_BOUNDS,
  LIGHTPROBE_IRRADIANCE_OFFSET,
  LIGHTPROBE_IRRADIANCE_RAY,
  LIGHTPROBE_IRRADIANCE_LOAD,
  LIGHTPROBE_IRRADIANCE_WORLD,

  LOOKDEV_DISPLAY,

  MOTION_BLUR_GATHER,
  MOTION_BLUR_TILE_DILATE,
  MOTION_BLUR_TILE_FLATTEN_RGBA,
  MOTION_BLUR_TILE_FLATTEN_RG,

  RAY_DENOISE_BILATERAL,
  RAY_DENOISE_SPATIAL,
  RAY_DENOISE_TEMPORAL,
  RAY_GENERATE,
  RAY_TILE_CLASSIFY,
  RAY_TILE_COMPACT,
  RAY_TRACE_FALLBACK,
  RAY_TRACE_PLANAR,
  RAY_TRACE_SCREEN,

  SPHERE_PROBE_CONVOLVE,
  SPHERE_PROBE_REMAP,
  SPHERE_PROBE_SELECT,
  SPHERE_PROBE_IRRADIANCE,

  SHADOW_CLIPMAP_CLEAR,
  SHADOW_DEBUG,
  SHADOW_PAGE_ALLOCATE,
  SHADOW_PAGE_CLEAR,
  SHADOW_PAGE_DEFRAG,
  SHADOW_PAGE_FREE,
  SHADOW_PAGE_MASK,
  SHADOW_PAGE_TILE_CLEAR,
  SHADOW_PAGE_TILE_STORE,
  SHADOW_TILEMAP_AMEND,
  SHADOW_TILEMAP_BOUNDS,
  SHADOW_TILEMAP_FINALIZE,
  SHADOW_TILEMAP_INIT,
  SHADOW_TILEMAP_TAG_UPDATE,
  SHADOW_TILEMAP_TAG_USAGE_OPAQUE,
  SHADOW_TILEMAP_TAG_USAGE_SURFELS,
  SHADOW_TILEMAP_TAG_USAGE_TRANSPARENT,
  SHADOW_TILEMAP_TAG_USAGE_VOLUME,

  SUBSURFACE_CONVOLVE,
  SUBSURFACE_SETUP,

  SURFEL_CLUSTER_BUILD,
  SURFEL_LIGHT,
  SURFEL_LIST_BUILD,
  SURFEL_LIST_SORT,
  SURFEL_RAY,

  VERTEX_COPY,

  VOLUME_INTEGRATION,
  VOLUME_OCCUPANCY_CONVERT,
  VOLUME_RESOLVE,
  VOLUME_SCATTER,
  VOLUME_SCATTER_WITH_LIGHTS,

  MAX_SHADER_TYPE,
};

/**
 * Shader module. shared between instances.
 */
class ShaderModule {
 private:
  std::array<GPUShader *, MAX_SHADER_TYPE> shaders_;

  /** Shared shader module across all engine instances. */
  static ShaderModule *g_shader_module;

 public:
  ShaderModule();
  ~ShaderModule();

  GPUShader *static_shader_get(eShaderType shader_type);
  GPUMaterial *material_shader_get(::Material *blender_mat,
                                   bNodeTree *nodetree,
                                   eMaterialPipeline pipeline_type,
                                   eMaterialGeometry geometry_type,
                                   bool deferred_compilation);
  GPUMaterial *world_shader_get(::World *blender_world,
                                bNodeTree *nodetree,
                                eMaterialPipeline pipeline_type);
  GPUMaterial *material_shader_get(const char *name,
                                   ListBase &materials,
                                   bNodeTree *nodetree,
                                   eMaterialPipeline pipeline_type,
                                   eMaterialGeometry geometry_type);

  void material_create_info_ammend(GPUMaterial *mat, GPUCodegenOutput *codegen);

  /** Only to be used by Instance constructor. */
  static ShaderModule *module_get();
  static void module_free();

 private:
  const char *static_shader_create_info_name_get(eShaderType shader_type);
};

}  // namespace blender::eevee
