/* SPDX-FileCopyrightText: 2022 Blender Authors
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

static VkImageAspectFlags to_vk_image_aspect_single_bit(const VkImageAspectFlags format,
                                                        bool stencil)
{
  switch (format) {
    case VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT:
      return (stencil) ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    default:
      break;
  }
  return format;
}

VKTexture::~VKTexture()
{
  if (vk_image_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE) {
    VKDevice &device = VKBackend::get().device_get();
    device.discard_image(vk_image_, allocation_);

    vk_image_ = VK_NULL_HANDLE;
    allocation_ = VK_NULL_HANDLE;
  }
}

void VKTexture::init(VkImage vk_image, VkImageLayout layout, eGPUTextureFormat texture_format)
{
  vk_image_ = vk_image;
  current_layout_ = layout;
  format_ = texture_format;
  device_format_ = texture_format;
}

void VKTexture::generate_mipmap()
{
  BLI_assert(!is_texture_view());
  if (mipmaps_ <= 1) {
    return;
  }

  VKContext &context = *VKContext::get();
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.submit();

  layout_ensure(context,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_ACCESS_MEMORY_WRITE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_READ_BIT);

  for (int src_mipmap : IndexRange(mipmaps_ - 1)) {
    int dst_mipmap = src_mipmap + 1;
    int3 src_size(1);
    int3 dst_size(1);
    mip_size_get(src_mipmap, src_size);
    mip_size_get(dst_mipmap, dst_size);

    /* GPU Texture stores the array length in the first unused dimension size.
     * Vulkan uses layers and the array length should be removed from the dimensions. */
    if (ELEM(this->type_get(), GPU_TEXTURE_1D_ARRAY)) {
      src_size.y = 1;
      src_size.z = 1;
      dst_size.y = 1;
      dst_size.z = 1;
    }
    if (ELEM(this->type_get(), GPU_TEXTURE_2D_ARRAY)) {
      src_size.z = 1;
      dst_size.z = 1;
    }

    layout_ensure(context,
                  IndexRange(src_mipmap, 1),
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_ACCESS_TRANSFER_READ_BIT);

    VkImageBlit image_blit = {};
    image_blit.srcOffsets[0] = {0, 0, 0};
    image_blit.srcOffsets[1] = {src_size.x, src_size.y, src_size.z};
    image_blit.srcSubresource.aspectMask = to_vk_image_aspect_flag_bits(device_format_);
    image_blit.srcSubresource.mipLevel = src_mipmap;
    image_blit.srcSubresource.baseArrayLayer = 0;
    image_blit.srcSubresource.layerCount = vk_layer_count(1);

    image_blit.dstOffsets[0] = {0, 0, 0};
    image_blit.dstOffsets[1] = {dst_size.x, dst_size.y, dst_size.z};
    image_blit.dstSubresource.aspectMask = to_vk_image_aspect_flag_bits(device_format_);
    image_blit.dstSubresource.mipLevel = dst_mipmap;
    image_blit.dstSubresource.baseArrayLayer = 0;
    image_blit.dstSubresource.layerCount = vk_layer_count(1);

    command_buffers.blit(*this,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         *this,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         Span<VkImageBlit>(&image_blit, 1));
  }

  /* Ensure that all mipmap levels are in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`. */
  layout_ensure(context,
                IndexRange(mipmaps_ - 1, 1),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_ACCESS_MEMORY_READ_BIT);
  current_layout_set(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

void VKTexture::copy_to(VKTexture &dst_texture, VkImageAspectFlags vk_image_aspect)
{
  VKContext &context = *VKContext::get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  dst_texture.layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkImageCopy region = {};
  region.srcSubresource.aspectMask = vk_image_aspect;
  region.srcSubresource.mipLevel = 0;
  region.srcSubresource.layerCount = vk_layer_count(1);
  region.dstSubresource.aspectMask = vk_image_aspect;
  region.dstSubresource.mipLevel = 0;
  region.dstSubresource.layerCount = vk_layer_count(1);
  region.extent = vk_extent_3d(0);

  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.copy(dst_texture, *this, Span<VkImageCopy>(&region, 1));
  context.flush();
}

void VKTexture::copy_to(Texture *tex)
{
  VKTexture *dst = unwrap(tex);
  VKTexture *src = this;
  BLI_assert(dst);
  BLI_assert(src->w_ == dst->w_ && src->h_ == dst->h_ && src->d_ == dst->d_);
  BLI_assert(src->device_format_ == dst->device_format_);
  BLI_assert(!is_texture_view());
  UNUSED_VARS_NDEBUG(src);

  copy_to(*dst, to_vk_image_aspect_flag_bits(device_format_));
}

void VKTexture::clear(eGPUDataFormat format, const void *data)
{
  BLI_assert(!is_texture_view());

  VKContext &context = *VKContext::get();
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  VkClearColorValue clear_color = to_vk_clear_color_value(format, data);
  VkImageSubresourceRange range = {0};
  range.aspectMask = to_vk_image_aspect_flag_bits(device_format_);
  range.levelCount = VK_REMAINING_MIP_LEVELS;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;
  layout_ensure(context, VK_IMAGE_LAYOUT_GENERAL);

  command_buffers.clear(
      vk_image_, current_layout_get(), clear_color, Span<VkImageSubresourceRange>(&range, 1));
}

void VKTexture::clear_depth_stencil(const eGPUFrameBufferBits buffers,
                                    float clear_depth,
                                    uint clear_stencil)
{
  BLI_assert(buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT));

  VKContext &context = *VKContext::get();
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  VkClearDepthStencilValue clear_depth_stencil;
  clear_depth_stencil.depth = clear_depth;
  clear_depth_stencil.stencil = clear_stencil;
  VkImageSubresourceRange range = {0};
  range.aspectMask = to_vk_image_aspect_flag_bits(buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT));
  range.levelCount = VK_REMAINING_MIP_LEVELS;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;

  layout_ensure(context, VK_IMAGE_LAYOUT_GENERAL);
  command_buffers.clear(vk_image_,
                        current_layout_get(),
                        clear_depth_stencil,
                        Span<VkImageSubresourceRange>(&range, 1));
}

