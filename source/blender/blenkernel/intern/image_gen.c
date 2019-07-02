/*
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
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <stdlib.h>

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "BKE_image.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLF_api.h"

typedef struct FillColorThreadData {
  unsigned char *rect;
  float *rect_float;
  int width;
  float color[4];
} FillColorThreadData;

static void image_buf_fill_color_slice(
    unsigned char *rect, float *rect_float, int width, int height, const float color[4])
{
  int x, y;

  /* blank image */
  if (rect_float) {
    float linear_color[4];
    srgb_to_linearrgb_v4(linear_color, color);
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        copy_v4_v4(rect_float, linear_color);
        rect_float += 4;
      }
    }
  }

  if (rect) {
    unsigned char ccol[4];
    rgba_float_to_uchar(ccol, color);
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
        rect[0] = ccol[0];
        rect[1] = ccol[1];
        rect[2] = ccol[2];
        rect[3] = ccol[3];
        rect += 4;
      }
    }
  }
}

static void image_buf_fill_color_thread_do(void *data_v, int start_scanline, int num_scanlines)
{
  FillColorThreadData *data = (FillColorThreadData *)data_v;
  size_t offset = ((size_t)start_scanline) * data->width * 4;
  unsigned char *rect = (data->rect != NULL) ? (data->rect + offset) : NULL;
  float *rect_float = (data->rect_float != NULL) ? (data->rect_float + offset) : NULL;
  image_buf_fill_color_slice(rect, rect_float, data->width, num_scanlines, data->color);
}

void BKE_image_buf_fill_color(
    unsigned char *rect, float *rect_float, int width, int height, const float color[4])
{
  if (((size_t)width) * height < 64 * 64) {
    image_buf_fill_color_slice(rect, rect_float, width, height, color);
  }
  else {
    FillColorThreadData data;
    data.rect = rect;
    data.rect_float = rect_float;
    data.width = width;
    copy_v4_v4(data.color, color);
    IMB_processor_apply_threaded_scanlines(height, image_buf_fill_color_thread_do, &data);
  }
}

static void image_buf_fill_checker_slice(
    unsigned char *rect, float *rect_float, int width, int height, int offset)
{
  /* these two passes could be combined into one, but it's more readable and
   * easy to tweak like this, speed isn't really that much of an issue in this situation... */

  int checkerwidth = 32, dark = 1;
  int x, y;

  unsigned char *rect_orig = rect;
  float *rect_float_orig = rect_float;

  float h = 0.0, hoffs = 0.0;
  float hsv[3] = {0.0f, 0.9f, 0.9f};
  float rgb[3];

  float dark_linear_color = 0.0f, bright_linear_color = 0.0f;
  if (rect_float != NULL) {
    dark_linear_color = srgb_to_linearrgb(0.25f);
    bright_linear_color = srgb_to_linearrgb(0.58f);
  }

  /* checkers */
  for (y = offset; y < height + offset; y++) {
    dark = powf(-1.0f, floorf(y / checkerwidth));

    for (x = 0; x < width; x++) {
      if (x % checkerwidth == 0) {
        dark = -dark;
      }

      if (rect_float) {
        if (dark > 0) {
          rect_float[0] = rect_float[1] = rect_float[2] = dark_linear_color;
          rect_float[3] = 1.0f;
        }
        else {
          rect_float[0] = rect_float[1] = rect_float[2] = bright_linear_color;
          rect_float[3] = 1.0f;
        }
        rect_float += 4;
      }
      else {
        if (dark > 0) {
          rect[0] = rect[1] = rect[2] = 64;
          rect[3] = 255;
        }
        else {
          rect[0] = rect[1] = rect[2] = 150;
          rect[3] = 255;
        }
        rect += 4;
      }
    }
  }

  rect = rect_orig;
  rect_float = rect_float_orig;

  /* 2nd pass, colored + */
  for (y = offset; y < height + offset; y++) {
    hoffs = 0.125f * floorf(y / checkerwidth);

    for (x = 0; x < width; x++) {
      h = 0.125f * floorf(x / checkerwidth);

      if ((abs((x % checkerwidth) - (checkerwidth / 2)) < 4) &&
          (abs((y % checkerwidth) - (checkerwidth / 2)) < 4)) {
        if ((abs((x % checkerwidth) - (checkerwidth / 2)) < 1) ||
            (abs((y % checkerwidth) - (checkerwidth / 2)) < 1)) {
          hsv[0] = fmodf(fabsf(h - hoffs), 1.0f);
          hsv_to_rgb_v(hsv, rgb);

          if (rect) {
            rect[0] = (char)(rgb[0] * 255.0f);
            rect[1] = (char)(rgb[1] * 255.0f);
            rect[2] = (char)(rgb[2] * 255.0f);
            rect[3] = 255;
          }

          if (rect_float) {
            srgb_to_linearrgb_v3_v3(rect_float, rgb);
            rect_float[3] = 1.0f;
          }
        }
      }

      if (rect_float) {
        rect_float += 4;
      }
      if (rect) {
        rect += 4;
      }
    }
  }
}

