/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_shader.h"

#include "COM_static_shader_manager.hh"

namespace blender::realtime_compositor {

StaticShaderManager::~StaticShaderManager()
{
  for (GPUShader *shader : shaders_.values()) {
    GPU_shader_free(shader);
  }
}

GPUShader *StaticShaderManager::get(const char *info_name)
{
  /* If a shader with the same info name already exists in the manager, return it, otherwise,
   * create a new shader from the info name and return it. */
  return shaders_.lookup_or_add_cb(
      info_name, [info_name]() { return GPU_shader_create_from_info_name(info_name); });
}

}  // namespace blender::realtime_compositor
