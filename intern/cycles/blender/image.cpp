/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "MEM_guardedalloc.h"

#include "blender/image.h"
#include "blender/session.h"
#include "blender/util.h"

#include "util/half.h"

CCL_NAMESPACE_BEGIN

/* Packed Images */

BlenderImageLoader::BlenderImageLoader(BL::Image b_image,
                                       const int frame,
                                       const int tile_number,
                                       const bool is_preview_render)
    : b_image(b_image),
      frame(frame),
      tile_number(tile_number),
      /* Don't free cache for preview render to avoid race condition from #93560, to be fixed
       * properly later as we are close to release. */
      free_cache(!is_preview_render && !b_image.has_data())
{
}

bool BlenderImageLoader::load_metadata(const ImageDeviceFeatures &, ImageMetaData &metadata)
{
  if (b_image.source() != BL::Image::source_TILED) {
    /* Image sequence might have different dimensions, and hence needs to be handled in a special
     * manner.
     * NOTE: Currently the sequences are not handled by this image loader. */
    assert(b_image.source() != BL::Image::source_SEQUENCE);

    metadata.width = b_image.size()[0];
    metadata.height = b_image.size()[1];
    metadata.channels = b_image.channels();
  }
  else {
    /* Different UDIM tiles might have different resolutions, so get resolution from the actual
     * tile. */
    BL::UDIMTile b_udim_tile = b_image.tiles.get(tile_number);
    if (b_udim_tile) {
      metadata.width = b_udim_tile.size()[0];
      metadata.height = b_udim_tile.size()[1];
      metadata.channels = b_udim_tile.channels();
    }
    else {
      metadata.width = 0;
      metadata.height = 0;
      metadata.channels = 0;
    }
  }

  metadata.depth = 1;

  if (b_image.is_float()) {
    if (metadata.channels == 1) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT;
    }
    else if (metadata.channels == 4) {
      metadata.type = IMAGE_DATA_TYPE_FLOAT4;
    }
    else {
      return false;
    }

    /* Float images are already converted on the Blender side,
     * no need to do anything in Cycles. */
    metadata.colorspace = u_colorspace_raw;
  }
  else {
    /* In some cases (e.g. #94135), the colorspace setting in Blender gets updated as part of the
     * metadata queries in this function, so update the colorspace setting here. */
    PointerRNA colorspace_ptr = b_image.colorspace_settings().ptr;
    metadata.colorspace = get_enum_identifier(colorspace_ptr, "name");

    if (metadata.channels == 1) {
      metadata.type = IMAGE_DATA_TYPE_BYTE;
    }
    else if (metadata.channels == 4) {
      metadata.type = IMAGE_DATA_TYPE_BYTE4;
    }
    else {
      return false;
    }
  }

  return true;
}