typedef struct FillCheckerThreadData {
  unsigned char *rect;
  float *rect_float;
  int width;
} FillCheckerThreadData;

static void image_buf_fill_checker_thread_do(void *data_v, int start_scanline, int num_scanlines)
{
  FillCheckerThreadData *data = (FillCheckerThreadData *)data_v;
  size_t offset = ((size_t)start_scanline) * data->width * 4;
  unsigned char *rect = (data->rect != NULL) ? (data->rect + offset) : NULL;
  float *rect_float = (data->rect_float != NULL) ? (data->rect_float + offset) : NULL;
  image_buf_fill_checker_slice(rect, rect_float, data->width, num_scanlines, start_scanline);
}

void BKE_image_buf_fill_checker(unsigned char *rect, float *rect_float, int width, int height)
{
  if (((size_t)width) * height < 64 * 64) {
    image_buf_fill_checker_slice(rect, rect_float, width, height, 0);
  }
  else {
    FillCheckerThreadData data;
    data.rect = rect;
    data.rect_float = rect_float;
    data.width = width;
    IMB_processor_apply_threaded_scanlines(height, image_buf_fill_checker_thread_do, &data);
  }
}

/* Utility functions for BKE_image_buf_fill_checker_color */

#define BLEND_FLOAT(real, add) (real + add <= 1.0f) ? (real + add) : 1.0f
#define BLEND_CHAR(real, add) \
  ((real + (char)(add * 255.0f)) <= 255) ? (real + (char)(add * 255.0f)) : 255

static void checker_board_color_fill(
    unsigned char *rect, float *rect_float, int width, int height, int offset, int total_height)
{
  int hue_step, y, x;
  float hsv[3], rgb[3];

  hsv[1] = 1.0;

  hue_step = power_of_2_max_i(width / 8);
  if (hue_step < 8) {
    hue_step = 8;
  }

  for (y = offset; y < height + offset; y++) {
    /* Use a number lower then 1.0 else its too bright. */
    hsv[2] = 0.1 + (y * (0.4 / total_height));

    for (x = 0; x < width; x++) {
      hsv[0] = (float)((double)(x / hue_step) * 1.0 / width * hue_step);
      hsv_to_rgb_v(hsv, rgb);

      if (rect) {
        rect[0] = (char)(rgb[0] * 255.0f);
        rect[1] = (char)(rgb[1] * 255.0f);
        rect[2] = (char)(rgb[2] * 255.0f);
        rect[3] = 255;

        rect += 4;
      }

      if (rect_float) {
        rect_float[0] = rgb[0];
        rect_float[1] = rgb[1];
        rect_float[2] = rgb[2];
        rect_float[3] = 1.0f;

        rect_float += 4;
      }
    }
  }
}

