/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Compile time automation of shader compilation and validation.
 */

#include <iostream>

#include "GHOST_C-api.h"

#include "GPU_context.h"
#include "GPU_init_exit.h"
#include "gpu_shader_create_info_private.hh"

#include "CLG_log.h"

namespace blender::gpu::shader_builder {

class ShaderBuilder {
 private:
  GHOST_SystemHandle ghost_system_;
  GHOST_ContextHandle ghost_context_;
  GPUContext *gpu_context_ = nullptr;

 public:
  void init();
  bool bake_create_infos();
  void exit();
};

bool ShaderBuilder::bake_create_infos()
{
  return gpu_shader_create_info_compile_all();
}

void ShaderBuilder::init()
{
  CLG_init();

  GHOST_GLSettings glSettings = {0};
  ghost_system_ = GHOST_CreateSystem();
  ghost_context_ = GHOST_CreateOpenGLContext(ghost_system_, glSettings);
  GHOST_ActivateOpenGLContext(ghost_context_);

  gpu_context_ = GPU_context_create(nullptr);
  GPU_init();
}

void ShaderBuilder::exit()
{
  GPU_exit();

  GPU_context_discard(gpu_context_);

  GHOST_DisposeOpenGLContext(ghost_system_, ghost_context_);
  GHOST_DisposeSystem(ghost_system_);

  CLG_exit();
}

}  // namespace blender::gpu::shader_builder

/** \brief Entry point for the shader_builder. */
int main(int argc, const char *argv[])
{
  if (argc < 2) {
    printf("Usage: %s <data_file_to>\n", argv[0]);
    exit(1);
  }

  int exit_code = 0;

  blender::gpu::shader_builder::ShaderBuilder builder;
  builder.init();
  if (!builder.bake_create_infos()) {
    exit_code = 1;
  }
  builder.exit();
  exit(exit_code);

  return exit_code;
}
