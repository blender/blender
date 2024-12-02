/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "image_shader.hh"

namespace blender::image_engine {

ShaderModule *ShaderModule::g_shader_module = nullptr;

ShaderModule &ShaderModule::module_get()
{
  if (g_shader_module == nullptr) {
    g_shader_module = new ShaderModule();
  }
  return *g_shader_module;
}

void ShaderModule::module_free()
{
  delete g_shader_module;
  g_shader_module = nullptr;
}

}  // namespace blender::image_engine
