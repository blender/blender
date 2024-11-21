/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_private.hh"

#include "vk_common.hh"

#include "shaderc/shaderc.hpp"

namespace blender::gpu {
class VKShader;

/**
 * Shader module.
 *
 * A shader module contains shader code and can be used as a vertex/geometry/fragment/compute stage
 * of a shader. The shader code in this sense doesn't need to be GLSL, but can also be SPIR-V
 * binary.
 *
 * For now this is just a data container so code can be reused when working with multiple shader
 * stages.
 *
 * Later we could load the SPIR-V binary directly from disk to skip front end compilation
 * phase completely or skip shader module at all when the cache is already aware of this shader by
 * using VK_EXT_shader_module_identifier.
 */
class VKShaderModule {
 public:
  /**
   * Single string containing GLSL source code.
   *
   * Is cleared after compilation phase has completed. (VKShader::finalize_post).
   */
  std::string combined_sources;

  /**
   * Hash of the combined sources. Used to generate the name inside spirv cache.
   */
  std::string sources_hash;

  /**
   * Vulkan handler of the shader module.
   */
  VkShaderModule vk_shader_module = VK_NULL_HANDLE;

  /**
   * Compilation result when compiling the shader module.
   *
   * Is cleared after compilation phase has completed. (VKShader::finalize_post).
   */
  shaderc::SpvCompilationResult compilation_result;
  Vector<uint32_t> spirv_binary;

  /**
   * Is compilation needed and is the compilation step done.
   *
   * Is set to false when GLSL sources are loaded and will be set to true again after the
   * compilation step. It will also be true when compilation has failed.
   */
  bool is_ready = true;

  ~VKShaderModule();

  /**
   * Finalize the shader module.
   *
   * When compilation succeeded the VkShaderModule will be created and stored in
   * `vk_shader_module`.
   */
  void finalize(StringRefNull name);

  /** Build the sources hash from the combined_sources. */
  void build_sources_hash();
};

}  // namespace blender::gpu
