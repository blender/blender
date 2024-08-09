/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

ShaderModule *ShaderModule::g_shader_modules[2][2] = {{nullptr}};

ShaderModule::ShaderPtr ShaderModule::shader(
    const char *create_info_name,
    const FunctionRef<void(gpu::shader::ShaderCreateInfo &info)> patch)
{
  const gpu::shader::ShaderCreateInfo *info_ptr =
      reinterpret_cast<const gpu::shader::ShaderCreateInfo *>(
          GPU_shader_create_info_get(create_info_name));
  BLI_assert(info_ptr != nullptr);

  /* Perform a copy for patching. */
  gpu::shader::ShaderCreateInfo info = *info_ptr;

  patch(info);

  return ShaderPtr(
      GPU_shader_create_from_info(reinterpret_cast<const GPUShaderCreateInfo *>(&info)));
}

ShaderModule::ShaderPtr ShaderModule::selectable_shader(const char *create_info_name)
{
  /* TODO: This is what it should be like with all variations defined with create infos. */
  // std::string create_info_name = base_create_info;
  // create_info_name += SelectEngineT::shader_suffix;
  // create_info_name += clipping_enabled_ ? "_clipped" : "";
  // this->shader_ = GPU_shader_create_from_info_name(create_info_name.c_str());
  UNUSED_VARS(clipping_enabled_);

  /* WORKAROUND: ... but for now, we have to patch the create info used by the old engine. */
  gpu::shader::ShaderCreateInfo info = *reinterpret_cast<const gpu::shader::ShaderCreateInfo *>(
      GPU_shader_create_info_get(create_info_name));

  info.define("OVERLAY_NEXT");

  if (selection_type_ != SelectionType::DISABLED) {
    info.define("SELECT_ENABLE");
  }

  return ShaderPtr(
      GPU_shader_create_from_info(reinterpret_cast<const GPUShaderCreateInfo *>(&info)));
}

ShaderModule::ShaderPtr ShaderModule::selectable_shader(
    const char *create_info_name,
    const FunctionRef<void(gpu::shader::ShaderCreateInfo &info)> patch)
{
  gpu::shader::ShaderCreateInfo info = *reinterpret_cast<const gpu::shader::ShaderCreateInfo *>(
      GPU_shader_create_info_get(create_info_name));

  patch(info);

  info.define("OVERLAY_NEXT");

  if (selection_type_ != SelectionType::DISABLED) {
    info.define("SELECT_ENABLE");
    /* Replace additional info. */
    for (StringRefNull &str : info.additional_infos_) {
      if (str == "draw_modelmat_new") {
        str = "draw_modelmat_new_with_custom_id";
      }
    }
    info.additional_info("select_id_patch");
  }

  return ShaderPtr(
      GPU_shader_create_from_info(reinterpret_cast<const GPUShaderCreateInfo *>(&info)));
}

using namespace blender::gpu::shader;

static void shader_patch_common(gpu::shader::ShaderCreateInfo &info)
{
  info.additional_infos_.clear();
  info.additional_info(
      "draw_view", "draw_modelmat_new", "draw_resource_handle_new", "draw_globals");
}

static void shader_patch_edit_mesh_normal_common(gpu::shader::ShaderCreateInfo &info)
{
  shader_patch_common(info);
  info.defines_.clear(); /* Removes WORKAROUND_INDEX_LOAD_INCLUDE. */
  info.vertex_inputs_.clear();
  info.additional_info("gpu_index_load");
  info.storage_buf(1, Qualifier::READ, "float", "pos[]", Frequency::GEOMETRY);
}

