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
#include "vk_framebuffer.hh"
#include "vk_pixel_buffer.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"
#include "vk_vertex_buffer.hh"

#include "BLI_math_vector.hh"

#include "BKE_global.hh"

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
    VKDevice &device = VKBackend::get().device;
    device.discard_pool_for_current_thread().discard_image(vk_image_, allocation_);
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
  /* Allow users to provide mipmaps stored in compressed textures.
   * Skip generating mipmaps to avoid overriding the existing ones. */
  if (format_flag_ & GPU_FORMAT_COMPRESSED) {
    return;
  }

  VKContext &context = *VKContext::get();
  render_graph::VKUpdateMipmapsNode::Data update_mipmaps = {};
  update_mipmaps.vk_image = vk_image_handle();
  update_mipmaps.l0_size = int3(1);
  mip_size_get(0, update_mipmaps.l0_size);
  if (ELEM(this->type_get(), GPU_TEXTURE_1D_ARRAY)) {
    update_mipmaps.l0_size.y = 1;
    update_mipmaps.l0_size.z = 1;
  }
  else if (ELEM(this->type_get(), GPU_TEXTURE_2D_ARRAY)) {
    update_mipmaps.l0_size.z = 1;
  }
  update_mipmaps.vk_image_aspect = to_vk_image_aspect_flag_bits(device_format_);
  update_mipmaps.mipmaps = mipmaps_;
  update_mipmaps.layer_count = vk_layer_count(1);
  context.render_graph.add_node(update_mipmaps);
}

void VKTexture::copy_to(VKTexture &dst_texture, VkImageAspectFlags vk_image_aspect)
{
  render_graph::VKCopyImageNode::CreateInfo copy_image = {};
  copy_image.node_data.src_image = vk_image_handle();
  copy_image.node_data.dst_image = dst_texture.vk_image_handle();
  copy_image.node_data.region.srcSubresource.aspectMask = vk_image_aspect;
  copy_image.node_data.region.srcSubresource.mipLevel = 0;
  copy_image.node_data.region.srcSubresource.layerCount = vk_layer_count(1);
  copy_image.node_data.region.dstSubresource.aspectMask = vk_image_aspect;
  copy_image.node_data.region.dstSubresource.mipLevel = 0;
  copy_image.node_data.region.dstSubresource.layerCount = vk_layer_count(1);
  copy_image.node_data.region.extent = vk_extent_3d(0);
  copy_image.vk_image_aspect = to_vk_image_aspect_flag_bits(device_format_get());

  VKContext &context = *VKContext::get();
  context.render_graph.add_node(copy_image);
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
  render_graph::VKClearColorImageNode::CreateInfo clear_color_image = {};
  clear_color_image.vk_clear_color_value = to_vk_clear_color_value(format, data);
  clear_color_image.vk_image = vk_image_handle();
  clear_color_image.vk_image_subresource_range.aspectMask = to_vk_image_aspect_flag_bits(
      device_format_);

  IndexRange layers = layer_range();
  clear_color_image.vk_image_subresource_range.baseArrayLayer = layers.start();
  clear_color_image.vk_image_subresource_range.layerCount = layers.size();
  IndexRange levels = mip_map_range();
  clear_color_image.vk_image_subresource_range.baseMipLevel = levels.start();
  clear_color_image.vk_image_subresource_range.levelCount = levels.size();

  VKContext &context = *VKContext::get();

  context.render_graph.add_node(clear_color_image);
}

void VKTexture::clear_depth_stencil(const eGPUFrameBufferBits buffers,
                                    float clear_depth,
                                    uint clear_stencil)
{
  BLI_assert(buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT));
  VkImageAspectFlags vk_image_aspect_device = to_vk_image_aspect_flag_bits(device_format_get());
  VkImageAspectFlags vk_image_aspect = to_vk_image_aspect_flag_bits(
                                           buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) &
                                       vk_image_aspect_device;
  if (vk_image_aspect == VK_IMAGE_ASPECT_NONE) {
    /* Early exit: texture doesn't have any aspect that needs to be cleared. */
    return;
  }

  render_graph::VKClearDepthStencilImageNode::CreateInfo clear_depth_stencil_image = {};
  clear_depth_stencil_image.node_data.vk_image = vk_image_handle();
  clear_depth_stencil_image.vk_image_aspects = vk_image_aspect_device;
  clear_depth_stencil_image.node_data.vk_clear_depth_stencil_value.depth = clear_depth;
  clear_depth_stencil_image.node_data.vk_clear_depth_stencil_value.stencil = clear_stencil;
  clear_depth_stencil_image.node_data.vk_image_subresource_range.aspectMask = vk_image_aspect;
  clear_depth_stencil_image.node_data.vk_image_subresource_range.layerCount =
      VK_REMAINING_ARRAY_LAYERS;
  clear_depth_stencil_image.node_data.vk_image_subresource_range.levelCount =
      VK_REMAINING_MIP_LEVELS;

  VKContext &context = *VKContext::get();
  context.render_graph.add_node(clear_depth_stencil_image);
}

