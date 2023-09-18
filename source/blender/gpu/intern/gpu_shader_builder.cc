/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "BLI_vector.hh"

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

  GHOST_GPUSettings gpuSettings = {0};
  switch (GPU_backend_type_selection_get()) {
#ifdef WITH_OPENGL_BACKEND
    case GPU_BACKEND_OPENGL:
      gpuSettings.context_type = GHOST_kDrawingContextTypeOpenGL;
      break;
#endif

#ifdef WITH_METAL_BACKEND
    case GPU_BACKEND_METAL:
      gpuSettings.context_type = GHOST_kDrawingContextTypeMetal;
      break;
#endif

#ifdef WITH_VULKAN_BACKEND
    case GPU_BACKEND_VULKAN:
      gpuSettings.context_type = GHOST_kDrawingContextTypeVulkan;
      break;
#endif

    default:
      BLI_assert_unreachable();
      break;
  }

  ghost_system_ = GHOST_CreateSystemBackground();
  ghost_context_ = GHOST_CreateGPUContext(ghost_system_, gpuSettings);
  GHOST_ActivateGPUContext(ghost_context_);

  gpu_context_ = GPU_context_create(nullptr, ghost_context_);
  GPU_init();
}

void ShaderBuilder::exit()
{
  GPU_exit();

  GPU_context_discard(gpu_context_);

  GHOST_DisposeGPUContext(ghost_system_, ghost_context_);
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

  struct NamedBackend {
    std::string name;
    eGPUBackendType backend;
  };

  blender::Vector<NamedBackend> backends_to_validate;
  backends_to_validate.append({"OpenGL", GPU_BACKEND_OPENGL});
#ifdef WITH_METAL_BACKEND
  backends_to_validate.append({"Metal", GPU_BACKEND_METAL});
#endif
#ifdef WITH_VULKAN_BACKEND
  backends_to_validate.append({"Vulkan", GPU_BACKEND_VULKAN});
#endif
  for (NamedBackend &backend : backends_to_validate) {
    GPU_backend_type_selection_set(backend.backend);
    if (!GPU_backend_supported()) {
      printf("%s isn't supported on this platform. Shader compilation is skipped\n",
             backend.name.c_str());
      continue;
    }
    blender::gpu::shader_builder::ShaderBuilder builder;
    builder.init();
    if (!builder.bake_create_infos()) {
      printf("Shader compilation failed for %s backend\n", backend.name.c_str());
      exit_code = 1;
    }
    else {
      printf("%s backend shader compilation succeeded.\n", backend.name.c_str());
    }
    builder.exit();
  }

  exit(exit_code);
  return exit_code;
}
