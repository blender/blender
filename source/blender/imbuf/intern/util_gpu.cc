/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_mutex.hh"
#include "BLI_rect.hh"
#include "BLI_time.hh"
#include "BLI_utildefines.hh"
#include "BLI_utility_mixins.hh"

#include "MEM_guardedalloc.h"

#include <mutex>

#include "CLG_log.h"

#include "GPU_capabilities.hh"
#include "GPU_texture.hh"

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_partial_update.hh"

namespace blender {

static CLG_LogRef LOG = {"image.gpu"};

/* gpu ibuf utils */

static bool imb_is_grayscale_texture_format_compatible(const ImBuf *ibuf)
{
  if (ibuf->color_mode != ImColorMode::BW) {
    return false;
  }

  if (ibuf->byte_data() && !ibuf->float_data()) {

    if (IMB_colormanagement_space_is_scene_linear_srgb(ibuf->byte_buffer.colorspace) ||
        IMB_colormanagement_space_is_scene_linear(ibuf->byte_buffer.colorspace))
    {
      /* Grey-scale byte buffers with these color transforms utilize float buffers under the hood
       * and can therefore be optimized. */
      return true;
    }
    /* TODO: Support gray-scale byte buffers.
     * The challenge is that Blender always stores byte images as RGBA. */
    return false;
  }

  /* Only #IMBuf's with color-space that do not modify the chrominance of the texture data relative
   * to the scene color space can be uploaded as single channel textures. */
  if (IMB_colormanagement_space_is_data(ibuf->float_buffer.colorspace) ||
      IMB_colormanagement_space_is_scene_linear_srgb(ibuf->float_buffer.colorspace) ||
      IMB_colormanagement_space_is_scene_linear(ibuf->float_buffer.colorspace))
  {
    return true;
  }
  return false;
}

static void imb_gpu_get_format(const ImBuf *ibuf,
                               bool high_bitdepth,
                               bool use_grayscale,
                               gpu::TextureFormat *r_texture_format)
{
  const bool float_rect = (ibuf->float_data() != nullptr);
  const bool is_grayscale = use_grayscale && imb_is_grayscale_texture_format_compatible(ibuf);

  if (float_rect) {
    /* Float. */
    const bool use_high_bitdepth = (!(ibuf->foptions.flag & OPENEXR_HALF) && high_bitdepth);
    *r_texture_format = is_grayscale ?
                            (use_high_bitdepth ? gpu::TextureFormat::SFLOAT_32 :
                                                 gpu::TextureFormat::SFLOAT_16) :
                            (use_high_bitdepth ? gpu::TextureFormat::SFLOAT_32_32_32_32 :
                                                 gpu::TextureFormat::SFLOAT_16_16_16_16);
  }
  else {
    if (IMB_colormanagement_space_is_data(ibuf->byte_buffer.colorspace) ||
        IMB_colormanagement_space_is_scene_linear(ibuf->byte_buffer.colorspace))
    {
      /* Non-color data or scene linear, just store buffer as is. */
      *r_texture_format = (is_grayscale) ? gpu::TextureFormat::UNORM_8 :
                                           gpu::TextureFormat::UNORM_8_8_8_8;
    }
    else if (IMB_colormanagement_space_is_scene_linear_srgb(ibuf->byte_buffer.colorspace)) {
      /* scene linear + sRGB, store as byte texture that the GPU can decode directly. */
      *r_texture_format = (is_grayscale) ? gpu::TextureFormat::SFLOAT_16 :
                                           gpu::TextureFormat::SRGBA_8_8_8_8;
    }
    else {
      /* Other colorspace, store as half float texture to avoid precision loss. */
      *r_texture_format = (is_grayscale) ? gpu::TextureFormat::SFLOAT_16 :
                                           gpu::TextureFormat::SFLOAT_16_16_16_16;
    }
  }
}

static const char *imb_gpu_get_swizzle(const ImBuf *ibuf)
{
  return imb_is_grayscale_texture_format_compatible(ibuf) ? "rrra" : "rgba";
}

/* Return false if no suitable format was found. */
bool IMB_gpu_get_compressed_format(const ImBuf *ibuf, gpu::TextureFormat *r_texture_format)
{
  if (ibuf->ftype != IMB_FTYPE_DDS) {
    return false;
  }

  /* Compressed DDS files can really only express sRGB or data/linear. */
  const bool use_srgb = (!IMB_colormanagement_space_is_data(ibuf->byte_buffer.colorspace) &&
                         !IMB_colormanagement_space_is_scene_linear(ibuf->byte_buffer.colorspace));
  if (ibuf->foptions.flag & DDS_COMPRESSED_DXT1) {
    *r_texture_format = use_srgb ? gpu::TextureFormat::SRGB_DXT1 : gpu::TextureFormat::SNORM_DXT1;
    return true;
  }
  if (ibuf->foptions.flag & DDS_COMPRESSED_DXT3) {
    *r_texture_format = use_srgb ? gpu::TextureFormat::SRGB_DXT3 : gpu::TextureFormat::SNORM_DXT3;
    return true;
  }
  if (ibuf->foptions.flag & DDS_COMPRESSED_DXT5) {
    *r_texture_format = use_srgb ? gpu::TextureFormat::SRGB_DXT5 : gpu::TextureFormat::SNORM_DXT5;
    return true;
  }
  return false;
}

/* Extract the first channel of an RGBA buffer into a single channel, for uploading grayscale. */
template<typename T>
static void imb_gpu_extract_first_channel(
    const T *src, const int channels, const int stride, const int w, const int h, T *r_gray)
{
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      r_gray[size_t(row) * w + col] = src[(size_t(row) * stride + col) * channels];
    }
  }
}

