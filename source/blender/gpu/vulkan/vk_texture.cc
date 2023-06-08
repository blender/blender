/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include "vk_vertex_buffer.hh"

#include "BLI_math_vector.hh"

#include "BKE_global.h"

namespace blender::gpu {

VKTexture::~VKTexture()
{
  if (is_allocated()) {
    const VKDevice &device = VKBackend::get().device_get();
    vmaDestroyImage(device.mem_allocator_get(), vk_image_, allocation_);
  }
}

void VKTexture::init(VkImage vk_image, VkImageLayout layout)
{
  vk_image_ = vk_image;
  current_layout_ = layout;
}

void VKTexture::generate_mipmap()
{
  if (mipmaps_ <= 1) {
    return;
  }

  ensure_allocated();

  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  for (int src_mipmap : IndexRange(mipmaps_ - 1)) {
    int dst_mipmap = src_mipmap + 1;
    int3 src_size(1);
    int3 dst_size(1);
    mip_size_get(src_mipmap, src_size);
    mip_size_get(dst_mipmap, dst_size);

    layout_ensure(context,
                  IndexRange(src_mipmap, 1),
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageBlit image_blit = {};
    image_blit.srcOffsets[0] = {0, 0, 0};
    image_blit.srcOffsets[1] = {src_size.x, src_size.y, src_size.z};
    image_blit.srcSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
    image_blit.srcSubresource.mipLevel = src_mipmap;
    image_blit.srcSubresource.baseArrayLayer = 0;
    image_blit.srcSubresource.layerCount = layer_count();

    image_blit.dstOffsets[0] = {0, 0, 0};
    image_blit.dstOffsets[1] = {dst_size.x, dst_size.y, dst_size.z};
    image_blit.dstSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
    image_blit.dstSubresource.mipLevel = dst_mipmap;
    image_blit.dstSubresource.baseArrayLayer = 0;
    image_blit.dstSubresource.layerCount = layer_count();

    command_buffer.blit(*this,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        *this,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        Span<VkImageBlit>(&image_blit, 1));
    /* TODO: Until we do actual command encoding we need to submit each transfer operation
     * individually. */
    command_buffer.submit();
  }
  /* Ensure that all mipmap levels are in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`. All miplevels are
   * except the last one. */
  layout_ensure(context,
                IndexRange(mipmaps_ - 1, 1),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  current_layout_set(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

void VKTexture::copy_to(Texture *tex)
{
  VKTexture *dst = unwrap(tex);
  VKTexture *src = this;
  BLI_assert(dst);
  BLI_assert(src->w_ == dst->w_ && src->h_ == dst->h_ && src->d_ == dst->d_);
  BLI_assert(src->format_ == dst->format_);
  UNUSED_VARS_NDEBUG(src);

  VKContext &context = *VKContext::get();
  ensure_allocated();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  dst->ensure_allocated();
  dst->layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkImageCopy region = {};
  region.srcSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.srcSubresource.mipLevel = 0;
  region.srcSubresource.layerCount = 1;
  region.dstSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.dstSubresource.mipLevel = 0;
  region.dstSubresource.layerCount = 1;
  region.extent.width = w_;
  region.extent.height = max_ii(h_, 1);
  region.extent.depth = max_ii(d_, 1);

  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(*dst, *this, Span<VkImageCopy>(&region, 1));
  command_buffer.submit();
}

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

void VKTexture::swizzle_set(const char /*swizzle_mask*/[4])
{
  NOT_YET_IMPLEMENTED;
}

void VKTexture::mip_range_set(int min, int max)
{
  mip_min_ = min;
  mip_max_ = max;

  flags_ |= IMAGE_VIEW_DIRTY;
}

void VKTexture::read_sub(int mip, eGPUDataFormat format, const int area[4], void *r_data)
{
  VKContext &context = *VKContext::get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKBuffer staging_buffer;

  size_t sample_len = area[2] * area[3];
  size_t device_memory_size = sample_len * to_bytesize(format_);

  staging_buffer.create(
      device_memory_size, GPU_USAGE_DEVICE_ONLY, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  VkBufferImageCopy region = {};
  region.imageOffset.x = area[0];
  region.imageOffset.y = area[1];
  region.imageExtent.width = area[2];
  region.imageExtent.height = area[3];
  region.imageExtent.depth = 1;
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.layerCount = 1;

  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(staging_buffer, *this, Span<VkBufferImageCopy>(&region, 1));
  command_buffer.submit();

  convert_device_to_host(r_data, staging_buffer.mapped_memory_get(), sample_len, format, format_);
}

void *VKTexture::read(int mip, eGPUDataFormat format)
{
  int mip_size[3] = {1, 1, 1};
  mip_size_get(mip, mip_size);
  size_t sample_len = mip_size[0] * mip_size[1];
  size_t host_memory_size = sample_len * to_bytesize(format_, format);

  void *data = MEM_mallocN(host_memory_size, __func__);
  int area[4] = {0, 0, mip_size[0], mip_size[1]};
  read_sub(mip, format, area, data);
  return data;
}

void VKTexture::update_sub(
    int mip, int offset[3], int extent_[3], eGPUDataFormat format, const void *data)
{
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
  NOT_YET_IMPLEMENTED;
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

  const VKDevice &device = VKBackend::get().device_get();
  const VKWorkarounds &workarounds = device.workarounds_get();
  if (format_ == GPU_DEPTH_COMPONENT24 && workarounds.not_aligned_pixel_formats) {
    format_ = GPU_DEPTH_COMPONENT32F;
  }
  if (format_ == GPU_DEPTH24_STENCIL8 && workarounds.not_aligned_pixel_formats) {
    format_ = GPU_DEPTH32F_STENCIL8;
  }

  /* TODO: return false when texture format isn't supported. */
  return true;
}

bool VKTexture::init_internal(GPUVertBuf *vbo)
{
  if (!allocate()) {
    return false;
  }

  VKVertexBuffer *vertex_buffer = unwrap(unwrap(vbo));

  VkBufferImageCopy region = {};
  region.imageExtent.width = w_;
  region.imageExtent.height = 1;
  region.imageExtent.depth = 1;
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(format_);
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.layerCount = 1;

  VKContext &context = *VKContext::get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.copy(*this, vertex_buffer->buffer_, Span<VkBufferImageCopy>(&region, 1));
  command_buffer.submit();

  return true;
}

bool VKTexture::init_internal(GPUTexture * /*src*/,
                              int /*mip_offset*/,
                              int /*layer_offset*/,
                              bool /*use_stencil*/)
{
  NOT_YET_IMPLEMENTED;
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
  image_info.mipLevels = max_ii(mipmaps_, 1);
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
  BLI_assert(is_allocated());
  const VkImageLayout current_layout = current_layout_get();
  if (current_layout == requested_layout) {
    return;
  }
  layout_ensure(context, IndexRange(0, VK_REMAINING_MIP_LEVELS), current_layout, requested_layout);
  current_layout_set(requested_layout);
}

void VKTexture::layout_ensure(VKContext &context,
                              const IndexRange mipmap_range,
                              const VkImageLayout current_layout,
                              const VkImageLayout requested_layout)
{
  BLI_assert(is_allocated());
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = current_layout;
  barrier.newLayout = requested_layout;
  barrier.image = vk_image_;
  barrier.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(format_);
  barrier.subresourceRange.baseMipLevel = uint32_t(mipmap_range.start());
  barrier.subresourceRange.levelCount = uint32_t(mipmap_range.size());
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  context.command_buffer_get().pipeline_barrier(Span<VkImageMemoryBarrier>(&barrier, 1));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Views
 * \{ */

void VKTexture::image_view_ensure()
{
  if (flags_ & IMAGE_VIEW_DIRTY) {
    image_view_update();
    flags_ &= ~IMAGE_VIEW_DIRTY;
  }
}

void VKTexture::image_view_update()
{
  VK_ALLOCATION_CALLBACKS
  VkImageViewCreateInfo image_view_info = {};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = vk_image_;
  image_view_info.viewType = to_vk_image_view_type(type_);
  image_view_info.format = to_vk_format(format_);
  image_view_info.components = to_vk_component_mapping(format_);
  image_view_info.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(format_);
  IndexRange mip_range = mip_map_range();
  image_view_info.subresourceRange.baseMipLevel = mip_range.first();
  image_view_info.subresourceRange.levelCount = mip_range.size();
  image_view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  const VKDevice &device = VKBackend::get().device_get();
  VkImageView image_view = VK_NULL_HANDLE;
  vkCreateImageView(device.device_get(), &image_view_info, vk_allocation_callbacks, &image_view);
  debug::object_label(image_view, name_);
  image_view_.emplace(image_view);
}

IndexRange VKTexture::mip_map_range() const
{
  return IndexRange(mip_min_, mip_max_ - mip_min_ + 1);
}

/** \} */

}  // namespace blender::gpu
