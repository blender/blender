/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#include "overlay_private.hh"

namespace blender::draw::overlay {

StaticShader ShaderModule::shader_clippable(const char *create_info_name)
{
  std::string name = create_info_name;

  if (clipping_enabled_) {
    name += "_clipped";
  }

  return StaticShader(name);
}

StaticShader ShaderModule::shader_selectable(const char *create_info_name)
{
  std::string name = create_info_name;

  if (selection_type_ != SelectionType::DISABLED) {
    name += "_selectable";
  }

  if (clipping_enabled_) {
    name += "_clipped";
  }

  return StaticShader(name);
}

StaticShader ShaderModule::shader_selectable_no_clip(const char *create_info_name)
{
  std::string name = create_info_name;

  if (selection_type_ != SelectionType::DISABLED) {
    name += "_selectable";
  }

  return StaticShader(name);
}

using namespace blender::gpu::shader;

ShaderModule &ShaderModule::module_get(SelectionType selection_type, bool clipping_enabled)
{
  int selection_index = selection_type == SelectionType::DISABLED ? 0 : 1;
  return get_static_cache()[selection_index][clipping_enabled].get(selection_type,
                                                                   clipping_enabled);
}

void ShaderModule::module_free()
{
  for (int i : IndexRange(2)) {
    for (int j : IndexRange(2)) {
      get_static_cache()[i][j].release();
    }
  }
}

}  // namespace blender::draw::overlay
