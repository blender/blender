/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "intern/image.h"
#include "intern/utildefines.h"
#include "libmv/tracking/track_region.h"

#include <cassert>
#include <png.h>

using libmv::FloatImage;
using libmv::SamplePlanarPatch;

void libmv_floatImageDestroy(libmv_FloatImage *image) {
  delete [] image->buffer;
}

/* Image <-> buffers conversion */

void libmv_byteBufferToFloatImage(const unsigned char* buffer,
                                  int width,
                                  int height,
                                  int channels,
                                  FloatImage* image) {
  image->Resize(height, width, channels);
  for (int y = 0, a = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      for (int k = 0; k < channels; k++) {
        (*image)(y, x, k) = (float)buffer[a++] / 255.0f;
      }
    }
  }
}

void libmv_floatBufferToFloatImage(const float* buffer,
                                   int width,
                                   int height,
                                   int channels,
                                   FloatImage* image) {
  image->Resize(height, width, channels);
  for (int y = 0, a = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      for (int k = 0; k < channels; k++) {
        (*image)(y, x, k) = buffer[a++];
      }
    }
  }
}

void libmv_floatImageToFloatBuffer(const FloatImage &image,
                                   float* buffer) {
  for (int y = 0, a = 0; y < image.Height(); y++) {
    for (int x = 0; x < image.Width(); x++) {
      for (int k = 0; k < image.Depth(); k++) {
        buffer[a++] = image(y, x, k);
      }
    }
  }
}

void libmv_floatImageToByteBuffer(const libmv::FloatImage &image,
                                  unsigned char* buffer) {
  for (int y = 0, a= 0; y < image.Height(); y++) {
    for (int x = 0; x < image.Width(); x++) {
      for (int k = 0; k < image.Depth(); k++) {
        buffer[a++] = image(y, x, k) * 255.0f;
      }
    }
  }
}

static bool savePNGImage(png_bytep* row_pointers,
                         int width,
                         int height,
                         int depth,
                         int color_type,
                         const char* file_name) {
  png_infop info_ptr;
  png_structp png_ptr;
  FILE *fp = fopen(file_name, "wb");

  if (fp == NULL) {
    return false;
  }

  /* Initialize stuff */
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  info_ptr = png_create_info_struct(png_ptr);

  if (setjmp(png_jmpbuf(png_ptr))) {
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr, fp);

  /* Write PNG header. */
  if (setjmp(png_jmpbuf(png_ptr))) {
    fclose(fp);
    return false;
  }

  png_set_IHDR(png_ptr,
               info_ptr,
               width,
               height,
               depth,
               color_type,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);

  png_write_info(png_ptr, info_ptr);

  /* Write bytes/ */
  if (setjmp(png_jmpbuf(png_ptr))) {
    fclose(fp);
    return false;
  }

  png_write_image(png_ptr, row_pointers);

  /* End write/ */
  if (setjmp(png_jmpbuf(png_ptr))) {
    fclose(fp);
    return false;
  }

  png_write_end(png_ptr, NULL);
  fclose(fp);

  return true;
}

bool libmv_saveImage(const FloatImage& image,
                     const char* prefix,
                     int x0,
                     int y0) {
  int x, y;
  png_bytep *row_pointers;

  assert(image.Depth() == 1);

  row_pointers = new png_bytep[image.Height()];

  for (y = 0; y < image.Height(); y++) {
    row_pointers[y] = new png_byte[4 * image.Width()];

    for (x = 0; x < image.Width(); x++) {
      if (x0 == x && image.Height() - y0 - 1 == y) {
        row_pointers[y][x * 4 + 0] = 255;
        row_pointers[y][x * 4 + 1] = 0;
        row_pointers[y][x * 4 + 2] = 0;
        row_pointers[y][x * 4 + 3] = 255;
      } else {
        float pixel = image(image.Height() - y - 1, x, 0);
        row_pointers[y][x * 4 + 0] = pixel * 255;
        row_pointers[y][x * 4 + 1] = pixel * 255;
        row_pointers[y][x * 4 + 2] = pixel * 255;
        row_pointers[y][x * 4 + 3] = 255;
      }
    }
  }

  static int image_counter = 0;
  char file_name[128];
  snprintf(file_name, sizeof(file_name),
           "%s_%02d.png",
           prefix, ++image_counter);
  bool result = savePNGImage(row_pointers,
                             image.Width(),
                             image.Height(),
                             8,
                             PNG_COLOR_TYPE_RGBA,
                             file_name);

  for (y = 0; y < image.Height(); y++) {
    delete [] row_pointers[y];
  }
  delete [] row_pointers;

  return result;
}

void libmv_samplePlanarPatchFloat(const float* image,
                                  int width,
                                  int height,
                                  int channels,
                                  const double* xs,
                                  const double* ys,
                                  int num_samples_x,
                                  int num_samples_y,
                                  const float* mask,
                                  float* patch,
                                  double* warped_position_x,
                                  double* warped_position_y) {
  FloatImage libmv_image, libmv_patch, libmv_mask;
  FloatImage *libmv_mask_for_sample = NULL;

  libmv_floatBufferToFloatImage(image, width, height, channels, &libmv_image);

  if (mask) {
    libmv_floatBufferToFloatImage(mask, width, height, 1, &libmv_mask);
    libmv_mask_for_sample = &libmv_mask;
  }

  SamplePlanarPatch(libmv_image,
                    xs, ys,
                    num_samples_x, num_samples_y,
                    libmv_mask_for_sample,
                    &libmv_patch,
                    warped_position_x,
                    warped_position_y);

  libmv_floatImageToFloatBuffer(libmv_patch, patch);
}

void libmv_samplePlanarPatchByte(const unsigned char* image,
                                  int width,
                                  int height,
                                  int channels,
                                  const double* xs,
                                  const double* ys,
                                  int num_samples_x,
                                  int num_samples_y,
                                  const float* mask,
                                  unsigned char* patch,
                                  double* warped_position_x,
                                  double* warped_position_y) {
  libmv::FloatImage libmv_image, libmv_patch, libmv_mask;
  libmv::FloatImage *libmv_mask_for_sample = NULL;

  libmv_byteBufferToFloatImage(image, width, height, channels, &libmv_image);

  if (mask) {
    libmv_floatBufferToFloatImage(mask, width, height, 1, &libmv_mask);
    libmv_mask_for_sample = &libmv_mask;
  }

  libmv::SamplePlanarPatch(libmv_image,
                           xs, ys,
                           num_samples_x, num_samples_y,
                           libmv_mask_for_sample,
                           &libmv_patch,
                           warped_position_x,
                           warped_position_y);

  libmv_floatImageToByteBuffer(libmv_patch, patch);
}
