/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_descriptor_set.hh"
#include "vk_index_buffer.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

void VKDescriptorSetTracker::update_descriptor_set(VKContext &context,
                                                   render_graph::VKResourceAccessInfo &access_info,
                                                   render_graph::VKPipelineData &r_pipeline_data)
{
  VKShader &shader = *unwrap(context.shader);
  VKStateManager &state_manager = context.state_manager_get();

  /* Can we reuse previous descriptor set. */
  if (!state_manager.is_dirty &&
      !assign_if_different(vk_descriptor_set_layout_, shader.vk_descriptor_set_layout_get()) &&
      shader.push_constants.layout_get().storage_type_get() !=
          VKPushConstants::StorageType::UNIFORM_BUFFER)
  {
    return;
  }
  state_manager.is_dirty = false;

  VKDevice &device = VKBackend::get().device;
  VkDescriptorSetLayout vk_descriptor_set_layout = shader.vk_descriptor_set_layout_get();
  VKDescriptorSetUpdator *updator = &descriptor_sets;
  if (device.extensions_get().descriptor_buffer) {
    updator = &descriptor_buffers;
  }
  updator->allocate_new_descriptor_set(
      device, context, shader, vk_descriptor_set_layout, r_pipeline_data);
  updator->bind_shader_resources(device, state_manager, shader, access_info);
}

void VKDescriptorSetTracker::upload_descriptor_sets()
{
  VKDevice &device = VKBackend::get().device;
  if (device.extensions_get().descriptor_buffer) {
    descriptor_buffers.upload_descriptor_sets();
  }
  else {
    descriptor_sets.upload_descriptor_sets();
  }
  vk_descriptor_set_layout_ = VK_NULL_HANDLE;
}

/* -------------------------------------------------------------------- */
/** \name VKDescriptorSetUpdator
 * \{ */

void VKDescriptorSetUpdator::bind_image_resource(const VKStateManager &state_manager,
                                                 const VKResourceBinding &resource_binding,
                                                 render_graph::VKResourceAccessInfo &access_info)
{
  VKTexture &texture = *state_manager.images_.get(resource_binding.binding);
  bind_image(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      VK_NULL_HANDLE,
      texture.image_view_get(resource_binding.arrayed, VKImageViewFlags::NO_SWIZZLING).vk_handle(),
      VK_IMAGE_LAYOUT_GENERAL,
      resource_binding.location);
  /* Update access info. */
  uint32_t layer_base = 0;
  uint32_t layer_count = VK_REMAINING_ARRAY_LAYERS;
  if (resource_binding.arrayed == VKImageViewArrayed::ARRAYED && texture.is_texture_view()) {
    IndexRange layer_range = texture.layer_range();
    layer_base = layer_range.start();
    layer_count = layer_range.size();
  }
  access_info.images.append({texture.vk_image_handle(),
                             resource_binding.access_mask,
                             to_vk_image_aspect_flag_bits(texture.device_format_get()),
                             layer_base,
                             layer_count});
}

void VKDescriptorSetUpdator::bind_texture_resource(const VKDevice &device,
                                                   const VKStateManager &state_manager,
                                                   const VKResourceBinding &resource_binding,
                                                   render_graph::VKResourceAccessInfo &access_info)
{
  const BindSpaceTextures::Elem *elem_ptr = state_manager.textures_.get(resource_binding.binding);
  if (!elem_ptr) {
    /* Unbound resource. */
    BLI_assert_unreachable();
    return;
  }
  const BindSpaceTextures::Elem &elem = *elem_ptr;
  switch (elem.resource_type) {
    case BindSpaceTextures::Type::VertexBuffer: {
      VKVertexBuffer &vertex_buffer = *static_cast<VKVertexBuffer *>(elem.resource);
      vertex_buffer.ensure_updated();
      bind_texel_buffer(vertex_buffer, resource_binding.location);
      access_info.buffers.append({vertex_buffer.vk_handle(), resource_binding.access_mask});
      break;
    }
    case BindSpaceTextures::Type::Texture: {
      VKTexture *texture = static_cast<VKTexture *>(elem.resource);
      if (texture->type_ == GPU_TEXTURE_BUFFER) {
        /* Texture buffers are no textures, but wrap around vertex buffers and need to be
         * bound as texel buffers. */
        /* TODO: Investigate if this can be improved in the API. */
        VKVertexBuffer &vertex_buffer = *texture->source_buffer_;
        vertex_buffer.ensure_updated();
        bind_texel_buffer(vertex_buffer, resource_binding.location);
        access_info.buffers.append({vertex_buffer.vk_handle(), resource_binding.access_mask});
      }
      else {
        const VKSampler &sampler = device.samplers().get(elem.sampler);
        bind_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   sampler.vk_handle(),
                   texture->image_view_get(resource_binding.arrayed, VKImageViewFlags::DEFAULT)
                       .vk_handle(),
                   VK_IMAGE_LAYOUT_GENERAL,
                   resource_binding.location);
        access_info.images.append({texture->vk_image_handle(),
                                   resource_binding.access_mask,
                                   to_vk_image_aspect_flag_bits(texture->device_format_get()),
                                   0,
                                   VK_REMAINING_ARRAY_LAYERS});
      }
      break;
    }
    case BindSpaceTextures::Type::Unused: {
      BLI_assert_unreachable();
    }
  }
}

