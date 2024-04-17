/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_array.hh"

#include "gpu_shader_create_info.hh"
#include "gpu_shader_interface.hh"

#include "BLI_array.hh"

#include "vk_descriptor_set_layouts.hh"
#include "vk_push_constants.hh"

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
  Array<VKDescriptorSet::Location> descriptor_set_locations_;
  Array<shader::ShaderCreateInfo::Resource::BindType> descriptor_set_bind_types_;
  VKDescriptorSetLayoutInfo descriptor_set_layout_info_;

  VKPushConstants::Layout push_constants_layout_;

  shader::BuiltinBits shader_builtins_;

 public:
  VKShaderInterface() = default;

  void init(const shader::ShaderCreateInfo &info);

  const VKDescriptorSet::Location descriptor_set_location(
      const shader::ShaderCreateInfo::Resource &resource) const;
  const std::optional<VKDescriptorSet::Location> descriptor_set_location(
      const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const;

  /** Get the Layout of the shader. */
  const VKPushConstants::Layout &push_constants_layout_get() const
  {
    return push_constants_layout_;
  }

  const VKDescriptorSetLayoutInfo &descriptor_set_layout_info_get() const
  {
    return descriptor_set_layout_info_;
  }

  shader::Type get_attribute_type(int location) const
  {
    return static_cast<shader::Type>(attr_types_[location]);
  }

  bool is_point_shader() const
  {
    return (shader_builtins_ & shader::BuiltinBits::POINT_SIZE) == shader::BuiltinBits::POINT_SIZE;
  }

 private:
  void init_descriptor_set_layout_info(const shader::ShaderCreateInfo &info,
                                       int64_t resources_len,
                                       Span<shader::ShaderCreateInfo::Resource> resources,
                                       VKPushConstants::StorageType push_constants_storage);
  /**
   * Retrieve the shader input for the given resource.
   *
   * nullptr is returned when resource could not be found.
   * Should only happen when still developing the Vulkan shader.
   */
  const ShaderInput *shader_input_get(const shader::ShaderCreateInfo::Resource &resource) const;
  const ShaderInput *shader_input_get(
      const shader::ShaderCreateInfo::Resource::BindType &bind_type, int binding) const;
  const VKDescriptorSet::Location descriptor_set_location(const ShaderInput *shader_input) const;
  const shader::ShaderCreateInfo::Resource::BindType descriptor_set_bind_type(
      const ShaderInput *shader_input) const;
  void descriptor_set_location_update(
      const ShaderInput *shader_input,
      const VKDescriptorSet::Location location,
      const shader::ShaderCreateInfo::Resource::BindType bind_type);
};

}  // namespace blender::gpu
