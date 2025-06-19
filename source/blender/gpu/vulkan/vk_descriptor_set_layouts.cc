/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_descriptor_set_layouts.hh"
#include "vk_backend.hh"

namespace blender::gpu {
VKDescriptorSetLayouts::VKDescriptorSetLayouts()
{
  vk_descriptor_set_layout_create_info_ = {};
  vk_descriptor_set_layout_create_info_.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
}

VKDescriptorSetLayouts::~VKDescriptorSetLayouts()
{
  deinit();
}

VkDescriptorSetLayout VKDescriptorSetLayouts::get_or_create(const VKDescriptorSetLayoutInfo &info,
                                                            bool &r_created,
                                                            bool &r_needed)
{
  r_created = false;
  r_needed = !info.bindings.is_empty();
  if (r_needed == false) {
    return VK_NULL_HANDLE;
  }

  std::scoped_lock mutex(mutex_);

  VkDescriptorSetLayout *layout = vk_descriptor_set_layouts_.lookup_ptr(info);
  if (layout) {
    return *layout;
  }

  update_layout_bindings(info);
  const VKDevice &device = VKBackend::get().device;
  const bool use_descriptor_buffer = device.extensions_get().descriptor_buffer;

  vk_descriptor_set_layout_create_info_.bindingCount = vk_descriptor_set_layout_bindings_.size();
  vk_descriptor_set_layout_create_info_.pBindings = vk_descriptor_set_layout_bindings_.data();
  if (use_descriptor_buffer) {
    vk_descriptor_set_layout_create_info_.flags =
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
  }

  VkDescriptorSetLayout vk_descriptor_set_layout = VK_NULL_HANDLE;
  vkCreateDescriptorSetLayout(device.vk_handle(),
                              &vk_descriptor_set_layout_create_info_,
                              nullptr,
                              &vk_descriptor_set_layout);
  BLI_assert(vk_descriptor_set_layout != VK_NULL_HANDLE);

  vk_descriptor_set_layout_create_info_.bindingCount = 0;
  vk_descriptor_set_layout_create_info_.pBindings = nullptr;
  vk_descriptor_set_layout_bindings_.clear();
  vk_descriptor_set_layouts_.add(info, vk_descriptor_set_layout);
  r_created = true;

  if (use_descriptor_buffer) {
    descriptor_buffer_layouts_.add(
        vk_descriptor_set_layout,
        create_descriptor_buffer_layout(device, info, vk_descriptor_set_layout));
  }

  return vk_descriptor_set_layout;
}

void VKDescriptorSetLayouts::update_layout_bindings(const VKDescriptorSetLayoutInfo &info)
{
  BLI_assert(vk_descriptor_set_layout_bindings_.is_empty());
  vk_descriptor_set_layout_bindings_.reserve(info.bindings.size());

  uint32_t index = 0;
  for (const VkDescriptorType &vk_descriptor_type : info.bindings) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = index++;
    binding.descriptorCount = 1;
    binding.descriptorType = vk_descriptor_type;
    binding.pImmutableSamplers = VK_NULL_HANDLE;
    binding.stageFlags = vk_descriptor_type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ?
                             VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT) :
                             info.vk_shader_stage_flags;
    vk_descriptor_set_layout_bindings_.append(binding);
  }
}

void VKDescriptorSetLayouts::deinit()
{
  std::scoped_lock mutex(mutex_);
  const VKDevice &device = VKBackend::get().device;
  for (VkDescriptorSetLayout &vk_descriptor_set_layout : vk_descriptor_set_layouts_.values()) {
    vkDestroyDescriptorSetLayout(device.vk_handle(), vk_descriptor_set_layout, nullptr);
  }
  vk_descriptor_set_layouts_.clear();
}

static void align_size(VkDeviceSize &r_size, VkDeviceSize alignment)
{
  if (alignment > 1) {
    r_size = (r_size + alignment - 1) & ~(alignment - 1);
  }
}
VKDescriptorBufferLayout VKDescriptorSetLayouts::create_descriptor_buffer_layout(
    const VKDevice &device,
    const VKDescriptorSetLayoutInfo &info,
    VkDescriptorSetLayout vk_descriptor_set_layout) const
{
  const VkPhysicalDeviceDescriptorBufferPropertiesEXT &properties =
      device.physical_device_descriptor_buffer_properties_get();
  VKDescriptorBufferLayout result = {};

  device.functions.vkGetDescriptorSetLayoutSize(
      device.vk_handle(), vk_descriptor_set_layout, &result.size);
  align_size(result.size, properties.descriptorBufferOffsetAlignment);

  result.binding_offsets.resize(info.bindings.size());
  for (uint32_t binding : IndexRange(info.bindings.size())) {
    device.functions.vkGetDescriptorSetLayoutBindingOffset(
        device.vk_handle(), vk_descriptor_set_layout, binding, &result.binding_offsets[binding]);
  }

  return result;
}

}  // namespace blender::gpu