bool BlenderImageLoader::load_pixels(const ImageMetaData &metadata,
                                     void *out_pixels,
                                     const size_t out_pixels_size,
                                     const bool associate_alpha)
{
  const size_t num_pixels = ((size_t)metadata.width) * metadata.height;
  const int channels = metadata.channels;

  if (metadata.type == IMAGE_DATA_TYPE_FLOAT || metadata.type == IMAGE_DATA_TYPE_FLOAT4) {
    /* Float. */
    float *in_pixels = image_get_float_pixels_for_frame(b_image, frame, tile_number);

    if (in_pixels && num_pixels * channels == out_pixels_size) {
      /* Straight copy pixel data. */
      memcpy(out_pixels, in_pixels, out_pixels_size * sizeof(float));
    }
    else {
      /* Missing or invalid pixel data. */
      if (channels == 1) {
        memset(out_pixels, 0, num_pixels * sizeof(float));
      }
      else {
        const size_t num_pixels_safe = out_pixels_size / channels;
        float *out_pixel = (float *)out_pixels;
        for (int i = 0; i < num_pixels_safe; i++, out_pixel += channels) {
          out_pixel[0] = 1.0f;
          out_pixel[1] = 0.0f;
          out_pixel[2] = 1.0f;
          if (channels == 4) {
            out_pixel[3] = 1.0f;
          }
        }
      }
    }

    if (in_pixels) {
      MEM_freeN(in_pixels);
    }
  }
  else if (metadata.type == IMAGE_DATA_TYPE_HALF || metadata.type == IMAGE_DATA_TYPE_HALF4) {
    /* Half float. Blender does not have a half type, but in some cases
     * we up-sample byte to half to avoid precision loss for colorspace
     * conversion. */
    unsigned char *in_pixels = image_get_pixels_for_frame(b_image, frame, tile_number);

    if (in_pixels && num_pixels * channels == out_pixels_size) {
      /* Convert uchar to half. */
      const uchar *in_pixel = in_pixels;
      half *out_pixel = (half *)out_pixels;
      if (associate_alpha && channels == 4) {
        for (size_t i = 0; i < num_pixels; i++, in_pixel += 4, out_pixel += 4) {
          const float alpha = util_image_cast_to_float(in_pixel[3]);
          out_pixel[0] = float_to_half_image(util_image_cast_to_float(in_pixel[0]) * alpha);
          out_pixel[1] = float_to_half_image(util_image_cast_to_float(in_pixel[1]) * alpha);
          out_pixel[2] = float_to_half_image(util_image_cast_to_float(in_pixel[2]) * alpha);
          out_pixel[3] = float_to_half_image(alpha);
        }
      }
      else {
        for (size_t i = 0; i < num_pixels; i++) {
          for (int c = 0; c < channels; c++, in_pixel++, out_pixel++) {
            *out_pixel = float_to_half_image(util_image_cast_to_float(*in_pixel));
          }
        }
      }
    }
    else {
      /* Missing or invalid pixel data. */
      if (channels == 1) {
        memset(out_pixels, 0, num_pixels * sizeof(half));
      }
      else {
        const size_t num_pixels_safe = out_pixels_size / channels;
        half *out_pixel = (half *)out_pixels;
        for (int i = 0; i < num_pixels_safe; i++, out_pixel += channels) {
          out_pixel[0] = float_to_half_image(1.0f);
          out_pixel[1] = float_to_half_image(0.0f);
          out_pixel[2] = float_to_half_image(1.0f);
          if (channels == 4) {
            out_pixel[3] = float_to_half_image(1.0f);
          }
        }
      }
    }

    if (in_pixels) {
      MEM_freeN(in_pixels);
    }
  }
  else {
    /* Byte. */
    unsigned char *in_pixels = image_get_pixels_for_frame(b_image, frame, tile_number);

    if (in_pixels && num_pixels * channels == out_pixels_size) {
      /* Straight copy pixel data. */
      memcpy(out_pixels, in_pixels, out_pixels_size * sizeof(unsigned char));

      if (associate_alpha && channels == 4) {
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
      if (channels == 1) {
        memset(out_pixels, 0, out_pixels_size * sizeof(unsigned char));
      }
      else {
        const size_t num_pixels_safe = out_pixels_size / channels;
        unsigned char *out_pixel = (unsigned char *)out_pixels;
        for (size_t i = 0; i < num_pixels_safe; i++, out_pixel += channels) {
          out_pixel[0] = 255;
          out_pixel[1] = 0;
          out_pixel[2] = 255;
          if (channels == 4) {
            out_pixel[3] = 255;
          }
        }
      }
    }

    if (in_pixels) {
      MEM_freeN(in_pixels);
    }
  }

  /* Free image buffers to save memory during render. */
  if (free_cache) {
    b_image.buffers_free();
  }

  return true;
}

string BlenderImageLoader::name() const
{
  return BL::Image(b_image).name();
}

bool BlenderImageLoader::equals(const ImageLoader &other) const
{
  const BlenderImageLoader &other_loader = (const BlenderImageLoader &)other;
  return b_image == other_loader.b_image && frame == other_loader.frame &&
         tile_number == other_loader.tile_number;
}

int BlenderImageLoader::get_tile_number() const
{
  return tile_number;
}

/* Point Density */

BlenderPointDensityLoader::BlenderPointDensityLoader(BL::Depsgraph b_depsgraph,
                                                     BL::ShaderNodeTexPointDensity b_node)
    : b_depsgraph(b_depsgraph), b_node(b_node)
{
}

bool BlenderPointDensityLoader::load_metadata(const ImageDeviceFeatures &, ImageMetaData &metadata)
{
  metadata.channels = 4;
  metadata.width = b_node.resolution();
  metadata.height = metadata.width;
  metadata.depth = metadata.width;
  metadata.type = IMAGE_DATA_TYPE_FLOAT4;
  return true;
}

bool BlenderPointDensityLoader::load_pixels(const ImageMetaData &,
                                            void *pixels,
                                            const size_t,
                                            const bool)
{
  int length;
  b_node.calc_point_density(b_depsgraph, &length, (float **)&pixels);
  return true;
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
  ImageManager *manager = session->scene->image_manager;
  Device *device = session->device;
  manager->device_load_builtin(device, session->scene, session->progress);
}

string BlenderPointDensityLoader::name() const
{
  return BL::ShaderNodeTexPointDensity(b_node).name();
}

bool BlenderPointDensityLoader::equals(const ImageLoader &other) const
{
  const BlenderPointDensityLoader &other_loader = (const BlenderPointDensityLoader &)other;
  return b_node == other_loader.b_node && b_depsgraph == other_loader.b_depsgraph;
}

CCL_NAMESPACE_END