void VKDescriptorSetUpdator::bind_input_attachment_resource(
    const VKDevice &device,
    const VKStateManager &state_manager,
    const VKResourceBinding &resource_binding,
    render_graph::VKResourceAccessInfo &access_info)
{
  const bool supports_local_read = device.extensions_get().dynamic_rendering_local_read;
  if (supports_local_read) {
    VKTexture *texture = static_cast<VKTexture *>(
        state_manager.images_.get(resource_binding.binding));
    BLI_assert(texture);
    bind_image(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
               VK_NULL_HANDLE,
               texture->image_view_get(resource_binding.arrayed, VKImageViewFlags::NO_SWIZZLING)
                   .vk_handle(),
               VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
               resource_binding.location);
    VkImage vk_image = texture->vk_image_handle();
    if (vk_image != VK_NULL_HANDLE) {
      access_info.images.append({texture->vk_image_handle(),
                                 resource_binding.access_mask,
                                 to_vk_image_aspect_flag_bits(texture->device_format_get()),
                                 0,
                                 VK_REMAINING_ARRAY_LAYERS});
    }
  }
  else {
    bool supports_dynamic_rendering = device.extensions_get().dynamic_rendering;
    const BindSpaceTextures::Elem *elem_ptr = state_manager.textures_.get(
        resource_binding.binding);
    if (!elem_ptr) {
      /* Unbound resource. */
      BLI_assert_unreachable();
      return;
    }
    const BindSpaceTextures::Elem &elem = *elem_ptr;
    VKTexture *texture = static_cast<VKTexture *>(elem.resource);
    BLI_assert(texture);
    BLI_assert(elem.resource_type == BindSpaceTextures::Type::Texture);
    if (supports_dynamic_rendering) {
      const VKSampler &sampler = device.samplers().get(elem.sampler);
      bind_image(
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          sampler.vk_handle(),
          texture->image_view_get(resource_binding.arrayed, VKImageViewFlags::DEFAULT).vk_handle(),
          VK_IMAGE_LAYOUT_GENERAL,
          resource_binding.location);
      VkImage vk_image = texture->vk_image_handle();
      if (vk_image != VK_NULL_HANDLE) {
        access_info.images.append({vk_image,
                                   resource_binding.access_mask,
                                   to_vk_image_aspect_flag_bits(texture->device_format_get()),
                                   0,
                                   VK_REMAINING_ARRAY_LAYERS});
      }
    }
    else {
      /* Fall back to render-passes / sub-passes. */
      bind_image(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                 VK_NULL_HANDLE,
                 texture->image_view_get(resource_binding.arrayed, VKImageViewFlags::NO_SWIZZLING)
                     .vk_handle(),
                 VK_IMAGE_LAYOUT_GENERAL,
                 resource_binding.location);
      VkImage vk_image = texture->vk_image_handle();
      if (vk_image != VK_NULL_HANDLE) {
        access_info.images.append({vk_image,
                                   resource_binding.access_mask,
                                   to_vk_image_aspect_flag_bits(texture->device_format_get()),
                                   0,
                                   VK_REMAINING_ARRAY_LAYERS});
      }
    }
  }
}

