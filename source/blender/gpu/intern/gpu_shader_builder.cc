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

#include "GPU_context.hh"
#include "GPU_init_exit.hh"
#include "gpu_shader_create_info_private.hh"

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "CLG_log.h"

namespace blender::gpu::shader_builder {

class ShaderBuilder {
 private:
  GHOST_SystemHandle ghost_system_;
  GHOST_ContextHandle ghost_context_ = nullptr;
  GPUContext *gpu_context_ = nullptr;

 public:
  void init_system();
  bool init_context();
  bool bake_create_infos(const char *name_starts_with_filter);
  void exit_context();
  void exit_system();
};

bool ShaderBuilder::bake_create_infos(const char *name_starts_with_filter)
{
  return gpu_shader_create_info_compile(name_starts_with_filter);
}

void ShaderBuilder::init_system()
{
  CLG_init();
  ghost_system_ = GHOST_CreateSystemBackground();
}

bool ShaderBuilder::init_context()
{
  BLI_assert(ghost_system_);
  BLI_assert(ghost_context_ == nullptr);
  BLI_assert(gpu_context_ == nullptr);

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

  ghost_context_ = GHOST_CreateGPUContext(ghost_system_, gpuSettings);
  if (ghost_context_ == nullptr) {
    GHOST_DisposeSystem(ghost_system_);
    return false;
  }

  GHOST_ActivateGPUContext(ghost_context_);

  gpu_context_ = GPU_context_create(nullptr, ghost_context_);
  GPU_init();
  return true;
}

void ShaderBuilder::exit_context()
{
  BLI_assert(ghost_context_);
  BLI_assert(gpu_context_);
  GPU_exit();
  GPU_context_discard(gpu_context_);
  GHOST_DisposeGPUContext(ghost_system_, ghost_context_);
  gpu_context_ = nullptr;
  ghost_context_ = nullptr;
}

void ShaderBuilder::exit_system()
{
  GHOST_DisposeSystem(ghost_system_);
  CLG_exit();
}

}  // namespace blender::gpu::shader_builder

/** \brief Entry point for the shader_builder. */
int main(int argc, const char *argv[])
{
  std::string gpu_backend_arg;
  std::string shader_name_starts_with_filter_arg;
  std::string result_file_arg;

  int arg = 1;
  while (arg < argc) {
    if (arg < argc - 2) {
      blender::StringRefNull argument = argv[arg];
      if (argument == "--gpu-backend") {
        gpu_backend_arg = std::string(argv[arg + 1]);
        arg += 2;
      }
      else if (argument == "--gpu-shader-filter") {
        shader_name_starts_with_filter_arg = std::string(argv[arg + 1]);
        arg += 2;
      }
      else {
        break;
      }
    }
    else if (arg == argc - 1) {
      result_file_arg = argv[arg];
      arg += 1;
    }
    else {
      break;
    }
  }

  if (result_file_arg.empty() || (!ELEM(gpu_backend_arg, "", "vulkan", "metal", "opengl"))) {
    std::cout << "Usage: " << argv[0];
    std::cout << " [--gpu-backend ";
#ifdef WITH_METAL_BACKEND
    std::cout << "metal";
#endif
#ifdef WITH_OPENGL_BACKEND
    std::cout << "opengl";
#endif
#ifdef WITH_VULKAN_BACKEND
    std::cout << ",vulkan";
#endif
    std::cout << "]";
    std::cout << " [--gpu-shader-filter <shader-name>]";
    std::cout << " <data_file_out>\n";
    exit(1);
  }

  int exit_code = 0;

  blender::gpu::shader_builder::ShaderBuilder builder;
  builder.init_system();

  struct NamedBackend {
    std::string name;
    eGPUBackendType backend;
  };

  blender::Vector<NamedBackend> backends_to_validate;
#ifdef WITH_OPENGL_BACKEND
  if (ELEM(gpu_backend_arg, "", "opengl")) {
    backends_to_validate.append({"OpenGL", GPU_BACKEND_OPENGL});
  }
#endif
#ifdef WITH_METAL_BACKEND
  if (ELEM(gpu_backend_arg, "", "metal")) {
    backends_to_validate.append({"Metal", GPU_BACKEND_METAL});
  }
#endif
#ifdef WITH_VULKAN_BACKEND
  if (ELEM(gpu_backend_arg, "", "vulkan")) {
    backends_to_validate.append({"Vulkan", GPU_BACKEND_VULKAN});
  }
#endif

  for (NamedBackend &backend : backends_to_validate) {
    GPU_backend_type_selection_set(backend.backend);
    if (!GPU_backend_supported()) {
      printf("%s isn't supported on this platform. Shader compilation is skipped\n",
             backend.name.c_str());
      continue;
    }
    if (builder.init_context()) {
      if (!builder.bake_create_infos(shader_name_starts_with_filter_arg.c_str())) {
        printf("Shader compilation failed for %s backend\n", backend.name.c_str());
        exit_code = 1;
      }
      else {
        printf("%s backend shader compilation succeeded.\n", backend.name.c_str());
      }
      builder.exit_context();
    }
    else {
      printf("Shader compilation skipped for %s backend. Context could not be created.\n",
             backend.name.c_str());
    }
  }

  builder.exit_system();

  exit(exit_code);
  return exit_code;
}
