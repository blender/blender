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

#ifndef NDEBUG
  /* Ensure all shader are described. */
  for (auto i : IndexRange(MAX_SHADER_TYPE)) {
    const char *name = static_shader_create_info_name_get(eShaderType(i));
    if (name == nullptr) {
      std::cerr << "EEVEE: Missing case for eShaderType(" << i
                << ") in static_shader_create_info_name_get().";
      BLI_assert(0);
    }
    const GPUShaderCreateInfo *create_info = GPU_shader_create_info_get(name);
    BLI_assert_msg(create_info != nullptr, "EEVEE: Missing create info for static shader.");
  }
#endif
}

ShaderModule::~ShaderModule()
{
  for (GPUShader *&shader : shaders_) {
    DRW_SHADER_FREE_SAFE(shader);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Static shaders
 *
 * \{ */

const char *ShaderModule::static_shader_create_info_name_get(eShaderType shader_type)
{
  switch (shader_type) {
    case AMBIENT_OCCLUSION_PASS:
      return "eevee_ambient_occlusion_pass";
    case FILM_FRAG:
      return "eevee_film_frag";
    case FILM_COMP:
      return "eevee_film_comp";
    case FILM_CRYPTOMATTE_POST:
      return "eevee_film_cryptomatte_post";
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
    case DEFERRED_TILE_CLASSIFY:
      return "eevee_deferred_tile_classify";
    case DEFERRED_TILE_COMPACT:
      return "eevee_deferred_tile_compact";
    case DEFERRED_TILE_STENCIL:
      return "eevee_deferred_tile_stencil";
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
    case DISPLAY_PROBE_GRID:
      return "eevee_display_probe_grid";
    case DISPLAY_PROBE_REFLECTION:
      return "eevee_display_probe_reflection";
    case DISPLAY_PROBE_PLANAR:
      return "eevee_display_probe_planar";
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
    case LIGHTPROBE_IRRADIANCE_BOUNDS:
      return "eevee_lightprobe_irradiance_bounds";
    case LIGHTPROBE_IRRADIANCE_OFFSET:
      return "eevee_lightprobe_irradiance_offset";
    case LIGHTPROBE_IRRADIANCE_RAY:
      return "eevee_lightprobe_irradiance_ray";
    case LIGHTPROBE_IRRADIANCE_LOAD:
      return "eevee_lightprobe_irradiance_load";
    case LIGHTPROBE_IRRADIANCE_WORLD:
      return "eevee_lightprobe_irradiance_world";
    case SPHERE_PROBE_CONVOLVE:
      return "eevee_reflection_probe_convolve";
    case SPHERE_PROBE_REMAP:
      return "eevee_reflection_probe_remap";
    case SPHERE_PROBE_IRRADIANCE:
      return "eevee_reflection_probe_irradiance";
    case SPHERE_PROBE_SELECT:
      return "eevee_reflection_probe_select";
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
  if (shaders_[shader_type] == nullptr) {
    const char *shader_name = static_shader_create_info_name_get(shader_type);

    shaders_[shader_type] = GPU_shader_create_from_info_name(shader_name);

    if (shaders_[shader_type] == nullptr) {
      fprintf(stderr, "EEVEE: error: Could not compile static shader \"%s\"\n", shader_name);
    }
    BLI_assert(shaders_[shader_type] != nullptr);
  }
  return shaders_[shader_type];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Materials
 *
 * \{ */

void ShaderModule::material_create_info_ammend(GPUMaterial *gpumat, GPUCodegenOutput *codegen_)
{
  using namespace blender::gpu::shader;

  uint64_t shader_uuid = GPU_material_uuid_get(gpumat);

  eMaterialPipeline pipeline_type;
  eMaterialGeometry geometry_type;
  eMaterialDisplacement displacement_type;
  bool transparent_shadows;
  material_type_from_shader_uuid(
      shader_uuid, pipeline_type, geometry_type, displacement_type, transparent_shadows);

  GPUCodegenOutput &codegen = *codegen_;
  ShaderCreateInfo &info = *reinterpret_cast<ShaderCreateInfo *>(codegen.create_info);

  /* WORKAROUND: Replace by new ob info. */
  int64_t ob_info_index = info.additional_infos_.first_index_of_try("draw_object_infos");
  if (ob_info_index != -1) {
    info.additional_infos_[ob_info_index] = "draw_object_infos_new";
  }

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

  /* First indices are reserved by the engine.
   * Put material samplers in reverse order, starting from the last slot. */
  int sampler_slot = GPU_max_textures_frag() - 1;
  for (auto &resource : info.batch_resources_) {
    if (resource.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      resource.slot = sampler_slot--;
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
          info.sampler(sampler_slot--, ImageType::FLOAT_3D, input.name, Frequency::BATCH);
        }
        info.vertex_inputs_.clear();
        /* Volume materials require these for loading the grid attributes from smoke sims. */
        info.additional_info("draw_volume_infos");
        if (ob_info_index == -1) {
          info.additional_info("draw_object_infos_new");
        }
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
          info.sampler(sampler_slot--, ImageType::FLOAT_BUFFER, input.name, Frequency::BATCH);
        }
      }
      info.vertex_inputs_.clear();
      break;
    case MAT_GEOM_WORLD:
      if (pipeline_type == MAT_PIPE_VOLUME_MATERIAL) {
        /* Even if world do not have grid attributes, we use dummy texture binds to pass correct
         * defaults. So we have to replace all attributes as samplers. */
        for (auto &input : info.vertex_inputs_) {
          info.sampler(sampler_slot--, ImageType::FLOAT_3D, input.name, Frequency::BATCH);
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
        info.sampler(sampler_slot--, ImageType::FLOAT_3D, input.name, Frequency::BATCH);
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
  attr_load << ((!codegen.attr_load.empty()) ? codegen.attr_load : "");
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
    const bool use_vertex_displacement = (!codegen.displacement.empty()) &&
                                         (displacement_type != MAT_DISPLACEMENT_BUMP) &&
                                         (!ELEM(geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME));

    vert_gen << "vec3 nodetree_displacement()\n";
    vert_gen << "{\n";
    vert_gen << ((use_vertex_displacement) ? codegen.displacement : "return vec3(0);\n");
    vert_gen << "}\n\n";

    info.vertex_source_generated = vert_gen.str();
  }

  if (pipeline_type != MAT_PIPE_VOLUME_OCCUPANCY) {
    frag_gen << ((!codegen.material_functions.empty()) ? codegen.material_functions : "\n");

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
    frag_gen << ((!codegen.surface.empty()) ? codegen.surface : "return Closure(0);\n");
    frag_gen << "}\n\n";

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
        frag_gen << "vec3 ls_dimensions = safe_rcp(abs(OrcoTexCoFactors[1].xyz));\n";
        frag_gen << "vec3 ws_dimensions = (ModelMatrix * vec4(ls_dimensions, 1.0)).xyz;\n";
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
    frag_gen << ((!codegen.volume.empty()) ? codegen.volume : "return Closure(0);\n");
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
  reinterpret_cast<ShaderModule *>(thunk)->material_create_info_ammend(mat, codegen);
}

GPUMaterial *ShaderModule::material_shader_get(::Material *blender_mat,
                                               bNodeTree *nodetree,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               bool deferred_compilation)
{
  bool is_volume = ELEM(pipeline_type, MAT_PIPE_VOLUME_MATERIAL, MAT_PIPE_VOLUME_OCCUPANCY);

  eMaterialDisplacement displacement_type = to_displacement_type(blender_mat->displacement_method);

  uint64_t shader_uuid = shader_uuid_from_material_type(
      pipeline_type, geometry_type, displacement_type, blender_mat->blend_flag);

  return DRW_shader_from_material(blender_mat,
                                  nodetree,
                                  GPU_MAT_EEVEE,
                                  shader_uuid,
                                  is_volume,
                                  deferred_compilation,
                                  codegen_callback,
                                  this);
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

/* Variation to compile a material only with a nodetree. Caller needs to maintain the list of
 * materials and call GPU_material_free on it to update the material. */
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
