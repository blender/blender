/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_private.hh"

#include "BLI_map.hh"
#include "BLI_task.h"
#include "BLI_vector.hh"

#include "shaderc/shaderc.hpp"

#include <mutex>

namespace blender::gpu {
class VKShader;
class VKShaderModule;

/**
 * Vulkan shader compiler.
 *
 * Is used for both single threaded compilation by calling `VKShaderCompiler::compile_module` or
 * batch based compilation.
 */
class VKShaderCompiler {
 public:
  static bool compile_module(VKShader &shader,
                             shaderc_shader_kind stage,
                             VKShaderModule &shader_module);

  static void cache_dir_clear_old();
};
}  // namespace blender::gpu