void VKTexture::swizzle_set(const char swizzle_mask[4])
{
  vk_component_mapping_.r = to_vk_component_swizzle(swizzle_mask[0]);
  vk_component_mapping_.g = to_vk_component_swizzle(swizzle_mask[1]);
  vk_component_mapping_.b = to_vk_component_swizzle(swizzle_mask[2]);
  vk_component_mapping_.a = to_vk_component_swizzle(swizzle_mask[3]);

  flags_ |= IMAGE_VIEW_DIRTY;
}

void VKTexture::mip_range_set(int min, int max)
{
  mip_min_ = min;
  mip_max_ = max;

  flags_ |= IMAGE_VIEW_DIRTY;
}

void VKTexture::read_sub(
    int mip, eGPUDataFormat format, const int region[6], const IndexRange layers, void *r_data)
{
  VKContext &context = *VKContext::get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKBuffer staging_buffer;

  size_t sample_len = (region[5] - region[2]) * (region[3] - region[0]) * (region[4] - region[1]) *
                      layers.size();
  size_t device_memory_size = sample_len * to_bytesize(device_format_);

  staging_buffer.create(device_memory_size, GPU_USAGE_DYNAMIC, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  VkBufferImageCopy buffer_image_copy = {};
  buffer_image_copy.imageOffset.x = region[0];
  buffer_image_copy.imageOffset.y = region[1];
  buffer_image_copy.imageOffset.z = region[2];
  buffer_image_copy.imageExtent.width = region[3];
  buffer_image_copy.imageExtent.height = region[4];
  buffer_image_copy.imageExtent.depth = region[5];
  buffer_image_copy.imageSubresource.aspectMask = to_vk_image_aspect_single_bit(
      to_vk_image_aspect_flag_bits(device_format_), false);
  buffer_image_copy.imageSubresource.mipLevel = mip;
  buffer_image_copy.imageSubresource.baseArrayLayer = layers.start();
  buffer_image_copy.imageSubresource.layerCount = layers.size();

  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.copy(staging_buffer, *this, Span<VkBufferImageCopy>(&buffer_image_copy, 1));
  context.flush();

  convert_device_to_host(
      r_data, staging_buffer.mapped_memory_get(), sample_len, format, format_, device_format_);
}

void *VKTexture::read(int mip, eGPUDataFormat format)
{
  int mip_size[3] = {1, 1, 1};
  VkImageType vk_image_type = to_vk_image_type(type_);
  mip_size_get(mip, mip_size);
  switch (vk_image_type) {
    case VK_IMAGE_TYPE_1D: {
      mip_size[1] = 1;
      mip_size[2] = 1;
    } break;
    case VK_IMAGE_TYPE_2D: {
      mip_size[2] = 1;
    } break;
    case VK_IMAGE_TYPE_3D:
    default:
      break;
  }

  if (mip_size[2] == 0) {
    mip_size[2] = 1;
  }
  IndexRange layers = IndexRange(layer_offset_, vk_layer_count(1));
  size_t sample_len = mip_size[0] * mip_size[1] * mip_size[2] * layers.size();
  size_t host_memory_size = sample_len * to_bytesize(format_, format);

  void *data = MEM_mallocN(host_memory_size, __func__);
  int region[6] = {0, 0, 0, mip_size[0], mip_size[1], mip_size[2]};
  read_sub(mip, format, region, layers, data);
  return data;
}

void VKTexture::update_sub(
    int mip, int offset[3], int extent_[3], eGPUDataFormat format, const void *data)
{
  BLI_assert(!is_texture_view());

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKContext &context = *VKContext::get();
  int layers = vk_layer_count(1);
  int3 extent = int3(extent_[0], max_ii(extent_[1], 1), max_ii(extent_[2], 1));
  size_t sample_len = extent.x * extent.y * extent.z;
  size_t device_memory_size = sample_len * to_bytesize(device_format_);

  if (type_ & GPU_TEXTURE_1D) {
    extent.y = 1;
    extent.z = 1;
  }
  if (type_ & (GPU_TEXTURE_2D | GPU_TEXTURE_CUBE)) {
    extent.z = 1;
  }

  VKBuffer staging_buffer;
  staging_buffer.create(device_memory_size, GPU_USAGE_DYNAMIC, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  convert_host_to_device(
      staging_buffer.mapped_memory_get(), data, sample_len, format, format_, device_format_);

  VkBufferImageCopy region = {};
  region.imageExtent.width = extent.x;
  region.imageExtent.height = extent.y;
  region.imageExtent.depth = extent.z;
  region.bufferRowLength = context.state_manager_get().texture_unpack_row_length_get();
  region.imageOffset.x = offset[0];
  region.imageOffset.y = offset[1];
  region.imageOffset.z = offset[2];
  region.imageSubresource.aspectMask = to_vk_image_aspect_single_bit(
      to_vk_image_aspect_flag_bits(device_format_), false);
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.layerCount = layers;

  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.copy(*this, staging_buffer, Span<VkBufferImageCopy>(&region, 1));
  context.flush();
}

void VKTexture::update_sub(int /*offset*/[3],
                           int /*extent*/[3],
                           eGPUDataFormat /*format*/,
                           GPUPixelBuffer * /*pixbuf*/)
{
  BLI_assert(!is_texture_view());
  NOT_YET_IMPLEMENTED;
}

/* TODO(fclem): Legacy. Should be removed at some point. */
uint VKTexture::gl_bindcode_get() const
{
  return 0;
}

bool VKTexture::init_internal()
{
  const VKDevice &device = VKBackend::get().device_get();
  const VKWorkarounds &workarounds = device.workarounds_get();
  device_format_ = format_;
  if (device_format_ == GPU_DEPTH_COMPONENT24 && workarounds.not_aligned_pixel_formats) {
    device_format_ = GPU_DEPTH_COMPONENT32F;
  }
  if (device_format_ == GPU_DEPTH24_STENCIL8 && workarounds.not_aligned_pixel_formats) {
    device_format_ = GPU_DEPTH32F_STENCIL8;
  }
  /* R16G16F16 formats are typically not supported (<1%) but R16G16B16A16 is
   * typically supported (+90%). */
  if (device_format_ == GPU_RGB16F) {
    device_format_ = GPU_RGBA16F;
  }
  if (device_format_ == GPU_RGB32F) {
    device_format_ = GPU_RGBA32F;
  }

  if (!allocate()) {
    return false;
  }

  return true;
}

bool VKTexture::init_internal(GPUVertBuf *vbo)
{
  device_format_ = format_;
  if (!allocate()) {
    return false;
  }

  VKVertexBuffer *vertex_buffer = unwrap(unwrap(vbo));

  VkBufferImageCopy region = {};
  region.imageExtent.width = w_;
  region.imageExtent.height = 1;
  region.imageExtent.depth = 1;
  region.imageSubresource.aspectMask = to_vk_image_aspect_flag_bits(device_format_);
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.layerCount = 1;

  VKContext &context = *VKContext::get();
  layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.copy(*this, vertex_buffer->buffer_, Span<VkBufferImageCopy>(&region, 1));
  context.flush();

  return true;
}

bool VKTexture::init_internal(GPUTexture *src, int mip_offset, int layer_offset, bool use_stencil)
{
  BLI_assert(source_texture_ == nullptr);
  BLI_assert(src);

  VKTexture *texture = unwrap(unwrap(src));
  source_texture_ = texture;
  device_format_ = texture->device_format_;
  mip_min_ = mip_offset;
  mip_max_ = mip_offset;
  layer_offset_ = layer_offset;
  use_stencil_ = use_stencil;
  flags_ |= IMAGE_VIEW_DIRTY;

  return true;
}

bool VKTexture::is_texture_view() const
{
  return source_texture_ != nullptr;
}

static VkImageUsageFlags to_vk_image_usage(const eGPUTextureUsage usage,
                                           const eGPUTextureFormatFlag format_flag)
{
  VkImageUsageFlags result = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT;
  if (usage & GPU_TEXTURE_USAGE_SHADER_READ) {
    result |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (usage & GPU_TEXTURE_USAGE_SHADER_WRITE) {
    result |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (usage & GPU_TEXTURE_USAGE_ATTACHMENT) {
    if (format_flag & GPU_FORMAT_COMPRESSED) {
      /* These formats aren't supported as an attachment. When using GPU_TEXTURE_USAGE_DEFAULT they
       * are still being evaluated to be attachable. So we need to skip them. */
    }
    else {
      if (format_flag & (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL)) {
        result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
      else {
        result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      }
    }
  }
  if (usage & GPU_TEXTURE_USAGE_HOST_READ) {
    result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  /* Disable some usages based on the given format flag to support more devices. */
  if (format_flag & GPU_FORMAT_SRGB) {
    /* NVIDIA devices don't create SRGB textures when it storage bit is set. */
    result &= ~VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (format_flag & (GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL)) {
    /* NVIDIA devices don't create depth textures when it storage bit is set. */
    result &= ~VK_IMAGE_USAGE_STORAGE_BIT;
  }

  return result;
}

static VkImageCreateFlags to_vk_image_create(const eGPUTextureType texture_type,
                                             const eGPUTextureFormatFlag format_flag,
                                             const eGPUTextureUsage usage)
{
  VkImageCreateFlags result = 0;

  if (ELEM(texture_type, GPU_TEXTURE_CUBE, GPU_TEXTURE_CUBE_ARRAY)) {
    result |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  /* sRGB textures needs to be mutable as they can be used as non-sRGB frame-buffer attachments. */
  if (usage & GPU_TEXTURE_USAGE_ATTACHMENT && format_flag & GPU_FORMAT_SRGB) {
    result |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
  }

  return result;
}

bool VKTexture::allocate()
{
  BLI_assert(vk_image_ == VK_NULL_HANDLE);
  BLI_assert(!is_texture_view());

  VKContext &context = *VKContext::get();
  const VKDevice &device = VKBackend::get().device_get();
  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.flags = to_vk_image_create(type_, format_flag_, usage_get());
  image_info.imageType = to_vk_image_type(type_);
  image_info.extent = vk_extent_3d(0);
  image_info.mipLevels = max_ii(mipmaps_, 1);
  image_info.arrayLayers = vk_layer_count(1);
  image_info.format = to_vk_format(device_format_);
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

void VKTexture::bind(int binding,
                     shader::ShaderCreateInfo::Resource::BindType bind_type,
                     const GPUSamplerState sampler_state)
{
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  const VKShaderInterface &shader_interface = shader->interface_get();
  const std::optional<VKDescriptorSet::Location> location =
      shader_interface.descriptor_set_location(bind_type, binding);
  if (location) {
    VKDescriptorSetTracker &descriptor_set = context.descriptor_set_get();
    if (bind_type == shader::ShaderCreateInfo::Resource::BindType::IMAGE) {
      descriptor_set.image_bind(*this, *location);
    }
    else {
      VKDevice &device = VKBackend::get().device_get();
      const VKSampler &sampler = device.samplers().get(sampler_state);
      descriptor_set.bind(*this, *location, sampler);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Image Layout
 * \{ */

VkImageLayout VKTexture::current_layout_get() const
{
  if (is_texture_view()) {
    return source_texture_->current_layout_get();
  }
  return current_layout_;
}

void VKTexture::current_layout_set(const VkImageLayout new_layout)
{
  BLI_assert(!is_texture_view());
  current_layout_ = new_layout;
}

void VKTexture::layout_ensure(VKContext &context,
                              const VkImageLayout requested_layout,
                              const VkPipelineStageFlags src_stage,
                              const VkAccessFlags src_access,
                              const VkPipelineStageFlags dst_stage,
                              const VkAccessFlags dst_access)
{
  if (is_texture_view()) {
    source_texture_->layout_ensure(context, requested_layout);
    return;
  }
  const VkImageLayout current_layout = current_layout_get();
  if (current_layout == requested_layout) {
    return;
  }
  layout_ensure(context,
                IndexRange(0, VK_REMAINING_MIP_LEVELS),
                current_layout,
                requested_layout,
                src_stage,
                src_access,
                dst_stage,
                dst_access);
  current_layout_set(requested_layout);
}

void VKTexture::layout_ensure(VKContext &context,
                              const IndexRange mipmap_range,
                              const VkImageLayout current_layout,
                              const VkImageLayout requested_layout,
                              const VkPipelineStageFlags src_stages,
                              const VkAccessFlags src_access,
                              const VkPipelineStageFlags dst_stages,
                              const VkAccessFlags dst_access)
{
  BLI_assert(vk_image_ != VK_NULL_HANDLE);
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = current_layout;
  barrier.newLayout = requested_layout;
  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;
  barrier.image = vk_image_;
  barrier.subresourceRange.aspectMask = to_vk_image_aspect_flag_bits(device_format_);
  barrier.subresourceRange.baseMipLevel = uint32_t(mipmap_range.start());
  barrier.subresourceRange.levelCount = uint32_t(mipmap_range.size());
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
  context.command_buffers_get().pipeline_barrier(
      src_stages, dst_stages, Span<VkImageMemoryBarrier>(&barrier, 1));
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
  image_view_.emplace(VKImageView(*this,
                                  eImageViewUsage::ShaderBinding,
                                  layer_range(),
                                  mip_map_range(),
                                  use_stencil_,
                                  true,
                                  name_));
}

IndexRange VKTexture::mip_map_range() const
{
  return IndexRange(mip_min_, mip_max_ - mip_min_ + 1);
}

IndexRange VKTexture::layer_range() const
{
  if (is_texture_view()) {
    return IndexRange(layer_offset_, 1);
  }
  else {
    return IndexRange(
        0, ELEM(type_, GPU_TEXTURE_CUBE, GPU_TEXTURE_CUBE_ARRAY) ? d_ : VK_REMAINING_ARRAY_LAYERS);
  }
}

int VKTexture::vk_layer_count(int non_layered_value) const
{
  if (is_texture_view()) {
    return 1;
  }
  return type_ == GPU_TEXTURE_CUBE   ? d_ :
         (type_ & GPU_TEXTURE_ARRAY) ? layer_count() :
                                       non_layered_value;
}

VkExtent3D VKTexture::vk_extent_3d(int mip_level) const
{
  int extent[3] = {1, 1, 1};
  mip_size_get(mip_level, extent);
  if (ELEM(type_, GPU_TEXTURE_CUBE, GPU_TEXTURE_CUBE_ARRAY, GPU_TEXTURE_2D_ARRAY)) {
    extent[2] = 1;
  }
  if (ELEM(type_, GPU_TEXTURE_1D_ARRAY)) {
    extent[1] = 1;
    extent[2] = 1;
  }

  VkExtent3D result{uint32_t(extent[0]), uint32_t(extent[1]), uint32_t(extent[2])};
  return result;
}

/** \} */

}  // namespace blender::gpu
