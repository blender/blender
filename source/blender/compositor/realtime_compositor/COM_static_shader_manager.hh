/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "GPU_shader.h"

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------------------------------------
 *  Static Shader Manager
 *
 * A static shader manager is a map of shaders identified by their info name that can be acquired
 * and reused throughout the evaluation of the compositor and are only freed when the shader
 * manager is destroyed. Once a shader is acquired for the first time, it will be cached in the
 * manager to be potentially acquired later if needed without the shader creation overhead. */
class StaticShaderManager {
 private:
  /* The set of shaders identified by their info name that are currently available in the manager
   * to be acquired. */
  Map<StringRef, GPUShader *> shaders_;

 public:
  ~StaticShaderManager();

  /* Check if there is an available shader with the given info name in the manager, if such shader
   * exists, return it, otherwise, return a newly created shader and add it to the manager. */
  GPUShader *get(const char *info_name);
};

}  // namespace blender::realtime_compositor
