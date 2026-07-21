/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "vk_ray_tracing.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_index_buffer.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_buffer.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"blender.gpu"};

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Top level acceleration structure
 * \{ */

VKTopLevelAS::VKTopLevelAS(const char *name) : TopLevelAS(name), max_primitive_count_(0)
{
  render_graph::VKBuildAccelerationStructureNode::Data &node_data =
      build_acceleration_structure_info_.node_data;

  node_data.vk_acceleration_structure_build_geometry_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      nullptr,
      VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
          VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      VK_NULL_HANDLE,
      VK_NULL_HANDLE,
      0,
      nullptr,
      nullptr,
      {0}};
}

VKTopLevelAS::~VKTopLevelAS()
{
  VKDiscardPool &discard_pool = VKDiscardPool::discard_pool_get();
  if (vk_acceleration_structure_) {
    discard_pool.discard_acceleration_structure(vk_acceleration_structure_);
    vk_acceleration_structure_ = VK_NULL_HANDLE;
  }
  if (buffer_.is_allocated()) {
    buffer_.free();
  }
}

InstanceID VKTopLevelAS::add_instance(const BottomLevelAS &blas_,
                                      const float4x4 &mat,
                                      const uint8_t mask)
{
  BLI_assert_msg(vk_acceleration_structure_ == VK_NULL_HANDLE,
                 "Adding instances to an existing acceleration structure isn't supported. "
                 "Updating an existing instance of an acceleration structure is supported via the "
                 "update_instance methods.");

  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceAccelerationStructurePropertiesKHR &acceleration_structure_properties =
      device.physical_device_acceleration_structure_properties_get();
  if (max_primitive_count_ == acceleration_structure_properties.maxInstanceCount) {
    CLOG_ERROR(&LOG,
               "Cannot add instance to top level acceleration structure as the number of "
               "instances is larger than the GPU can handle.");
    return {-1};
  }

  InstanceID instance_id = {max_primitive_count_};
  /* The API order is row major. Transpose the matrix. */
  instances_.append({
      {{{mat.x.x, mat.y.x, mat.z.x, mat.w.x},
        {mat.x.y, mat.y.y, mat.z.y, mat.w.y},
        {mat.x.z, mat.y.z, mat.z.z, mat.w.z}}},
      0,    /* instanceCustomIndex */
      mask, /* mask */
      0,    /* instanceShaderBindingTableRecordOffset */
      0,    /* flags */
      0,    /* device address */
  });
  blas_per_instance_.append(&blas_);
  max_primitive_count_ += 1;
  is_dirty_ = true;

  return instance_id;
}

void VKTopLevelAS::update_instance(InstanceID instance_id, const float4x4 &mat, uint8_t mask)
{
  BLI_assert(instance_id.id < max_primitive_count_);
  if (instance_id.id < 0 || instance_id.id >= max_primitive_count_) {
    return;
  }

  VkAccelerationStructureInstanceKHR &instance = instances_[instance_id.id];
  /* The API order is row major. Transpose the matrix. */
  instance.transform.matrix[0][0] = mat.x.x;
  instance.transform.matrix[0][1] = mat.y.x;
  instance.transform.matrix[0][2] = mat.z.x;
  instance.transform.matrix[0][3] = mat.w.x;
  instance.transform.matrix[1][0] = mat.x.y;
  instance.transform.matrix[1][1] = mat.y.y;
  instance.transform.matrix[1][2] = mat.z.y;
  instance.transform.matrix[1][3] = mat.w.y;
  instance.transform.matrix[2][0] = mat.x.z;
  instance.transform.matrix[2][1] = mat.y.z;
  instance.transform.matrix[2][2] = mat.z.z;
  instance.transform.matrix[2][3] = mat.w.z;
  instance.mask = mask;

  is_dirty_ = true;
}

