/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DRW_render.hh"

#include "GPU_shader.hh"

namespace blender::image_engine {

/**
 * Shader module. Shared between instances.
 */
class ShaderModule {
 private:
  struct ShaderDeleter {
    void operator()(GPUShader *shader)
    {
      GPU_SHADER_FREE_SAFE(shader);
    }
  };
  using ShaderPtr = std::unique_ptr<GPUShader, ShaderDeleter>;

  /** Shared shader module across all engine instances. */
  static ShaderModule *g_shader_module;

 public:
  /** Shaders */
  ShaderPtr depth = shader("image_engine_depth_shader");
  ShaderPtr color = shader("image_engine_color_shader");

  /** Module */
  /** Only to be used by Instance constructor. */
  static ShaderModule &module_get();
  static void module_free();

 private:
  ShaderPtr shader(const char *create_info_name)
  {
    return ShaderPtr(GPU_shader_create_from_info_name(create_info_name));
  }
};

}  // namespace blender::image_engine
