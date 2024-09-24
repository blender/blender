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

void VKDescriptorSetTracker::reset()
{
  vk_descriptor_image_infos_.clear();
  vk_descriptor_buffer_infos_.clear();
  vk_buffer_views_.clear();
  vk_write_descriptor_sets_.clear();
}

void VKDescriptorSetTracker::bind_buffer(VkDescriptorType vk_descriptor_type,
                                         VkBuffer vk_buffer,
                                         VkDeviceSize size_in_bytes,
                                         VKDescriptorSet::Location location)
{
  vk_descriptor_buffer_infos_.append({vk_buffer, 0, size_in_bytes});
  vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    VK_NULL_HANDLE,
                                    location,
                                    0,
                                    1,
                                    vk_descriptor_type,
                                    nullptr,
                                    nullptr,
                                    nullptr});
}

void VKDescriptorSetTracker::bind_texel_buffer(VKVertexBuffer &vertex_buffer,
                                               const VKDescriptorSet::Location location)
{
  vk_buffer_views_.append(vertex_buffer.vk_buffer_view_get());
  vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    VK_NULL_HANDLE,
                                    location,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                    nullptr,
                                    nullptr,
                                    nullptr});
}

void VKDescriptorSetTracker::bind_image(VkDescriptorType vk_descriptor_type,
                                        VkSampler vk_sampler,
                                        VkImageView vk_image_view,
                                        VkImageLayout vk_image_layout,
                                        VKDescriptorSet::Location location)
{
  vk_descriptor_image_infos_.append({vk_sampler, vk_image_view, vk_image_layout});
  vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    VK_NULL_HANDLE,
                                    location,
                                    0,
                                    1,
                                    vk_descriptor_type,
                                    nullptr,
                                    nullptr,
                                    nullptr});
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

  /* Populate the final addresses and handles */
  int buffer_index = 0;
  int buffer_view_index = 0;
  int image_index = 0;
  for (int write_index : vk_write_descriptor_sets_.index_range()) {
    VkWriteDescriptorSet &vk_write_descriptor_set = vk_write_descriptor_sets_[write_index++];
    vk_write_descriptor_set.dstSet = vk_descriptor_set;

    switch (vk_write_descriptor_set.descriptorType) {
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        vk_write_descriptor_set.pImageInfo = &vk_descriptor_image_infos_[image_index++];
        break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        vk_write_descriptor_set.pTexelBufferView = &vk_buffer_views_[buffer_view_index++];
        break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        vk_write_descriptor_set.pBufferInfo = &vk_descriptor_buffer_infos_[buffer_index++];
        break;

      default:
        BLI_assert_unreachable();
        break;
    }
  }

  /* Update the descriptor set on the device. */
  const VKDevice &device = VKBackend::get().device;
  vkUpdateDescriptorSets(device.vk_handle(),
                         vk_write_descriptor_sets_.size(),
                         vk_write_descriptor_sets_.data(),
                         0,
                         nullptr);
  reset();
}

std::unique_ptr<VKDescriptorSet> VKDescriptorSetTracker::create_resource(VKContext &context)
{
  return context.descriptor_pools_get().allocate(active_vk_descriptor_set_layout);
}

}  // namespace blender::gpu
