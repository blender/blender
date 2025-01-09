/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>

#include "DNA_image_types.h"

#include "IMB_imbuf_types.hh"

#include "BKE_image.hh"

#include "blender/image.h"
#include "blender/session.h"

#include "util/half.h"
#include "util/transform.h"
#include "util/types_float4.h"

CCL_NAMESPACE_BEGIN

/* Packed Images */

BlenderImageLoader::BlenderImageLoader(Image *b_image,
                                       ImageUser *b_iuser,
                                       const int frame,
                                       const int tile_number,
                                       const bool is_preview_render)
    : b_image(b_image),
      b_iuser(*b_iuser),
      /* Don't free cache for preview render to avoid race condition from #93560, to be fixed
       * properly later as we are close to release. */
      free_cache(!is_preview_render && !BKE_image_has_loaded_ibuf(b_image))
{
  this->b_iuser.framenr = frame;
  if (b_image->source != IMA_SRC_TILED) {
    /* Image sequences currently not supported by this image loader. */
    assert(b_image->source != IMA_SRC_SEQUENCE);
  }
  else {
    /* Set UDIM tile, each can have different resolution. */
    this->b_iuser.tile = tile_number;
  }
}

bool BlenderImageLoader::load_metadata(const ImageDeviceFeatures & /*features*/,
                                       ImageMetaData &metadata)
{
  bool is_float = false;

  {
    void *lock;
    ImBuf *ibuf = BKE_image_acquire_ibuf(b_image, &b_iuser, &lock);
    if (ibuf) {
      is_float = ibuf->float_buffer.data != nullptr;
      metadata.width = ibuf->x;
      metadata.height = ibuf->y;
      metadata.channels = (is_float) ? ibuf->channels : 4;
    }
    else {
      metadata.width = 0;
      metadata.height = 0;
      metadata.channels = 0;
    }
    BKE_image_release_ibuf(b_image, ibuf, lock);
  }

  if (is_float) {
    if (metadata.channels == 1) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT;
    }
    else {
      metadata.channels = 4;
      metadata.type = IMAGE_DATA_TYPE_FLOAT4;
    }

    /* Float images are already converted on the Blender side,
     * no need to do anything in Cycles. */
    metadata.colorspace = u_colorspace_raw;
  }
  else {
    /* In some cases (e.g. #94135), the colorspace setting in Blender gets updated as part of the
     * metadata queries in this function, so update the colorspace setting here. */
    metadata.colorspace = b_image->colorspace_settings.name;
    metadata.type = IMAGE_DATA_TYPE_BYTE4;
  }

  return true;
}

static void load_float_pixels(const ImBuf *ibuf, const ImageMetaData &metadata, float *out_pixels)
{
  const size_t num_pixels = ((size_t)metadata.width) * metadata.height;
  const int out_channels = metadata.channels;
  const int in_channels = ibuf->channels;
  const float *in_pixels = ibuf->float_buffer.data;

  if (in_pixels && out_channels == in_channels) {
    /* Straight copy pixel data. */
    memcpy(out_pixels, in_pixels, num_pixels * out_channels * sizeof(float));
  }
  else if (in_pixels && out_channels == 4) {
    /* Fill channels to 4. */
    float *out_pixel = out_pixels;
    const float *in_pixel = in_pixels;
    for (size_t i = 0; i < num_pixels; i++) {
      out_pixel[0] = in_pixel[0];
      out_pixel[1] = (in_channels >= 2) ? in_pixel[1] : 0.0f;
      out_pixel[2] = (in_channels >= 3) ? in_pixel[2] : 0.0f;
      out_pixel[3] = (in_channels >= 4) ? in_pixel[3] : 1.0f;
      out_pixel += out_channels;
      in_pixel += in_channels;
    }
  }
  else {
    /* Missing or invalid pixel data. */
    if (out_channels == 1) {
      std::fill(out_pixels, out_pixels + num_pixels, 0.0f);
    }
    else {
      std::fill((float4 *)out_pixels,
                (float4 *)out_pixels + num_pixels,
                make_float4(1.0f, 0.0f, 1.0f, 1.0f));
    }
  }
}

