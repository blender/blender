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

ShaderModule *ShaderModule::g_shader_module = nullptr;

ShaderModule *ShaderModule::module_get()
{
  if (g_shader_module == nullptr) {
    /* TODO(@fclem) thread-safety. */
    g_shader_module = new ShaderModule();
  }
  return g_shader_module;
}

void ShaderModule::module_free()
{
  if (g_shader_module != nullptr) {
    /* TODO(@fclem) thread-safety. */
    delete g_shader_module;
    g_shader_module = nullptr;
  }
}

ShaderModule::ShaderModule()
{
  for (GPUShader *&shader : shaders_) {
    shader = nullptr;
  }

  Vector<const GPUShaderCreateInfo *> infos;
  infos.reserve(MAX_SHADER_TYPE);

  for (auto i : IndexRange(MAX_SHADER_TYPE)) {
    const char *name = static_shader_create_info_name_get(eShaderType(i));
    const GPUShaderCreateInfo *create_info = GPU_shader_create_info_get(name);
    infos.append(create_info);

#ifndef NDEBUG
    if (name == nullptr) {
      std::cerr << "EEVEE: Missing case for eShaderType(" << i
                << ") in static_shader_create_info_name_get().";
      BLI_assert(0);
    }
    BLI_assert_msg(create_info != nullptr, "EEVEE: Missing create info for static shader.");
#endif
  }

  if (GPU_use_parallel_compilation()) {
    compilation_handle_ = GPU_shader_batch_create_from_infos(infos);
  }
}

