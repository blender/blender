/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shader module that manage shader libraries, deferred compilation,
 * and static shader usage.
 */

#include "GPU_capabilities.hh"

#include "BKE_material.hh"
#include "BKE_node_runtime.hh"

#include "DNA_world_types.h"

#include "gpu_shader_create_info.hh"

#include "eevee_shader.hh"

#include "eevee_shadow.hh"

#include "BLI_assert.h"
#include "BLI_math_bits.h"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Module
 *
 * \{ */

ShaderModule *ShaderModule::module_get()
{
  return &get_static_cache().get();
}

void ShaderModule::module_free()
{
  get_static_cache().release();
}

ShaderModule::ShaderModule()
{
  for (auto i : IndexRange(MAX_SHADER_TYPE)) {
    const char *name = static_shader_create_info_name_get(eShaderType(i));
#ifndef NDEBUG
    if (name == nullptr) {
      std::cerr << "EEVEE: Missing case for eShaderType(" << i
                << ") in static_shader_create_info_name_get().";
      BLI_assert(0);
    }
    const GPUShaderCreateInfo *create_info = GPU_shader_create_info_get(name);
    BLI_assert_msg(create_info != nullptr, "EEVEE: Missing create info for static shader.");
#endif
    shaders_[i] = StaticShader(name);
  }
}

ShaderModule::~ShaderModule()
{
  /* Cancel compilation to avoid asserts on exit at ShaderCompiler destructor. */

  /* Specializations first, to avoid releasing the base shader while the specialization compilation
   * is still in flight. */
  for (SpecializationBatchHandle &handle : specialization_handles_.values()) {
    if (handle) {
      GPU_shader_batch_specializations_cancel(handle);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Static shaders
 *
 * \{ */

ShaderGroups ShaderModule::static_shaders_load(const ShaderGroups request_bits,
                                               bool block_until_ready)
{
  std::lock_guard lock(mutex_);

  ShaderGroups ready = ShaderGroups::NONE;
  auto request = [&](ShaderGroups bit, Span<eShaderType> shader_types) {
    if (request_bits & bit) {
      bool all_loaded = true;
      for (eShaderType shader : shader_types) {
        if (shaders_[shader].is_ready()) {
          /* Noop. */
        }
        else if (block_until_ready) {
          shaders_[shader].get();
        }
        else {
          shaders_[shader].ensure_compile_async();
          all_loaded = false;
        }
      }
      if (all_loaded) {
        ready |= bit;
      }
    }
  };

#define AS_SPAN(arr) Span<eShaderType>(arr, ARRAY_SIZE(arr))
  {
    /* These are the slowest shaders by far. Submitting them first make sure they overlap with
     * other shaders compilation. */
    const eShaderType shader_list[] = {DEFERRED_LIGHT_TRIPLE,
                                       DEFERRED_LIGHT_SINGLE,
                                       DEFERRED_LIGHT_DOUBLE,
                                       DEFERRED_COMBINE,
                                       DEFERRED_AOV_CLEAR,
                                       DEFERRED_TILE_CLASSIFY};
    request(DEFERRED_LIGHTING_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {AMBIENT_OCCLUSION_PASS};
    request(AMBIENT_OCCLUSION_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {RENDERPASS_CLEAR,
                                       FILM_COPY,
                                       FILM_COMP,
                                       FILM_CRYPTOMATTE_POST,
                                       FILM_FRAG,
                                       FILM_PASS_CONVERT_COMBINED,
                                       FILM_PASS_CONVERT_DEPTH,
                                       FILM_PASS_CONVERT_VALUE,
                                       FILM_PASS_CONVERT_COLOR,
                                       FILM_PASS_CONVERT_CRYPTOMATTE};
    request(FILM_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {DEFERRED_CAPTURE_EVAL};
    request(DEFERRED_CAPTURE_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {DEFERRED_PLANAR_EVAL};
    request(DEFERRED_PLANAR_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {DOF_BOKEH_LUT,
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
                                       DOF_TILES_FLATTEN};
    request(DEPTH_OF_FIELD_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {HIZ_UPDATE, HIZ_UPDATE_LAYER};
    request(HIZ_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {
        HORIZON_DENOISE, HORIZON_RESOLVE, HORIZON_SCAN, HORIZON_SETUP};
    request(HORIZON_SCAN_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {LIGHT_CULLING_DEBUG,
                                       LIGHT_CULLING_SELECT,
                                       LIGHT_CULLING_SORT,
                                       LIGHT_CULLING_TILE,
                                       LIGHT_CULLING_ZBIN,
                                       LIGHT_SHADOW_SETUP};
    request(LIGHT_CULLING_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {
        LIGHTPROBE_IRRADIANCE_BOUNDS, LIGHTPROBE_IRRADIANCE_OFFSET, LIGHTPROBE_IRRADIANCE_RAY};
    request(IRRADIANCE_BAKE_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {MOTION_BLUR_GATHER,
                                       MOTION_BLUR_TILE_DILATE,
                                       MOTION_BLUR_TILE_FLATTEN_RGBA,
                                       MOTION_BLUR_TILE_FLATTEN_RG};
    request(MOTION_BLUR_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {RAY_DENOISE_BILATERAL,
                                       RAY_DENOISE_SPATIAL,
                                       RAY_DENOISE_TEMPORAL,
                                       RAY_GENERATE,
                                       RAY_TILE_CLASSIFY,
                                       RAY_TILE_COMPACT,
                                       RAY_TRACE_FALLBACK,
                                       RAY_TRACE_PLANAR,
                                       RAY_TRACE_SCREEN};
    request(RAYTRACING_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {SPHERE_PROBE_CONVOLVE,
                                       SPHERE_PROBE_IRRADIANCE,
                                       SPHERE_PROBE_REMAP,
                                       SPHERE_PROBE_SELECT,
                                       SPHERE_PROBE_SUNLIGHT};
    request(SPHERE_PROBE_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {LIGHTPROBE_IRRADIANCE_WORLD, LIGHTPROBE_IRRADIANCE_LOAD};
    request(VOLUME_PROBE_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {SHADOW_CLIPMAP_CLEAR,
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
                                       SHADOW_TILEMAP_TAG_USAGE_TRANSPARENT,
                                       SHADOW_VIEW_VISIBILITY};
    request(SHADOW_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {SUBSURFACE_CONVOLVE, SUBSURFACE_SETUP};
    request(SUBSURFACE_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {SURFEL_CLUSTER_BUILD,
                                       SURFEL_LIGHT,
                                       SURFEL_LIST_BUILD,
                                       SURFEL_LIST_SORT,
                                       SHADOW_TILEMAP_TAG_USAGE_SURFELS,
                                       SURFEL_RAY};
    request(SURFEL_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {VERTEX_COPY};
    request(VERTEX_COPY_SHADERS, AS_SPAN(shader_list));
  }
  {
    const eShaderType shader_list[] = {SHADOW_TILEMAP_TAG_USAGE_VOLUME,
                                       VOLUME_INTEGRATION,
                                       VOLUME_OCCUPANCY_CONVERT,
                                       VOLUME_RESOLVE,
                                       VOLUME_SCATTER,
                                       VOLUME_SCATTER_WITH_LIGHTS};
    request(VOLUME_EVAL_SHADERS, AS_SPAN(shader_list));
  }
#undef AS_SPAN
  return ready;
}

bool ShaderModule::request_specializations(bool block_until_ready,
                                           int render_buffers_shadow_id,
                                           int shadow_ray_count,
                                           int shadow_ray_step_count,
                                           bool use_split_indirect,
                                           bool use_lightprobe_eval)
{
  std::lock_guard lock(mutex_);

  SpecializationBatchHandle &specialization_handle = specialization_handles_.lookup_or_add_cb(
      {render_buffers_shadow_id,
       shadow_ray_count,
       shadow_ray_step_count,
       use_split_indirect,
       use_lightprobe_eval},
      [&]() {
        Vector<ShaderSpecialization> specializations;
        for (int i : IndexRange(3)) {
          gpu::Shader *sh = static_shader_get(eShaderType(DEFERRED_LIGHT_SINGLE + i));
          int render_pass_shadow_id_index = GPU_shader_get_constant(sh, "render_pass_shadow_id");
          int use_split_indirect_index = GPU_shader_get_constant(sh, "use_split_indirect");
          int use_lightprobe_eval_index = GPU_shader_get_constant(sh, "use_lightprobe_eval");
          int use_transmission_index = GPU_shader_get_constant(sh, "use_transmission");
          int shadow_ray_count_index = GPU_shader_get_constant(sh, "shadow_ray_count");
          int shadow_ray_step_count_index = GPU_shader_get_constant(sh, "shadow_ray_step_count");

          gpu::shader::SpecializationConstants sp = GPU_shader_get_default_constant_state(sh);

          for (bool use_transmission : {false, true}) {
            sp.set_value(render_pass_shadow_id_index, render_buffers_shadow_id);
            sp.set_value(use_split_indirect_index, use_split_indirect);
            sp.set_value(use_lightprobe_eval_index, use_lightprobe_eval);
            sp.set_value(use_transmission_index, use_transmission);
            sp.set_value(shadow_ray_count_index, shadow_ray_count);
            sp.set_value(shadow_ray_step_count_index, shadow_ray_step_count);

            specializations.append({sh, sp});
          }
        }

        return GPU_shader_batch_specializations(specializations);
      });

  if (specialization_handle) {
    while (!GPU_shader_batch_specializations_is_ready(specialization_handle) && block_until_ready)
    {
      /* Block until ready. */
    }
  }

  return specialization_handle == 0;
}

const char *ShaderModule::static_shader_create_info_name_get(eShaderType shader_type)
{
  switch (shader_type) {
    case AMBIENT_OCCLUSION_PASS:
      return "eevee_ambient_occlusion_pass";
    case FILM_COPY:
      return "eevee_film_copy_frag";
    case FILM_COMP:
      return "eevee_film_comp";
    case FILM_CRYPTOMATTE_POST:
      return "eevee_film_cryptomatte_post";
    case FILM_FRAG:
      return "eevee_film_frag";
    case FILM_PASS_CONVERT_COMBINED:
      return "eevee_film_pass_convert_combined";
    case FILM_PASS_CONVERT_DEPTH:
      return "eevee_film_pass_convert_depth";
    case FILM_PASS_CONVERT_VALUE:
      return "eevee_film_pass_convert_value";
    case FILM_PASS_CONVERT_COLOR:
      return "eevee_film_pass_convert_color";
    case FILM_PASS_CONVERT_CRYPTOMATTE:
      return "eevee_film_pass_convert_cryptomatte";
    case DEFERRED_COMBINE:
      return "eevee_deferred_combine";
    case DEFERRED_LIGHT_SINGLE:
      return "eevee_deferred_light_single";
    case DEFERRED_LIGHT_DOUBLE:
      return "eevee_deferred_light_double";
    case DEFERRED_LIGHT_TRIPLE:
      return "eevee_deferred_light_triple";
    case DEFERRED_AOV_CLEAR:
      return "eevee_deferred_aov_clear";
    case DEFERRED_CAPTURE_EVAL:
      return "eevee_deferred_capture_eval";
    case DEFERRED_PLANAR_EVAL:
      return "eevee_deferred_planar_eval";
    case DEFERRED_THICKNESS_AMEND:
      return "eevee_deferred_thickness_amend";
    case DEFERRED_TILE_CLASSIFY:
      return "eevee_deferred_tile_classify";
    case HIZ_DEBUG:
      return "eevee_hiz_debug";
    case HIZ_UPDATE:
      return "eevee_hiz_update";
    case HIZ_UPDATE_LAYER:
      return "eevee_hiz_update_layer";
    case HORIZON_DENOISE:
      return "eevee_horizon_denoise";
    case HORIZON_RESOLVE:
      return "eevee_horizon_resolve";
    case HORIZON_SCAN:
      return "eevee_horizon_scan";
    case HORIZON_SETUP:
      return "eevee_horizon_setup";
    case LOOKDEV_DISPLAY:
      return "eevee_lookdev_display";
    case MOTION_BLUR_GATHER:
      return "eevee_motion_blur_gather";
    case MOTION_BLUR_TILE_DILATE:
      return "eevee_motion_blur_tiles_dilate";
    case MOTION_BLUR_TILE_FLATTEN_RGBA:
      return "eevee_motion_blur_tiles_flatten_rgba";
    case MOTION_BLUR_TILE_FLATTEN_RG:
      return "eevee_motion_blur_tiles_flatten_rg";
    case DEBUG_SURFELS:
      return "eevee_debug_surfels";
    case DEBUG_IRRADIANCE_GRID:
      return "eevee_debug_irradiance_grid";
    case DEBUG_GBUFFER:
      return "eevee_debug_gbuffer";
    case DISPLAY_PROBE_VOLUME:
      return "eevee_display_lightprobe_volume";
    case DISPLAY_PROBE_SPHERE:
      return "eevee_display_lightprobe_sphere";
    case DISPLAY_PROBE_PLANAR:
      return "eevee_display_lightprobe_planar";
    case DOF_BOKEH_LUT:
      return "eevee_depth_of_field_bokeh_lut";
    case DOF_DOWNSAMPLE:
      return "eevee_depth_of_field_downsample";
    case DOF_FILTER:
      return "eevee_depth_of_field_filter";
    case DOF_GATHER_FOREGROUND_LUT:
      return "eevee_depth_of_field_gather_foreground_lut";
    case DOF_GATHER_FOREGROUND:
      return "eevee_depth_of_field_gather_foreground_no_lut";
    case DOF_GATHER_BACKGROUND_LUT:
      return "eevee_depth_of_field_gather_background_lut";
    case DOF_GATHER_BACKGROUND:
      return "eevee_depth_of_field_gather_background_no_lut";
    case DOF_GATHER_HOLE_FILL:
      return "eevee_depth_of_field_hole_fill";
    case DOF_REDUCE:
      return "eevee_depth_of_field_reduce";
    case DOF_RESOLVE:
      return "eevee_depth_of_field_resolve_no_lut";
    case DOF_RESOLVE_LUT:
      return "eevee_depth_of_field_resolve_lut";
    case DOF_SETUP:
      return "eevee_depth_of_field_setup";
    case DOF_SCATTER:
      return "eevee_depth_of_field_scatter";
    case DOF_STABILIZE:
      return "eevee_depth_of_field_stabilize";
    case DOF_TILES_DILATE_MINABS:
      return "eevee_depth_of_field_tiles_dilate_minabs";
    case DOF_TILES_DILATE_MINMAX:
      return "eevee_depth_of_field_tiles_dilate_minmax";
    case DOF_TILES_FLATTEN:
      return "eevee_depth_of_field_tiles_flatten";
    case LIGHT_CULLING_DEBUG:
      return "eevee_light_culling_debug";
    case LIGHT_CULLING_SELECT:
      return "eevee_light_culling_select";
    case LIGHT_CULLING_SORT:
      return "eevee_light_culling_sort";
    case LIGHT_CULLING_TILE:
      return "eevee_light_culling_tile";
    case LIGHT_CULLING_ZBIN:
      return "eevee_light_culling_zbin";
    case LIGHT_SHADOW_SETUP:
      return "eevee_light_shadow_setup";
    case RAY_DENOISE_SPATIAL:
      return "eevee_ray_denoise_spatial";
    case RAY_DENOISE_TEMPORAL:
      return "eevee_ray_denoise_temporal";
    case RAY_DENOISE_BILATERAL:
      return "eevee_ray_denoise_bilateral";
    case RAY_GENERATE:
      return "eevee_ray_generate";
    case RAY_TRACE_FALLBACK:
      return "eevee_ray_trace_fallback";
    case RAY_TRACE_PLANAR:
      return "eevee_ray_trace_planar";
    case RAY_TRACE_SCREEN:
      return "eevee_ray_trace_screen";
    case RAY_TILE_CLASSIFY:
      return "eevee_ray_tile_classify";
    case RAY_TILE_COMPACT:
      return "eevee_ray_tile_compact";
    case RENDERPASS_CLEAR:
      return "eevee_renderpass_clear";
    case LIGHTPROBE_IRRADIANCE_BOUNDS:
      return "eevee_lightprobe_volume_bounds";
    case LIGHTPROBE_IRRADIANCE_OFFSET:
      return "eevee_lightprobe_volume_offset";
    case LIGHTPROBE_IRRADIANCE_RAY:
      return "eevee_lightprobe_volume_ray";
    case LIGHTPROBE_IRRADIANCE_LOAD:
      return "eevee_lightprobe_volume_load";
    case LIGHTPROBE_IRRADIANCE_WORLD:
      return "eevee_lightprobe_volume_world";
    case SPHERE_PROBE_CONVOLVE:
      return "eevee_lightprobe_sphere_convolve";
    case SPHERE_PROBE_REMAP:
      return "eevee_lightprobe_sphere_remap";
    case SPHERE_PROBE_IRRADIANCE:
      return "eevee_lightprobe_sphere_irradiance";
    case SPHERE_PROBE_SELECT:
      return "eevee_lightprobe_sphere_select";
    case SPHERE_PROBE_SUNLIGHT:
      return "eevee_lightprobe_sphere_sunlight";
    case SHADOW_CLIPMAP_CLEAR:
      return "eevee_shadow_clipmap_clear";
    case SHADOW_DEBUG:
      return "eevee_shadow_debug";
    case SHADOW_PAGE_ALLOCATE:
      return "eevee_shadow_page_allocate";
    case SHADOW_PAGE_CLEAR:
      return "eevee_shadow_page_clear";
    case SHADOW_PAGE_DEFRAG:
      return "eevee_shadow_page_defrag";
    case SHADOW_PAGE_FREE:
      return "eevee_shadow_page_free";
    case SHADOW_PAGE_MASK:
      return "eevee_shadow_page_mask";
    case SHADOW_TILEMAP_AMEND:
      return "eevee_shadow_tilemap_amend";
    case SHADOW_TILEMAP_BOUNDS:
      return "eevee_shadow_tilemap_bounds";
    case SHADOW_TILEMAP_FINALIZE:
      return "eevee_shadow_tilemap_finalize";
    case SHADOW_TILEMAP_RENDERMAP:
      return "eevee_shadow_tilemap_rendermap";
    case SHADOW_TILEMAP_INIT:
      return "eevee_shadow_tilemap_init";
    case SHADOW_TILEMAP_TAG_UPDATE:
      return "eevee_shadow_tag_update";
    case SHADOW_TILEMAP_TAG_USAGE_OPAQUE:
      return "eevee_shadow_tag_usage_opaque";
    case SHADOW_TILEMAP_TAG_USAGE_SURFELS:
      return "eevee_shadow_tag_usage_surfels";
    case SHADOW_TILEMAP_TAG_USAGE_TRANSPARENT:
      return "eevee_shadow_tag_usage_transparent";
    case SHADOW_PAGE_TILE_CLEAR:
      return "eevee_shadow_page_tile_clear";
    case SHADOW_PAGE_TILE_STORE:
      return "eevee_shadow_page_tile_store";
    case SHADOW_TILEMAP_TAG_USAGE_VOLUME:
      return "eevee_shadow_tag_usage_volume";
    case SHADOW_VIEW_VISIBILITY:
      return "eevee_shadow_view_visibility";
    case SUBSURFACE_CONVOLVE:
      return "eevee_subsurface_convolve";
    case SUBSURFACE_SETUP:
      return "eevee_subsurface_setup";
    case SURFEL_CLUSTER_BUILD:
      return "eevee_surfel_cluster_build";
    case SURFEL_LIGHT:
      return "eevee_surfel_light";
    case SURFEL_LIST_BUILD:
      return "eevee_surfel_list_build";
    case SURFEL_LIST_FLATTEN:
      return "eevee_surfel_list_flatten";
    case SURFEL_LIST_PREFIX:
      return "eevee_surfel_list_prefix";
    case SURFEL_LIST_PREPARE:
      return "eevee_surfel_list_prepare";
    case SURFEL_LIST_SORT:
      return "eevee_surfel_list_sort";
    case SURFEL_RAY:
      return "eevee_surfel_ray";
    case VERTEX_COPY:
      return "eevee_vertex_copy";
    case VOLUME_INTEGRATION:
      return "eevee_volume_integration";
    case VOLUME_OCCUPANCY_CONVERT:
      return "eevee_volume_occupancy_convert";
    case VOLUME_RESOLVE:
      return "eevee_volume_resolve";
    case VOLUME_SCATTER:
      return "eevee_volume_scatter";
    case VOLUME_SCATTER_WITH_LIGHTS:
      return "eevee_volume_scatter_with_lights";
    /* To avoid compiler warning about missing case. */
    case MAX_SHADER_TYPE:
      return "";
  }
  return "";
}

gpu::Shader *ShaderModule::static_shader_get(eShaderType shader_type)
{
  return shaders_[shader_type].get();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Materials
 *
 * \{ */

/* Helper class to get free sampler slots for materials. */
class SamplerSlots {
  int first_reserved_;
  int last_reserved_;
  int index_;

 public:
  SamplerSlots(eMaterialPipeline pipeline_type,
               eMaterialGeometry geometry_type,
               bool has_shader_to_rgba)
  {
    index_ = 0;
    if (ELEM(geometry_type, MAT_GEOM_POINTCLOUD, MAT_GEOM_CURVES)) {
      index_ = 2;
    }

    first_reserved_ = MATERIAL_TEXTURE_RESERVED_SLOT_FIRST;
    last_reserved_ = MATERIAL_TEXTURE_RESERVED_SLOT_LAST_NO_EVAL;
    if (geometry_type == MAT_GEOM_WORLD) {
      last_reserved_ = MATERIAL_TEXTURE_RESERVED_SLOT_LAST_WORLD;
    }
    else if (pipeline_type == MAT_PIPE_DEFERRED && has_shader_to_rgba) {
      last_reserved_ = MATERIAL_TEXTURE_RESERVED_SLOT_LAST_HYBRID;
    }
    else if (pipeline_type == MAT_PIPE_FORWARD) {
      last_reserved_ = MATERIAL_TEXTURE_RESERVED_SLOT_LAST_FORWARD;
    }
  }

  int get()
  {
    if (index_ == first_reserved_) {
      index_ = last_reserved_ + 1;
    }
    return index_++;
  }
};

void ShaderModule::material_create_info_amend(GPUMaterial *gpumat, GPUCodegenOutput *codegen_)
{
  using namespace blender::gpu::shader;

  uint64_t shader_uuid = GPU_material_uuid_get(gpumat);

  eMaterialPipeline pipeline_type;
  eMaterialGeometry geometry_type;
  eMaterialDisplacement displacement_type;
  eMaterialThickness thickness_type;
  bool transparent_shadows;
  material_type_from_shader_uuid(shader_uuid,
                                 pipeline_type,
                                 geometry_type,
                                 displacement_type,
                                 thickness_type,
                                 transparent_shadows);

  GPUCodegenOutput &codegen = *codegen_;
  ShaderCreateInfo &info = *reinterpret_cast<ShaderCreateInfo *>(codegen.create_info);

  /* WORKAROUND: Add new ob attr buffer. */
  if (GPU_material_uniform_attributes(gpumat) != nullptr) {
    info.additional_info("draw_object_attributes");

    /* Search and remove the old object attribute UBO which would creating bind point collision. */
    for (auto &resource_info : info.batch_resources_) {
      if (resource_info.bind_type == ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER &&
          resource_info.uniformbuf.name == GPU_ATTRIBUTE_UBO_BLOCK_NAME "[512]")
      {
        info.batch_resources_.remove_first_occurrence_and_reorder(resource_info);
        break;
      }
    }
    /* Remove references to the UBO. */
    info.define("UNI_ATTR(a)", "float4(0.0)");
  }

  SamplerSlots sampler_slots(
      pipeline_type, geometry_type, GPU_material_flag_get(gpumat, GPU_MATFLAG_SHADER_TO_RGBA));

  for (auto &resource : info.batch_resources_) {
    if (resource.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      resource.slot = sampler_slots.get();
    }
  }

  bool use_ao_node = false;

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_AO) &&
      ELEM(pipeline_type, MAT_PIPE_FORWARD, MAT_PIPE_DEFERRED) &&
      geometry_type_has_surface(geometry_type))
  {
    info.define("MAT_AMBIENT_OCCLUSION");
    use_ao_node = true;
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) {
    if (pipeline_type != MAT_PIPE_SHADOW || transparent_shadows) {
      info.define("MAT_TRANSPARENT");
    }
    /* Transparent material do not have any velocity specific pipeline. */
    if (pipeline_type == MAT_PIPE_PREPASS_FORWARD_VELOCITY) {
      pipeline_type = MAT_PIPE_PREPASS_FORWARD;
    }
  }

  /* Only deferred material allow use of cryptomatte and render passes. */
  if (pipeline_type == MAT_PIPE_DEFERRED) {
    info.additional_info("eevee_render_pass_out");
    info.additional_info("eevee_cryptomatte_out");
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE)) {
    info.define("MAT_DIFFUSE");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SUBSURFACE)) {
    info.define("MAT_SUBSURFACE");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT)) {
    info.define("MAT_REFRACTION");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSLUCENT)) {
    info.define("MAT_TRANSLUCENT");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY)) {
    info.define("MAT_REFLECTION");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_COAT)) {
    info.define("MAT_CLEARCOAT");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_REFLECTION_MAYBE_COLORED) == false) {
    info.define("MAT_REFLECTION_COLORLESS");
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACTION_MAYBE_COLORED) == false) {
    info.define("MAT_REFRACTION_COLORLESS");
  }

  const eClosureBits closure_bits = shader_closure_bits_from_flag(gpumat);

  int32_t closure_bin_count = to_gbuffer_bin_count(closure_bits);
  switch (closure_bin_count) {
    /* These need to be separated since the strings need to be static. */
    case 0:
    case 1:
      info.define("CLOSURE_BIN_COUNT", "1");
      break;
    case 2:
      info.define("CLOSURE_BIN_COUNT", "2");
      break;
    case 3:
      info.define("CLOSURE_BIN_COUNT", "3");
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  if (pipeline_type == MAT_PIPE_DEFERRED) {
    switch (closure_bin_count) {
      /* These need to be separated since the strings need to be static. */
      case 0:
      case 1:
        info.define("GBUFFER_LAYER_MAX", "1");
        break;
      case 2:
        info.define("GBUFFER_LAYER_MAX", "2");
        break;
      case 3:
        info.define("GBUFFER_LAYER_MAX", "3");
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    if (closure_bin_count == 2) {
      /* In a lot of cases, we can predict that we do not need the extra GBuffer layers. This
       * simplifies the shader code and improves compilation time (see #145347). */
      const bool colorless_reflection = !GPU_material_flag_get(
          gpumat, GPU_MATFLAG_REFLECTION_MAYBE_COLORED);
      const bool colorless_refraction = !GPU_material_flag_get(
          gpumat, GPU_MATFLAG_REFRACTION_MAYBE_COLORED);
      int closure_layer_count = 0;
      if (closure_bits & CLOSURE_DIFFUSE) {
        closure_layer_count += 1;
      }
      if (closure_bits & CLOSURE_SSS) {
        closure_layer_count += 2;
      }
      if (closure_bits & CLOSURE_REFLECTION) {
        closure_layer_count += colorless_reflection ? 1 : 2;
      }
      if (closure_bits & CLOSURE_REFRACTION) {
        closure_layer_count += colorless_refraction ? 1 : 2;
      }
      if (closure_bits & CLOSURE_TRANSLUCENT) {
        closure_layer_count += 1;
      }
      if (closure_bits & CLOSURE_CLEARCOAT) {
        closure_layer_count += colorless_reflection ? 1 : 2;
      }

      if (closure_layer_count <= 2) {
        info.define("GBUFFER_SIMPLE_CLOSURE_LAYOUT");
      }
    }
  }

  if ((pipeline_type == MAT_PIPE_FORWARD) ||
      GPU_material_flag_get(gpumat, GPU_MATFLAG_SHADER_TO_RGBA))
  {
    switch (closure_bin_count) {
      case 0:
        /* Define nothing. This will in turn define SKIP_LIGHT_EVAL. */
        break;
      /* These need to be separated since the strings need to be static. */
      case 1:
        info.define("LIGHT_CLOSURE_EVAL_COUNT", "1");
        break;
      case 2:
        info.define("LIGHT_CLOSURE_EVAL_COUNT", "2");
        break;
      case 3:
        info.define("LIGHT_CLOSURE_EVAL_COUNT", "3");
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_BARYCENTRIC)) {
    switch (geometry_type) {
      case MAT_GEOM_MESH:
        /* Support using gpu builtin barycentrics. */
        info.define("USE_BARYCENTRICS");
        info.builtins(BuiltinBits::BARYCENTRIC_COORD);
        break;
      case MAT_GEOM_CURVES:
        /* Support using one float2 attribute. See #hair_get_barycentric(). */
        info.define("USE_BARYCENTRICS");
        break;
      default:
        /* No support */
        break;
    }
  }

  /* Allow to use Reverse-Z on OpenGL. Does nothing in other backend. */
  info.builtins(BuiltinBits::CLIP_CONTROL);

  std::stringstream global_vars;
  switch (geometry_type) {
    case MAT_GEOM_MESH:
      if (pipeline_type == MAT_PIPE_VOLUME_MATERIAL) {
        /* If mesh has a volume output, it can receive volume grid attributes from smoke
         * simulation modifier. But the vertex shader might still need access to the vertex
         * attribute for displacement. */
        /* TODO(fclem): Eventually, we could add support for loading both. For now, remove the
         * vertex inputs after conversion (avoid name collision). */
        for (auto &input : info.vertex_inputs_) {
          info.sampler(sampler_slots.get(), ImageType::Float3D, input.name, Frequency::BATCH);
        }
        info.vertex_inputs_.clear();
        /* Volume materials require these for loading the grid attributes from smoke sims. */
        info.additional_info("draw_volume_infos");
      }
      break;
    case MAT_GEOM_POINTCLOUD:
    case MAT_GEOM_CURVES:
      /** Hair attributes come from sampler buffer. Transfer attributes to sampler. */
      for (auto &input : info.vertex_inputs_) {
        if (input.name == "orco") {
          /** NOTE: Orco is generated from strand position for now. */
          global_vars << input.type << " " << input.name << ";\n";
        }
        else {
          info.sampler(sampler_slots.get(), ImageType::FloatBuffer, input.name, Frequency::BATCH);
        }
      }
      info.vertex_inputs_.clear();
      break;
    case MAT_GEOM_WORLD:
      if (pipeline_type == MAT_PIPE_VOLUME_MATERIAL) {
        /* Even if world do not have grid attributes, we use dummy texture binds to pass correct
         * defaults. So we have to replace all attributes as samplers. */
        for (auto &input : info.vertex_inputs_) {
          info.sampler(sampler_slots.get(), ImageType::Float3D, input.name, Frequency::BATCH);
        }
        info.vertex_inputs_.clear();
      }
      /**
       * Only orco layer is supported by world and it is procedurally generated. These are here to
       * make the attribs_load function calls valid.
       */
      for (auto &input : info.vertex_inputs_) {
        global_vars << input.type << " " << input.name << ";\n";
      }
      info.vertex_inputs_.clear();
      break;
    case MAT_GEOM_VOLUME:
      /** Volume grid attributes come from 3D textures. Transfer attributes to samplers. */
      for (auto &input : info.vertex_inputs_) {
        info.sampler(sampler_slots.get(), ImageType::Float3D, input.name, Frequency::BATCH);
      }
      info.vertex_inputs_.clear();
      break;
  }

  const bool support_volume_attributes = ELEM(geometry_type, MAT_GEOM_MESH, MAT_GEOM_VOLUME);
  const bool do_vertex_attrib_load = !ELEM(geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME) &&
                                     (pipeline_type != MAT_PIPE_VOLUME_MATERIAL ||
                                      !support_volume_attributes);

  if (!do_vertex_attrib_load && !info.vertex_out_interfaces_.is_empty()) {
    /* Codegen outputs only one interface. */
    const StageInterfaceInfo &iface = *info.vertex_out_interfaces_.first();
    /* Globals the attrib_load() can write to when it is in the fragment shader. */
    global_vars << "struct " << iface.name << " {\n";
    for (const auto &inout : iface.inouts) {
      global_vars << "  " << inout.type << " " << inout.name << ";\n";
    }
    global_vars << "};\n";
    global_vars << iface.name << " " << iface.instance_name << ";\n";

    info.vertex_out_interfaces_.clear();
  }

  const char *domain_type_frag = "";
  const char *domain_type_vert = "";
  switch (geometry_type) {
    case MAT_GEOM_MESH:
      domain_type_frag = (pipeline_type == MAT_PIPE_VOLUME_MATERIAL) ? "VolumePoint" :
                                                                       "MeshVertex";
      domain_type_vert = "MeshVertex";
      break;
    case MAT_GEOM_POINTCLOUD:
      domain_type_frag = domain_type_vert = "PointCloudPoint";
      break;
    case MAT_GEOM_CURVES:
      domain_type_frag = domain_type_vert = "CurvesPoint";
      break;
    case MAT_GEOM_WORLD:
      domain_type_frag = (pipeline_type == MAT_PIPE_VOLUME_MATERIAL) ? "VolumePoint" :
                                                                       "WorldPoint";
      domain_type_vert = "WorldPoint";
      break;
    case MAT_GEOM_VOLUME:
      domain_type_frag = domain_type_vert = "VolumePoint";
      break;
  }

  std::stringstream attr_load;
  attr_load << "{\n";
  attr_load << (!codegen.attr_load.empty() ? codegen.attr_load : "");
  attr_load << "}\n\n";

  std::stringstream vert_gen, frag_gen;

  if (do_vertex_attrib_load) {
    vert_gen << global_vars.str() << "void attrib_load(" << domain_type_vert << " domain)"
             << attr_load.str();
    frag_gen << "void attrib_load(" << domain_type_frag << " domain) {}\n"; /* Placeholder. */
  }
  else {
    vert_gen << "void attrib_load(" << domain_type_vert << " domain) {}\n"; /* Placeholder. */
    frag_gen << global_vars.str() << "void attrib_load(" << domain_type_frag << " domain)"
             << attr_load.str();
  }

  {
    const bool use_vertex_displacement = !codegen.displacement.empty() &&
                                         (displacement_type != MAT_DISPLACEMENT_BUMP) &&
                                         !ELEM(geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME);

    vert_gen << "float3 nodetree_displacement()\n";
    vert_gen << "{\n";
    vert_gen << ((use_vertex_displacement) ? codegen.displacement.serialized :
                                             "return float3(0);\n");
    vert_gen << "}\n\n";

    Vector<StringRefNull> dependencies = {};
    if (use_vertex_displacement) {
      dependencies.append("eevee_geom_types_lib.glsl");
      dependencies.append("eevee_nodetree_lib.glsl");
      dependencies.extend(codegen.displacement.dependencies);
    }

    info.generated_sources.append({"eevee_nodetree_vert_lib.glsl", dependencies, vert_gen.str()});
  }

  if (pipeline_type != MAT_PIPE_VOLUME_OCCUPANCY) {
    Vector<StringRefNull> dependencies;
    if (use_ao_node) {
      dependencies.append("eevee_ambient_occlusion_lib.glsl");
    }
    dependencies.append("eevee_geom_types_lib.glsl");
    dependencies.append("eevee_nodetree_lib.glsl");

    for (const auto &graph : codegen.material_functions) {
      frag_gen << graph.serialized;
      dependencies.extend(graph.dependencies);
    }

    if (!codegen.displacement.empty()) {
      /* Bump displacement. Needed to recompute normals after displacement. */
      info.define("MAT_DISPLACEMENT_BUMP");

      frag_gen << "float3 nodetree_displacement()\n";
      frag_gen << "{\n";
      frag_gen << codegen.displacement.serialized;
      dependencies.extend(codegen.displacement.dependencies);
      frag_gen << "}\n\n";
    }

    frag_gen << "Closure nodetree_surface(float closure_rand)\n";
    frag_gen << "{\n";
    frag_gen << "  closure_weights_reset(closure_rand);\n";
    frag_gen << codegen.surface.serialized_or_default("return Closure(0);\n");
    dependencies.extend(codegen.surface.dependencies);
    frag_gen << "}\n\n";

    /* TODO(fclem): Find a way to pass material parameters inside the material UBO. */
    info.define("thickness_mode", thickness_type == MAT_THICKNESS_SLAB ? "-1.0" : "1.0");

    frag_gen << "float nodetree_thickness()\n";
    frag_gen << "{\n";
    if (codegen.thickness.empty()) {
      /* Check presence of closure needing thickness to not add mandatory dependency on obinfos. */
      if (!GPU_material_flag_get(
              gpumat, GPU_MATFLAG_SUBSURFACE | GPU_MATFLAG_REFRACT | GPU_MATFLAG_TRANSLUCENT))
      {
        frag_gen << "return 0.0;\n";
      }
      else {
        if (info.additional_infos_.first_index_of_try("draw_object_infos") == -1) {
          info.additional_info("draw_object_infos");
        }
        /* TODO(fclem): Should use `to_scale` but the gpu_shader_math_matrix_lib.glsl isn't
         * included everywhere yet. */
        frag_gen << "float3 ob_scale;\n";
        frag_gen << "ob_scale.x = length(drw_modelmat()[0].xyz);\n";
        frag_gen << "ob_scale.y = length(drw_modelmat()[1].xyz);\n";
        frag_gen << "ob_scale.z = length(drw_modelmat()[2].xyz);\n";
        frag_gen << "float3 ls_dimensions = safe_rcp(abs(drw_object_infos().orco_mul.xyz));\n";
        frag_gen << "float3 ws_dimensions = ob_scale * ls_dimensions;\n";
        /* Choose the minimum axis so that cuboids are better represented. */
        frag_gen << "return reduce_min(ws_dimensions);\n";
      }
    }
    else {
      frag_gen << codegen.thickness.serialized;
      dependencies.extend(codegen.thickness.dependencies);
    }
    frag_gen << "}\n\n";

    frag_gen << "Closure nodetree_volume()\n";
    frag_gen << "{\n";
    frag_gen << "  closure_weights_reset(0.0);\n";
    frag_gen << codegen.volume.serialized_or_default("return Closure(0);\n");
    dependencies.extend(codegen.volume.dependencies);
    frag_gen << "}\n\n";

    info.generated_sources.append({"eevee_nodetree_frag_lib.glsl", dependencies, frag_gen.str()});
  }

  int reserved_attr_slots = 0;

  /* Geometry Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      info.additional_info("eevee_geom_world");
      break;
    case MAT_GEOM_CURVES:
      info.additional_info("eevee_geom_curves");
      break;
    case MAT_GEOM_MESH:
      info.additional_info("eevee_geom_mesh");
      reserved_attr_slots = 2; /* Number of vertex attributes inside eevee_geom_mesh. */
      break;
    case MAT_GEOM_POINTCLOUD:
      info.additional_info("eevee_geom_pointcloud");
      break;
    case MAT_GEOM_VOLUME:
      info.additional_info("eevee_geom_volume");
      reserved_attr_slots = 1; /* Number of vertex attributes inside eevee_geom_mesh. */
      break;
  }

  /* Make shaders that have as too many attributes fail compilation and have correct error
   * report instead of raising an error. */
  if (info.vertex_inputs_.size() > 0) {
    const int last_attr_index = info.vertex_inputs_.last().index;
    if (last_attr_index - reserved_attr_slots < 0) {
      const char *material_name = (info.name_.c_str() + 2);
      std::cerr << "Error: EEVEE: Material " << material_name << " uses too many attributes."
                << std::endl;
      /* Avoid assert in ShaderCreateInfo::finalize. */
      info.vertex_inputs_.clear();
    }
  }

  /* Pipeline Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      switch (pipeline_type) {
        case MAT_PIPE_VOLUME_MATERIAL:
          info.additional_info("eevee_surf_volume");
          break;
        default:
          info.additional_info("eevee_surf_world");
          break;
      }
      break;
    default:
      switch (pipeline_type) {
        case MAT_PIPE_PREPASS_FORWARD_VELOCITY:
        case MAT_PIPE_PREPASS_DEFERRED_VELOCITY:
          info.additional_info("eevee_surf_depth", "eevee_velocity_geom");
          break;
        case MAT_PIPE_PREPASS_OVERLAP:
        case MAT_PIPE_PREPASS_FORWARD:
        case MAT_PIPE_PREPASS_DEFERRED:
          info.additional_info("eevee_surf_depth");
          break;
        case MAT_PIPE_PREPASS_PLANAR:
          info.additional_info("eevee_surf_depth", "eevee_clip_plane");
          break;
        case MAT_PIPE_SHADOW:
          /* Determine surface shadow shader depending on used update technique. */
          switch (ShadowModule::shadow_technique) {
            case ShadowTechnique::ATOMIC_RASTER: {
              info.additional_info("eevee_surf_shadow_atomic");
            } break;
            case ShadowTechnique::TILE_COPY: {
              info.additional_info("eevee_surf_shadow_tbdr");
            } break;
            default: {
              BLI_assert_unreachable();
            } break;
          }
          break;
        case MAT_PIPE_VOLUME_OCCUPANCY:
          info.additional_info("eevee_surf_occupancy");
          break;
        case MAT_PIPE_VOLUME_MATERIAL:
          info.additional_info("eevee_surf_volume");
          break;
        case MAT_PIPE_CAPTURE:
          info.additional_info("eevee_surf_capture");
          break;
        case MAT_PIPE_DEFERRED:
          if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SHADER_TO_RGBA)) {
            info.additional_info("eevee_surf_deferred_hybrid");
          }
          else {
            info.additional_info("eevee_surf_deferred");
          }
          break;
        case MAT_PIPE_FORWARD:
          info.additional_info("eevee_surf_forward");
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
      break;
  }
}

struct CallbackThunk {
  ShaderModule *shader_module;
  ::Material *default_mat;
};

/* WATCH: This can be called from another thread! Needs to not touch the shader module in any
 * thread unsafe manner. */
static void codegen_callback(void *void_thunk, GPUMaterial *mat, GPUCodegenOutput *codegen)
{
  CallbackThunk *thunk = static_cast<CallbackThunk *>(void_thunk);
  thunk->shader_module->material_create_info_amend(mat, codegen);
}

static GPUPass *pass_replacement_cb(void *void_thunk, GPUMaterial *mat)
{
  using namespace blender::gpu::shader;

  CallbackThunk *thunk = static_cast<CallbackThunk *>(void_thunk);

  const ::Material *blender_mat = GPU_material_get_material(mat);

  uint64_t shader_uuid = GPU_material_uuid_get(mat);

  eMaterialPipeline pipeline_type;
  eMaterialGeometry geometry_type;
  eMaterialDisplacement displacement_type;
  eMaterialThickness thickness_type;
  bool transparent_shadows;
  material_type_from_shader_uuid(shader_uuid,
                                 pipeline_type,
                                 geometry_type,
                                 displacement_type,
                                 thickness_type,
                                 transparent_shadows);

  bool is_shadow_pass = pipeline_type == eMaterialPipeline::MAT_PIPE_SHADOW;
  bool is_prepass = ELEM(pipeline_type,
                         eMaterialPipeline::MAT_PIPE_PREPASS_DEFERRED,
                         eMaterialPipeline::MAT_PIPE_PREPASS_DEFERRED_VELOCITY,
                         eMaterialPipeline::MAT_PIPE_PREPASS_OVERLAP,
                         eMaterialPipeline::MAT_PIPE_PREPASS_FORWARD,
                         eMaterialPipeline::MAT_PIPE_PREPASS_FORWARD_VELOCITY,
                         eMaterialPipeline::MAT_PIPE_PREPASS_PLANAR);

  bool has_vertex_displacement = GPU_material_has_displacement_output(mat) &&
                                 displacement_type != eMaterialDisplacement::MAT_DISPLACEMENT_BUMP;
  bool has_transparency = GPU_material_flag_get(mat, GPU_MATFLAG_TRANSPARENT);
  bool has_shadow_transparency = has_transparency && transparent_shadows;
  bool has_raytraced_transmission = blender_mat && (blender_mat->blend_flag & MA_BL_SS_REFRACTION);

  bool can_use_default = (is_shadow_pass &&
                          (!has_vertex_displacement && !has_shadow_transparency)) ||
                         (is_prepass && (!has_vertex_displacement && !has_transparency &&
                                         !has_raytraced_transmission));
  if (can_use_default) {
    GPUMaterial *mat = thunk->shader_module->material_shader_get(thunk->default_mat,
                                                                 thunk->default_mat->nodetree,
                                                                 pipeline_type,
                                                                 geometry_type,
                                                                 false,
                                                                 nullptr);
    return GPU_material_get_pass(mat);
  }

  return nullptr;
}

static void store_node_tree_errors(GPUMaterialFromNodeTreeResult &material_from_tree)
{
  Depsgraph *depsgraph = DRW_context_get()->depsgraph;
  if (!depsgraph) {
    return;
  }
  if (!DEG_is_active(depsgraph)) {
    return;
  }
  for (const GPUMaterialFromNodeTreeResult::Error &error : material_from_tree.errors) {
    const bNodeTree &tree = error.node->owner_tree();
    if (const bNodeTree *tree_orig = DEG_get_original(&tree)) {
      std::lock_guard lock(tree_orig->runtime->shader_node_errors_mutex);
      tree_orig->runtime->shader_node_errors.lookup_or_add_default(error.node->identifier)
          .add(error.message);
    }
  }
}

GPUMaterial *ShaderModule::material_shader_get(::Material *blender_mat,
                                               bNodeTree *nodetree,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               bool deferred_compilation,
                                               ::Material *default_mat)
{
  eMaterialDisplacement displacement_type = to_displacement_type(blender_mat->displacement_method);
  eMaterialThickness thickness_type = to_thickness_type(blender_mat->thickness_mode);

  uint64_t shader_uuid = shader_uuid_from_material_type(
      pipeline_type, geometry_type, displacement_type, thickness_type, blender_mat->blend_flag);

  bool is_default_material = default_mat == nullptr;
  BLI_assert(blender_mat != default_mat);

  CallbackThunk thunk = {this, default_mat};

  GPUMaterialFromNodeTreeResult material_from_tree = GPU_material_from_nodetree(
      blender_mat,
      nodetree,
      &blender_mat->gpumaterial,
      blender_mat->id.name,
      GPU_MAT_EEVEE,
      shader_uuid,
      deferred_compilation,
      codegen_callback,
      &thunk,
      is_default_material ? nullptr : pass_replacement_cb);
  store_node_tree_errors(material_from_tree);
  return material_from_tree.material;
}

GPUMaterial *ShaderModule::world_shader_get(::World *blender_world,
                                            bNodeTree *nodetree,
                                            eMaterialPipeline pipeline_type,
                                            bool deferred_compilation)
{
  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, MAT_GEOM_WORLD);

  CallbackThunk thunk = {this, nullptr};

  GPUMaterialFromNodeTreeResult material_from_tree = GPU_material_from_nodetree(
      nullptr,
      nodetree,
      &blender_world->gpumaterial,
      blender_world->id.name,
      GPU_MAT_EEVEE,
      shader_uuid,
      deferred_compilation,
      codegen_callback,
      &thunk);
  store_node_tree_errors(material_from_tree);
  return material_from_tree.material;
}

/** \} */

}  // namespace blender::eevee
