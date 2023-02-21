/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"
#include "gpu_shader_interface.hh"

namespace blender::gpu {
class VKShaderInterface : public ShaderInterface {
 private:
  /**
   * Offset when searching for a shader input based on a binding number.
   *
   * When shaders combine images and samplers, the images have to be offset to find the correct
   * shader input. Both textures and images are stored in the uniform list and their ID can be
   * overlapping.
   */
  uint32_t image_offset_ = 0;

 public:
  VKShaderInterface() = default;

  void init(const shader::ShaderCreateInfo &info);
  /**
   * Retrieve the shader input for the given resource.
   *
   * nullptr is returned when resource could not be found.
   * Should only happen when still developing the Vulkan shader.
   */
  const ShaderInput *shader_input_get(const shader::ShaderCreateInfo::Resource &resource) const;
  const ShaderInput *shader_input_get(
      const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const;
};
}  // namespace blender::gpu