void VKDescriptorSetUpdator::bind_storage_buffer_resource(
    const VKStateManager &state_manager,
    const VKResourceBinding &resource_binding,
    render_graph::VKResourceAccessInfo &access_info)
{
  const BindSpaceStorageBuffers::Elem &elem = state_manager.storage_buffers_.get(
      resource_binding.binding);
  VkBuffer vk_buffer = VK_NULL_HANDLE;
  VkDeviceSize vk_device_size = 0;
  VkDeviceAddress vk_device_address = 0;
  switch (elem.resource_type) {
    case BindSpaceStorageBuffers::Type::IndexBuffer: {
      VKIndexBuffer *index_buffer = static_cast<VKIndexBuffer *>(elem.resource);
      index_buffer->ensure_updated();
      vk_buffer = index_buffer->vk_handle();
      vk_device_size = index_buffer->size_get();
      vk_device_address = index_buffer->device_address_get();
      break;
    }
    case BindSpaceStorageBuffers::Type::VertexBuffer: {
      VKVertexBuffer *vertex_buffer = static_cast<VKVertexBuffer *>(elem.resource);
      vertex_buffer->ensure_updated();
      vk_buffer = vertex_buffer->vk_handle();
      vk_device_size = vertex_buffer->size_used_get();
      vk_device_address = vertex_buffer->device_address_get();
      break;
    }
    case BindSpaceStorageBuffers::Type::UniformBuffer: {
      VKUniformBuffer *uniform_buffer = static_cast<VKUniformBuffer *>(elem.resource);
      uniform_buffer->ensure_updated();
      vk_buffer = uniform_buffer->vk_handle();
      vk_device_size = uniform_buffer->size_in_bytes();
      vk_device_address = uniform_buffer->device_address_get();
      break;
    }
    case BindSpaceStorageBuffers::Type::StorageBuffer: {
      VKStorageBuffer *storage_buffer = static_cast<VKStorageBuffer *>(elem.resource);
      storage_buffer->ensure_allocated();
      vk_buffer = storage_buffer->vk_handle();
      vk_device_size = storage_buffer->size_in_bytes();
      vk_device_address = storage_buffer->device_address_get();
      break;
    }
    case BindSpaceStorageBuffers::Type::Buffer: {
      VKBuffer *buffer = static_cast<VKBuffer *>(elem.resource);
      vk_buffer = buffer->vk_handle();
      vk_device_size = buffer->size_in_bytes();
      vk_device_address = buffer->device_address_get();
      break;
    }
    case BindSpaceStorageBuffers::Type::Unused: {
      BLI_assert_unreachable();
    }
  }

  bind_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              vk_buffer,
              vk_device_address,
              elem.offset,
              vk_device_size - elem.offset,
              resource_binding.location);
  if (vk_buffer != VK_NULL_HANDLE) {
    access_info.buffers.append({vk_buffer, resource_binding.access_mask});
  }
}

void VKDescriptorSetUpdator::bind_uniform_buffer_resource(
    const VKStateManager &state_manager,
    const VKResourceBinding &resource_binding,
    render_graph::VKResourceAccessInfo &access_info)
{
  VKUniformBuffer &uniform_buffer = *state_manager.uniform_buffers_.get(resource_binding.binding);
  uniform_buffer.ensure_updated();
  bind_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              uniform_buffer.vk_handle(),
              uniform_buffer.device_address_get(),
              0,
              uniform_buffer.size_in_bytes(),
              resource_binding.location);
  access_info.buffers.append({uniform_buffer.vk_handle(), resource_binding.access_mask});
}

void VKDescriptorSetUpdator::bind_push_constants(VKPushConstants &push_constants,
                                                 render_graph::VKResourceAccessInfo &access_info)
{
  if (push_constants.layout_get().storage_type_get() !=
      VKPushConstants::StorageType::UNIFORM_BUFFER)
  {
    return;
  }
  push_constants.update_uniform_buffer();
  const VKUniformBuffer &uniform_buffer = *push_constants.uniform_buffer_get().get();
  bind_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              uniform_buffer.vk_handle(),
              uniform_buffer.device_address_get(),
              0,
              uniform_buffer.size_in_bytes(),
              push_constants.layout_get().descriptor_set_location_get());
  access_info.buffers.append({uniform_buffer.vk_handle(), VK_ACCESS_UNIFORM_READ_BIT});
}

