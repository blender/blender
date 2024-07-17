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
  other.vk_descriptor_set_ = VK_NULL_HANDLE;
  other.vk_descriptor_pool_ = VK_NULL_HANDLE;
}

VKDescriptorSet::~VKDescriptorSet()
{
  if (vk_descriptor_set_ != VK_NULL_HANDLE) {
    /* Handle should be given back to the pool. */
    const VKDevice &device = VKBackend::get().device;
    vkFreeDescriptorSets(device.vk_handle(), vk_descriptor_pool_, 1, &vk_descriptor_set_);

    vk_descriptor_set_ = VK_NULL_HANDLE;
    vk_descriptor_pool_ = VK_NULL_HANDLE;
  }
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
                                        const VKDescriptorSet::Location location,
                                        VKImageViewArrayed arrayed)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  binding.texture = &texture;
  binding.arrayed = arrayed;
}

void VKDescriptorSetTracker::bind(VKTexture &texture,
                                  const VKDescriptorSet::Location location,
                                  const VKSampler &sampler,
                                  VKImageViewArrayed arrayed)
{
  Binding &binding = ensure_location(location);
  binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.texture = &texture;
  binding.vk_sampler = sampler.vk_handle();
  binding.arrayed = arrayed;
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
  const VKShader &shader = *unwrap(context.shader);
  VkDescriptorSetLayout vk_descriptor_set_layout = shader.vk_descriptor_set_layout_get();
  active_vk_descriptor_set_layout = vk_descriptor_set_layout;
  tracked_resource_for(context, true);
  std::unique_ptr<VKDescriptorSet> &descriptor_set = active_descriptor_set();
  VkDescriptorSet vk_descriptor_set = descriptor_set->vk_handle();
  BLI_assert(vk_descriptor_set != VK_NULL_HANDLE);
  debug::object_label(vk_descriptor_set, shader.name_get());

  /* Ensure that the local arrays contain enough space to store the bindings. */
  BLI_assert_msg(vk_descriptor_image_infos_.is_empty() && vk_descriptor_buffer_infos_.is_empty() &&
                     vk_write_descriptor_sets_.is_empty(),
                 "Incorrect usage, internal vectors should be empty.");
  if (vk_descriptor_buffer_infos_.capacity() < bindings_.size()) {
    vk_descriptor_buffer_infos_.reserve(bindings_.size() * 2);
    vk_descriptor_image_infos_.reserve(bindings_.size() * 2);
  }

  for (const Binding &binding : bindings_) {
    if (!binding.is_buffer()) {
      continue;
    }
    vk_descriptor_buffer_infos_.append({binding.vk_buffer, 0, binding.buffer_size});
    vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      nullptr,
                                      vk_descriptor_set,
                                      binding.location,
                                      0,
                                      1,
                                      binding.type,
                                      nullptr,
                                      &vk_descriptor_buffer_infos_.last(),
                                      nullptr});
  }

  for (const Binding &binding : bindings_) {
    if (!binding.is_texel_buffer()) {
      continue;
    }
    vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      nullptr,
                                      vk_descriptor_set,
                                      binding.location,
                                      0,
                                      1,
                                      binding.type,
                                      nullptr,
                                      nullptr,
                                      &binding.vk_buffer_view});
  }

  for (const Binding &binding : bindings_) {
    if (!binding.is_image()) {
      continue;
    }
    /* TODO: Based on the actual usage we should use
     * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL. */
    vk_descriptor_image_infos_.append(
        {binding.vk_sampler,
         binding.texture->image_view_get(binding.arrayed).vk_handle(),
         VK_IMAGE_LAYOUT_GENERAL});
    vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      nullptr,
                                      vk_descriptor_set,
                                      binding.location,
                                      0,
                                      1,
                                      binding.type,
                                      &vk_descriptor_image_infos_.last(),
                                      nullptr,
                                      nullptr});
  }

  const VKDevice &device = VKBackend::get().device;
  vkUpdateDescriptorSets(device.vk_handle(),
                         vk_write_descriptor_sets_.size(),
                         vk_write_descriptor_sets_.data(),
                         0,
                         nullptr);

  bindings_.clear();
  vk_descriptor_image_infos_.clear();
  vk_descriptor_buffer_infos_.clear();
  vk_write_descriptor_sets_.clear();
}

std::unique_ptr<VKDescriptorSet> VKDescriptorSetTracker::create_resource(VKContext &context)
{
  return context.descriptor_pools_get().allocate(active_vk_descriptor_set_layout);
}

void VKDescriptorSetTracker::debug_print() const
{
  for (const Binding &binding : bindings_) {
    binding.debug_print();
  }
}

void VKDescriptorSetTracker::Binding::debug_print() const
{
  std::cout << "VkDescriptorSetTrackker::Binding(type: " << type
            << ", location:" << location.binding << ")\n";
}

}  // namespace blender::gpu
