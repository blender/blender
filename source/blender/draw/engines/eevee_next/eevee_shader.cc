/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Shader module that manage shader libraries, deferred compilation,
 * and static shader usage.
 */

#include "GPU_capabilities.h"

#include "gpu_shader_create_info.hh"

#include "eevee_shader.hh"

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

#ifdef DEBUG
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
    case DEFERRED_LIGHT:
      return "eevee_deferred_light";
    case DEFERRED_CAPTURE_EVAL:
      return "eevee_deferred_capture_eval";
    case HIZ_DEBUG:
      return "eevee_hiz_debug";
    case HIZ_UPDATE:
      return "eevee_hiz_update";
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
    case DISPLAY_PROBE_GRID:
      return "eevee_display_probe_grid";
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
    case RAY_DENOISE_SPATIAL_REFLECT:
      return "eevee_ray_denoise_spatial_reflect";
    case RAY_DENOISE_SPATIAL_REFRACT:
      return "eevee_ray_denoise_spatial_refract";
    case RAY_DENOISE_TEMPORAL:
      return "eevee_ray_denoise_temporal";
    case RAY_DENOISE_BILATERAL_REFLECT:
      return "eevee_ray_denoise_bilateral_reflect";
    case RAY_DENOISE_BILATERAL_REFRACT:
      return "eevee_ray_denoise_bilateral_refract";
    case RAY_GENERATE_REFLECT:
      return "eevee_ray_generate_reflect";
    case RAY_GENERATE_REFRACT:
      return "eevee_ray_generate_refract";
    case RAY_TRACE_FALLBACK:
      return "eevee_ray_trace_fallback";
    case RAY_TRACE_SCREEN_REFLECT:
      return "eevee_ray_trace_screen_reflect";
    case RAY_TRACE_SCREEN_REFRACT:
      return "eevee_ray_trace_screen_refract";
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
    case REFLECTION_PROBE_REMAP:
      return "eevee_reflection_probe_remap";
    case REFLECTION_PROBE_UPDATE_IRRADIANCE:
      return "eevee_reflection_probe_update_irradiance";
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
    case SHADOW_TILEMAP_TAG_USAGE_VOLUME:
      return "eevee_shadow_tag_usage_volume";
    case SUBSURFACE_EVAL:
      return "eevee_subsurface_eval";
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
    case VOLUME_INTEGRATION:
      return "eevee_volume_integration";
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
  material_type_from_shader_uuid(shader_uuid, pipeline_type, geometry_type);

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

  /* WORKAROUND: Needed because node_tree isn't present in test shaders. */
  if (pipeline_type == MAT_PIPE_DEFERRED) {
    info.additional_info("eevee_render_pass_out");
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_AO) &&
      ELEM(pipeline_type, MAT_PIPE_FORWARD, MAT_PIPE_DEFERRED) &&
      ELEM(geometry_type, MAT_GEOM_MESH, MAT_GEOM_CURVES))
  {
    info.define("MAT_AMBIENT_OCCLUSION");
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) {
    info.define("MAT_TRANSPARENT");
    /* Transparent material do not have any velocity specific pipeline. */
    if (pipeline_type == MAT_PIPE_FORWARD_PREPASS_VELOCITY) {
      pipeline_type = MAT_PIPE_FORWARD_PREPASS;
    }
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT) == false &&
      pipeline_type == MAT_PIPE_FORWARD)
  {
    /* Opaque forward do support AOVs and render pass if not using transparency. */
    info.additional_info("eevee_render_pass_out");
    info.additional_info("eevee_cryptomatte_out");
  }

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SUBSURFACE) && pipeline_type == MAT_PIPE_FORWARD) {
    info.define("SSS_TRANSMITTANCE");
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
      /** Noop. */
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
    case MAT_GEOM_VOLUME_OBJECT:
    case MAT_GEOM_VOLUME_WORLD:
      /** Volume grid attributes come from 3D textures. Transfer attributes to samplers. */
      for (auto &input : info.vertex_inputs_) {
        info.sampler(sampler_slot--, ImageType::FLOAT_3D, input.name, Frequency::BATCH);
      }
      info.vertex_inputs_.clear();
      break;
  }

  const bool do_vertex_attrib_load = !ELEM(
      geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME_WORLD, MAT_GEOM_VOLUME_OBJECT);

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
  attr_load << ((codegen.attr_load) ? codegen.attr_load : "");
  attr_load << "}\n\n";

  std::stringstream vert_gen, frag_gen, comp_gen;

  bool is_compute = pipeline_type == MAT_PIPE_VOLUME;

  if (do_vertex_attrib_load) {
    vert_gen << global_vars.str() << attr_load.str();
  }
  else if (!is_compute) {
    frag_gen << global_vars.str() << attr_load.str();
  }
  else {
    comp_gen << global_vars.str() << attr_load.str();
  }

  if (!is_compute) {
    if (!ELEM(geometry_type, MAT_GEOM_WORLD, MAT_GEOM_VOLUME_WORLD, MAT_GEOM_VOLUME_OBJECT)) {
      vert_gen << "vec3 nodetree_displacement()\n";
      vert_gen << "{\n";
      vert_gen << ((codegen.displacement) ? codegen.displacement : "return vec3(0);\n");
      vert_gen << "}\n\n";
    }

    info.vertex_source_generated = vert_gen.str();
  }

  if (!is_compute) {
    frag_gen << ((codegen.material_functions) ? codegen.material_functions : "\n");

    if (codegen.displacement) {
      /* Bump displacement. Needed to recompute normals after displacement. */
      info.define("MAT_DISPLACEMENT_BUMP");

      frag_gen << "vec3 nodetree_displacement()\n";
      frag_gen << "{\n";
      frag_gen << codegen.displacement;
      frag_gen << "}\n\n";
    }

    frag_gen << "Closure nodetree_surface()\n";
    frag_gen << "{\n";
    frag_gen << "  closure_weights_reset();\n";
    frag_gen << ((codegen.surface) ? codegen.surface : "return Closure(0);\n");
    frag_gen << "}\n\n";

    frag_gen << "float nodetree_thickness()\n";
    frag_gen << "{\n";
    /* TODO(fclem): Better default. */
    frag_gen << ((codegen.thickness) ? codegen.thickness : "return 0.1;\n");
    frag_gen << "}\n\n";

    info.fragment_source_generated = frag_gen.str();
  }

  if (is_compute) {
    comp_gen << ((codegen.material_functions) ? codegen.material_functions : "\n");

    comp_gen << "Closure nodetree_volume()\n";
    comp_gen << "{\n";
    comp_gen << "  closure_weights_reset();\n";
    comp_gen << ((codegen.volume) ? codegen.volume : "return Closure(0);\n");
    comp_gen << "}\n\n";

    info.compute_source_generated = comp_gen.str();
  }

  /* Geometry Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      info.additional_info("eevee_geom_world");
      break;
    case MAT_GEOM_VOLUME_WORLD:
      info.additional_info("eevee_volume_world");
      break;
    case MAT_GEOM_VOLUME_OBJECT:
      info.additional_info("eevee_volume_object");
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
  }
  /* Pipeline Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      info.additional_info("eevee_surf_world");
      break;
    case MAT_GEOM_VOLUME_OBJECT:
    case MAT_GEOM_VOLUME_WORLD:
      break;
    default:
      switch (pipeline_type) {
        case MAT_PIPE_FORWARD_PREPASS_VELOCITY:
        case MAT_PIPE_DEFERRED_PREPASS_VELOCITY:
          info.additional_info("eevee_surf_depth", "eevee_velocity_geom");
          break;
        case MAT_PIPE_FORWARD_PREPASS:
        case MAT_PIPE_DEFERRED_PREPASS:
          info.additional_info("eevee_surf_depth");
          break;
        case MAT_PIPE_SHADOW:
          info.additional_info("eevee_surf_shadow");
          break;
        case MAT_PIPE_CAPTURE:
          info.additional_info("eevee_surf_capture");
          break;
        case MAT_PIPE_DEFERRED:
          info.additional_info("eevee_surf_deferred");
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
  bool is_volume = (pipeline_type == MAT_PIPE_VOLUME);

  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, geometry_type);

  return DRW_shader_from_material(
      blender_mat, nodetree, shader_uuid, is_volume, deferred_compilation, codegen_callback, this);
}

GPUMaterial *ShaderModule::world_shader_get(::World *blender_world,
                                            bNodeTree *nodetree,
                                            eMaterialPipeline pipeline_type)
{
  bool is_volume = (pipeline_type == MAT_PIPE_VOLUME);
  bool defer_compilation = is_volume;

  eMaterialGeometry geometry_type = is_volume ? MAT_GEOM_VOLUME_WORLD : MAT_GEOM_WORLD;

  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, geometry_type);

  return DRW_shader_from_world(
      blender_world, nodetree, shader_uuid, is_volume, defer_compilation, codegen_callback, this);
}

/* Variation to compile a material only with a nodetree. Caller needs to maintain the list of
 * materials and call GPU_material_free on it to update the material. */
GPUMaterial *ShaderModule::material_shader_get(const char *name,
                                               ListBase &materials,
                                               bNodeTree *nodetree,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               bool is_lookdev)
{
  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, geometry_type);

  bool is_volume = (pipeline_type == MAT_PIPE_VOLUME);

  GPUMaterial *gpumat = GPU_material_from_nodetree(nullptr,
                                                   nullptr,
                                                   nodetree,
                                                   &materials,
                                                   name,
                                                   shader_uuid,
                                                   is_volume,
                                                   is_lookdev,
                                                   codegen_callback,
                                                   this);
  GPU_material_status_set(gpumat, GPU_MAT_QUEUED);
  GPU_material_compile(gpumat);
  /* Queue deferred material optimization. */
  DRW_shader_queue_optimize_material(gpumat);
  return gpumat;
}

/** \} */

}  // namespace blender::eevee