void VKDescriptorSetUpdator::bind_shader_resources(const VKDevice &device,
                                                   const VKStateManager &state_manager,
                                                   VKShader &shader,
                                                   render_graph::VKResourceAccessInfo &access_info)
{
  const VKShaderInterface &shader_interface = shader.interface_get();
  for (const VKResourceBinding &resource_binding : shader_interface.resource_bindings_get()) {
    if (resource_binding.binding == -1) {
      continue;
    }

    switch (resource_binding.bind_type) {
      case VKBindType::UNIFORM_BUFFER:
        bind_uniform_buffer_resource(state_manager, resource_binding, access_info);
        break;

      case VKBindType::STORAGE_BUFFER:
        bind_storage_buffer_resource(state_manager, resource_binding, access_info);
        break;

      case VKBindType::SAMPLER:
        bind_texture_resource(device, state_manager, resource_binding, access_info);
        break;

      case VKBindType::IMAGE:
        bind_image_resource(state_manager, resource_binding, access_info);
        break;

      case VKBindType::INPUT_ATTACHMENT:
        bind_input_attachment_resource(device, state_manager, resource_binding, access_info);
        break;
    }
  }

  /* Bind uniform push constants to descriptor set. */
  bind_push_constants(shader.push_constants, access_info);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VKDescriptorSetPoolUpdator
 * \{ */

void VKDescriptorSetPoolUpdator::allocate_new_descriptor_set(
    VKDevice & /*device*/,
    VKContext &context,
    VKShader &shader,
    VkDescriptorSetLayout vk_descriptor_set_layout,
    render_graph::VKPipelineData &r_pipeline_data)
{
  /* Use descriptor pools/sets. */
  vk_descriptor_set = context.descriptor_pools_get().allocate(vk_descriptor_set_layout);
  BLI_assert(vk_descriptor_set != VK_NULL_HANDLE);
  debug::object_label(vk_descriptor_set, shader.name_get());
  r_pipeline_data.vk_descriptor_set = vk_descriptor_set;
}

void VKDescriptorSetPoolUpdator::bind_buffer(VkDescriptorType vk_descriptor_type,
                                             VkBuffer vk_buffer,
                                             VkDeviceAddress /*vk_device_address*/,
                                             VkDeviceSize buffer_offset,
                                             VkDeviceSize size_in_bytes,
                                             VKDescriptorSet::Location location)
{
  vk_descriptor_buffer_infos_.append({vk_buffer, buffer_offset, size_in_bytes});
  vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    vk_descriptor_set,
                                    location,
                                    0,
                                    1,
                                    vk_descriptor_type,
                                    nullptr,
                                    nullptr,
                                    nullptr});
}

void VKDescriptorSetPoolUpdator::bind_texel_buffer(VKVertexBuffer &vertex_buffer,
                                                   const VKDescriptorSet::Location location)
{
  vertex_buffer.ensure_buffer_view();
  vk_buffer_views_.append(vertex_buffer.vk_buffer_view_get());
  vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    vk_descriptor_set,
                                    location,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                    nullptr,
                                    nullptr,
                                    nullptr});
}

void VKDescriptorSetPoolUpdator::bind_image(VkDescriptorType vk_descriptor_type,
                                            VkSampler vk_sampler,
                                            VkImageView vk_image_view,
                                            VkImageLayout vk_image_layout,
                                            VKDescriptorSet::Location location)
{
  vk_descriptor_image_infos_.append({vk_sampler, vk_image_view, vk_image_layout});
  vk_write_descriptor_sets_.append({VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    vk_descriptor_set,
                                    location,
                                    0,
                                    1,
                                    vk_descriptor_type,
                                    nullptr,
                                    nullptr,
                                    nullptr});
}