void VKTopLevelAS::build()
{
  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceAccelerationStructurePropertiesKHR &acceleration_structure_properties =
      device.physical_device_acceleration_structure_properties_get();

  const bool do_update = vk_acceleration_structure_ != VK_NULL_HANDLE;
  if (do_update && !is_dirty_) {
    return;
  }

  build_acceleration_structure_info_.src_buffers.clear_and_keep_capacity();

  for (int64_t blas_index : instances_.index_range()) {
    VkAccelerationStructureInstanceKHR &instance = instances_[blas_index];
    const VKBottomLevelAS &blas = unwrap(*blas_per_instance_[blas_index]);
    if (blas.vk_device_address() == 0) {
      CLOG_ERROR(&LOG,
                 "Cannot add blas to top level acceleration structure as the blas "
                 "doesn't have a device address.");
      return;
    }
    instance.accelerationStructureReference = blas.vk_device_address();
    build_acceleration_structure_info_.src_buffers.add(blas.vk_buffer());
  }

  /* Create the instances buffer and upload the instance data. */
  VKContext &context = *VKContext::get();
  size_t instances_buffer_size = instances_.size() * sizeof(VkAccelerationStructureInstanceKHR);
  if (!instances_buffer_.is_allocated()) {
    instances_buffer_.create(
        instances_buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        0.8f,
        false,
        name_get());
  }

  /* Update the instances buffer. */
  if (instances_buffer_size != 0) {
    if (instances_buffer_size < 65536) {
      /* Only use udpate_render_graph for buffers < 64Kb. */
      /* Make a copy of the instances data as the update render graph takes ownership and needs to
       * be a guarded allocation. */
      void *copy_of_data = MEM_new_uninitialized(instances_buffer_size, __func__);
      memcpy(copy_of_data, instances_.data(), instances_buffer_size);
      instances_buffer_.update_render_graph(context, copy_of_data);
    }
    else {
      /* Create a staging buffer otherwise. */
      VKStagingBuffer staging_buffer(instances_buffer_, VKStagingBuffer::Direction::HostToDevice);
      VKBuffer &buffer = staging_buffer.host_buffer_get();
      if (buffer.is_allocated()) {
        staging_buffer.host_buffer_get().update_immediately(instances_.data());
        staging_buffer.copy_to_device(context);
      }
      else {
        buffer_.clear(context, 0u);
        CLOG_ERROR(
            &LOG,
            "Unable to upload data to TLAS buffer via a staging buffer as the staging buffer "
            "could not be allocated.");
      }
    }
  }

  build_acceleration_structure_info_.src_buffers.add(instances_buffer_.resource());

  render_graph::VKBuildAccelerationStructureNode::Data &node_data =
      build_acceleration_structure_info_.node_data;
  VkAccelerationStructureKHR old_acceleration_structure = vk_acceleration_structure_;

  if (!do_update) {
    /* Initialize the geometry data with the instances. */
    node_data.vk_acceleration_structure_build_range_infos.append({max_primitive_count_, 0, 0, 0});
    VkAccelerationStructureGeometryKHR vk_acceleration_structure_geometry = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        nullptr,
        VK_GEOMETRY_TYPE_INSTANCES_KHR};
    vk_acceleration_structure_geometry.geometry.instances = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        nullptr,
        VK_FALSE,
        {instances_buffer_.device_address_get()}};
    vk_acceleration_structure_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    node_data.vk_acceleration_structure_geometries.append(vk_acceleration_structure_geometry);
  }
  else {
    node_data.vk_acceleration_structure_build_geometry_info.mode =
        VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    node_data.vk_acceleration_structure_build_geometry_info.srcAccelerationStructure =
        old_acceleration_structure;
  }

  VkAccelerationStructureBuildGeometryInfoKHR build_geometry_infos = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      nullptr,
      VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
          VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      do_update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR :
                  VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      do_update ? old_acceleration_structure : VK_NULL_HANDLE,
      VK_NULL_HANDLE,
      uint32_t(node_data.vk_acceleration_structure_geometries.size()),
      node_data.vk_acceleration_structure_geometries.data(),
      nullptr,
      {0}};

  /* Determine acceleration structure + scratch space */
  VkAccelerationStructureBuildSizesInfoKHR vk_acceleration_structure_build_sizes_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  device.functions.vkGetAccelerationStructureBuildSizesKHR(
      device.vk_handle(),
      VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &build_geometry_infos,
      &max_primitive_count_,
      &vk_acceleration_structure_build_sizes_info);

  /* Allocate acceleration structure + backing buffer */
  if (buffer_.is_allocated()) {
    buffer_.free();
  }
  buffer_.create(vk_acceleration_structure_build_sizes_info.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                 0,
                 1.0f,
                 false,
                 name_get());
  BLI_assert(buffer_.is_allocated());

  VkAccelerationStructureCreateInfoKHR vk_acceleration_structure_create_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      nullptr,
      0,
      buffer_.vk_handle(),
      0,
      vk_acceleration_structure_build_sizes_info.accelerationStructureSize,
      VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      0};
  device.functions.vkCreateAccelerationStructureKHR(device.vk_handle(),
                                                    &vk_acceleration_structure_create_info,
                                                    nullptr,
                                                    &vk_acceleration_structure_);
  BLI_assert(vk_acceleration_structure_ != VK_NULL_HANDLE);
  debug::object_label(vk_acceleration_structure_, name_get());

  BLI_assert((do_update && vk_device_address_ != 0) || (!do_update && vk_device_address_ == 0));
  vk_device_address_ = 0;
  VkAccelerationStructureDeviceAddressInfoKHR vk_acceleration_structure_device_address_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      nullptr,
      vk_acceleration_structure_,
  };
  vk_device_address_ = device.functions.vkGetAccelerationStructureDeviceAddressKHR(
      device.vk_handle(), &vk_acceleration_structure_device_address_info);
  BLI_assert(vk_device_address_ != 0);

  /* Create scratch space for building */
  /* TODO: Only create when size differs from previous allocation. */
  VKBuffer device_scratch_space;
  device_scratch_space.create(
      ceil_to_multiple_ul(
          do_update ? vk_acceleration_structure_build_sizes_info.updateScratchSize :
                      vk_acceleration_structure_build_sizes_info.buildScratchSize,
          acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment) +
          acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      VmaAllocationCreateFlags(0),
      1.0,
      false,
      name_get());
  BLI_assert(device_scratch_space.is_allocated());
  BLI_assert(device_scratch_space.device_address_get());

  build_geometry_infos.scratchData.deviceAddress = device_scratch_space.device_address_get();
  node_data.vk_acceleration_structure_build_geometry_info.scratchData.deviceAddress =
      ceil_to_multiple_ul(
          device_scratch_space.device_address_get(),
          acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment);
  node_data.vk_acceleration_structure_build_geometry_info.dstAccelerationStructure =
      vk_acceleration_structure_;
  build_acceleration_structure_info_.dst_acceleration_structure = buffer_.resource();
  build_acceleration_structure_info_.scratch_buffer = device_scratch_space.resource();

  render_graph::VKRenderGraph &render_graph = context.render_graph();
  render_graph.add_node(build_acceleration_structure_info_);
  if (old_acceleration_structure) {
    context.discard_pool.discard_acceleration_structure(old_acceleration_structure);
  }
  is_dirty_ = false;
}

