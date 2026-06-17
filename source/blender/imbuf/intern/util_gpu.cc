/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_mutex.hh"
#include "BLI_time.hh"
#include "BLI_utildefines.hh"

#include "MEM_guardedalloc.h"

#include <mutex>

#include "CLG_log.h"

#include "GPU_capabilities.hh"
#include "GPU_texture.hh"

#include "IMB_colormanagement.hh"
#include "IMB_filetype.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

namespace blender {

static CLG_LogRef LOG = {"image.gpu"};

/* gpu ibuf utils */

static bool imb_is_grayscale_texture_format_compatible(const ImBuf *ibuf)
{
  if (ibuf->color_mode != ImColorMode::BW) {
    return false;
  }

  if (ibuf->byte_data() && !ibuf->float_data()) {

    if (IMB_colormanagement_space_is_srgb(ibuf->byte_buffer.colorspace) ||
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
      IMB_colormanagement_space_is_srgb(ibuf->float_buffer.colorspace) ||
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
    else if (IMB_colormanagement_space_is_srgb(ibuf->byte_buffer.colorspace)) {
      /* sRGB, store as byte texture that the GPU can decode directly. */
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

/* Returns an image buffer containing the data from the source buffer but suitable for uploading to
 * GPU textures. The output should be freed by the caller. If rescale_size is not nullopt, the
 * image will be scaled using a box filter to match the rescale size. If premultiplied_alpha is
 * true and alpha is not packed, alpha is ensured to be premultiplied, otherwise, it is ensured to
 * be straight. If allow_grayscale is true, if the image could be stored as a grayscale image with
 * no data loss, it will be return as so, otherwise, it will be RGBA. */
static ImBuf *get_gpu_texture_data(ImBuf *source_buffer,
                                   const std::optional<int2> rescale_size,
                                   const bool premultiplied_alpha,
                                   const bool allow_grayscale)
{
  /* Start with a copy of the source buffer, this will be processed further if needed below. */
  ImBuf *output_buffer = IMB_allocImBuf(source_buffer->x, source_buffer->y, ImBufFlags::Zero);
  output_buffer->color_mode = source_buffer->color_mode;
  output_buffer->flags = source_buffer->flags;
  output_buffer->float_buffer = source_buffer->float_buffer;
  output_buffer->channels = source_buffer->channels;
  output_buffer->byte_buffer = source_buffer->byte_buffer;

  const bool is_grayscale = allow_grayscale &&
                            imb_is_grayscale_texture_format_compatible(source_buffer);

  /* Ensure that the output buffer is RGBA with the given alpha association. */
  if (output_buffer->float_data()) {
    /* Float images are already in scene linear color space or contain non-color data with
     * premultiplied alpha by convention, so no color space conversion is needed. But we need to
     * convert to RGBA and unpremultiply alpha if needed. */
    if (output_buffer->channels != 4 || !premultiplied_alpha) {
      ImBuf *buffer = IMB_allocImBuf(output_buffer->x,
                                     output_buffer->y,
                                     ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
      IMB_colormanagement_imbuf_to_float_texture(buffer->float_data_for_write(),
                                                 0,
                                                 0,
                                                 output_buffer->x,
                                                 output_buffer->y,
                                                 output_buffer,
                                                 premultiplied_alpha);
      IMB_freeImBuf(output_buffer);
      output_buffer = buffer;
    }
  }
  else {
    /* Byte images are in original color space from the file with straight alpha, so we need to
     * convert to scene linear color space and premultiply alpha if needed. An exception is images
     * in sRGB color space, see the relevant case below for more information.  */
    if (IMB_colormanagement_space_is_data(source_buffer->byte_buffer.colorspace)) {
      /* Non-color data, used as is. */
    }
    else if (IMB_colormanagement_space_is_scene_linear(source_buffer->byte_buffer.colorspace)) {
      /* Color space is already linear, we just need to premultiply the alpha if needed. */
      if (!is_grayscale && premultiplied_alpha) {
        ImBuf *buffer = IMB_allocImBuf(output_buffer->x,
                                       output_buffer->y,
                                       ImBufFlags::ByteData | ImBufFlags::UninitializedPixels);
        IMB_colormanagement_imbuf_to_byte_texture(buffer->byte_data_for_write(),
                                                  0,
                                                  0,
                                                  output_buffer->x,
                                                  output_buffer->y,
                                                  output_buffer,
                                                  premultiplied_alpha);
        IMB_freeImBuf(output_buffer);
        output_buffer = buffer;
      }
    }
    else if (IMB_colormanagement_space_is_srgb(source_buffer->byte_buffer.colorspace)) {
      /* Images in sRGB color space are a special case since they can be stored in sRGB textures on
       * GPU, where scene linear space conversion will happen during sampling in the shader,
       * however, this is not possible for grayscale images, so we need to do the conversion here,
       * converting to a float image to prevent precision loss.
       *
       * It should be noted that for other color spaces, color space conversion happen before alpha
       * premultiplication, while for sRGB, premultiplication will happen first since color space
       * conversion happen in the shader as mentioned above. This will manifest as differences near
       * alpha edges. But we generally accept this due to the advantages of sRGB textures. If this
       * is problematic, the source image buffer should be linearised first. */
      if (is_grayscale) {
        ImBuf *buffer = IMB_allocImBuf(output_buffer->x,
                                       output_buffer->y,
                                       ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
        IMB_colormanagement_imbuf_to_float_texture(buffer->float_data_for_write(),
                                                   0,
                                                   0,
                                                   output_buffer->x,
                                                   output_buffer->y,
                                                   output_buffer,
                                                   premultiplied_alpha);
        IMB_freeImBuf(output_buffer);
        output_buffer = buffer;
      }
      else if (premultiplied_alpha) {
        /* We need to premultiply the alpha. */
        ImBuf *buffer = IMB_allocImBuf(output_buffer->x,
                                       output_buffer->y,
                                       ImBufFlags::ByteData | ImBufFlags::UninitializedPixels);
        IMB_colormanagement_imbuf_to_byte_texture(buffer->byte_data_for_write(),
                                                  0,
                                                  0,
                                                  output_buffer->x,
                                                  output_buffer->y,
                                                  output_buffer,
                                                  premultiplied_alpha);
        IMB_freeImBuf(output_buffer);
        output_buffer = buffer;
      }
    }
    else {
      /* Other color-space, convert to linear color space and premultiply alpha if needed.
       * Conversion happen as a float to avoid precision loss. */
      ImBuf *buffer = IMB_allocImBuf(output_buffer->x,
                                     output_buffer->y,
                                     ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
      IMB_colormanagement_imbuf_to_float_texture(buffer->float_data_for_write(),
                                                 0,
                                                 0,
                                                 output_buffer->x,
                                                 output_buffer->y,
                                                 output_buffer,
                                                 premultiplied_alpha);
      IMB_freeImBuf(output_buffer);
      output_buffer = buffer;
    }
  }

  /* Rescale the image using a box filter if needed. */
  if (rescale_size.has_value()) {
    if (output_buffer->float_data()) {
      ImBuf *buffer = IMB_allocImBuf(rescale_size->x,
                                     rescale_size->y,
                                     ImBufFlags::FloatData | ImBufFlags::UninitializedPixels);
      IMB_scale_box(output_buffer->float_data(),
                    int2(output_buffer->x, output_buffer->y),
                    4,
                    buffer->float_data_for_write(),
                    *rescale_size,
                    true);
      IMB_freeImBuf(output_buffer);
      output_buffer = buffer;
    }
    else {
      ImBuf *buffer = IMB_allocImBuf(rescale_size->x,
                                     rescale_size->y,
                                     ImBufFlags::ByteData | ImBufFlags::UninitializedPixels);
      IMB_scale_box(output_buffer->byte_data(),
                    int2(output_buffer->x, output_buffer->y),
                    4,
                    buffer->byte_data_for_write(),
                    *rescale_size,
                    true);
      IMB_freeImBuf(output_buffer);
      output_buffer = buffer;
    }
  }

  /* So far, the output buffer is RGBA, if it should be grayscale, we need to convert it to a
   * single channel image. */
  if (is_grayscale) {
    const size_t buffer_size = output_buffer->x * output_buffer->y;
    if (output_buffer->float_data()) {
      ImBuf *buffer = IMB_allocImBuf(output_buffer->x, output_buffer->y, ImBufFlags::Zero);
      IMB_alloc_float_pixels(buffer, 1);
      const float *source_pixels = output_buffer->float_data();
      float *target_pixels = buffer->float_data_for_write();
      for (size_t i = 0; i < buffer_size; i++) {
        target_pixels[i] = source_pixels[i * 4];
      }
      IMB_freeImBuf(output_buffer);
      output_buffer = buffer;
    }
    else {
      ImBuf *buffer = IMB_allocImBuf(output_buffer->x, output_buffer->y, ImBufFlags::Zero);
      buffer->color_mode = ImColorMode::BW;
      buffer->assign_byte_data(MEM_new_array_uninitialized<uint8_t>(buffer_size, __func__));
      const uint8_t *source_pixels = output_buffer->byte_data();
      uint8_t *target_pixels = buffer->byte_data_for_write();
      for (size_t i = 0; i < buffer_size; i++) {
        target_pixels[i] = source_pixels[i * 4];
      }
      IMB_freeImBuf(output_buffer);
      output_buffer = buffer;
    }
  }

  return output_buffer;
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
  const std::optional<int2> rescale_size = (ibuf->x != w || ibuf->y != h) ?
                                               std::optional<int2>(int2(w, h)) :
                                               std::nullopt;
  ImBuf *data = get_gpu_texture_data(ibuf, rescale_size, use_premult, use_grayscale);

  if (data->float_data()) {
    GPU_texture_update_sub(tex, GPU_DATA_FLOAT, data->float_data(), x, y, z, w, h, 1);
  }
  else {
    GPU_texture_update_sub(tex, GPU_DATA_UBYTE, data->byte_data(), x, y, z, w, h, 1);
  }
  IMB_freeImBuf(data);
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
  const std::optional<int2> rescale_size = do_rescale ? std::optional<int2>(int2(size)) :
                                                        std::nullopt;
  ImBuf *data = get_gpu_texture_data(
      ibuf, rescale_size, flag_is_set(flags, GPUTextureCreateFlags::Premultiplied), true);
  if (data->float_data()) {
    GPU_texture_update(tex, GPU_DATA_FLOAT, data->float_data());
  }
  else {
    GPU_texture_update(tex, GPU_DATA_UBYTE, data->byte_data());
  }
  IMB_freeImBuf(data);

  GPU_texture_swizzle_set(tex, imb_gpu_get_swizzle(ibuf));
  GPU_texture_anisotropic_filter(tex, true);

  return tex;
}

gpu::Texture *IMB_acquire_gpu_texture(const char *name,
                                      ImBuf *ibuf,
                                      bool use_high_bitdepth,
                                      bool use_premult,
                                      bool limit_size,
                                      bool try_only)
{
  if (ibuf == nullptr) {
    return nullptr;
  }

  std::scoped_lock lock(ibuf->gpu.mutex);
  if (ibuf->gpu.texture != nullptr) {
    ibuf->gpu.lastused = BLI_time_now_seconds_i();
    GPU_texture_ref(ibuf->gpu.texture);
    return ibuf->gpu.texture;
  }
  if (try_only) {
    return nullptr;
  }

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
    return nullptr;
  }

  GPU_texture_extend_mode(tex, GPU_SAMPLER_EXTEND_MODE_REPEAT);

  if (!(ibuf->gpu.flag & IMB_GPU_DISABLE_MIPMAP_UPDATE)) {
    GPU_texture_update_mipmap_chain(tex);
    GPU_texture_mipmap_mode(tex, true, true);
    ibuf->gpu.flag |= IMB_GPU_MIPMAP_COMPLETE;
  }
  else {
    GPU_texture_mipmap_mode(tex, false, true);
  }

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