ShaderModule::~ShaderModule()
{
  if (compilation_handle_) {
    /* Finish compilation to avoid asserts on exit at GLShaderCompiler destructor. */
    is_ready(true);
  }

  for (GPUShader *&shader : shaders_) {
    DRW_SHADER_FREE_SAFE(shader);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Static shaders
 *
 * \{ */

void ShaderModule::precompile_specializations(int render_buffers_shadow_id,
                                              int shadow_ray_count,
                                              int shadow_ray_step_count)
{
  BLI_assert(specialization_handle_ == 0);

  if (!GPU_use_parallel_compilation()) {
    return;
  }

  Vector<ShaderSpecialization> specializations;
  for (int i = 0; i < 3; i++) {
    GPUShader *sh = static_shader_get(eShaderType(DEFERRED_LIGHT_SINGLE + i));
    for (bool use_split_indirect : {false, true}) {
      for (bool use_lightprobe_eval : {false, true}) {
        for (bool use_transmission : {false, true}) {
          specializations.append({sh,
                                  {{"render_pass_shadow_id", render_buffers_shadow_id},
                                   {"use_split_indirect", use_split_indirect},
                                   {"use_lightprobe_eval", use_lightprobe_eval},
                                   {"use_transmission", use_transmission},
                                   {"shadow_ray_count", shadow_ray_count},
                                   {"shadow_ray_step_count", shadow_ray_step_count}}});
        }
      }
    }
  }

  specialization_handle_ = GPU_shader_batch_specializations(specializations);
}

bool ShaderModule::is_ready(bool block)
{
  if (compilation_handle_) {
    if (GPU_shader_batch_is_ready(compilation_handle_) || block) {
      Vector<GPUShader *> shaders = GPU_shader_batch_finalize(compilation_handle_);
      for (int i : IndexRange(MAX_SHADER_TYPE)) {
        shaders_[i] = shaders[i];
      }
    }
  }

  if (specialization_handle_) {
    while (!GPU_shader_batch_specializations_is_ready(specialization_handle_) && block) {
      /* Block until ready. */
    }
  }

  return compilation_handle_ == 0 && specialization_handle_ == 0;
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

GPUShader *ShaderModule::static_shader_get(eShaderType shader_type)
{
  BLI_assert(is_ready());
  if (shaders_[shader_type] == nullptr) {
    const char *shader_name = static_shader_create_info_name_get(shader_type);
    if (GPU_use_parallel_compilation()) {
      fprintf(stderr, "EEVEE: error: Could not compile static shader \"%s\"\n", shader_name);
      BLI_assert(0);
    }
    else {
      shaders_[shader_type] = GPU_shader_create_from_info_name(shader_name);
    }
  }
  return shaders_[shader_type];
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
    if (ELEM(geometry_type, MAT_GEOM_POINT_CLOUD, MAT_GEOM_CURVES)) {
      index_ = 1;
    }
    else if (geometry_type == MAT_GEOM_GPENCIL) {
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
    info.additional_info("draw_object_attribute_new");

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
    info.define("UNI_ATTR(a)", "vec4(0.0)");
  }

  SamplerSlots sampler_slots(
      pipeline_type, geometry_type, GPU_material_flag_get(gpumat, GPU_MATFLAG_SHADER_TO_RGBA));

  for (auto &resource : info.batch_resources_) {
    if (resource.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      resource.slot = sampler_slots.get();
    }
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_AO) &&
      ELEM(pipeline_type, MAT_PIPE_FORWARD, MAT_PIPE_DEFERRED) &&
      geometry_type_has_surface(geometry_type))
  {
    info.define("MAT_AMBIENT_OCCLUSION");
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

  int32_t closure_data_slots = 0;
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE)) {
    info.define("MAT_DIFFUSE");
    if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSLUCENT) &&
        !GPU_material_flag_get(gpumat, GPU_MATFLAG_COAT))
    {
      /* Special case to allow translucent with diffuse without noise.
       * Revert back to noise if clear coat is present. */
      closure_data_slots |= (1 << 2);
    }
    else {
      closure_data_slots |= (1 << 0);
    }
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SUBSURFACE)) {
    info.define("MAT_SUBSURFACE");
    closure_data_slots |= (1 << 0);
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT)) {
    info.define("MAT_REFRACTION");
    closure_data_slots |= (1 << 0);
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSLUCENT)) {
    info.define("MAT_TRANSLUCENT");
    closure_data_slots |= (1 << 0);
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY)) {
    info.define("MAT_REFLECTION");
    closure_data_slots |= (1 << 1);
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_COAT)) {
    info.define("MAT_CLEARCOAT");
    closure_data_slots |= (1 << 2);
  }

  int32_t closure_bin_count = count_bits_i(closure_data_slots);
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
        /* Support using one vec2 attribute. See #hair_get_barycentric(). */
        info.define("USE_BARYCENTRICS");
        break;
      default:
        /* No support */
        break;
    }
  }

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
          info.sampler(sampler_slots.get(), ImageType::FLOAT_3D, input.name, Frequency::BATCH);
        }
        info.vertex_inputs_.clear();
        /* Volume materials require these for loading the grid attributes from smoke sims. */
        info.additional_info("draw_volume_infos");
      }
      break;
    case MAT_GEOM_POINT_CLOUD:
    case MAT_GEOM_CURVES:
      /** Hair attributes come from sampler buffer. Transfer attributes to sampler. */
      for (auto &input : info.vertex_inputs_) {
        if (input.name == "orco") {
          /** NOTE: Orco is generated from strand position for now. */
          global_vars << input.type << " " << input.name << ";\n";
        }
        else {
          info.sampler(sampler_slots.get(), ImageType::FLOAT_BUFFER, input.name, Frequency::BATCH);
        }
      }
      info.vertex_inputs_.clear();
      break;
    case MAT_GEOM_WORLD:
      if (pipeline_type == MAT_PIPE_VOLUME_MATERIAL) {
        /* Even if world do not have grid attributes, we use dummy texture binds to pass correct
         * defaults. So we have to replace all attributes as samplers. */
        for (auto &input : info.vertex_inputs_) {
          info.sampler(sampler_slots.get(), ImageType::FLOAT_3D, input.name, Frequency::BATCH);
        }
        info.vertex_inputs_.clear();
      }
      /**
       * Only orco layer is supported by world and it is procedurally generated. These are here to
       * make the attribs_load function calls valid.
       */
      ATTR_FALLTHROUGH;
    case MAT_GEOM_GPENCIL:
      /**
       * Only one uv and one color attribute layer are supported by gpencil objects and they are
       * already declared in another createInfo. These are here to make the attribs_load
       * function calls valid.
       */
      for (auto &input : info.vertex_inputs_) {
        global_vars << input.type << " " << input.name << ";\n";
      }
      info.vertex_inputs_.clear();
      break;
    case MAT_GEOM_VOLUME:
      /** Volume grid attributes come from 3D textures. Transfer attributes to samplers. */
      for (auto &input : info.vertex_inputs_) {
        info.sampler(sampler_slots.get(), ImageType::FLOAT_3D, input.name, Frequency::BATCH);
      }
      info.vertex_inputs_.clear();
      break;
  }

  const bool do_vertex_attrib_load = !ELEM(geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME) &&
                                     (pipeline_type != MAT_PIPE_VOLUME_MATERIAL);

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

  std::stringstream attr_load;
  attr_load << "void attrib_load()\n";
  attr_load << "{\n";
  attr_load << (!codegen.attr_load.empty() ? codegen.attr_load : "");
  attr_load << "}\n\n";

  std::stringstream vert_gen, frag_gen, comp_gen;

  if (do_vertex_attrib_load) {
    vert_gen << global_vars.str() << attr_load.str();
    frag_gen << "void attrib_load() {}\n"; /* Placeholder. */
  }
  else {
    vert_gen << "void attrib_load() {}\n"; /* Placeholder. */
    frag_gen << global_vars.str() << attr_load.str();
  }

  {
    const bool use_vertex_displacement = !codegen.displacement.empty() &&
                                         (displacement_type != MAT_DISPLACEMENT_BUMP) &&
                                         !ELEM(geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME);

    vert_gen << "vec3 nodetree_displacement()\n";
    vert_gen << "{\n";
    vert_gen << ((use_vertex_displacement) ? codegen.displacement : "return vec3(0);\n");
    vert_gen << "}\n\n";

    info.vertex_source_generated = vert_gen.str();
  }

  if (pipeline_type != MAT_PIPE_VOLUME_OCCUPANCY) {
    frag_gen << (!codegen.material_functions.empty() ? codegen.material_functions : "\n");

    if (!codegen.displacement.empty()) {
      /* Bump displacement. Needed to recompute normals after displacement. */
      info.define("MAT_DISPLACEMENT_BUMP");

      frag_gen << "vec3 nodetree_displacement()\n";
      frag_gen << "{\n";
      frag_gen << codegen.displacement;
      frag_gen << "}\n\n";
    }

    frag_gen << "Closure nodetree_surface(float closure_rand)\n";
    frag_gen << "{\n";
    frag_gen << "  closure_weights_reset(closure_rand);\n";
    frag_gen << (!codegen.surface.empty() ? codegen.surface : "return Closure(0);\n");
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
        if (info.additional_infos_.first_index_of_try("draw_object_infos_new") == -1) {
          info.additional_info("draw_object_infos_new");
        }
        /* TODO(fclem): Should use `to_scale` but the gpu_shader_math_matrix_lib.glsl isn't
         * included everywhere yet. */
        frag_gen << "vec3 ob_scale;\n";
        frag_gen << "ob_scale.x = length(ModelMatrix[0].xyz);\n";
        frag_gen << "ob_scale.y = length(ModelMatrix[1].xyz);\n";
        frag_gen << "ob_scale.z = length(ModelMatrix[2].xyz);\n";
        frag_gen << "vec3 ls_dimensions = safe_rcp(abs(OrcoTexCoFactors[1].xyz));\n";
        frag_gen << "vec3 ws_dimensions = ob_scale * ls_dimensions;\n";
        /* Choose the minimum axis so that cuboids are better represented. */
        frag_gen << "return reduce_min(ws_dimensions);\n";
      }
    }
    else {
      frag_gen << codegen.thickness;
    }
    frag_gen << "}\n\n";

    frag_gen << "Closure nodetree_volume()\n";
    frag_gen << "{\n";
    frag_gen << "  closure_weights_reset(0.0);\n";
    frag_gen << (!codegen.volume.empty() ? codegen.volume : "return Closure(0);\n");
    frag_gen << "}\n\n";

    info.fragment_source_generated = frag_gen.str();
  }

  /* Geometry Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      info.additional_info("eevee_geom_world");
      break;
    case MAT_GEOM_GPENCIL:
      info.additional_info("eevee_geom_gpencil");
      break;
    case MAT_GEOM_CURVES:
      info.additional_info("eevee_geom_curves");
      break;
    case MAT_GEOM_MESH:
      info.additional_info("eevee_geom_mesh");
      break;
    case MAT_GEOM_POINT_CLOUD:
      info.additional_info("eevee_geom_point_cloud");
      break;
    case MAT_GEOM_VOLUME:
      info.additional_info("eevee_geom_volume");
      break;
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

/* WATCH: This can be called from another thread! Needs to not touch the shader module in any
 * thread unsafe manner. */
