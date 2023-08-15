/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_descriptor_set.hh"
#include "vk_index_buffer.hh"
#include "vk_sampler.hh"
#include "vk_shader.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_assert.h"

namespace blender::gpu {

VKDescriptorSet::VKDescriptorSet(VKDescriptorSet &&other)
    : vk_descriptor_pool_(other.vk_descriptor_pool_), vk_descriptor_set_(other.vk_descriptor_set_)
{
  other.mark_freed();
}

VKDescriptorSet::~VKDescriptorSet()
{
  if (vk_descriptor_set_ != VK_NULL_HANDLE) {
    /* Handle should be given back to the pool. */
    VKDevice &device = VKBackend::get().device_;
    device.descriptor_pools_get().free(*this);
    BLI_assert(vk_descriptor_set_ == VK_NULL_HANDLE);
  }
}

void VKDescriptorSet::mark_freed()
{
  vk_descriptor_set_ = VK_NULL_HANDLE;
  vk_descriptor_pool_ = VK_NULL_HANDLE;
}

void VKDescriptorSetTracker::bind(VKStorageBuffer &buffer,
                                  const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_in_bytes();
}

void VKDescriptorSetTracker::bind_as_ssbo(VKVertexBuffer &buffer,
                                          const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_used_get();
}

void VKDescriptorSetTracker::bind(VKUniformBuffer &buffer,
                                  const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_in_bytes();
}

void VKDescriptorSetTracker::bind_as_ssbo(VKIndexBuffer &buffer,
                                          const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_get();
}

void VKDescriptorSetTracker::bind_as_ssbo(VKUniformBuffer &buffer,
                                          const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  binding.vk_buffer = buffer.vk_handle();
  binding.buffer_size = buffer.size_in_bytes();
}

void VKDescriptorSetTracker::image_bind(VKTexture &texture,
                                        const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  binding.texture = &texture;
}

void VKDescriptorSetTracker::bind(VKTexture &texture,
                                  const VKDescriptorSet::Location location,
                                  const VKSampler &sampler)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.texture = &texture;
  binding.vk_sampler = sampler.vk_handle();
}

void VKDescriptorSetTracker::bind(VKVertexBuffer &vertex_buffer,
                                  const VKDescriptorSet::Location location)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
  binding.vk_buffer_view = vertex_buffer.vk_buffer_view_get();
  binding.buffer_size = vertex_buffer.size_alloc_get();
}

VKDescriptorSetTracker::Binding &VKDescriptorSetTracker::ensure_location(
    const VKDescriptorSet::Location location)
{
  for (Binding &binding : bindings_) {
    if (binding.location == location) {
      return binding;
    }
  }

  Binding binding = {};
  binding.location = location;
  bindings_.append(binding);
  return bindings_.last();
}

void VKDescriptorSetTracker::update(VKContext &context)
{
  BLI_assert(layout_ != VK_NULL_HANDLE);
  tracked_resource_for(context, !bindings_.is_empty());
  std::unique_ptr<VKDescriptorSet> &descriptor_set = active_descriptor_set();
  VkDescriptorSet vk_descriptor_set = descriptor_set->vk_handle();
  BLI_assert(vk_descriptor_set != VK_NULL_HANDLE);

  Vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(16);
  Vector<VkWriteDescriptorSet> descriptor_writes;

  for (const Binding &binding : bindings_) {
    if (!binding.is_buffer()) {
      continue;
    }
    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = binding.vk_buffer;
    buffer_info.range = binding.buffer_size;
    buffer_infos.append(buffer_info);

    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set;
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pBufferInfo = &buffer_infos.last();
    descriptor_writes.append(write_descriptor);
  }

  for (const Binding &binding : bindings_) {
    if (!binding.is_texel_buffer()) {
      continue;
    }
    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set;
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pTexelBufferView = &binding.vk_buffer_view;
    descriptor_writes.append(write_descriptor);
  }

  Vector<VkDescriptorImageInfo> image_infos;
  image_infos.reserve(16);
  for (const Binding &binding : bindings_) {
    if (!binding.is_image()) {
      continue;
    }
    /* TODO: Based on the actual usage we should use
     * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL/VK_IMAGE_LAYOUT_GENERAL. */
    binding.texture->layout_ensure(context, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorImageInfo image_info = {};
    image_info.sampler = binding.vk_sampler;
    image_info.imageView = binding.texture->image_view_get().vk_handle();
    image_info.imageLayout = binding.texture->current_layout_get();
    image_infos.append(image_info);

    VkWriteDescriptorSet write_descriptor = {};
    write_descriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor.dstSet = vk_descriptor_set;
    write_descriptor.dstBinding = binding.location;
    write_descriptor.descriptorCount = 1;
    write_descriptor.descriptorType = binding.type;
    write_descriptor.pImageInfo = &image_infos.last();
    descriptor_writes.append(write_descriptor);
  }

  const VKDevice &device = VKBackend::get().device_get();
  vkUpdateDescriptorSets(
      device.device_get(), descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);

  bindings_.clear();
}

std::unique_ptr<VKDescriptorSet> VKDescriptorSetTracker::create_resource(VKContext & /*context*/)
{
  VKDevice &device = VKBackend::get().device_;
  return device.descriptor_pools_get().allocate(layout_);
}

}  // namespace blender::gpu
