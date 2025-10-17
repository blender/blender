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

#include "BLI_enum_flags.hh"
#include "BLI_map.hh"
#include "BLI_mutex.hh"

#include "DRW_render.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"

#include "eevee_material_shared.hh"

namespace blender::eevee {

using StaticShader = gpu::StaticShader;

/* Keep alphabetical order and clean prefix. */
enum eShaderType {
  AMBIENT_OCCLUSION_PASS = 0,

  FILM_COPY,
  FILM_COMP,
  FILM_CRYPTOMATTE_POST,
  FILM_FRAG,
  FILM_PASS_CONVERT_COMBINED,
  FILM_PASS_CONVERT_DEPTH,
  FILM_PASS_CONVERT_VALUE,
  FILM_PASS_CONVERT_COLOR,
  FILM_PASS_CONVERT_CRYPTOMATTE,

  DEFERRED_AOV_CLEAR,
  DEFERRED_CAPTURE_EVAL,
  DEFERRED_COMBINE,
  DEFERRED_LIGHT_SINGLE,
  DEFERRED_LIGHT_DOUBLE,
  DEFERRED_LIGHT_TRIPLE,
  DEFERRED_PLANAR_EVAL,
  DEFERRED_THICKNESS_AMEND,
  DEFERRED_TILE_CLASSIFY,

  DEBUG_GBUFFER,
  DEBUG_SURFELS,
  DEBUG_IRRADIANCE_GRID,

  DISPLAY_PROBE_VOLUME,
  DISPLAY_PROBE_SPHERE,
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
  LIGHT_SHADOW_SETUP,

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

  RENDERPASS_CLEAR,

  SPHERE_PROBE_CONVOLVE,
  SPHERE_PROBE_IRRADIANCE,
  SPHERE_PROBE_REMAP,
  SPHERE_PROBE_SELECT,
  SPHERE_PROBE_SUNLIGHT,

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
  SHADOW_TILEMAP_RENDERMAP,
  SHADOW_TILEMAP_INIT,
  SHADOW_TILEMAP_TAG_UPDATE,
  SHADOW_TILEMAP_TAG_USAGE_OPAQUE,
  SHADOW_TILEMAP_TAG_USAGE_SURFELS,
  SHADOW_TILEMAP_TAG_USAGE_TRANSPARENT,
  SHADOW_TILEMAP_TAG_USAGE_VOLUME,
  SHADOW_VIEW_VISIBILITY,

  SUBSURFACE_CONVOLVE,
  SUBSURFACE_SETUP,

  SURFEL_CLUSTER_BUILD,
  SURFEL_LIGHT,
  SURFEL_LIST_BUILD,
  SURFEL_LIST_FLATTEN,
  SURFEL_LIST_PREFIX,
  SURFEL_LIST_PREPARE,
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
 * Bitmask representing the shader categories.
 * This allows the loading of certain parts of the engine to kick-in as soon as the shaders that
 * depends on it are compiled.
 */
enum ShaderGroups : uint32_t {
  NONE = 0,
  DEFERRED_LIGHTING_SHADERS = 1 << 0,
  DEFERRED_CAPTURE_SHADERS = 1 << 1,
  DEFERRED_PLANAR_SHADERS = 1 << 2,
  DEPTH_OF_FIELD_SHADERS = 1 << 3,
  HIZ_SHADERS = 1 << 4,
  HORIZON_SCAN_SHADERS = 1 << 5,
  LIGHT_CULLING_SHADERS = 1 << 6,
  IRRADIANCE_BAKE_SHADERS = 1 << 7,
  SPHERE_PROBE_SHADERS = 1 << 8,
  SHADOW_SHADERS = 1 << 9,
  AMBIENT_OCCLUSION_SHADERS = 1 << 10,
  MOTION_BLUR_SHADERS = 1 << 11,
  RAYTRACING_SHADERS = 1 << 12,
  FILM_SHADERS = 1 << 13,
  SUBSURFACE_SHADERS = 1 << 14,
  SURFEL_SHADERS = 1 << 15,
  VERTEX_COPY_SHADERS = 1 << 16,
  VOLUME_EVAL_SHADERS = 1 << 17,
  DEFAULT_MATERIALS = 1 << 18,
  WORLD_SHADERS = 1 << 19,
  MATERIAL_SHADERS = 1 << 20,
  VOLUME_PROBE_SHADERS = 1 << 21,
};
ENUM_OPERATORS(ShaderGroups)

/**
 * Shader module. shared between instances.
 */
class ShaderModule {
 private:
  std::array<StaticShader, MAX_SHADER_TYPE> shaders_;

