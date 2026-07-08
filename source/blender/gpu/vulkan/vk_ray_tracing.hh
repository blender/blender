/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

#include "GPU_ray_tracing.hh"

#include "vk_buffer.hh"
#include "vk_common.hh"

#include "render_graph/nodes/vk_build_acceleration_structure_node.hh"

namespace blender::gpu {

class VKTopLevelAS : public TopLevelAS {
  /** Handle of the acceleration structure. */
  VkAccelerationStructureKHR vk_acceleration_structure_ = VK_NULL_HANDLE;
  VkDeviceAddress vk_device_address_ = 0;
  uint32_t max_primitive_count_;

  /** Backing buffer for the VkAccelerationStructure handle.  */
  VKBuffer buffer_;
  /** Create info for building the acceleration structure using the render graph.  */
  render_graph::VKBuildAccelerationStructureNode::CreateInfo build_acceleration_structure_info_ =
      {};

  Vector<VkAccelerationStructureInstanceKHR> instances_;
  Vector<const BottomLevelAS *> blas_per_instance_;
  VKBuffer instances_buffer_;
  bool is_dirty_ = true;

 public:
  VKTopLevelAS(const char *name);
  ~VKTopLevelAS();
  InstanceID add_instance(const BottomLevelAS &blas, const float4x4 &mat, uint8_t mask) override;
  void update_instance(InstanceID instance_id, const float4x4 &mat, uint8_t mask) override;
  void build() override;
  void bind(int slot) override;

  VkAccelerationStructureKHR vk_acceleration_structure() const
  {
    BLI_assert(vk_acceleration_structure_ != VK_NULL_HANDLE);
    return vk_acceleration_structure_;
  }

  const VKResourceWithHandle<VkBuffer> &vk_buffer() const  // TODO: Rename?
  {
    return buffer_.resource();
  }

  VkDeviceAddress vk_device_address() const
  {
    BLI_assert(vk_device_address_ != 0);
    return vk_device_address_;
  }
};

class VKBottomLevelAS : public BottomLevelAS {
  /** Handle of the acceleration structure. */
  VkAccelerationStructureKHR vk_acceleration_structure_ = VK_NULL_HANDLE;
  VkDeviceAddress vk_device_address_ = 0;
  Vector<uint32_t> max_primitive_count_per_geometry_;

  /** Backing buffer for the VkAccelerationStructure handle.  */
  VKBuffer buffer_;
  /** Create info for building the acceleration structure using the render graph.  */
  render_graph::VKBuildAccelerationStructureNode::CreateInfo build_acceleration_structure_info_ =
      {};

 public:
  VKBottomLevelAS(const char *name);
  ~VKBottomLevelAS();

  void add_geometry(IndexBuf &index_buffer, VertBuf &vertex_buffer) override;
  void build() override;

  VkAccelerationStructureKHR vk_acceleration_structure() const
  {
    BLI_assert(vk_acceleration_structure_ != VK_NULL_HANDLE);
    return vk_acceleration_structure_;
  }

  const VKResourceWithHandle<VkBuffer> &vk_buffer() const  // TODO: Rename?
  {
    return buffer_.resource();
  }

  VkDeviceAddress vk_device_address() const
  {
    BLI_assert(vk_device_address_ != 0);
    return vk_device_address_;
  }
};

static inline const VKTopLevelAS &unwrap(const TopLevelAS &blas)
{
  return static_cast<const VKTopLevelAS &>(blas);
}
static inline const VKBottomLevelAS &unwrap(const BottomLevelAS &blas)
{
  return static_cast<const VKBottomLevelAS &>(blas);
}
}  // namespace blender::gpu