void VKTopLevelAS::bind(int slot)
{
  VKContext &context = *VKContext::get();
  context.state_manager_get().toplevelas_bind(*this, slot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bottom level acceleration structure
 * \{ */

VKBottomLevelAS::VKBottomLevelAS(const char *name) : BottomLevelAS(name)
{
  render_graph::VKBuildAccelerationStructureNode::Data &node_data =
      build_acceleration_structure_info_.node_data;
  node_data.vk_acceleration_structure_build_geometry_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      nullptr,
      VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      VK_NULL_HANDLE,
      VK_NULL_HANDLE,
      0,
      nullptr,
      nullptr,
      {0}};
}

VKBottomLevelAS::~VKBottomLevelAS()
{
  VKDiscardPool &discard_pool = VKDiscardPool::discard_pool_get();
  if (vk_acceleration_structure_) {
    discard_pool.discard_acceleration_structure(vk_acceleration_structure_);
    vk_acceleration_structure_ = VK_NULL_HANDLE;
  }
  if (buffer_.is_allocated()) {
    buffer_.free();
  }
}

void VKBottomLevelAS::add_geometry(IndexBuf &index_buffer_, VertBuf &vertex_buffer_)
{
  BLI_assert_msg(vk_acceleration_structure_ == VK_NULL_HANDLE,
                 "Updating an existing acceleration structure isn't implemented.");

  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceAccelerationStructurePropertiesKHR &acceleration_structure_properties =
      device.physical_device_acceleration_structure_properties_get();
  if (build_acceleration_structure_info_.node_data.vk_acceleration_structure_geometries.size() >=
      acceleration_structure_properties.maxGeometryCount)
  {
    CLOG_ERROR(&LOG,
               "Cannot add geometry to bottom level acceleration structure as the number of "
               "geometries is larger than the GPU can handle.");
    return;
  }

  VKVertexBuffer &vertex_buffer = unwrap(vertex_buffer_);
  VKIndexBuffer &index_buffer = unwrap(index_buffer_);
  index_buffer.ensure_updated();
  vertex_buffer.ensure_updated();

  if (!vertex_buffer.has_device_address()) {
    CLOG_ERROR(&LOG,
               "Cannot add geometry to bottom level acceleration structure as the vertex buffer "
               "doesn't have a device address. This could be an out of memory issue.");
    return;
  }
  if (!index_buffer.has_device_address()) {
    CLOG_ERROR(&LOG,
               "Cannot add geometry to bottom level acceleration structure as the index buffer "
               "doesn't have a device address. This could be an out of memory issue.");
    return;
  }
  const VkFormat vertex_format = vertex_buffer.to_vk_format();
  if (vertex_format == VK_FORMAT_UNDEFINED) {
    CLOG_ERROR(&LOG,
               "Cannot add geometry to bottom level acceleration structure as the format of the "
               "vertex buffer cannot be determined.");
    return;
  }

  build_acceleration_structure_info_.node_data.vk_acceleration_structure_geometries.append(
      {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
       nullptr,
       VK_GEOMETRY_TYPE_TRIANGLES_KHR,
       {
           {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            nullptr,
            vertex_format,
            {vertex_buffer.device_address_get()},
            vertex_buffer.format.stride,
            vertex_buffer.vertex_len - 1,
            index_buffer.vk_index_type(),
            {index_buffer.device_address_get()},
            {0}},
       },
       VK_GEOMETRY_OPAQUE_BIT_KHR});
  const uint32_t num_primitives = uint32_t(index_buffer.index_len_get() / 3);
  build_acceleration_structure_info_.node_data.vk_acceleration_structure_build_range_infos.append(
      {num_primitives,
       index_buffer.index_start_get() * (index_buffer.is_32bit() ? 4u : 2u),
       index_buffer.index_base_get(),
       0});
  max_primitive_count_per_geometry_.append(num_primitives);

  build_acceleration_structure_info_.src_buffers.add(index_buffer.resource());
  build_acceleration_structure_info_.src_buffers.add(vertex_buffer.resource());
}

void VKBottomLevelAS::build()
{
  BLI_assert(vk_acceleration_structure_ == VK_NULL_HANDLE);
  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceAccelerationStructurePropertiesKHR &acceleration_structure_properties =
      device.physical_device_acceleration_structure_properties_get();

  render_graph::VKBuildAccelerationStructureNode::Data &node_data =
      build_acceleration_structure_info_.node_data;

  VkAccelerationStructureBuildGeometryInfoKHR build_geometry_infos = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      nullptr,
      VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
      VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      VK_NULL_HANDLE,
      VK_NULL_HANDLE,
      uint32_t(node_data.vk_acceleration_structure_geometries.size()),
      node_data.vk_acceleration_structure_geometries.data(),
      nullptr,
      {0}};

  /* Determine acceleration structure + scratch space */
  VkAccelerationStructureBuildSizesInfoKHR vk_acceleration_structure_build_sizes_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
  device.functions.vkGetAccelerationStructureBuildSizesKHR(
      device.vk_handle(),
      VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &build_geometry_infos,
      max_primitive_count_per_geometry_.data(),
      &vk_acceleration_structure_build_sizes_info);

  /* Allocate acceleration structure + backing buffer */
  buffer_.create(vk_acceleration_structure_build_sizes_info.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                 0,
                 1.0f,
                 false,
                 name_get());
  BLI_assert(buffer_.is_allocated());

  VkAccelerationStructureCreateInfoKHR vk_acceleration_structure_create_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      nullptr,
      0,
      buffer_.vk_handle(),
      0,
      vk_acceleration_structure_build_sizes_info.accelerationStructureSize,
      VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      0};
  device.functions.vkCreateAccelerationStructureKHR(device.vk_handle(),
                                                    &vk_acceleration_structure_create_info,
                                                    nullptr,
                                                    &vk_acceleration_structure_);
  BLI_assert(vk_acceleration_structure_ != VK_NULL_HANDLE);
  debug::object_label(vk_acceleration_structure_, name_get());

  BLI_assert(vk_device_address_ == 0);
  VkAccelerationStructureDeviceAddressInfoKHR vk_acceleration_structure_device_address_info = {
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      nullptr,
      vk_acceleration_structure_,
  };
  vk_device_address_ = device.functions.vkGetAccelerationStructureDeviceAddressKHR(
      device.vk_handle(), &vk_acceleration_structure_device_address_info);
  BLI_assert(vk_device_address_ != 0);

  /* Create scratch space for building */
  VKBuffer device_scratch_space;
  device_scratch_space.create(
      ceil_to_multiple_ul(
          vk_acceleration_structure_build_sizes_info.buildScratchSize,
          acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment) +
          acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
      0,
      1.0,
      false,
      name_get());
  BLI_assert(device_scratch_space.is_allocated());
  BLI_assert(device_scratch_space.device_address_get());

  build_geometry_infos.scratchData.deviceAddress = device_scratch_space.device_address_get();
  node_data.vk_acceleration_structure_build_geometry_info.scratchData.deviceAddress =
      ceil_to_multiple_ul(
          device_scratch_space.device_address_get(),
          acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment);
  node_data.vk_acceleration_structure_build_geometry_info.dstAccelerationStructure =
      vk_acceleration_structure_;
  build_acceleration_structure_info_.dst_acceleration_structure = buffer_.resource();
  build_acceleration_structure_info_.scratch_buffer = device_scratch_space.resource();

  VKContext &context = *VKContext::get();
  render_graph::VKRenderGraph &render_graph = context.render_graph();
  render_graph.add_node(build_acceleration_structure_info_);
}

/** \} */

}  // namespace blender::gpu