static void checker_board_color_tint(unsigned char *rect,
                                     float *rect_float,
                                     int width,
                                     int height,
                                     int size,
                                     float blend,
                                     int offset)
{
  int x, y;
  float blend_half = blend * 0.5f;

  for (y = offset; y < height + offset; y++) {
    for (x = 0; x < width; x++) {
      if (((y / size) % 2 == 1 && (x / size) % 2 == 1) ||
          ((y / size) % 2 == 0 && (x / size) % 2 == 0)) {
        if (rect) {
          rect[0] = (char)BLEND_CHAR(rect[0], blend);
          rect[1] = (char)BLEND_CHAR(rect[1], blend);
          rect[2] = (char)BLEND_CHAR(rect[2], blend);
          rect[3] = 255;

          rect += 4;
        }
        if (rect_float) {
          rect_float[0] = BLEND_FLOAT(rect_float[0], blend);
          rect_float[1] = BLEND_FLOAT(rect_float[1], blend);
          rect_float[2] = BLEND_FLOAT(rect_float[2], blend);
          rect_float[3] = 1.0f;

          rect_float += 4;
        }
      }
      else {
        if (rect) {
          rect[0] = (char)BLEND_CHAR(rect[0], blend_half);
          rect[1] = (char)BLEND_CHAR(rect[1], blend_half);
          rect[2] = (char)BLEND_CHAR(rect[2], blend_half);
          rect[3] = 255;

          rect += 4;
        }
        if (rect_float) {
          rect_float[0] = BLEND_FLOAT(rect_float[0], blend_half);
          rect_float[1] = BLEND_FLOAT(rect_float[1], blend_half);
          rect_float[2] = BLEND_FLOAT(rect_float[2], blend_half);
          rect_float[3] = 1.0f;

          rect_float += 4;
        }
      }
    }
  }
}

static void checker_board_grid_fill(
    unsigned char *rect, float *rect_float, int width, int height, float blend, int offset)
{
  int x, y;
  for (y = offset; y < height + offset; y++) {
    for (x = 0; x < width; x++) {
      if (((y % 32) == 0) || ((x % 32) == 0) || x == 0) {
        if (rect) {
          rect[0] = BLEND_CHAR(rect[0], blend);
          rect[1] = BLEND_CHAR(rect[1], blend);
          rect[2] = BLEND_CHAR(rect[2], blend);
          rect[3] = 255;

          rect += 4;
        }
        if (rect_float) {
          rect_float[0] = BLEND_FLOAT(rect_float[0], blend);
          rect_float[1] = BLEND_FLOAT(rect_float[1], blend);
          rect_float[2] = BLEND_FLOAT(rect_float[2], blend);
          rect_float[3] = 1.0f;

          rect_float += 4;
        }
      }
      else {
        if (rect_float) {
          rect_float += 4;
        }
        if (rect) {
          rect += 4;
        }
      }
    }
  }
}

/* defined in image.c */

