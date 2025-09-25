/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_task.hh"

#include "BKE_image.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BLF_api.hh"

void BKE_image_buf_fill_color(
    uchar *rect_byte, float *rect_float, int width, int height, const float color[4])
{
  using namespace blender;
  threading::parallel_for(
      IndexRange(int64_t(width) * height), 64 * 1024, [&](const IndexRange i_range) {
        if (rect_float != nullptr) {
          float *dst = rect_float + i_range.first() * 4;
          for ([[maybe_unused]] const int64_t i : i_range) {
            copy_v4_v4(dst, color);
            dst += 4;
          }
        }
        if (rect_byte != nullptr) {
          uchar ccol[4];
          rgba_float_to_uchar(ccol, color);
          uchar *dst = rect_byte + i_range.first() * 4;
          for ([[maybe_unused]] const int64_t i : i_range) {
            dst[0] = ccol[0];
            dst[1] = ccol[1];
            dst[2] = ccol[2];
            dst[3] = ccol[3];
            dst += 4;
          }
        }
      });
}

static void image_buf_fill_checker_slice(
    uchar *rect, float *rect_float, int width, int height, int offset)
{
  /* these two passes could be combined into one, but it's more readable and
   * easy to tweak like this, speed isn't really that much of an issue in this situation... */
  const int checker_size = 32;
  const int checker_size_half = checker_size / 2;

  uchar *rect_orig = rect;
  float *rect_float_orig = rect_float;

  float hsv[3] = {0.0f, 0.9f, 0.9f};
  float rgb[3];

  float dark_linear_color = 0.0f, bright_linear_color = 0.0f;
  if (rect_float != nullptr) {
    dark_linear_color = srgb_to_linearrgb(0.25f);
    bright_linear_color = srgb_to_linearrgb(0.58f);
  }

  /* checkers */
  for (int y = offset; y < height + offset; y++) {
    int dark = powf(-1.0f, floorf(y / checker_size));

    for (int x = 0; x < width; x++) {
      if (x % checker_size == 0) {
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

  /* 2nd pass, colored `+`. */
  for (int y = offset; y < height + offset; y++) {
    float hoffs = 0.125f * floorf(y / checker_size);

    for (int x = 0; x < width; x++) {
      float h = 0.125f * floorf(x / checker_size);
      int test_x, test_y;
      /* Note that this `+` is not exactly centered since it's a 1px wide line being
       * drawn inside an even sized square, keep as-is since solving requires either
       * using odd sized checkers or double-width lines, see #112653. */
      if (((test_x = abs((x % checker_size) - checker_size_half)) < 4) &&
          ((test_y = abs((y % checker_size) - checker_size_half)) < 4) &&
          ((test_x < 1) || (test_y < 1)))
      {
        hsv[0] = fmodf(fabsf(h - hoffs), 1.0f);
        hsv_to_rgb_v(hsv, rgb);

        if (rect) {
          rect[0] = char(rgb[0] * 255.0f);
          rect[1] = char(rgb[1] * 255.0f);
          rect[2] = char(rgb[2] * 255.0f);
          rect[3] = 255;
        }

        if (rect_float) {
          IMB_colormanagement_srgb_to_scene_linear_v3(rect_float, rgb);
          rect_float[3] = 1.0f;
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

void BKE_image_buf_fill_checker(uchar *rect, float *rect_float, int width, int height)
{
  using namespace blender;
  threading::parallel_for(IndexRange(height), 64, [&](const IndexRange y_range) {
    int64_t offset = y_range.first() * width * 4;
    uchar *dst_byte = (rect != nullptr) ? (rect + offset) : nullptr;
    float *dst_float = (rect_float != nullptr) ? (rect_float + offset) : nullptr;
    image_buf_fill_checker_slice(dst_byte, dst_float, width, y_range.size(), y_range.first());
  });
}

/* Utility functions for BKE_image_buf_fill_checker_color */

#define BLEND_FLOAT(real, add) (real + add <= 1.0f) ? (real + add) : 1.0f
#define BLEND_CHAR(real, add) \
  ((real + char(add * 255.0f)) <= 255) ? (real + char(add * 255.0f)) : 255

static void checker_board_color_fill(
    uchar *rect, float *rect_float, int width, int height, int offset, int total_height)
{
  int hue_step, y, x;
  float hsv[3], rgb[3];

  hsv[1] = 1.0;

  hue_step = power_of_2_max_i(width / 8);
  hue_step = std::max(hue_step, 8);

  for (y = offset; y < height + offset; y++) {
    /* Use a number lower than 1.0 else its too bright. */
    hsv[2] = 0.1 + (y * (0.4 / total_height));

    for (x = 0; x < width; x++) {
      hsv[0] = float(double(x / hue_step) * 1.0 / width * hue_step);
      hsv_to_rgb_v(hsv, rgb);

      if (rect) {
        rect[0] = char(rgb[0] * 255.0f);
        rect[1] = char(rgb[1] * 255.0f);
        rect[2] = char(rgb[2] * 255.0f);
        rect[3] = 255;

        rect += 4;
      }

      if (rect_float) {
        IMB_colormanagement_rec709_to_scene_linear(rect_float, rgb);
        rect_float[3] = 1.0f;

        rect_float += 4;
      }
    }
  }
}

static void checker_board_color_tint(
    uchar *rect, float *rect_float, int width, int height, int size, float blend, int offset)
{
  int x, y;
  float blend_half = blend * 0.5f;

  for (y = offset; y < height + offset; y++) {
    for (x = 0; x < width; x++) {
      if (((y / size) % 2 == 1 && (x / size) % 2 == 1) ||
          ((y / size) % 2 == 0 && (x / size) % 2 == 0))
      {
        if (rect) {
          rect[0] = char(BLEND_CHAR(rect[0], blend));
          rect[1] = char(BLEND_CHAR(rect[1], blend));
          rect[2] = char(BLEND_CHAR(rect[2], blend));
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
          rect[0] = char(BLEND_CHAR(rect[0], blend_half));
          rect[1] = char(BLEND_CHAR(rect[1], blend_half));
          rect[2] = char(BLEND_CHAR(rect[2], blend_half));
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
    uchar *rect, float *rect_float, int width, int height, float blend, int offset)
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

/* Defined in `image.cc`. */

static void checker_board_text(
    uchar *rect, float *rect_float, int width, int height, int step, int outline)
{
  int x, y;
  int pen_x, pen_y;
  char text[3] = {'A', '1', '\0'};
  const int mono = blf_mono_font_render;

  BLF_size(mono, 54.0f); /* hard coded size! */

  /* Using nullptr will assume the byte buffer has sRGB colorspace, which currently
   * matches the default colorspace of new images. */
  BLF_buffer(mono, rect_float, rect, width, height, nullptr);

  const float text_color[4] = {0.0, 0.0, 0.0, 1.0};
  const float text_outline[4] = {1.0, 1.0, 1.0, 1.0};

  const char char_array[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  /* Subtract one because of null termination. */
  const int char_num = sizeof(char_array) - 1;

  int first_char_index = 0;
  for (y = 0; y < height; y += step) {
    text[0] = char_array[first_char_index];

    int second_char_index = 27;
    for (x = 0; x < width; x += step) {
      text[1] = char_array[second_char_index];

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

      second_char_index = (second_char_index + 1) % char_num;
    }
    first_char_index = (first_char_index + 1) % char_num;
  }

  /* cleanup the buffer. */
  BLF_buffer(mono, nullptr, nullptr, 0, 0, nullptr);
}

static void checker_board_color_prepare_slice(
    uchar *rect, float *rect_float, int width, int height, int offset, int total_height)
{
  checker_board_color_fill(rect, rect_float, width, height, offset, total_height);
  checker_board_color_tint(rect, rect_float, width, height, 1, 0.03f, offset);
  checker_board_color_tint(rect, rect_float, width, height, 4, 0.05f, offset);
  checker_board_color_tint(rect, rect_float, width, height, 32, 0.07f, offset);
  checker_board_color_tint(rect, rect_float, width, height, 128, 0.15f, offset);
  checker_board_grid_fill(rect, rect_float, width, height, 1.0f / 4.0f, offset);
}

void BKE_image_buf_fill_checker_color(uchar *rect, float *rect_float, int width, int height)
{
  using namespace blender;
  threading::parallel_for(IndexRange(height), 64, [&](const IndexRange y_range) {
    int64_t offset = y_range.first() * width * 4;
    uchar *dst_byte = (rect != nullptr) ? (rect + offset) : nullptr;
    float *dst_float = (rect_float != nullptr) ? (rect_float + offset) : nullptr;
    checker_board_color_prepare_slice(
        dst_byte, dst_float, width, y_range.size(), y_range.first(), height);
  });

  checker_board_text(rect, rect_float, width, height, 128, 2);

  if (rect_float != nullptr) {
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
