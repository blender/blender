/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "gpu_shader_private.hh"

#include "vk_common.hh"

namespace blender::gpu {
class VKStorageBuffer;
class VKVertexBuffer;
class VKIndexBuffer;
class VKTexture;

/**
 * In vulkan shader resources (images and buffers) are grouped in descriptor sets.
 *
 * The resources inside a descriptor set can be updated and bound per set.
 *
 * Currently Blender only supports a single descriptor set per shader, but it is planned to be able
 * to use 2 descriptor sets per shader. Only for each #blender::gpu::shader::Frequency.
 */
class VKDescriptorSet : NonCopyable {
  struct Binding;

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

    Location() = default;

   public:
    Location(const ShaderInput *shader_input) : binding(shader_input->location)
    {
    }

    bool operator==(const Location &other) const
    {
      return binding == other.binding;
    }

    operator uint32_t() const
    {
      return binding;
    }

    friend struct Binding;
  };

 private:
  struct Binding {
    Location location;
    VkDescriptorType type;

    VkBuffer vk_buffer = VK_NULL_HANDLE;
    VkDeviceSize buffer_size = 0;

    VkImageView vk_image_view = VK_NULL_HANDLE;

    Binding()
    {
      location.binding = 0;
    }

    bool is_buffer() const
    {
      return ELEM(type, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    bool is_image() const
    {
      return ELEM(type, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }
  };

  VkDescriptorPool vk_descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet vk_descriptor_set_ = VK_NULL_HANDLE;

  /** A list of bindings that needs to be updated.*/
  Vector<Binding> bindings_;

 public:
  VKDescriptorSet() = default;
  VKDescriptorSet(VkDescriptorPool vk_descriptor_pool, VkDescriptorSet vk_descriptor_set)
      : vk_descriptor_pool_(vk_descriptor_pool), vk_descriptor_set_(vk_descriptor_set)
  {
  }
  virtual ~VKDescriptorSet();

  VKDescriptorSet &operator=(VKDescriptorSet &&other)
  {
    vk_descriptor_set_ = other.vk_descriptor_set_;
    vk_descriptor_pool_ = other.vk_descriptor_pool_;
    other.mark_freed();
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

  void bind_as_ssbo(VKVertexBuffer &buffer, Location location);
  void bind_as_ssbo(VKIndexBuffer &buffer, Location location);
  void bind(VKStorageBuffer &buffer, Location location);
  void image_bind(VKTexture &texture, Location location);

  /**
   * Update the descriptor set on the device.
   */
  void update(VkDevice vk_device);

  void mark_freed();

 private:
  Binding &ensure_location(Location location);
};

}  // namespace blender::gpu
