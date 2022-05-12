/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "MEM_guardedalloc.h"

#include "blender/image.h"
#include "blender/session.h"
#include "blender/util.h"

CCL_NAMESPACE_BEGIN

/* Packed Images */

BlenderImageLoader::BlenderImageLoader(BL::Image b_image,
                                       const int frame,
                                       const int tile_number,
                                       const bool is_preview_render)
    : b_image(b_image),
      frame(frame),
      tile_number(tile_number),
      /* Don't free cache for preview render to avoid race condition from T93560, to be fixed
         properly later as we are close to release. */
      free_cache(!is_preview_render && !b_image.has_data())
{
}

bool BlenderImageLoader::load_metadata(const ImageDeviceFeatures &, ImageMetaData &metadata)
{
  metadata.width = b_image.size()[0];
  metadata.height = b_image.size()[1];
  metadata.depth = 1;
  metadata.channels = b_image.channels();

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
                                     void *pixels,
                                     const size_t pixels_size,
                                     const bool associate_alpha)
{
  const size_t num_pixels = ((size_t)metadata.width) * metadata.height;
  const int channels = metadata.channels;

  if (b_image.is_float()) {
    /* image data */
    float *image_pixels;
    image_pixels = image_get_float_pixels_for_frame(b_image, frame, tile_number);

    if (image_pixels && num_pixels * channels == pixels_size) {
      memcpy(pixels, image_pixels, pixels_size * sizeof(float));
    }
    else {
      if (channels == 1) {
        memset(pixels, 0, num_pixels * sizeof(float));
      }
      else {
        const size_t num_pixels_safe = pixels_size / channels;
        float *fp = (float *)pixels;
        for (int i = 0; i < num_pixels_safe; i++, fp += channels) {
          fp[0] = 1.0f;
          fp[1] = 0.0f;
          fp[2] = 1.0f;
          if (channels == 4) {
            fp[3] = 1.0f;
          }
        }
      }
    }

    if (image_pixels) {
      MEM_freeN(image_pixels);
    }
  }
  else {
    unsigned char *image_pixels = image_get_pixels_for_frame(b_image, frame, tile_number);

    if (image_pixels && num_pixels * channels == pixels_size) {
      memcpy(pixels, image_pixels, pixels_size * sizeof(unsigned char));
    }
    else {
      if (channels == 1) {
        memset(pixels, 0, pixels_size * sizeof(unsigned char));
      }
      else {
        const size_t num_pixels_safe = pixels_size / channels;
        unsigned char *cp = (unsigned char *)pixels;
        for (size_t i = 0; i < num_pixels_safe; i++, cp += channels) {
          cp[0] = 255;
          cp[1] = 0;
          cp[2] = 255;
          if (channels == 4) {
            cp[3] = 255;
          }
        }
      }
    }

    if (image_pixels) {
      MEM_freeN(image_pixels);
    }

    if (associate_alpha) {
      /* Premultiply, byte images are always straight for Blender. */
      unsigned char *cp = (unsigned char *)pixels;
      for (size_t i = 0; i < num_pixels; i++, cp += channels) {
        cp[0] = (cp[0] * cp[3]) / 255;
        cp[1] = (cp[1] * cp[3]) / 255;
        cp[2] = (cp[2] * cp[3]) / 255;
      }
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
