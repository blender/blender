/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Multiple shaders can use the same descriptor set layout. VKDescriptorSetLayouts has a mechanism
 * to create and reuse existing descriptor set layouts.
 *
 * This makes it easier to detect layout changes between shaders. If the same layout is used, we
 * will be able to reuse the descriptor set if the bindings are also the same.
 *
 * These resources are freed when the Vulkan backend is freed. Descriptor set layouts are Vulkan
 * driver resources, but they are virtually unlimited.
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_mutex.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"

namespace blender::gpu {

class VKDevice;

/**
 * Key of descriptor set layout
 *
 * Contains information to identify same descriptor set layouts.
 */
struct VKDescriptorSetLayoutInfo {
  using Bindings = Vector<VkDescriptorType>;

  Bindings bindings;
  VkShaderStageFlags vk_shader_stage_flags;

  bool operator==(const VKDescriptorSetLayoutInfo &other) const
  {
    return vk_shader_stage_flags == other.vk_shader_stage_flags && bindings == other.bindings;
  };
};

};  // namespace blender::gpu

namespace blender {
/**
 * Default hash for blender::gpu::VKDescriptorSetLayoutInfo.
 *
 * NOTE: DefaultHash needs to be implemented in namespace `blender`.
 */
template<> struct DefaultHash<gpu::VKDescriptorSetLayoutInfo> {
  uint64_t operator()(const gpu::VKDescriptorSetLayoutInfo &key) const
  {
    uint64_t hash = uint64_t(key.vk_shader_stage_flags);
    for (VkDescriptorType vk_descriptor_type : key.bindings) {
      hash = hash * 33 ^ uint64_t(vk_descriptor_type);
    }
    return hash;
  }
};
}  // namespace blender

namespace blender::gpu {

/**
 * Registries of descriptor set layouts.
 */
class VKDescriptorSetLayouts : NonCopyable {
  friend class VKDevice;

 private:
  /**
   * Map containing all created descriptor set layouts.
   */
  Map<VKDescriptorSetLayoutInfo, VkDescriptorSetLayout> vk_descriptor_set_layouts_;

  /**
   * Reusable descriptor set layout create info.
   */
  VkDescriptorSetLayoutCreateInfo vk_descriptor_set_layout_create_info_;
  Vector<VkDescriptorSetLayoutBinding> vk_descriptor_set_layout_bindings_;
  Mutex mutex_;

 public:
  VKDescriptorSetLayouts();
  virtual ~VKDescriptorSetLayouts();

  /**
   * Get an existing descriptor set layout, or create when not available.
   * `r_created` is set to true when a new descriptor set layout was created, set to false when an
   * existing descriptor set layout is returned.
   * `r_needed` is set to true, when a descriptor set layout is needed
   */
  VkDescriptorSetLayout get_or_create(const VKDescriptorSetLayoutInfo &info,
                                      bool &r_created,
                                      bool &r_needed);

  /**
   * Free all descriptor set layouts.
   *
   * This method is called when the VKDevice is destroyed.
   */
  void deinit();

  /**
   * Return the number of descriptor set layouts.
   */
  int64_t size() const
  {
    return vk_descriptor_set_layouts_.size();
  }

 private:
  void update_layout_bindings(const VKDescriptorSetLayoutInfo &info);
};

}  // namespace blender::gpu