void VKDescriptorSetPoolUpdator::upload_descriptor_sets()
{
  if (vk_write_descriptor_sets_.is_empty()) {
    return;
  }

  /* Finalize pointers that could have changed due to reallocations. */
  int buffer_index = 0;
  int buffer_view_index = 0;
  int image_index = 0;
  for (VkWriteDescriptorSet &vk_write_descriptor_set : vk_write_descriptor_sets_) {
    switch (vk_write_descriptor_set.descriptorType) {
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
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

#if 0
  /* Uncomment this for rebalancing VKDescriptorPools::POOL_SIZE_* */
  {
    int storage_buffer_count = 0;
    int storage_image_count = 0;
    int combined_image_sampler_count = 0;
    int uniform_buffer_count = 0;
    int uniform_texel_buffer_count = 0;
    int input_attachment_count = 0;
    Set<VkDescriptorSet> descriptor_set_count;

    for (VkWriteDescriptorSet &vk_write_descriptor_set : vk_write_descriptor_sets_) {
      descriptor_set_count.add(vk_write_descriptor_set.dstSet);
      switch (vk_write_descriptor_set.descriptorType) {
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
          combined_image_sampler_count += 1;
          break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          storage_image_count += 1;
          break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          uniform_texel_buffer_count += 1;
          break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          uniform_buffer_count += 1;
          break;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          storage_buffer_count += 1;
          break;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
          input_attachment_count += 1;
          break;
        default:
          BLI_assert_unreachable();
      }
    }
    std::cout << __func__ << ": "
              << "descriptor_set=" << descriptor_set_count.size()
              << ", combined_image_sampler=" << combined_image_sampler_count
              << ", storage_image=" << storage_image_count
              << ", uniform_texel_buffer=" << uniform_texel_buffer_count
              << ", uniform_buffer=" << uniform_buffer_count
              << ", storage_buffer=" << storage_buffer_count
              << ", input_attachment=" << input_attachment_count << "\n";
  }
#endif

  /* Update the descriptor set on the device. */
  const VKDevice &device = VKBackend::get().device;
  vkUpdateDescriptorSets(device.vk_handle(),
                         vk_write_descriptor_sets_.size(),
                         vk_write_descriptor_sets_.data(),
                         0,
                         nullptr);

  vk_descriptor_image_infos_.clear();
  vk_descriptor_buffer_infos_.clear();
  vk_buffer_views_.clear();
  vk_write_descriptor_sets_.clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VKDescriptorBufferUpdator
 * \{ */

void VKDescriptorBufferUpdator::allocate_new_descriptor_set(
    VKDevice &device,
    VKContext & /*context*/,
    VKShader & /*shader*/,
    VkDescriptorSetLayout vk_descriptor_set_layout,
    render_graph::VKPipelineData &r_pipeline_data)
{
  /* Use descriptor buffer. */
  descriptor_set_head = descriptor_set_tail;
  layout = device.descriptor_set_layouts_get().descriptor_buffer_layout_get(
      vk_descriptor_set_layout);

  /* Ensure if there is still place left in the current buffer. */
  if (buffers.is_empty() ||
      layout.size > buffers.last().get()->size_in_bytes() - descriptor_set_head)
  {
    const VkDeviceSize default_buffer_size = 8 * 1024 * 1024;
    buffers.append(std::make_unique<VKBuffer>());
    VKBuffer *buffer = buffers.last().get();
    buffer->create(default_buffer_size,
                   VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                       VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                   0,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    debug::object_label(buffer->vk_handle(), "DescriptorBuffer");
    descriptor_buffer_data = static_cast<uint8_t *>(buffer->mapped_memory_get());
    descriptor_buffer_device_address = buffer->device_address_get();
    descriptor_buffer_offset = 0;
    descriptor_set_head = 0;
    descriptor_set_tail = 0;
  }

  descriptor_set_tail = descriptor_set_head + layout.size;

  /* Update the current descriptor buffer and its offset to point to the active descriptor set. */
  descriptor_buffer_offset = descriptor_set_head;

  r_pipeline_data.descriptor_buffer_device_address = descriptor_buffer_device_address;
  r_pipeline_data.descriptor_buffer_offset = descriptor_buffer_offset;
}

void VKDescriptorBufferUpdator::bind_buffer(VkDescriptorType vk_descriptor_type,
                                            VkBuffer /*vk_buffer*/,
                                            VkDeviceAddress vk_device_address,
                                            VkDeviceSize buffer_offset,
                                            VkDeviceSize size_in_bytes,
                                            VKDescriptorSet::Location location)
{
  BLI_assert(vk_device_address != 0);
  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceDescriptorBufferPropertiesEXT &vk_descriptor_buffer_properties =
      device.physical_device_descriptor_buffer_properties_get();
  VkDescriptorAddressInfoEXT descriptor_address_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
      nullptr,
      vk_device_address + buffer_offset,
      size_in_bytes};

  VkDescriptorGetInfoEXT vk_descriptor_get_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT, nullptr, vk_descriptor_type};
  VkDeviceSize descriptor_size = 0;
  switch (vk_descriptor_type) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      vk_descriptor_get_info.data.pUniformBuffer = &descriptor_address_info;
      descriptor_size = vk_descriptor_buffer_properties.uniformBufferDescriptorSize;
      break;

    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      vk_descriptor_get_info.data.pStorageBuffer = &descriptor_address_info;
      descriptor_size = vk_descriptor_buffer_properties.storageBufferDescriptorSize;
      break;

    default:
      BLI_assert_unreachable();
  }

  uint8_t *descriptor_ptr = get_descriptor_binding_ptr(location);
  device.functions.vkGetDescriptor(
      device.vk_handle(), &vk_descriptor_get_info, descriptor_size, descriptor_ptr);
}

void VKDescriptorBufferUpdator::bind_texel_buffer(VKVertexBuffer &vertex_buffer,
                                                  const VKDescriptorSet::Location location)
{
  VkDeviceAddress vk_device_address = vertex_buffer.device_address_get();
  BLI_assert(vk_device_address != 0);
  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceDescriptorBufferPropertiesEXT &vk_descriptor_buffer_properties =
      device.physical_device_descriptor_buffer_properties_get();
  VkDescriptorAddressInfoEXT descriptor_address_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
      nullptr,
      vk_device_address,
      vertex_buffer.size_used_get(),
      vertex_buffer.to_vk_format()};

  VkDescriptorGetInfoEXT vk_descriptor_get_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT, nullptr, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER};
  vk_descriptor_get_info.data.pUniformTexelBuffer = &descriptor_address_info;
  VkDeviceSize descriptor_size = vk_descriptor_buffer_properties.uniformTexelBufferDescriptorSize;

  uint8_t *descriptor_ptr = get_descriptor_binding_ptr(location);
  device.functions.vkGetDescriptor(
      device.vk_handle(), &vk_descriptor_get_info, descriptor_size, descriptor_ptr);
}

