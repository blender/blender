/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "gpu_shader_private.hh"

#include "vk_buffer.hh"
#include "vk_common.hh"
#include "vk_resource_tracker.hh"
#include "vk_uniform_buffer.hh"

namespace blender::gpu {
class VKIndexBuffer;
class VKShaderInterface;
class VKStorageBuffer;
class VKTexture;
class VKUniformBuffer;
class VKVertexBuffer;
class VKDescriptorSetTracker;
class VKSampler;

/**
 * In vulkan shader resources (images and buffers) are grouped in descriptor sets.
 *
 * The resources inside a descriptor set can be updated and bound per set.
 *
 * Currently Blender only supports a single descriptor set per shader, but it is planned to be able
 * to use 2 descriptor sets per shader. One for each #blender::gpu::shader::Frequency.
 */
class VKDescriptorSet : NonCopyable {

 public:
  /**
   * Binding location of a resource in a descriptor set.
   *
   * Locations and bindings are used for different reasons. In the Vulkan backend we use
   * ShaderInput.location to store the descriptor set + the resource binding inside the descriptor
   * set. To ease the development the VKDescriptorSet::Location will be used to hide this
   * confusion.
   *
   * NOTE: [future development] When supporting multiple descriptor sets the encoding/decoding can
   * be centralized here. Location will then also contain the descriptor set index.
   */
  struct Location {
   private:
    /**
     * References to a binding in the descriptor set.
     */
    uint32_t binding;

    Location(uint32_t binding) : binding(binding) {}

   public:
    Location() = default;

    bool operator==(const Location &other) const
    {
      return binding == other.binding;
    }

    operator uint32_t() const
    {
      return binding;
    }

    friend class VKDescriptorSetTracker;
    friend class VKShaderInterface;
  };

  VkDescriptorPool vk_descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet vk_descriptor_set_ = VK_NULL_HANDLE;

 public:
  VKDescriptorSet() = default;
  VKDescriptorSet(VkDescriptorPool vk_descriptor_pool, VkDescriptorSet vk_descriptor_set)
      : vk_descriptor_pool_(vk_descriptor_pool), vk_descriptor_set_(vk_descriptor_set)
  {
    BLI_assert(vk_descriptor_set_ != VK_NULL_HANDLE);
  }
  VKDescriptorSet(VKDescriptorSet &&other);
  virtual ~VKDescriptorSet();

  VKDescriptorSet &operator=(VKDescriptorSet &&other)
  {
    BLI_assert(other.vk_descriptor_set_ != VK_NULL_HANDLE);
    vk_descriptor_set_ = other.vk_descriptor_set_;
    vk_descriptor_pool_ = other.vk_descriptor_pool_;
    other.vk_descriptor_set_ = VK_NULL_HANDLE;
    other.vk_descriptor_pool_ = VK_NULL_HANDLE;
    return *this;
  }

  VkDescriptorSet vk_handle() const
  {
    return vk_descriptor_set_;
  }

  VkDescriptorPool vk_pool_handle() const
  {
    return vk_descriptor_pool_;
  }
};

class VKDescriptorSetTracker : protected VKResourceTracker<VKDescriptorSet> {
  friend class VKDescriptorSet;

  Vector<VkBufferView> vk_buffer_views_;
  Vector<VkDescriptorBufferInfo> vk_descriptor_buffer_infos_;
  Vector<VkDescriptorImageInfo> vk_descriptor_image_infos_;
  Vector<VkWriteDescriptorSet> vk_write_descriptor_sets_;

 public:
  VkDescriptorSetLayout active_vk_descriptor_set_layout = VK_NULL_HANDLE;

  VKDescriptorSetTracker() {}

  void reset();

  void bind_texel_buffer(VKVertexBuffer &vertex_buffer, VKDescriptorSet::Location location);
  void bind_buffer(VkDescriptorType vk_descriptor_type,
                   VkBuffer vk_buffer,
                   VkDeviceSize size_in_bytes,
                   VKDescriptorSet::Location location);
  void bind_image(VkDescriptorType vk_descriptor_type,
                  VkSampler vk_sampler,
                  VkImageView vk_image_view,
                  VkImageLayout vk_image_layout,
                  VKDescriptorSet::Location location);

  std::unique_ptr<VKDescriptorSet> &active_descriptor_set()
  {
    return active_resource();
  }

  /**
   * Update the descriptor set on the device.
   */
  void update(VKContext &context);

 protected:
  std::unique_ptr<VKDescriptorSet> create_resource(VKContext &context) override;

 private:
};

}  // namespace blender::gpu
