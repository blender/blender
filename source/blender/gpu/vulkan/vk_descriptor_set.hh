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

#include "render_graph/nodes/vk_pipeline_data.hh"
#include "render_graph/vk_resource_access_info.hh"
#include "vk_buffer.hh"
#include "vk_common.hh"
#include "vk_descriptor_set_layouts.hh"
#include "vk_uniform_buffer.hh"

namespace blender::gpu {
struct VKResourceBinding;
class VKStateManager;
class VKDevice;
class VKPushConstants;
class VKShader;
class VKDescriptorSetTracker;
class VKVertexBuffer;

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
    friend class VKDescriptorSetTracker;
    friend class VKShaderInterface;
    friend struct VKResourceBinding;

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
  };
};

class VKDescriptorSetUpdator {
 public:
  virtual ~VKDescriptorSetUpdator() {};

  virtual void allocate_new_descriptor_set(VKDevice &device,
                                           VKContext &context,
                                           VKShader &shader,
                                           VkDescriptorSetLayout vk_descriptor_set_layout,
                                           render_graph::VKPipelineData &r_pipeline_data) = 0;
  void bind_shader_resources(const VKDevice &device,
                             const VKStateManager &state_manager,
                             VKShader &shader,
                             render_graph::VKResourceAccessInfo &access_info);
  virtual void upload_descriptor_sets() = 0;

 private:
  void bind_image_resource(const VKStateManager &state_manager,
                           const VKResourceBinding &resource_binding,
                           render_graph::VKResourceAccessInfo &access_info);
  void bind_texture_resource(const VKDevice &device,
                             const VKStateManager &state_manager,
                             const VKResourceBinding &resource_binding,
                             render_graph::VKResourceAccessInfo &access_info);
  void bind_storage_buffer_resource(const VKStateManager &state_manager,
                                    const VKResourceBinding &resource_binding,
                                    render_graph::VKResourceAccessInfo &access_info);
  void bind_uniform_buffer_resource(const VKStateManager &state_manager,
                                    const VKResourceBinding &resource_binding,
                                    render_graph::VKResourceAccessInfo &access_info);
  void bind_input_attachment_resource(const VKDevice &device,
                                      const VKStateManager &state_manager,
                                      const VKResourceBinding &resource_binding,
                                      render_graph::VKResourceAccessInfo &access_info);

  void bind_push_constants(VKPushConstants &push_constants,
                           render_graph::VKResourceAccessInfo &access_info);

 protected:
  virtual void bind_texel_buffer(VKVertexBuffer &vertex_buffer,
                                 VKDescriptorSet::Location location) = 0;
  virtual void bind_buffer(VkDescriptorType vk_descriptor_type,
                           VkBuffer vk_buffer,
                           VkDeviceAddress vk_device_address,
                           VkDeviceSize buffer_offset,
                           VkDeviceSize size_in_bytes,
                           VKDescriptorSet::Location location) = 0;
  virtual void bind_image(VkDescriptorType vk_descriptor_type,
                          VkSampler vk_sampler,
                          VkImageView vk_image_view,
                          VkImageLayout vk_image_layout,
                          VKDescriptorSet::Location location) = 0;
};

class VKDescriptorSetPoolUpdator : public VKDescriptorSetUpdator {
 public:
  VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;

  void allocate_new_descriptor_set(VKDevice &device,
                                   VKContext &context,
                                   VKShader &shader,
                                   VkDescriptorSetLayout vk_descriptor_set_layout,
                                   render_graph::VKPipelineData &r_pipeline_data) override;

  void upload_descriptor_sets() override;

 protected:
  void bind_texel_buffer(VKVertexBuffer &vertex_buffer,
                         VKDescriptorSet::Location location) override;
  void bind_buffer(VkDescriptorType vk_descriptor_type,
                   VkBuffer vk_buffer,
                   VkDeviceAddress vk_device_address,
                   VkDeviceSize buffer_offset,
                   VkDeviceSize size_in_bytes,
                   VKDescriptorSet::Location location) override;
  void bind_image(VkDescriptorType vk_descriptor_type,
                  VkSampler vk_sampler,
                  VkImageView vk_image_view,
                  VkImageLayout vk_image_layout,
                  VKDescriptorSet::Location location) override;

 private:
  Vector<VkBufferView> vk_buffer_views_;
  Vector<VkDescriptorBufferInfo> vk_descriptor_buffer_infos_;
  Vector<VkDescriptorImageInfo> vk_descriptor_image_infos_;
  Vector<VkWriteDescriptorSet> vk_write_descriptor_sets_;
};

class VKDescriptorSetTracker {
  friend class VKDescriptorSet;

  /* Last used layout to identify changes. */
  VkDescriptorSetLayout vk_descriptor_set_layout_ = VK_NULL_HANDLE;

 public:
  class VKDescriptorSetPoolUpdator descriptor_sets;

  VKDescriptorSetTracker() {}

  /**
   * Update the descriptor set. Reuses previous descriptor set when no changes are detected. This
   * improves performance when working with large grease pencil scenes.
   */
  void update_descriptor_set(VKContext &context,
                             render_graph::VKResourceAccessInfo &resource_access_info,
                             render_graph::VKPipelineData &r_pipeline_data);

  /**
   * Upload all descriptor sets to the device.
   */
  void upload_descriptor_sets();

 private:
};

}  // namespace blender::gpu