  Mutex mutex_;

  class SpecializationsKey {
   private:
    uint64_t hash_value_;

   public:
    SpecializationsKey(int render_buffers_shadow_id,
                       int shadow_ray_count,
                       int shadow_ray_step_count,
                       bool use_split_indirect,
                       bool use_lightprobe_eval)
    {
      BLI_assert(render_buffers_shadow_id >= -1);
      BLI_assert(shadow_ray_count >= 1 && shadow_ray_count <= 4);
      BLI_assert(shadow_ray_step_count >= 1 && shadow_ray_step_count <= 16);
      BLI_assert(uint64_t(use_split_indirect) >= 0 && uint64_t(use_split_indirect) <= 1);
      hash_value_ = render_buffers_shadow_id + 1;
      hash_value_ = (hash_value_ << 2) | (shadow_ray_count - 1);
      hash_value_ = (hash_value_ << 4) | (shadow_ray_step_count - 1);
      hash_value_ = (hash_value_ << 1) | uint64_t(use_split_indirect);
      hash_value_ = (hash_value_ << 1) | uint64_t(use_lightprobe_eval);
    }

    uint64_t hash() const
    {
      return hash_value_;
    }

    bool operator==(const SpecializationsKey &k) const
    {
      return hash_value_ == k.hash_value_;
    }
  };

  Map<SpecializationsKey, SpecializationBatchHandle> specialization_handles_;

  static gpu::StaticShaderCache<ShaderModule> &get_static_cache()
  {
    /** Shared shader module across all engine instances. */
    static gpu::StaticShaderCache<ShaderModule> static_cache;
    return static_cache;
  }

 public:
  ShaderModule();
  ~ShaderModule();

  /* Trigger async compilation for the given shaders groups. */
  ShaderGroups static_shaders_load_async(ShaderGroups request_bits)
  {
    return static_shaders_load(request_bits, false);
  }
  /* Wait for async compilation to finish for the given shaders groups.
   * If shaders are not scheduled to async compile, this will do blocking compilation. */
  ShaderGroups static_shaders_wait_ready(ShaderGroups request_bits)
  {
    return static_shaders_load(request_bits, true);
  }

  bool request_specializations(bool block_until_ready,
                               int render_buffers_shadow_id,
                               int shadow_ray_count,
                               int shadow_ray_step_count,
                               bool use_split_indirect,
                               bool use_lightprobe_eval);

  gpu::Shader *static_shader_get(eShaderType shader_type);
  GPUMaterial *material_shader_get(::Material *blender_mat,
                                   bNodeTree *nodetree,
                                   eMaterialPipeline pipeline_type,
                                   eMaterialGeometry geometry_type,
                                   bool deferred_compilation,
                                   ::Material *default_mat);
  GPUMaterial *world_shader_get(::World *blender_world,
                                bNodeTree *nodetree,
                                eMaterialPipeline pipeline_type,
                                bool deferred_compilation);

  void material_create_info_amend(GPUMaterial *mat, GPUCodegenOutput *codegen);

  /** Only to be used by Instance constructor. */
  static ShaderModule *module_get();
  static void module_free();

 private:
  const char *static_shader_create_info_name_get(eShaderType shader_type);
  ShaderGroups static_shaders_load(ShaderGroups request_bits, bool block_until_ready);
};

}  // namespace blender::eevee
