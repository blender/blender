/* SPDX-FileCopyrightText: 2023 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "gpencil_shader.hh"

namespace blender::draw::greasepencil {

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
      std::cerr << "GPencil: Missing case for eShaderType(" << i
                << ") in static_shader_create_info_name_get()." << std::endl;
      BLI_assert(0);
    }
    const GPUShaderCreateInfo *create_info = GPU_shader_create_info_get(name);
    BLI_assert_msg(create_info != nullptr, "GPencil: Missing create info for static shader.");
  }
#endif
}

ShaderModule::~ShaderModule()
{
  for (GPUShader *&shader : shaders_) {
    DRW_SHADER_FREE_SAFE(shader);
  }
}

const char *ShaderModule::static_shader_create_info_name_get(eShaderType shader_type)
{
  switch (shader_type) {
    case ANTIALIASING_EDGE_DETECT:
      return "gpencil_antialiasing_stage_0";
    case ANTIALIASING_BLEND_WEIGHT:
      return "gpencil_antialiasing_stage_1";
    case ANTIALIASING_RESOLVE:
      return "gpencil_antialiasing_stage_2";
    case GREASE_PENCIL:
      return "gpencil_geometry_next";
    case LAYER_BLEND:
      return "gpencil_layer_blend";
    case DEPTH_MERGE:
      return "gpencil_depth_merge";
    case MASK_INVERT:
      return "gpencil_mask_invert";
    case FX_COMPOSITE:
      return "gpencil_fx_composite";
    case FX_COLORIZE:
      return "gpencil_fx_colorize";
    case FX_BLUR:
      return "gpencil_fx_blur";
    case FX_GLOW:
      return "gpencil_fx_glow";
    case FX_PIXEL:
      return "gpencil_fx_pixelize";
    case FX_RIM:
      return "gpencil_fx_rim";
    case FX_SHADOW:
      return "gpencil_fx_shadow";
    case FX_TRANSFORM:
      return "gpencil_fx_transform";
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
      std::cerr << "GPencil: error: Could not compile static shader \"" << shader_name << "\""
                << std::endl;
    }
    BLI_assert(shaders_[shader_type] != nullptr);
  }
  return shaders_[shader_type];
}

}  // namespace blender::draw::greasepencil