/* Determine image buffer conversion needed before GPU upload. */
enum class GPUTextureConversion { None, Byte, Float };

static GPUTextureConversion imb_gpu_texture_conversion(const ImBuf *ibuf,
                                                       const bool is_grayscale,
                                                       const bool store_premultiplied)
{
  if (ibuf->float_data()) {
    /* Float images are already in scene linear color space or contain non-color data with
     * premultiplied alpha by convention, so no color space conversion is needed. But we need to
     * convert to RGBA and unpremultiply alpha if needed. */
    return (ibuf->channels != 4 || !store_premultiplied) ? GPUTextureConversion::Float :
                                                           GPUTextureConversion::None;
  }

  /* Byte images are in original color space from the file with straight alpha, so we need to
   * convert to scene linear color space and premultiply alpha if needed. An exception is images
   * in sRGB color space, see the relevant case below for more information.  */
  const ColorSpace *colorspace = ibuf->byte_buffer.colorspace;
  if (IMB_colormanagement_space_is_data(colorspace)) {
    /* Non-color data, used as is. */
    return GPUTextureConversion::None;
  }
  if (IMB_colormanagement_space_is_scene_linear(colorspace)) {
    /* Color space is already linear, we just need to premultiply the alpha if needed. */
    const bool premultiply = !is_grayscale && store_premultiplied && IMB_alpha_affects_rgb(ibuf);
    return premultiply ? GPUTextureConversion::Byte : GPUTextureConversion::None;
  }
  if (IMB_colormanagement_space_is_scene_linear_srgb(colorspace)) {
    /* Images in scene linear + sRGB color space are a special case since they can be stored in
     * sRGB textures on GPU, where scene linear space conversion will happen during sampling in
     * the shader, however, this is not possible for grayscale images, so we need to do the
     * conversion here, converting to a float image to prevent precision loss.
     *
     * It should be noted that for other color spaces, color space conversion happen before alpha
     * premultiplication, while for sRGB, premultiplication will happen first since color space
     * conversion happen in the shader as mentioned above. This will manifest as differences near
     * alpha edges. But we generally accept this due to the advantages of sRGB textures. If this
     * is problematic, the source image buffer should be linearised first. */
    if (is_grayscale) {
      return GPUTextureConversion::Float;
    }
    const bool premultiply = store_premultiplied && IMB_alpha_affects_rgb(ibuf);
    return premultiply ? GPUTextureConversion::Byte : GPUTextureConversion::None;
  }
  /* Other color-space, convert to linear color space and premultiply alpha if needed.
   * Conversion happen as a float to avoid precision loss. */
  return GPUTextureConversion::Float;
}