static void load_half_pixels(const ImBuf *ibuf,
                             const ImageMetaData &metadata,
                             half *out_pixels,
                             const bool associate_alpha)
{
  /* Half float. Blender does not have a half type, but in some cases
   * we up-sample byte to half to avoid precision loss for colorspace
   * conversion. */
  const size_t num_pixels = ((size_t)metadata.width) * metadata.height;
  const int out_channels = metadata.channels;
  const int in_channels = 4;
  const uchar *in_pixels = ibuf->byte_buffer.data;

  if (in_pixels) {
    /* Convert uchar to half. */
    const uchar *in_pixel = in_pixels;
    half *out_pixel = out_pixels;
    if (associate_alpha && out_channels == in_channels) {
      for (size_t i = 0; i < num_pixels; i++, in_pixel += in_channels, out_pixel += out_channels) {
        const float alpha = util_image_cast_to_float(in_pixel[3]);
        out_pixel[0] = float_to_half_image(util_image_cast_to_float(in_pixel[0]) * alpha);
        out_pixel[1] = float_to_half_image(util_image_cast_to_float(in_pixel[1]) * alpha);
        out_pixel[2] = float_to_half_image(util_image_cast_to_float(in_pixel[2]) * alpha);
        out_pixel[3] = float_to_half_image(alpha);
      }
    }
    else {
      for (size_t i = 0; i < num_pixels; i++) {
        for (int c = 0; c < out_channels; c++, in_pixel++, out_pixel++) {
          *out_pixel = float_to_half_image(util_image_cast_to_float(*in_pixel));
        }
      }
    }
  }
  else {
    /* Missing or invalid pixel data. */
    if (out_channels == 1) {
      std::fill(out_pixels, out_pixels + num_pixels, float_to_half_image(0.0f));
    }
    else {
      std::fill((half4 *)out_pixels,
                (half4 *)out_pixels + num_pixels,
                float4_to_half4_display(make_float4(1.0f, 0.0f, 1.0f, 1.0f)));
    }
  }
}

static void load_byte_pixels(const ImBuf *ibuf,
                             const ImageMetaData &metadata,
                             uchar *out_pixels,
                             const bool associate_alpha)
{
  const size_t num_pixels = ((size_t)metadata.width) * metadata.height;
  const int out_channels = metadata.channels;
  const int in_channels = 4;
  const uchar *in_pixels = ibuf->byte_buffer.data;

  if (in_pixels) {
    /* Straight copy pixel data. */
    memcpy(out_pixels, in_pixels, num_pixels * in_channels * sizeof(unsigned char));

    if (associate_alpha && out_channels == in_channels) {
      /* Premultiply, byte images are always straight for Blender. */
      unsigned char *out_pixel = (unsigned char *)out_pixels;
      for (size_t i = 0; i < num_pixels; i++, out_pixel += 4) {
        out_pixel[0] = (out_pixel[0] * out_pixel[3]) / 255;
        out_pixel[1] = (out_pixel[1] * out_pixel[3]) / 255;
        out_pixel[2] = (out_pixel[2] * out_pixel[3]) / 255;
      }
    }
  }
  else {
    /* Missing or invalid pixel data. */
    if (out_channels == 1) {
      std::fill(out_pixels, out_pixels + num_pixels, 0.0f);
    }
    else {
      std::fill(
          (uchar4 *)out_pixels, (uchar4 *)out_pixels + num_pixels, make_uchar4(255, 0, 255, 255));
    }
  }
}

bool BlenderImageLoader::load_pixels(const ImageMetaData &metadata,
                                     void *out_pixels,
                                     const size_t /*out_pixels_size*/,
                                     const bool associate_alpha)
{
  void *lock;
  ImBuf *ibuf = BKE_image_acquire_ibuf(b_image, &b_iuser, &lock);

  /* Image changed since we requested metadata, assume we'll get a signal to reload it later. */
  const bool mismatch = (ibuf == nullptr || ibuf->x != metadata.width ||
                         ibuf->y != metadata.height);

  if (!mismatch) {
    if (metadata.type == IMAGE_DATA_TYPE_FLOAT || metadata.type == IMAGE_DATA_TYPE_FLOAT4) {
      load_float_pixels(ibuf, metadata, (float *)out_pixels);
    }
    else if (metadata.type == IMAGE_DATA_TYPE_HALF || metadata.type == IMAGE_DATA_TYPE_HALF4) {
      load_half_pixels(ibuf, metadata, (half *)out_pixels, associate_alpha);
    }
    else {
      load_byte_pixels(ibuf, metadata, (uchar *)out_pixels, associate_alpha);
    }
  }

  BKE_image_release_ibuf(b_image, ibuf, lock);

  /* Free image buffers to save memory during render. */
  if (free_cache) {
    BKE_image_free_buffers_ex(b_image, true);
  }

  return !mismatch;
}

string BlenderImageLoader::name() const
{
  return b_image->id.name + 2;
}

bool BlenderImageLoader::equals(const ImageLoader &other) const
{
  const BlenderImageLoader &other_loader = (const BlenderImageLoader &)other;
  return b_image == other_loader.b_image && b_iuser.framenr == other_loader.b_iuser.framenr &&
         b_iuser.tile == other_loader.b_iuser.tile;
}

int BlenderImageLoader::get_tile_number() const
{
  return b_iuser.tile;
}

void BlenderSession::builtin_images_load()
{
  /* Force builtin images to be loaded along with Blender data sync. This
   * is needed because we may be reading from depsgraph evaluated data which
   * can be freed by Blender before Cycles reads it.
   *
   * TODO: the assumption that no further access to builtin image data will
   * happen is really weak, and likely to break in the future. We should find
   * a better solution to hand over the data directly to the image manager
   * instead of through callbacks whose timing is difficult to control. */
  ImageManager *manager = session->scene->image_manager.get();
  Device *device = session->device.get();
  manager->device_load_builtin(device, session->scene.get(), session->progress);
}

CCL_NAMESPACE_END