void VKTexture::swizzle_set(const char swizzle_mask[4])
{
  memcpy(image_view_info_.swizzle, swizzle_mask, 4);
}

void VKTexture::mip_range_set(int min, int max)
{
  mip_min_ = min;
  mip_max_ = max;
}

void VKTexture::read_sub(
    int mip, eGPUDataFormat format, const int region[6], const IndexRange layers, void *r_data)
{
  const int3 extent = int3(region[3] - region[0], region[4] - region[1], region[5] - region[2]);
  size_t sample_len = extent.x * extent.y * extent.z * layers.size();

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKBuffer staging_buffer;
  size_t device_memory_size = sample_len * to_bytesize(device_format_);
  staging_buffer.create(device_memory_size, GPU_USAGE_DYNAMIC, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  render_graph::VKCopyImageToBufferNode::CreateInfo copy_image_to_buffer = {};
  copy_image_to_buffer.src_image = vk_image_handle();
  copy_image_to_buffer.dst_buffer = staging_buffer.vk_handle();
  copy_image_to_buffer.region.imageOffset.x = region[0];
  copy_image_to_buffer.region.imageOffset.y = region[1];
  copy_image_to_buffer.region.imageOffset.z = region[2];
  copy_image_to_buffer.region.imageExtent.width = extent.x;
  copy_image_to_buffer.region.imageExtent.height = extent.y;
  copy_image_to_buffer.region.imageExtent.depth = extent.z;
  copy_image_to_buffer.region.imageSubresource.aspectMask = to_vk_image_aspect_single_bit(
      to_vk_image_aspect_flag_bits(device_format_), false);
  copy_image_to_buffer.region.imageSubresource.mipLevel = mip;
  copy_image_to_buffer.region.imageSubresource.baseArrayLayer = layers.start();
  copy_image_to_buffer.region.imageSubresource.layerCount = layers.size();

  VKContext &context = *VKContext::get();
  context.rendering_end();
  context.render_graph.add_node(copy_image_to_buffer);
  context.descriptor_set_get().upload_descriptor_sets();
  context.render_graph.submit_buffer_for_read(staging_buffer.vk_handle());

  convert_device_to_host(
      r_data, staging_buffer.mapped_memory_get(), sample_len, format, format_, device_format_);
}

void *VKTexture::read(int mip, eGPUDataFormat format)
{
  BLI_assert(!(format_flag_ & GPU_FORMAT_COMPRESSED));

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
    int mip, int offset_[3], int extent_[3], eGPUDataFormat format, const void *data)
{
  BLI_assert(!is_texture_view());

  const bool is_compressed = (format_flag_ & GPU_FORMAT_COMPRESSED);

  int3 extent = int3(extent_[0], max_ii(extent_[1], 1), max_ii(extent_[2], 1));
  int3 offset = int3(offset_[0], offset_[1], offset_[2]);
  int layers = 1;
  int start_layer = 0;
  if (type_ & GPU_TEXTURE_1D) {
    layers = extent.y;
    start_layer = offset.y;
    extent.y = 1;
    extent.z = 1;
    offset.y = 0;
    offset.z = 0;
  }
  if (type_ & (GPU_TEXTURE_2D | GPU_TEXTURE_CUBE)) {
    layers = extent.z;
    start_layer = offset.z;
    extent.z = 1;
    offset.z = 0;
  }

  /* Vulkan images cannot be directly mapped to host memory and requires a staging buffer. */
  VKContext &context = *VKContext::get();
  size_t sample_len = size_t(extent.x) * extent.y * extent.z * layers;
  size_t device_memory_size = sample_len * to_bytesize(device_format_);

  if (is_compressed) {
    BLI_assert_msg(extent.z == 1, "Compressed 3D textures are not supported");
    size_t block_size = to_block_size(device_format_);
    size_t blocks_x = divide_ceil_u(extent.x, 4);
    size_t blocks_y = divide_ceil_u(extent.y, 4);
    device_memory_size = blocks_x * blocks_y * block_size;
    /* `convert_buffer` later on will use `sample_len * to_bytesize(device_format_)`
     * as total memory size calculation. Make that work for compressed case. */
    sample_len = device_memory_size / to_bytesize(device_format_);
  }

  VKBuffer staging_buffer;
  staging_buffer.create(device_memory_size, GPU_USAGE_DYNAMIC, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  convert_host_to_device(
      staging_buffer.mapped_memory_get(), data, sample_len, format, format_, device_format_);

  render_graph::VKCopyBufferToImageNode::CreateInfo copy_buffer_to_image = {};
  copy_buffer_to_image.src_buffer = staging_buffer.vk_handle();
  copy_buffer_to_image.dst_image = vk_image_handle();
  copy_buffer_to_image.region.imageExtent.width = extent.x;
  copy_buffer_to_image.region.imageExtent.height = extent.y;
  copy_buffer_to_image.region.imageExtent.depth = extent.z;
  copy_buffer_to_image.region.bufferRowLength =
      context.state_manager_get().texture_unpack_row_length_get();
  copy_buffer_to_image.region.imageOffset.x = offset.x;
  copy_buffer_to_image.region.imageOffset.y = offset.y;
  copy_buffer_to_image.region.imageOffset.z = offset.z;
  copy_buffer_to_image.region.imageSubresource.aspectMask = to_vk_image_aspect_single_bit(
      to_vk_image_aspect_flag_bits(device_format_), false);
  copy_buffer_to_image.region.imageSubresource.mipLevel = mip;
  copy_buffer_to_image.region.imageSubresource.baseArrayLayer = start_layer;
  copy_buffer_to_image.region.imageSubresource.layerCount = layers;

  context.render_graph.add_node(copy_buffer_to_image);
}

void VKTexture::update_sub(int offset_[3],
                           int extent_[3],
                           eGPUDataFormat format,
                           GPUPixelBuffer *pixbuf)
{
  VKPixelBuffer &pixel_buffer = *unwrap(unwrap(pixbuf));
  update_sub(0, offset_, extent_, format, pixel_buffer.map());
}

/* TODO(fclem): Legacy. Should be removed at some point. */
uint VKTexture::gl_bindcode_get() const
{
  return 0;
}

bool VKTexture::init_internal()
{
  const VKDevice &device = VKBackend::get().device;
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
  this->mip_range_set(0, mipmaps_ - 1);

  return true;
}

bool VKTexture::init_internal(VertBuf *vbo)
{
  BLI_assert(source_buffer_ == nullptr);
  device_format_ = format_;
  source_buffer_ = unwrap(vbo);
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

  VKDevice &device = VKBackend::get().device;
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

  device.resources.add_image(vk_image_,
                             image_info.arrayLayers,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             render_graph::ResourceOwner::APPLICATION,
                             name_);

  return result == VK_SUCCESS;
}

/* -------------------------------------------------------------------- */
/** \name Image Views
 * \{ */

IndexRange VKTexture::mip_map_range() const
{
  return IndexRange(mip_min_, mip_max_ - mip_min_ + 1);
}

IndexRange VKTexture::layer_range() const
{
  if (is_texture_view()) {
    return IndexRange(layer_offset_, layer_count());
  }
  else {
    return IndexRange(
        0, ELEM(type_, GPU_TEXTURE_CUBE, GPU_TEXTURE_CUBE_ARRAY) ? d_ : VK_REMAINING_ARRAY_LAYERS);
  }
}

int VKTexture::vk_layer_count(int non_layered_value) const
{
  if (is_texture_view()) {
    return layer_count();
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

const VKImageView &VKTexture::image_view_get(const VKImageViewInfo &info)
{
  if (is_texture_view()) {
    /* TODO: API should be improved as we don't support image view specialization.
     * In the current API this is still possible to setup when using attachments. */
    return image_view_get(info.arrayed);
  }
  for (const VKImageView &image_view : image_views_) {
    if (image_view.info == info) {
      return image_view;
    }
  }

  image_views_.append(VKImageView(*this, info, name_));
  return image_views_.last();
}

const VKImageView &VKTexture::image_view_get(VKImageViewArrayed arrayed)
{
  image_view_info_.mip_range = mip_map_range();
  image_view_info_.use_srgb = true;
  image_view_info_.use_stencil = use_stencil_;
  image_view_info_.arrayed = arrayed;
  image_view_info_.layer_range = layer_range();
  if (arrayed == VKImageViewArrayed::NOT_ARRAYED) {
    image_view_info_.layer_range = image_view_info_.layer_range.slice(
        0, ELEM(type_, GPU_TEXTURE_CUBE, GPU_TEXTURE_CUBE_ARRAY) ? 6 : 1);
  }

  if (is_texture_view()) {
    return source_texture_->image_view_get(image_view_info_);
  }
  return image_view_get(image_view_info_);
}

/** \} */

}  // namespace blender::gpu