static void codegen_callback(void *thunk, GPUMaterial *mat, GPUCodegenOutput *codegen)
{
  reinterpret_cast<ShaderModule *>(thunk)->material_create_info_amend(mat, codegen);
}

static GPUPass *pass_replacement_cb(void *thunk, GPUMaterial *mat)
{
  using namespace blender::gpu::shader;

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
    GPUMaterial *mat = reinterpret_cast<ShaderModule *>(thunk)->material_default_shader_get(
        pipeline_type, geometry_type);
    return GPU_material_get_pass(mat);
  }

  return nullptr;
}

GPUMaterial *ShaderModule::material_default_shader_get(eMaterialPipeline pipeline_type,
                                                       eMaterialGeometry geometry_type)
{
  bool is_volume = ELEM(pipeline_type, MAT_PIPE_VOLUME_MATERIAL, MAT_PIPE_VOLUME_OCCUPANCY);
  ::Material *blender_mat = (is_volume) ? BKE_material_default_volume() :
                                          BKE_material_default_surface();

  return material_shader_get(
      blender_mat, blender_mat->nodetree, pipeline_type, geometry_type, false);
}

GPUMaterial *ShaderModule::material_shader_get(::Material *blender_mat,
                                               bNodeTree *nodetree,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               bool deferred_compilation)
{
  bool is_volume = ELEM(pipeline_type, MAT_PIPE_VOLUME_MATERIAL, MAT_PIPE_VOLUME_OCCUPANCY);

  eMaterialDisplacement displacement_type = to_displacement_type(blender_mat->displacement_method);
  eMaterialThickness thickness_type = to_thickness_type(blender_mat->thickness_mode);

  uint64_t shader_uuid = shader_uuid_from_material_type(
      pipeline_type, geometry_type, displacement_type, thickness_type, blender_mat->blend_flag);

  bool is_default_material = ELEM(
      blender_mat, BKE_material_default_surface(), BKE_material_default_volume());

  GPUMaterial *mat = DRW_shader_from_material(blender_mat,
                                              nodetree,
                                              GPU_MAT_EEVEE,
                                              shader_uuid,
                                              is_volume,
                                              deferred_compilation,
                                              codegen_callback,
                                              this,
                                              is_default_material ? nullptr : pass_replacement_cb);

  return mat;
}