/**
 * Returns an image buffer containing the data from the source buffer but suitable for uploading to
 * GPU textures. If #scaled_size is not nullopt, the image will be scaled using a box filter to
 * match the scaled size. If #premultiplied_alpha is true and alpha is not packed, alpha is ensured
 * to be premultiplied, otherwise, it is ensured to be straight. If #is_grayscale is true, if the
 * image will be stored as a grayscale image with no data loss, it will be return as so, otherwise,
 * it will be RGBA.
 */
struct GPUTextureUpload : NonCopyable, NonMovable {
  const void *data = nullptr;
  eGPUDataFormat format = GPU_DATA_FLOAT;
  int stride = 0;
  int2 size = int2(0);
  ImBuf *tmp_ibuf = nullptr;

  GPUTextureUpload() = default;
  ~GPUTextureUpload()
  {
    if (tmp_ibuf) {
      IMB_freeImBuf(tmp_ibuf);
    }
  }
};

static void get_gpu_texture_data(ImBuf *source_buffer,
                                 const int2 src_offset,
                                 const int2 src_size,
                                 const std::optional<int2> scaled_size,
                                 const bool premultiplied_alpha,
                                 const bool is_grayscale,
                                 GPUTextureUpload &r_upload)
{
  const GPUTextureConversion conversion = imb_gpu_texture_conversion(
      source_buffer, is_grayscale, premultiplied_alpha);

  int channels;

  if (conversion == GPUTextureConversion::Byte) {
    /* Convert to byte buffer. */
    r_upload.tmp_ibuf = IMB_allocImBuf(
        src_size.x, src_size.y, ImBufFlags::ByteData | ImBufFlags::UninitializedPixels);
    if (r_upload.tmp_ibuf == nullptr) {
      return;
    }
    IMB_colormanagement_imbuf_to_byte_texture(r_upload.tmp_ibuf->byte_data_for_write(),
                                              src_offset.x,
                                              src_offset.y,
                                              src_size.x,
                                              src_size.y,
                                              source_buffer,
                                              premultiplied_alpha);
    r_upload.data = r_upload.tmp_ibuf->byte_data();
    r_upload.format = GPU_DATA_UBYTE;
    r_upload.stride = src_size.x;
    channels = 4;
  }
  else if (conversion == GPUTextureConversion::Float) {
    /* Convert to float buffer. */
    r_upload.tmp_ibuf = IMB_allocImBuf(
        src_size.x, src_size.y, ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
    if (r_upload.tmp_ibuf == nullptr) {
      return;
    }
    IMB_colormanagement_imbuf_to_float_texture(r_upload.tmp_ibuf->float_data_for_write(),
                                               src_offset.x,
                                               src_offset.y,
                                               src_size.x,
                                               src_size.y,
                                               source_buffer,
                                               premultiplied_alpha);
    r_upload.data = r_upload.tmp_ibuf->float_data();
    r_upload.format = GPU_DATA_FLOAT;
    r_upload.stride = src_size.x;
    channels = 4;
  }
  else {
    /* No conversion needed, point directly into the source buffer. */
    channels = source_buffer->float_data() ? source_buffer->channels : 4;
    const int64_t offset = int64_t(channels) *
                           (int64_t(src_offset.y) * source_buffer->x + src_offset.x);
    if (source_buffer->float_data()) {
      r_upload.data = source_buffer->float_data() + offset;
      r_upload.format = GPU_DATA_FLOAT;
    }
    else {
      r_upload.data = source_buffer->byte_data() + offset;
      r_upload.format = GPU_DATA_UBYTE;
    }
    r_upload.stride = source_buffer->x;
  }

  int2 size = src_size;

  /* Rescale. */
  if (scaled_size.has_value()) {
    const bool is_float = (r_upload.format == GPU_DATA_FLOAT);
    ImBuf *buffer = IMB_allocImBuf(scaled_size->x,
                                   scaled_size->y,
                                   (is_float ? ImBufFlags::FloatData : ImBufFlags::ByteData) |
                                       ImBufFlags::UninitializedPixels);

    if (buffer == nullptr) {
      r_upload.data = nullptr;
      return;
    }

    /* Avoid excessive overhead with small updates. */
    const bool threaded = size.x >= 1024;

    if (is_float) {
      IMB_scale_box(static_cast<const float *>(r_upload.data),
                    size,
                    channels,
                    buffer->float_data_for_write(),
                    *scaled_size,
                    threaded,
                    r_upload.stride);
    }
    else {
      IMB_scale_box(static_cast<const uchar *>(r_upload.data),
                    size,
                    channels,
                    buffer->byte_data_for_write(),
                    *scaled_size,
                    threaded,
                    r_upload.stride);
    }
    if (r_upload.tmp_ibuf) {
      IMB_freeImBuf(r_upload.tmp_ibuf);
    }
    r_upload.tmp_ibuf = buffer;
    r_upload.data = is_float ? static_cast<const void *>(buffer->float_data()) :
                               static_cast<const void *>(buffer->byte_data());
    r_upload.stride = scaled_size->x;
    size = *scaled_size;
  }

  /* Convert to grayscale. */
  if (is_grayscale) {
    ImBuf *buffer = IMB_allocImBuf(size.x, size.y, ImBufFlags::Zero);

    if (buffer == nullptr) {
      r_upload.data = nullptr;
      return;
    }

    if (r_upload.format == GPU_DATA_FLOAT) {
      IMB_alloc_float_pixels(buffer, 1);
      imb_gpu_extract_first_channel(static_cast<const float *>(r_upload.data),
                                    channels,
                                    r_upload.stride,
                                    size.x,
                                    size.y,
                                    buffer->float_data_for_write());
    }
    else {
      buffer->color_mode = ImColorMode::BW;
      buffer->assign_byte_data(
          MEM_new_array_uninitialized<uint8_t>(size_t(size.x) * size.y, __func__));
      imb_gpu_extract_first_channel(static_cast<const uchar *>(r_upload.data),
                                    channels,
                                    r_upload.stride,
                                    size.x,
                                    size.y,
                                    buffer->byte_data_for_write());
    }
    if (r_upload.tmp_ibuf) {
      IMB_freeImBuf(r_upload.tmp_ibuf);
    }
    r_upload.tmp_ibuf = buffer;
    r_upload.data = (r_upload.format == GPU_DATA_FLOAT) ?
                        static_cast<const void *>(buffer->float_data()) :
                        static_cast<const void *>(buffer->byte_data());
    r_upload.stride = size.x;
  }

  r_upload.size = size;
}

gpu::Texture *IMB_touch_gpu_texture(const char *name,
                                    ImBuf *ibuf,
                                    int w,
                                    int h,
                                    int layers,
                                    bool use_high_bitdepth,
                                    bool use_grayscale)
{
  gpu::TextureFormat tex_format;
  imb_gpu_get_format(ibuf, use_high_bitdepth, use_grayscale, &tex_format);

  gpu::Texture *tex;
  if (layers > 0) {
    tex = GPU_texture_create_2d_array(name,
                                      w,
                                      h,
                                      layers,
                                      9999,
                                      tex_format,
                                      GPU_TEXTURE_USAGE_SHADER_READ |
                                          GPU_TEXTURE_USAGE_SHADER_WRITE,
                                      nullptr);
  }
  else {
    tex = GPU_texture_create_2d(name,
                                w,
                                h,
                                9999,
                                tex_format,
                                GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE,
                                nullptr);
  }

  GPU_texture_swizzle_set(tex, imb_gpu_get_swizzle(ibuf));
  GPU_texture_anisotropic_filter(tex, true);
  return tex;
}

void IMB_update_gpu_texture_sub(gpu::Texture *tex,
                                ImBuf *ibuf,
                                int x,
                                int y,
                                int z,
                                int w,
                                int h,
                                bool use_grayscale,
                                bool use_premult)
{
  const std::optional<int2> scaled_size = (ibuf->x != w || ibuf->y != h) ?
                                              std::optional<int2>(int2(w, h)) :
                                              std::nullopt;
  const bool is_grayscale = use_grayscale && imb_is_grayscale_texture_format_compatible(ibuf);
  GPUTextureUpload upload;
  get_gpu_texture_data(
      ibuf, int2(0), int2(ibuf->x, ibuf->y), scaled_size, use_premult, is_grayscale, upload);

  if (upload.data) {
    GPU_texture_update_sub(
        tex, upload.format, upload.data, x, y, z, upload.size.x, upload.size.y, 1, upload.stride);
  }
}

gpu::Texture *IMB_create_gpu_texture(const char *name,
                                     ImBuf *ibuf,
                                     const GPUTextureCreateFlags flags)
{
  ibuf->gpu.lastused = BLI_time_now_seconds_i();

  const bool use_mipmap = flag_is_set(flags, GPUTextureCreateFlags::EnableMipmaps);

  gpu::Texture *tex = nullptr;
  int size[2] = {ibuf->x, ibuf->y};
  if (flag_is_set(flags, GPUTextureCreateFlags::LimitSize)) {
    size[0] = GPU_texture_size_with_limit(ibuf->x);
    size[1] = GPU_texture_size_with_limit(ibuf->y);
  }
  bool do_rescale = (ibuf->x != size[0]) || (ibuf->y != size[1]);

  /* Correct the smaller size to maintain the original aspect ratio of the image. */
  if (do_rescale && ibuf->x != ibuf->y) {
    if (size[0] > size[1]) {
      size[1] = int(ibuf->y * (float(size[0]) / ibuf->x));
    }
    else {
      size[0] = int(ibuf->x * (float(size[1]) / ibuf->y));
    }
  }

  if (ibuf->ftype == IMB_FTYPE_DDS) {
    gpu::TextureFormat compressed_format;
    if (!IMB_gpu_get_compressed_format(ibuf, &compressed_format)) {
      CLOG_WARN(&LOG,
                "DDS image '%s' is not in a supported GPU compression format",
                ibuf->filepath.c_str());
    }
    else if (do_rescale) {
      CLOG_WARN(
          &LOG, "DDS image '%s' can't use compressed due to size limit", ibuf->filepath.c_str());
    }
    else if (!is_power_of_2_i(ibuf->x) || !is_power_of_2_i(ibuf->y)) {
      /* We require POT DXT/S3TC texture sizes not because something in there
       * intrinsically needs it, but because we flip them upside down at
       * load time, and that (when mipmaps are involved) is only possible
       * with POT height. */
      CLOG_WARN(&LOG,
                "DDS image '%s' can't use compressed due to non power of two size",
                ibuf->filepath.c_str());
    }
    else {

      int mip_count = 0;
      uint8_t *compressed_data = imb_load_dds_compressed_data(
          ibuf->filepath.c_str(), ibuf->x, ibuf->y, mip_count);
      if (compressed_data != nullptr) {
        tex = GPU_texture_create_compressed_2d(name,
                                               ibuf->x,
                                               ibuf->y,
                                               use_mipmap ? mip_count : 1,
                                               compressed_format,
                                               GPU_TEXTURE_USAGE_GENERAL,
                                               compressed_data);
        MEM_delete(compressed_data);
        if (tex != nullptr) {
          return tex;
        }
        CLOG_WARN(&LOG,
                  "DDS image '%s' failed to create compressed GPU texture",
                  ibuf->filepath.c_str());
      }
      else {
        CLOG_WARN(&LOG, "DDS image '%s' failed to load data from file", ibuf->filepath.c_str());
      }
    }
    /* Fallback to uncompressed texture. */
    CLOG_WARN(&LOG, "DDS image '%s' falling back to uncompressed", ibuf->filepath.c_str());
  }

  gpu::TextureFormat tex_format;
  imb_gpu_get_format(
      ibuf, flag_is_set(flags, GPUTextureCreateFlags::HighBitDepth), true, &tex_format);

  /* Create Texture. Specify read usage to allow both shader and host reads, the latter is needed
   * by the GPU compositor. */
  const eGPUTextureUsage usage = use_mipmap ?
                                     GPU_TEXTURE_USAGE_SHADER_READ |
                                         GPU_TEXTURE_USAGE_SHADER_WRITE |
                                         GPU_TEXTURE_USAGE_HOST_READ :
                                     GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_HOST_READ;
  tex = GPU_texture_create_2d(
      name, UNPACK2(size), use_mipmap ? 9999 : 1, tex_format, usage, nullptr);
  if (tex == nullptr) {
    size[0] = max_ii(1, size[0] / 2);
    size[1] = max_ii(1, size[1] / 2);
    tex = GPU_texture_create_2d(
        name, UNPACK2(size), use_mipmap ? 9999 : 1, tex_format, usage, nullptr);
    do_rescale = true;
  }
  BLI_assert(tex != nullptr);
  const std::optional<int2> scaled_size = do_rescale ? std::optional<int2>(int2(size)) :
                                                       std::nullopt;
  const bool is_grayscale = imb_is_grayscale_texture_format_compatible(ibuf);
  GPUTextureUpload upload;
  get_gpu_texture_data(ibuf,
                       int2(0),
                       int2(ibuf->x, ibuf->y),
                       scaled_size,
                       flag_is_set(flags, GPUTextureCreateFlags::Premultiplied),
                       is_grayscale,
                       upload);

  if (upload.data) {
    GPU_texture_update(tex, upload.format, upload.data);
  }

  GPU_texture_swizzle_set(tex, imb_gpu_get_swizzle(ibuf));
  GPU_texture_anisotropic_filter(tex, true);

  return tex;
}

/* Compute offset and size for partial update with scaling. */
static int2 imb_gpu_texture_update_offset_size(int2 &offset,
                                               const int2 size,
                                               const int2 limit,
                                               const int2 full)
{
  const float xratio = limit.x / float(full.x);
  const float yratio = limit.y / float(full.y);

  /* Find sub coordinates in scaled image. Take ceiling because we will be
   * losing 1 pixel due to rounding errors in x,y. */
  offset.x = int(offset.x * xratio);
  offset.y = int(offset.y * yratio);
  int2 scaled_size = int2(int(ceil(xratio * size.x)), int(ceil(yratio * size.y)));

  /* ...but take back if we are over the limit! */
  if (offset.x + scaled_size.x > limit.x) {
    scaled_size.x--;
  }
  if (offset.y + scaled_size.y > limit.y) {
    scaled_size.y--;
  }

  return scaled_size;
}

static void imb_gpu_texture_update_region(gpu::Texture *tex,
                                          ImBuf *ibuf,
                                          const bool store_premultiplied,
                                          int x,
                                          int y,
                                          int w,
                                          int h,
                                          const int layer,
                                          const int2 tile_offset,
                                          const int2 tile_size)
{
  /* The texture may be smaller than the image when its size was limited. */
  const int limit_w = (layer >= 0) ? tile_size.x : GPU_texture_width(tex);
  const int limit_h = (layer >= 0) ? tile_size.y : GPU_texture_height(tex);
  const bool scaled = (ibuf->x != limit_w) || (ibuf->y != limit_h);

  if (scaled) {
    /* Extra padding to account for bleed from neighboring pixels. */
    const int padding = 4;
    const int xmax = min_ii(x + w + padding, ibuf->x);
    const int ymax = min_ii(y + h + padding, ibuf->y);
    x = max_ii(x - padding, 0);
    y = max_ii(y - padding, 0);
    w = xmax - x;
    h = ymax - y;
  }

  const bool use_grayscale = GPU_texture_component_len(GPU_texture_format(tex)) == 1;

  int2 offset = int2(x, y);
  std::optional<int2> scaled_size;
  if (scaled) {
    scaled_size = imb_gpu_texture_update_offset_size(
        offset, int2(w, h), int2(limit_w, limit_h), int2(ibuf->x, ibuf->y));
  }

  if (layer >= 0) {
    offset += tile_offset;
  }

  GPUTextureUpload upload;
  get_gpu_texture_data(
      ibuf, int2(x, y), int2(w, h), scaled_size, store_premultiplied, use_grayscale, upload);

  if (upload.data) {
    GPU_texture_update_sub(tex,
                           upload.format,
                           upload.data,
                           offset.x,
                           offset.y,
                           math::max(layer, 0),
                           upload.size.x,
                           upload.size.y,
                           1,
                           upload.stride);
  }

  GPU_texture_unbind(tex);
}

void IMB_gpu_texture_apply_partial_update(gpu::Texture *tex,
                                          ImBuf *ibuf,
                                          const bool store_premultiplied,
                                          const imbuf::partial_update::Changes &changes,
                                          const int layer,
                                          const int2 tile_offset,
                                          const int2 tile_size)
{
  rcti buffer_rect;
  BLI_rcti_init(&buffer_rect, 0, ibuf->x, 0, ibuf->y);
  for (const rcti &region : changes.modified_regions()) {
    rcti clipped;
    if (!BLI_rcti_isect(&buffer_rect, &region, &clipped)) {
      continue;
    }
    imb_gpu_texture_update_region(tex,
                                  ibuf,
                                  store_premultiplied,
                                  clipped.xmin,
                                  clipped.ymin,
                                  BLI_rcti_size_x(&clipped),
                                  BLI_rcti_size_y(&clipped),
                                  layer,
                                  tile_offset,
                                  tile_size);
  }
}

static void imb_gpu_texture_apply_partial_updates(ImBuf *ibuf, const bool use_premult)
{
  if (ibuf->byte_data() == nullptr && ibuf->float_data() == nullptr) {
    return;
  }

  using imbuf::partial_update::Changes;
  IMB_partial_update_flush(ibuf);
  const int64_t new_changeset_id = IMB_partial_update_changeset_id_current();
  const Changes changes = IMB_partial_update_collect(ibuf, ibuf->gpu.partial_update_changeset);
  switch (changes.kind) {
    case Changes::Kind::Full:
    case Changes::Kind::Resized:
      GPU_texture_free(ibuf->gpu.texture);
      ibuf->gpu.texture = nullptr;
      ibuf->gpu.flag = ImBufGPUFlag(0);
      break;
    case Changes::Kind::Partial:
      IMB_gpu_texture_apply_partial_update(
          ibuf->gpu.texture, ibuf, use_premult, changes, -1, int2(0), int2(0));
      if (!(ibuf->gpu.flag & IMB_GPU_DISABLE_MIPMAP_UPDATE)) {
        GPU_texture_update_mipmap_chain(ibuf->gpu.texture);
        ibuf->gpu.flag |= IMB_GPU_MIPMAP_COMPLETE;
      }
      ibuf->gpu.partial_update_changeset = new_changeset_id;
      break;
    case Changes::Kind::None:
      ibuf->gpu.partial_update_changeset = new_changeset_id;
      break;
  }
}

gpu::Texture *IMB_acquire_gpu_texture(const char *name,
                                      ImBuf *ibuf,
                                      bool use_high_bitdepth,
                                      bool use_premult,
                                      bool limit_size,
                                      bool try_only)
{
  if (ibuf == nullptr || (ibuf->byte_data() == nullptr && ibuf->float_data() == nullptr &&
                          ibuf->gpu.texture == nullptr))
  {
    return nullptr;
  }

  std::scoped_lock lock(ibuf->gpu.mutex);
  if (ibuf->gpu.texture != nullptr) {
    imb_gpu_texture_apply_partial_updates(ibuf, use_premult);
    if (ibuf->gpu.texture != nullptr) {
      ibuf->gpu.lastused = BLI_time_now_seconds_i();
      GPU_texture_ref(ibuf->gpu.texture);
      return ibuf->gpu.texture;
    }
  }
  if (try_only) {
    return nullptr;
  }

  const int64_t changeset_id = IMB_partial_update_changeset_id_next();

  GPUTextureCreateFlags create_flags = GPUTextureCreateFlags::EnableMipmaps;
  if (use_high_bitdepth) {
    create_flags |= GPUTextureCreateFlags::HighBitDepth;
  }
  if (use_premult) {
    create_flags |= GPUTextureCreateFlags::Premultiplied;
  }
  if (limit_size) {
    create_flags |= GPUTextureCreateFlags::LimitSize;
  }
  gpu::Texture *tex = IMB_create_gpu_texture(name, ibuf, create_flags);
  if (tex == nullptr) {
    ibuf->gpu.flag |= IMB_GPU_LOAD_FAILED;
    ibuf->gpu.lastused = BLI_time_now_seconds_i();
    return nullptr;
  }
  ibuf->gpu.flag &= ~IMB_GPU_LOAD_FAILED;

  GPU_texture_extend_mode(tex, GPU_SAMPLER_EXTEND_MODE_REPEAT);

  if (!(ibuf->gpu.flag & IMB_GPU_DISABLE_MIPMAP_UPDATE)) {
    GPU_texture_update_mipmap_chain(tex);
    GPU_texture_mipmap_mode(tex, true, true);
    ibuf->gpu.flag |= IMB_GPU_MIPMAP_COMPLETE;
  }
  else {
    GPU_texture_mipmap_mode(tex, false, true);
  }

  ibuf->gpu.partial_update_changeset = changeset_id;
  ibuf->gpu.texture = tex;
  ibuf->gpu.lastused = BLI_time_now_seconds_i();
  GPU_texture_ref(tex);
  return tex;
}

gpu::TextureFormat IMB_gpu_get_texture_format(const ImBuf *ibuf,
                                              bool high_bitdepth,
                                              bool use_grayscale)
{
  gpu::TextureFormat gpu_texture_format;
  imb_gpu_get_format(ibuf, high_bitdepth, use_grayscale, &gpu_texture_format);
  return gpu_texture_format;
}

void IMB_free_gpu_textures(ImBuf *ibuf)
{
  if (!ibuf) {
    return;
  }

  std::scoped_lock lock(ibuf->gpu.mutex);
  if (ibuf->gpu.texture) {
    GPU_texture_free(ibuf->gpu.texture);
    ibuf->gpu.texture = nullptr;
  }
  ibuf->gpu.flag = ImBufGPUFlag(0);
}

void IMB_assign_gpu_texture(ImBuf *ibuf, gpu::Texture *texture)
{
  if (!ibuf) {
    return;
  }

  std::scoped_lock lock(ibuf->gpu.mutex);
  if (ibuf->gpu.texture) {
    GPU_texture_free(ibuf->gpu.texture);
    ibuf->gpu.texture = nullptr;
  }
  ibuf->gpu.flag = ImBufGPUFlag(0);
  ibuf->gpu.partial_update_changeset = IMB_partial_update_changeset_id_current();
  ibuf->gpu.texture = texture;
}

void IMB_gpu_clamp_half_float(ImBuf *image_buffer)
{
  const float half_min = -65504;
  const float half_max = 65504;
  if (!image_buffer->float_data()) {
    return;
  }

  float *rect_float = image_buffer->float_data_for_write();

  int rect_float_len = image_buffer->x * image_buffer->y *
                       (image_buffer->channels == 0 ? 4 : image_buffer->channels);

  for (int i = 0; i < rect_float_len; i++) {
    rect_float[i] = clamp_f(rect_float[i], half_min, half_max);
  }
}

}  // namespace blender