void VKDescriptorBufferUpdator::bind_image(VkDescriptorType vk_descriptor_type,
                                           VkSampler vk_sampler,
                                           VkImageView vk_image_view,
                                           VkImageLayout vk_image_layout,
                                           VKDescriptorSet::Location location)
{
  VKDevice &device = VKBackend::get().device;
  const VkPhysicalDeviceDescriptorBufferPropertiesEXT &vk_descriptor_buffer_properties =
      device.physical_device_descriptor_buffer_properties_get();
  VkDescriptorImageInfo vk_descriptor_image_info = {vk_sampler, vk_image_view, vk_image_layout};
  VkDescriptorGetInfoEXT vk_descriptor_get_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT, nullptr, vk_descriptor_type};
  VkDeviceSize descriptor_size = 0;
  switch (vk_descriptor_type) {
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      vk_descriptor_get_info.data.pCombinedImageSampler = &vk_descriptor_image_info;
      descriptor_size = vk_descriptor_buffer_properties.combinedImageSamplerDescriptorSize;
      break;

    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      vk_descriptor_get_info.data.pStorageImage = &vk_descriptor_image_info;
      descriptor_size = vk_descriptor_buffer_properties.storageImageDescriptorSize;
      break;

    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
      vk_descriptor_get_info.data.pInputAttachmentImage = &vk_descriptor_image_info;
      descriptor_size = vk_descriptor_buffer_properties.inputAttachmentDescriptorSize;
      break;

    default:
      BLI_assert_unreachable();
  }

  uint8_t *descriptor_ptr = get_descriptor_binding_ptr(location);
  device.functions.vkGetDescriptor(
      device.vk_handle(), &vk_descriptor_get_info, descriptor_size, descriptor_ptr);
}

void VKDescriptorBufferUpdator::upload_descriptor_sets()
{
  /* Buffers have already been updated. only need to discard the buffers. */
  buffers.clear();
  descriptor_buffer_data = nullptr;
  descriptor_buffer_device_address = 0;
  descriptor_buffer_offset = 0;
}

/** \} */

}  // namespace blender::gpu