GPUMaterial *ShaderModule::world_shader_get(::World *blender_world,
                                            bNodeTree *nodetree,
                                            eMaterialPipeline pipeline_type)
{
  bool is_volume = (pipeline_type == MAT_PIPE_VOLUME_MATERIAL);
  bool defer_compilation = is_volume;

  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, MAT_GEOM_WORLD);

  return DRW_shader_from_world(blender_world,
                               nodetree,
                               GPU_MAT_EEVEE,
                               shader_uuid,
                               is_volume,
                               defer_compilation,
                               codegen_callback,
                               this);
}

GPUMaterial *ShaderModule::material_shader_get(const char *name,
                                               ListBase &materials,
                                               bNodeTree *nodetree,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type)
{
  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, geometry_type);

  bool is_volume = ELEM(pipeline_type, MAT_PIPE_VOLUME_MATERIAL, MAT_PIPE_VOLUME_OCCUPANCY);

  GPUMaterial *gpumat = GPU_material_from_nodetree(nullptr,
                                                   nullptr,
                                                   nodetree,
                                                   &materials,
                                                   name,
                                                   GPU_MAT_EEVEE,
                                                   shader_uuid,
                                                   is_volume,
                                                   false,
                                                   codegen_callback,
                                                   this);
  GPU_material_status_set(gpumat, GPU_MAT_CREATED);
  GPU_material_compile(gpumat);
  /* Queue deferred material optimization. */
  DRW_shader_queue_optimize_material(gpumat);
  return gpumat;
}

/** \} */

}  // namespace blender::eevee
