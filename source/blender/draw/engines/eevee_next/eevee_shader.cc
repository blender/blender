/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Shader module that manage shader libraries, deferred compilation,
 * and static shader usage.
 */

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
    case VELOCITY_RESOLVE:
      return "eevee_velocity_resolve";
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

  info.auto_resource_location(true);

  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) {
    info.define("MAT_TRANSPARENT");
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
    case MAT_GEOM_CURVES:
      /** Hair attributes come from sampler buffer. Transfer attributes to sampler. */
      for (auto &input : info.vertex_inputs_) {
        if (input.name == "orco") {
          /** NOTE: Orco is generated from strand position for now. */
          global_vars << input.type << " " << input.name << ";\n";
        }
        else {
          info.sampler(0, ImageType::FLOAT_BUFFER, input.name, Frequency::BATCH);
        }
      }
      info.vertex_inputs_.clear();
      info.additional_info("draw_curves_infos");
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
    case MAT_GEOM_VOLUME:
      /** No attributes supported. */
      info.vertex_inputs_.clear();
      break;
  }

  const bool do_fragment_attrib_load = (geometry_type == MAT_GEOM_WORLD);

  if (do_fragment_attrib_load && !info.vertex_out_interfaces_.is_empty()) {
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

  std::stringstream vert_gen, frag_gen;

  if (do_fragment_attrib_load) {
    frag_gen << global_vars.str() << attr_load.str();
  }
  else {
    vert_gen << global_vars.str() << attr_load.str();
  }

  {
    /* Only mesh and curves support vertex displacement for now. */
    if (ELEM(geometry_type, MAT_GEOM_MESH, MAT_GEOM_CURVES, MAT_GEOM_GPENCIL)) {
      vert_gen << "vec3 nodetree_displacement()\n";
      vert_gen << "{\n";
      vert_gen << ((codegen.displacement) ? codegen.displacement : "return vec3(0);\n");
      vert_gen << "}\n\n";
    }

    info.vertex_source_generated = vert_gen.str();
  }

  {
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

    frag_gen << "Closure nodetree_volume()\n";
    frag_gen << "{\n";
    frag_gen << "  closure_weights_reset();\n";
    frag_gen << ((codegen.volume) ? codegen.volume : "return Closure(0);\n");
    frag_gen << "}\n\n";

    frag_gen << "float nodetree_thickness()\n";
    frag_gen << "{\n";
    /* TODO(fclem): Better default. */
    frag_gen << ((codegen.thickness) ? codegen.thickness : "return 0.1;\n");
    frag_gen << "}\n\n";

    info.fragment_source_generated = frag_gen.str();
  }

  /* Geometry Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      info.additional_info("eevee_geom_world");
      break;
    case MAT_GEOM_VOLUME:
      info.additional_info("eevee_geom_volume");
      break;
    case MAT_GEOM_GPENCIL:
      info.additional_info("eevee_geom_gpencil");
      break;
    case MAT_GEOM_CURVES:
      info.additional_info("eevee_geom_curves");
      break;
    case MAT_GEOM_MESH:
    default:
      info.additional_info("eevee_geom_mesh");
      break;
  }

  /* Pipeline Info. */
  switch (geometry_type) {
    case MAT_GEOM_WORLD:
      info.additional_info("eevee_surf_world");
      break;
    case MAT_GEOM_VOLUME:
      break;
    default:
      switch (pipeline_type) {
        case MAT_PIPE_FORWARD_PREPASS_VELOCITY:
        case MAT_PIPE_DEFERRED_PREPASS_VELOCITY:
          info.additional_info("eevee_surf_depth", "eevee_velocity_geom");
          break;
        case MAT_PIPE_FORWARD_PREPASS:
        case MAT_PIPE_DEFERRED_PREPASS:
        case MAT_PIPE_SHADOW:
          info.additional_info("eevee_surf_depth");
          break;
        case MAT_PIPE_DEFERRED:
          info.additional_info("eevee_surf_deferred");
          break;
        case MAT_PIPE_FORWARD:
          info.additional_info("eevee_surf_forward");
          break;
        default:
          BLI_assert(0);
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
                                               struct bNodeTree *nodetree,
                                               eMaterialPipeline pipeline_type,
                                               eMaterialGeometry geometry_type,
                                               bool deferred_compilation)
{
  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, geometry_type);

  bool is_volume = (pipeline_type == MAT_PIPE_VOLUME);

  return DRW_shader_from_material(
      blender_mat, nodetree, shader_uuid, is_volume, deferred_compilation, codegen_callback, this);
}

GPUMaterial *ShaderModule::world_shader_get(::World *blender_world, struct bNodeTree *nodetree)
{
  eMaterialPipeline pipeline_type = MAT_PIPE_DEFERRED; /* Unused. */
  eMaterialGeometry geometry_type = MAT_GEOM_WORLD;

  uint64_t shader_uuid = shader_uuid_from_material_type(pipeline_type, geometry_type);

  bool is_volume = (pipeline_type == MAT_PIPE_VOLUME);
  bool deferred_compilation = false;

  return DRW_shader_from_world(blender_world,
                               nodetree,
                               shader_uuid,
                               is_volume,
                               deferred_compilation,
                               codegen_callback,
                               this);
}

/* Variation to compile a material only with a nodetree. Caller needs to maintain the list of
 * materials and call GPU_material_free on it to update the material. */
GPUMaterial *ShaderModule::material_shader_get(const char *name,
                                               ListBase &materials,
                                               struct bNodeTree *nodetree,
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
  return gpumat;
}

/** \} */

}  // namespace blender::eevee
