/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_texture.hh"

#include "vk_buffer.hh"
#include "vk_context.hh"
#include "vk_data_conversion.hh"
#include "vk_memory.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"

#include "BLI_math_vector.hh"

#include "BKE_global.h"

namespace blender::gpu {

VKTexture::~VKTexture()
{
  VK_ALLOCATION_CALLBACKS
  if (is_allocated()) {
    const VKDevice &device = VKBackend::get().device_get();
    vmaDestroyImage(device.mem_allocator_get(), vk_image_, allocation_);
    vkDestroyImageView(device.device_get(), vk_image_view_, vk_allocation_callbacks);
  }
}

void VKTexture::init(VkImage vk_image, VkImageLayout layout)
{
  vk_image_ = vk_image;
  current_layout_ = layout;
}

void VKTexture::generate_mipmap() {}

void VKTexture::copy_to(Texture * /*tex*/) {}

void VKTexture::clear(eGPUDataFormat format, const void *data)
{
  if (!is_allocated()) {
    allocate();
  }

  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  VkClearColorValue clear_color = to_vk_clear_color_value(format, data);
  VkImageSubresourceRange range = {0};
  range.aspectMask = to_vk_image_aspect_flag_bits(format_);
  range.levelCount = VK_REMAINING_MIP_LEVELS;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;
  layout_ensure(context, VK_IMAGE_LAYOUT_GENERAL);

  command_buffer.clear(
      vk_image_, current_layout_get(), clear_color, Span<VkImageSubresourceRange>(&range, 1));
}

void VKTexture::swizzle_set(const char /*swizzle_mask*/[4]) {}

void VKTexture::mip_range_set(int /*min*/, int /*max*/) {}

void *VKTexture::read(int mip, eGPUDataFormat format)
{
  VKContext &context = *VKContext::get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKBuffer staging_buffer;

  /* NOTE: mip_size_get() won't override any dimension that is equal to 0. */
  int extent[3] = {1, 1, 1};
  mip_size_get(mip, extent);
  size_t sample_len = extent[0] * extent[1] * extent[2];
  size_t device_memory_size = sample_len * to_bytesize(format_);
  size_t host_memory_size = sample_len * to_bytesize(format_, format);

  staging_buffer.create(
      device_memory_size, GPU_USAGE_DEVICE_ONLY, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  VkBufferImageCopy region = {};
  region.imageExtent.width = extent[0];
  region.imageExtent.height = extent[1];
  region.imageExtent.depth = extent[2];
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.layerCount = 1;

  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(staging_buffer, *this, Span<VkBufferImageCopy>(&region, 1));
  command_buffer.submit();

  void *data = MEM_mallocN(host_memory_size, __func__);
  convert_device_to_host(data, staging_buffer.mapped_memory_get(), sample_len, format, format_);
  return data;
}

void VKTexture::update_sub(
    int mip, int offset[3], int extent_[3], eGPUDataFormat format, const void *data)
{
  if (mip != 0) {
    /* TODO: not implemented yet. */
    return;
  }
  if (!is_allocated()) {
    allocate();
  }

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKContext &context = *VKContext::get();
  VKBuffer staging_buffer;
  int3 extent = int3(extent_[0], max_ii(extent_[1], 1), max_ii(extent_[2], 1));
  size_t sample_len = extent.x * extent.y * extent.z;
  size_t device_memory_size = sample_len * to_bytesize(format_);

  staging_buffer.create(
      device_memory_size, GPU_USAGE_DEVICE_ONLY, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  uint buffer_row_length = context.state_manager_get().texture_unpack_row_length_get();
  if (buffer_row_length) {
    /* Use custom row length #GPU_texture_unpack_row_length */
    convert_host_to_device(staging_buffer.mapped_memory_get(),
                           data,
                           uint2(extent),
                           buffer_row_length,
                           format,
                           format_);
  }
  else {
    convert_host_to_device(staging_buffer.mapped_memory_get(), data, sample_len, format, format_);
  }

  VkBufferImageCopy region = {};
  region.imageExtent.width = extent.x;
  region.imageExtent.height = extent.y;
  region.imageExtent.depth = extent.z;
  region.imageOffset.x = offset[0];
  region.imageOffset.y = offset[1];
  region.imageOffset.z = offset[2];
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.layerCount = 1;

  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(*this, staging_buffer, Span<VkBufferImageCopy>(&region, 1));
  command_buffer.submit();
}

void VKTexture::update_sub(int /*offset*/[3],
                           int /*extent*/[3],
                           eGPUDataFormat /*format*/,
                           GPUPixelBuffer * /*pixbuf*/)
{
}

/* TODO(fclem): Legacy. Should be removed at some point. */
uint VKTexture::gl_bindcode_get() const
{
  return 0;
}

bool VKTexture::init_internal()
{
  /* Initialization can only happen after the usage is known. By the current API this isn't set
   * at this moment, so we cannot initialize here. The initialization is postponed until the
   * allocation of the texture on the device. */

  /* TODO: return false when texture format isn't supported. */
  return true;
}

bool VKTexture::init_internal(GPUVertBuf * /*vbo*/)
{
  return false;
}

bool VKTexture::init_internal(GPUTexture * /*src*/,
                              int /*mip_offset*/,
                              int /*layer_offset*/,
                              bool /*use_stencil*/)
{
  return false;
}

void VKTexture::ensure_allocated()
{
  if (!is_allocated()) {
    allocate();
  }
}

bool VKTexture::is_allocated() const
{
  return vk_image_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE;
}

static VkImageUsageFlagBits to_vk_image_usage(const eGPUTextureUsage usage,
                                              const eGPUTextureFormatFlag format_flag)
{
  VkImageUsageFlagBits result = static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                                  VK_IMAGE_USAGE_SAMPLED_BIT);
  if (usage & GPU_TEXTURE_USAGE_SHADER_READ) {
    result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_STORAGE_BIT);
  }
  if (usage & GPU_TEXTURE_USAGE_SHADER_WRITE) {
    result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_STORAGE_BIT);
  }
  if (usage & GPU_TEXTURE_USAGE_ATTACHMENT) {
    if (format_flag & GPU_FORMAT_COMPRESSED) {
      /* These formats aren't supported as an attachment. When using GPU_TEXTURE_USAGE_DEFAULT they
       * are still being evaluated to be attachable. So we need to skip them. */
    }
    else {
      if (format_flag & (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL)) {
        result = static_cast<VkImageUsageFlagBits>(result |
                                                   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
      }
      else {
        result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
      }
    }
  }
  if (usage & GPU_TEXTURE_USAGE_HOST_READ) {
    result = static_cast<VkImageUsageFlagBits>(result | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  }

  /* Disable some usages based on the given format flag to support more devices. */
  if (format_flag & GPU_FORMAT_SRGB) {
    /* NVIDIA devices don't create SRGB textures when it storage bit is set. */
    result = static_cast<VkImageUsageFlagBits>(result & ~VK_IMAGE_USAGE_STORAGE_BIT);
  }
  if (format_flag & (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL)) {
    /* NVIDIA devices don't create depth textures when it storage bit is set. */
    result = static_cast<VkImageUsageFlagBits>(result & ~VK_IMAGE_USAGE_STORAGE_BIT);
  }

  return result;
}

bool VKTexture::allocate()
{
  BLI_assert(vk_image_ == VK_NULL_HANDLE);
  BLI_assert(!is_allocated());

  int extent[3] = {1, 1, 1};
  mip_size_get(0, extent);

  VKContext &context = *VKContext::get();
  const VKDevice &device = VKBackend::get().device_get();
  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = to_vk_image_type(type_);
  image_info.extent.width = extent[0];
  image_info.extent.height = extent[1];
  image_info.extent.depth = extent[2];
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = to_vk_format(format_);
  /* Some platforms (NVIDIA) requires that attached textures are always tiled optimal.
   *
   * As image data are always accessed via an staging buffer we can enable optimal tiling for all
   * texture. Tilings based on actual usages should be done in `VKFramebuffer`.
   */
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = to_vk_image_usage(gpu_image_usage_flags_, format_flag_);
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  VkResult result;
  if (G.debug & G_DEBUG_GPU) {
    VkImageFormatProperties image_format = {};
    result = vkGetPhysicalDeviceImageFormatProperties(device.physical_device_get(),
                                                      image_info.format,
                                                      image_info.imageType,
                                                      image_info.tiling,
                                                      image_info.usage,
                                                      image_info.flags,
                                                      &image_format);
    if (result != VK_SUCCESS) {
      printf("Image type not supported on device.\n");
      return false;
    }
  }

  VmaAllocationCreateInfo allocCreateInfo = {};
  allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
  allocCreateInfo.priority = 1.0f;
  result = vmaCreateImage(device.mem_allocator_get(),
                          &image_info,
                          &allocCreateInfo,
                          &vk_image_,
                          &allocation_,
                          nullptr);
  if (result != VK_SUCCESS) {
    return false;
  }
  debug::object_label(vk_image_, name_);

  /* Promote image to the correct layout. */
  layout_ensure(context, VK_IMAGE_LAYOUT_GENERAL);

  VK_ALLOCATION_CALLBACKS
  VkImageViewCreateInfo image_view_info = {};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = vk_image_;
  image_view_info.viewType = to_vk_image_view_type(type_);
  image_view_info.format = to_vk_format(format_);
  image_view_info.components = to_vk_component_mapping(format_);
  image_view_info.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(format_);
  image_view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  image_view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  result = vkCreateImageView(
      device.device_get(), &image_view_info, vk_allocation_callbacks, &vk_image_view_);
  debug::object_label(vk_image_view_, name_);
  return result == VK_SUCCESS;
}

// TODO: move texture/image bindings to shader.
void VKTexture::bind(int unit, VKSampler &sampler)
{
  if (!is_allocated()) {
    allocate();
  }
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(
          shader::ShaderCreateInfo::Resource::BindType::SAMPLER, unit);
  if (location) {
    VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
    descriptor_set.bind(*this, *location, sampler);
  }
}

void VKTexture::image_bind(int binding)
{
  if (!is_allocated()) {
    allocate();
  }
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(shader::ShaderCreateInfo::Resource::BindType::IMAGE,
                                               binding);
  if (location) {
    VKDescriptorSetTracker &descriptor_set = shader->pipeline_get().descriptor_set_get();
    descriptor_set.image_bind(*this, *location);
  }
}

/* -------------------------------------------------------------------- */
/** \name Image Layout
 * \{ */

VkImageLayout VKTexture::current_layout_get() const
{
  return current_layout_;
}

void VKTexture::current_layout_set(const VkImageLayout new_layout)
{
  current_layout_ = new_layout;
}

void VKTexture::layout_ensure(VKContext &context, const VkImageLayout requested_layout)
{
  const VkImageLayout current_layout = current_layout_get();
  if (current_layout == requested_layout) {
    return;
  }
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = current_layout;
  barrier.newLayout = requested_layout;
  barrier.image = vk_image_;
  barrier.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(format_);
  barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  context.command_buffer_get().pipeline_barrier(Span<VkImageMemoryBarrier>(&barrier, 1));
  current_layout_set(requested_layout);
}
/** \} */

}  // namespace blender::gpu
