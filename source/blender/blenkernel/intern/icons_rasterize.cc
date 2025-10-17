/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_bitmap_draw_2d.h"
#include "BLI_math_color.h"
#include "BLI_math_geom.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "BKE_icons.hh"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

struct UserRasterInfo {
  int pt[3][2];
  const uint *color;
  /* only for smooth shading */
  struct {
    float pt_fl[3][2];
    uint color_u[3][4];
  } smooth;
  int rect_size[2];
  uint *rect;
};

static void tri_fill_flat(int x, int x_end, int y, void *user_data)
{
  UserRasterInfo *data = static_cast<UserRasterInfo *>(user_data);
  uint *p = &data->rect[(y * data->rect_size[1]) + x];
  uint col = data->color[0];
  while (x++ != x_end) {
    *p++ = col;
  }
}

static void tri_fill_smooth(int x, int x_end, int y, void *user_data)
{
  UserRasterInfo *data = static_cast<UserRasterInfo *>(user_data);
  uint *p = &data->rect[(y * data->rect_size[1]) + x];
  float pt_step_fl[2] = {float(x), float(y)};
  while (x++ != x_end) {
    float w[3];
    barycentric_weights_v2_clamped(UNPACK3(data->smooth.pt_fl), pt_step_fl, w);

    uint col_u[4] = {0, 0, 0, 0};
    for (uint corner = 0; corner < 3; corner++) {
      for (uint chan = 0; chan < 4; chan++) {
        col_u[chan] += data->smooth.color_u[corner][chan] * uint(w[corner] * 255.0f);
      }
    }
    union {
      uint as_u32;
      uchar as_bytes[4];
    } col;
    col.as_bytes[0] = uchar(col_u[0] / 255);
    col.as_bytes[1] = uchar(col_u[1] / 255);
    col.as_bytes[2] = uchar(col_u[2] / 255);
    col.as_bytes[3] = uchar(col_u[3] / 255);
    *p++ = col.as_u32;

    pt_step_fl[0] += 1.0f;
  }
}

ImBuf *BKE_icon_geom_rasterize(const Icon_Geom *geom, const uint size_x, const uint size_y)
{
  const int coords_len = geom->coords_len;

  const uchar(*pos)[2] = geom->coords;
  const uint *col = static_cast<const uint *>((void *)geom->colors);

  /* TODO(@ideasman42): Currently rasterizes to fixed size, then scales.
   * Should rasterize to double size for eg instead. */
  const int rect_size[2] = {max_ii(256, int(size_x) * 2), max_ii(256, int(size_y) * 2)};

  ImBuf *ibuf = IMB_allocImBuf(uint(rect_size[0]), uint(rect_size[1]), 32, IB_byte_data);

  UserRasterInfo data;

  data.rect_size[0] = rect_size[0];
  data.rect_size[1] = rect_size[1];

  data.rect = (uint *)ibuf->byte_buffer.data;

  float scale[2];
  const bool use_scale = (rect_size[0] != 256) || (rect_size[1] != 256);

  if (use_scale) {
    scale[0] = float(rect_size[0]) / 256.0f;
    scale[1] = float(rect_size[1]) / 256.0f;
  }

  for (int t = 0; t < coords_len; t += 1, pos += 3, col += 3) {
    if (use_scale) {
      ARRAY_SET_ITEMS(data.pt[0], int(pos[0][0] * scale[0]), int(pos[0][1] * scale[1]));
      ARRAY_SET_ITEMS(data.pt[1], int(pos[1][0] * scale[0]), int(pos[1][1] * scale[1]));
      ARRAY_SET_ITEMS(data.pt[2], int(pos[2][0] * scale[0]), int(pos[2][1] * scale[1]));
    }
    else {
      ARRAY_SET_ITEMS(data.pt[0], UNPACK2(pos[0]));
      ARRAY_SET_ITEMS(data.pt[1], UNPACK2(pos[1]));
      ARRAY_SET_ITEMS(data.pt[2], UNPACK2(pos[2]));
    }
    data.color = col;
    if ((col[0] == col[1]) && (col[0] == col[2])) {
      BLI_bitmap_draw_2d_tri_v2i(UNPACK3(data.pt), tri_fill_flat, &data);
    }
    else {
      ARRAY_SET_ITEMS(data.smooth.pt_fl[0], UNPACK2_EX((float), data.pt[0], ));
      ARRAY_SET_ITEMS(data.smooth.pt_fl[1], UNPACK2_EX((float), data.pt[1], ));
      ARRAY_SET_ITEMS(data.smooth.pt_fl[2], UNPACK2_EX((float), data.pt[2], ));
      ARRAY_SET_ITEMS(data.smooth.color_u[0], UNPACK4_EX((uint), ((uchar *)(col + 0)), ));
      ARRAY_SET_ITEMS(data.smooth.color_u[1], UNPACK4_EX((uint), ((uchar *)(col + 1)), ));
      ARRAY_SET_ITEMS(data.smooth.color_u[2], UNPACK4_EX((uint), ((uchar *)(col + 2)), ));
      BLI_bitmap_draw_2d_tri_v2i(UNPACK3(data.pt), tri_fill_smooth, &data);
    }
  }
  IMB_scale(ibuf, size_x, size_y, IMBScaleFilter::Box, false);
  return ibuf;
}

void BKE_icon_geom_invert_lightness(Icon_Geom *geom)
{
  const int length = 3 * geom->coords_len;

  for (int i = 0; i < length; i++) {
    float rgb[3], hsl[3];

    rgb_uchar_to_float(rgb, geom->colors[i]);
    rgb_to_hsl_v(rgb, hsl);
    hsl_to_rgb(hsl[0], hsl[1], 1.0f - hsl[2], &rgb[0], &rgb[1], &rgb[2]);
    rgb_float_to_uchar(geom->colors[i], rgb);
  }
}