ShaderModule::ShaderModule(const SelectionType selection_type, const bool clipping_enabled)
    : selection_type_(selection_type), clipping_enabled_(clipping_enabled)
{
  /** Shaders */

  mesh_analysis = shader("overlay_edit_mesh_analysis",
                         [](gpu::shader::ShaderCreateInfo &info) { shader_patch_common(info); });

  mesh_edit_face = shader("overlay_edit_mesh_face", [](gpu::shader::ShaderCreateInfo &info) {
    shader_patch_common(info);
    info.additional_info("overlay_edit_mesh_common");
  });
  mesh_edit_vert = shader("overlay_edit_mesh_vert", [](gpu::shader::ShaderCreateInfo &info) {
    shader_patch_common(info);
    info.additional_info("overlay_edit_mesh_common");
  });

  mesh_edit_depth = shader("overlay_edit_mesh_depth",
                           [](gpu::shader::ShaderCreateInfo &info) { shader_patch_common(info); });

  mesh_edit_skin_root = shader(
      "overlay_edit_mesh_skin_root", [](gpu::shader::ShaderCreateInfo &info) {
        shader_patch_common(info);
        /* TODO(fclem): Use correct vertex format. For now we read the format manually. */
        info.storage_buf(0, Qualifier::READ, "float", "size[]", Frequency::GEOMETRY);
        info.vertex_inputs_.clear();
        info.define("VERTEX_PULL");
      });

  mesh_face_normal = shader("overlay_edit_mesh_normal", [](gpu::shader::ShaderCreateInfo &info) {
    shader_patch_edit_mesh_normal_common(info);
    info.define("FACE_NORMAL");
    info.push_constant(gpu::shader::Type::BOOL, "hq_normals");
    info.storage_buf(0, Qualifier::READ, "uint", "norAndFlag[]", Frequency::GEOMETRY);
  });

  mesh_face_normal_subdiv = shader(
      "overlay_edit_mesh_normal", [](gpu::shader::ShaderCreateInfo &info) {
        shader_patch_edit_mesh_normal_common(info);
        info.define("FACE_NORMAL");
        info.define("FLOAT_NORMAL");
        info.storage_buf(0, Qualifier::READ, "vec4", "norAndFlag[]", Frequency::GEOMETRY);
      });

  mesh_loop_normal = shader("overlay_edit_mesh_normal", [](gpu::shader::ShaderCreateInfo &info) {
    shader_patch_edit_mesh_normal_common(info);
    info.define("LOOP_NORMAL");
    info.push_constant(gpu::shader::Type::BOOL, "hq_normals");
    info.storage_buf(0, Qualifier::READ, "uint", "lnor[]", Frequency::GEOMETRY);
  });

  mesh_loop_normal_subdiv = shader(
      "overlay_edit_mesh_normal", [](gpu::shader::ShaderCreateInfo &info) {
        shader_patch_edit_mesh_normal_common(info);
        info.define("LOOP_NORMAL");
        info.define("FLOAT_NORMAL");
        info.storage_buf(0, Qualifier::READ, "vec4", "lnor[]", Frequency::GEOMETRY);
      });

  mesh_vert_normal = shader("overlay_edit_mesh_normal", [](gpu::shader::ShaderCreateInfo &info) {
    shader_patch_edit_mesh_normal_common(info);
    info.define("VERT_NORMAL");
    info.storage_buf(0, Qualifier::READ, "uint", "vnor[]", Frequency::GEOMETRY);
  });

  /** Selectable Shaders */

  armature_sphere_outline = selectable_shader(
      "overlay_armature_sphere_outline", [](gpu::shader::ShaderCreateInfo &info) {
        info.storage_buf(0, Qualifier::READ, "mat4", "data_buf[]");
        info.define("inst_obmat", "data_buf[gl_InstanceID]");
        info.vertex_inputs_.pop_last();
      });

  depth_mesh = selectable_shader("overlay_depth_only", [](gpu::shader::ShaderCreateInfo &info) {
    info.additional_infos_.clear();
    info.additional_info("draw_view", "draw_modelmat_new", "draw_resource_handle_new");
  });

  extra_shape = selectable_shader("overlay_extra", [](gpu::shader::ShaderCreateInfo &info) {
    info.storage_buf(0, Qualifier::READ, "ExtraInstanceData", "data_buf[]");
    info.define("color", "data_buf[gl_InstanceID].color_");
    info.define("inst_obmat", "data_buf[gl_InstanceID].object_to_world_");
    info.vertex_inputs_.pop_last();
    info.vertex_inputs_.pop_last();
  });

  extra_wire = selectable_shader("overlay_extra_wire", [](gpu::shader::ShaderCreateInfo &info) {
    info.typedef_source("overlay_shader_shared.h");
    info.storage_buf(0, Qualifier::READ, "VertexData", "data_buf[]");
    info.push_constant(gpu::shader::Type::INT, "colorid");
    info.define("pos", "data_buf[gl_VertexID].pos_.xyz");
    info.define("color", "data_buf[gl_VertexID].color_");
    info.additional_infos_.clear();
    info.additional_info(
        "draw_view", "draw_modelmat_new", "draw_resource_handle_new", "draw_globals");
    info.vertex_inputs_.pop_last();
    info.vertex_inputs_.pop_last();
    info.vertex_inputs_.pop_last();
  });

  extra_wire_object = selectable_shader(
      "overlay_extra_wire", [](gpu::shader::ShaderCreateInfo &info) {
        info.define("OBJECT_WIRE");
        info.additional_infos_.clear();
        info.additional_info(
            "draw_view", "draw_modelmat_new", "draw_resource_handle_new", "draw_globals");
      });

  extra_loose_points = selectable_shader(
      "overlay_extra_loose_point", [](gpu::shader::ShaderCreateInfo &info) {
        info.typedef_source("overlay_shader_shared.h");
        info.storage_buf(0, Qualifier::READ, "VertexData", "data_buf[]");
        info.define("pos", "data_buf[gl_VertexID].pos_.xyz");
        info.define("vertex_color", "data_buf[gl_VertexID].color_");
        info.vertex_inputs_.pop_last();
        info.vertex_inputs_.pop_last();
        info.additional_infos_.clear();
        info.additional_info("draw_view", "draw_modelmat_new", "draw_globals");
      });

  lattice_points = selectable_shader(
      "overlay_edit_lattice_point", [](gpu::shader::ShaderCreateInfo &info) {
        info.additional_infos_.clear();
        info.additional_info(
            "draw_view", "draw_modelmat_new", "draw_resource_handle_new", "draw_globals");
      });

  lattice_wire = selectable_shader(
      "overlay_edit_lattice_wire", [](gpu::shader::ShaderCreateInfo &info) {
        info.additional_infos_.clear();
        info.additional_info(
            "draw_view", "draw_modelmat_new", "draw_resource_handle_new", "draw_globals");
      });

  extra_grid = selectable_shader("overlay_extra_grid", [](gpu::shader::ShaderCreateInfo &info) {
    info.additional_infos_.clear();
    info.additional_info(
        "draw_view", "draw_modelmat_new", "draw_resource_handle_new", "draw_globals");
  });

  extra_ground_line = selectable_shader(
      "overlay_extra_groundline", [](gpu::shader::ShaderCreateInfo &info) {
        info.storage_buf(0, Qualifier::READ, "vec4", "data_buf[]");
        info.define("inst_pos", "data_buf[gl_InstanceID].xyz");
        info.vertex_inputs_.pop_last();
      });
}

ShaderModule &ShaderModule::module_get(SelectionType selection_type, bool clipping_enabled)
{
  int selection_index = selection_type == SelectionType::DISABLED ? 0 : 1;
  ShaderModule *&g_shader_module = g_shader_modules[selection_index][clipping_enabled];
  if (g_shader_module == nullptr) {
    /* TODO(@fclem) thread-safety. */
    g_shader_module = new ShaderModule(selection_type, clipping_enabled);
  }
  return *g_shader_module;
}

void ShaderModule::module_free()
{
  for (int i : IndexRange(2)) {
    for (int j : IndexRange(2)) {
      if (g_shader_modules[i][j] != nullptr) {
        /* TODO(@fclem) thread-safety. */
        delete g_shader_modules[i][j];
        g_shader_modules[i][j] = nullptr;
      }
    }
  }
}

}  // namespace blender::draw::overlay
