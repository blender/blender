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

/**
 * Bind types to bind resources to a shader.
 *
 * Keep in sync with #gpu::shader::ShaderCreateInfo::Resource::BindType.
 * We add the term `INPUT_ATTACHMENT` as it is stored as a sub-pass
 * input in the shader create info.
 *
 * TODO: Investigate if `TEXEL_BUFFER` can be added as well.
 */
enum VKBindType {
  UNIFORM_BUFFER = 0,
  STORAGE_BUFFER,
  SAMPLER,
  IMAGE,
  INPUT_ATTACHMENT,
};

struct VKResourceBinding {
  VKBindType bind_type = VKBindType::UNIFORM_BUFFER;
  int binding = -1;

  VKDescriptorSet::Location location;
  VKImageViewArrayed arrayed = VKImageViewArrayed::DONT_CARE;
  VkAccessFlags access_mask = VK_ACCESS_NONE;
};

class VKShaderInterface : public ShaderInterface {
 private:
  /** Binding information for each shader input. */
  Array<VKResourceBinding> resource_bindings_;
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
    return flag_is_set(shader_builtins_, shader::BuiltinBits::POINT_SIZE);
  }

  const Span<VKResourceBinding> resource_bindings_get() const
  {
    return resource_bindings_;
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
  const VKResourceBinding &resource_binding_info(const ShaderInput *shader_input) const;

  void descriptor_set_location_update(
      const ShaderInput *shader_input,
      const VKDescriptorSet::Location location,
      const VKBindType bind_type,
      std::optional<const shader::ShaderCreateInfo::Resource> resource,
      VKImageViewArrayed arrayed);
};

}  // namespace blender::gpu
