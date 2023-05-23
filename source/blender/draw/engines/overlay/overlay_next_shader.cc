/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

ShaderModule *ShaderModule::g_shader_modules[2][2] = {{nullptr}};

ShaderModule::ShaderPtr ShaderModule::selectable_shader(const char *create_info_name)
{
  /* TODO: This is what it should be like with all variations defined with create infos. */
  // std::string create_info_name = base_create_info;
  // create_info_name += SelectEngineT::shader_suffix;
  // create_info_name += ClippingEnabled ? "_clipped" : "";
  // this->shader_ = GPU_shader_create_from_info_name(create_info_name.c_str());

  /* WORKAROUND: ... but for now, we have to patch the create info used by the old engine. */
  gpu::shader::ShaderCreateInfo info = *reinterpret_cast<const gpu::shader::ShaderCreateInfo *>(
      GPU_shader_create_info_get(create_info_name));

  if (selection_type_ != SelectionType::DISABLED) {
    info.define("SELECT_ENABLE");
  }

  return ShaderPtr(
      GPU_shader_create_from_info(reinterpret_cast<const GPUShaderCreateInfo *>(&info)));
}

ShaderModule::ShaderPtr ShaderModule::selectable_shader(
    const char *create_info_name, std::function<void(gpu::shader::ShaderCreateInfo &info)> patch)
{
  gpu::shader::ShaderCreateInfo info = *reinterpret_cast<const gpu::shader::ShaderCreateInfo *>(
      GPU_shader_create_info_get(create_info_name));

  patch(info);

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

ShaderModule::ShaderModule(const SelectionType selection_type, const bool clipping_enabled)
    : selection_type_(selection_type), clipping_enabled_(clipping_enabled)
{
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