static void checker_board_text(
    unsigned char *rect, float *rect_float, int width, int height, int step, int outline)
{
  int x, y;
  int pen_x, pen_y;
  char text[3] = {'A', '1', '\0'};
  const int mono = blf_mono_font_render;

  BLF_size(mono, 54, 72); /* hard coded size! */

  /* OCIO_TODO: using NULL as display will assume using sRGB display
   *            this is correct since currently generated images are assumed to be in sRGB space,
   *            but this would probably needed to be fixed in some way
   */
  BLF_buffer(mono, rect_float, rect, width, height, 4, NULL);

  const float text_color[4] = {0.0, 0.0, 0.0, 1.0};
  const float text_outline[4] = {1.0, 1.0, 1.0, 1.0};

  for (y = 0; y < height; y += step) {
    text[1] = '1';

    for (x = 0; x < width; x += step) {
      /* hard coded offset */
      pen_x = x + 33;
      pen_y = y + 44;

      /* terribly crappy outline font! */
      BLF_buffer_col(mono, text_outline);

      BLF_position(mono, pen_x - outline, pen_y, 0.0);
      BLF_draw_buffer(mono, text, 2);
      BLF_position(mono, pen_x + outline, pen_y, 0.0);
      BLF_draw_buffer(mono, text, 2);
      BLF_position(mono, pen_x, pen_y - outline, 0.0);
      BLF_draw_buffer(mono, text, 2);
      BLF_position(mono, pen_x, pen_y + outline, 0.0);
      BLF_draw_buffer(mono, text, 2);

      BLF_position(mono, pen_x - outline, pen_y - outline, 0.0);
      BLF_draw_buffer(mono, text, 2);
      BLF_position(mono, pen_x + outline, pen_y + outline, 0.0);
      BLF_draw_buffer(mono, text, 2);
      BLF_position(mono, pen_x - outline, pen_y + outline, 0.0);
      BLF_draw_buffer(mono, text, 2);
      BLF_position(mono, pen_x + outline, pen_y - outline, 0.0);
      BLF_draw_buffer(mono, text, 2);

      BLF_buffer_col(mono, text_color);
      BLF_position(mono, pen_x, pen_y, 0.0);
      BLF_draw_buffer(mono, text, 2);

      text[1]++;
    }
    text[0]++;
  }

  /* cleanup the buffer. */
  BLF_buffer(mono, NULL, NULL, 0, 0, 0, NULL);
}

static void checker_board_color_prepare_slice(
    unsigned char *rect, float *rect_float, int width, int height, int offset, int total_height)
{
  checker_board_color_fill(rect, rect_float, width, height, offset, total_height);
  checker_board_color_tint(rect, rect_float, width, height, 1, 0.03f, offset);
  checker_board_color_tint(rect, rect_float, width, height, 4, 0.05f, offset);
  checker_board_color_tint(rect, rect_float, width, height, 32, 0.07f, offset);
  checker_board_color_tint(rect, rect_float, width, height, 128, 0.15f, offset);
  checker_board_grid_fill(rect, rect_float, width, height, 1.0f / 4.0f, offset);
}

typedef struct FillCheckerColorThreadData {
  unsigned char *rect;
  float *rect_float;
  int width, height;
} FillCheckerColorThreadData;

static void checker_board_color_prepare_thread_do(void *data_v,
                                                  int start_scanline,
                                                  int num_scanlines)
{
  FillCheckerColorThreadData *data = (FillCheckerColorThreadData *)data_v;
  size_t offset = ((size_t)data->width) * start_scanline * 4;
  unsigned char *rect = (data->rect != NULL) ? (data->rect + offset) : NULL;
  float *rect_float = (data->rect_float != NULL) ? (data->rect_float + offset) : NULL;
  checker_board_color_prepare_slice(
      rect, rect_float, data->width, num_scanlines, start_scanline, data->height);
}

void BKE_image_buf_fill_checker_color(unsigned char *rect,
                                      float *rect_float,
                                      int width,
                                      int height)
{
  if (((size_t)width) * height < 64 * 64) {
    checker_board_color_prepare_slice(rect, rect_float, width, height, 0, height);
  }
  else {
    FillCheckerColorThreadData data;
    data.rect = rect;
    data.rect_float = rect_float;
    data.width = width;
    data.height = height;
    IMB_processor_apply_threaded_scanlines(height, checker_board_color_prepare_thread_do, &data);
  }

  checker_board_text(rect, rect_float, width, height, 128, 2);

  if (rect_float != NULL) {
    /* TODO(sergey): Currently it's easier to fill in form buffer and
     * linearize it afterwards. This could be optimized with some smart
     * trickery around blending factors and such.
     */
    IMB_buffer_float_from_float_threaded(rect_float,
                                         rect_float,
                                         4,
                                         IB_PROFILE_LINEAR_RGB,
                                         IB_PROFILE_SRGB,
                                         true,
                                         width,
                                         height,
                                         width,
                                         width);
  }
}
