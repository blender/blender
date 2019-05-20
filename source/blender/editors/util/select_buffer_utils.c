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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edutil
 *
 * Generic utilities for handling buffer selection where selection ID's are drawn onto
 * an off screen buffer.
 *
 * All coordinates are relative to the current region.
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_bitmap_draw_2d.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "ED_select_buffer_utils.h"

/* Only for #ED_view3d_select_id_read,
 * note that this file shouldn't have 3D view specific logic in it, we could have a more general
 * way to read from selection buffers that doesn't depend on the view3d API. */
#include "ED_view3d.h"

/* -------------------------------------------------------------------- */
/** \name Select Bitmap from ID's
 *
 * Given a buffer of select ID's, fill in a booleans (true/false) per index.
 * #BLI_bitmap is used for memory effeciency.
 *
 * \{ */

/**
 * \param bitmap_len: Number of indices in the selection id buffer.
 * \param rect: The rectangle to sample indices from (min/max inclusive).
 * \returns a #BLI_bitmap the length of \a bitmap_len or NULL on failure.
 */
uint *ED_select_buffer_bitmap_from_rect(const uint bitmap_len, const rcti *rect)
{
  uint buf_len;
  const uint *buf = ED_view3d_select_id_read(
      rect->xmin, rect->ymin, rect->xmax, rect->ymax, &buf_len);
  if (buf == NULL) {
    return NULL;
  }

  const uint *buf_iter = buf;

  BLI_bitmap *bitmap_buf = BLI_BITMAP_NEW(bitmap_len, __func__);

  while (buf_len--) {
    const uint index = *buf_iter - 1;
    if (index < bitmap_len) {
      BLI_BITMAP_ENABLE(bitmap_buf, index);
    }
    buf_iter++;
  }
  MEM_freeN((void *)buf);
  return bitmap_buf;
}

/**
 * \param bitmap_len: Number of indices in the selection id buffer.
 * \param center: Circle center.
 * \param radius: Circle radius.
 * \returns a #BLI_bitmap the length of \a bitmap_len or NULL on failure.
 */
uint *ED_select_buffer_bitmap_from_circle(const uint bitmap_len,
                                          const int center[2],
                                          const int radius)
{
  if (bitmap_len == 0) {
    return NULL;
  }

  const int xmin = center[0] - radius;
  const int xmax = center[0] + radius;
  const int ymin = center[1] - radius;
  const int ymax = center[1] + radius;

  const uint *buf = ED_view3d_select_id_read(xmin, ymin, xmax, ymax, NULL);
  if (buf == NULL) {
    return NULL;
  }

  const uint *buf_iter = buf;

  BLI_bitmap *bitmap_buf = BLI_BITMAP_NEW(bitmap_len, __func__);
  const int radius_sq = radius * radius;
  for (int yc = -radius; yc <= radius; yc++) {
    for (int xc = -radius; xc <= radius; xc++, buf_iter++) {
      if (xc * xc + yc * yc < radius_sq) {
        /* Intentionally wrap to max value if this is zero. */
        const uint index = *buf_iter - 1;
        if (index < bitmap_len) {
          BLI_BITMAP_ENABLE(bitmap_buf, index);
        }
      }
    }
  }
  MEM_freeN((void *)buf);
  return bitmap_buf;
}

struct PolyMaskData {
  BLI_bitmap *px;
  int width;
};

static void ed_select_buffer_mask_px_cb(int x, int x_end, int y, void *user_data)
{
  struct PolyMaskData *data = user_data;
  BLI_bitmap *px = data->px;
  int i = (y * data->width) + x;
  do {
    BLI_BITMAP_ENABLE(px, i);
    i++;
  } while (++x != x_end);
}

/**
 * \param bitmap_len: Number of indices in the selection id buffer.
 * \param center: Circle center.
 * \param radius: Circle radius.
 * \returns a #BLI_bitmap the length of \a bitmap_len or NULL on failure.
 */
uint *ED_select_buffer_bitmap_from_poly(const uint bitmap_len,
                                        const int poly[][2],
                                        const int poly_len,
                                        const rcti *rect)

{
  if (bitmap_len == 0) {
    return NULL;
  }

  struct PolyMaskData poly_mask_data;
  uint buf_len;
  const uint *buf = ED_view3d_select_id_read(
      rect->xmin, rect->ymin, rect->xmax, rect->ymax, &buf_len);
  if (buf == NULL) {
    return NULL;
  }

  BLI_bitmap *buf_mask = BLI_BITMAP_NEW(buf_len, __func__);
  poly_mask_data.px = buf_mask;
  poly_mask_data.width = (rect->xmax - rect->xmin) + 1;

  BLI_bitmap_draw_2d_poly_v2i_n(rect->xmin,
                                rect->ymin,
                                rect->xmax + 1,
                                rect->ymax + 1,
                                poly,
                                poly_len,
                                ed_select_buffer_mask_px_cb,
                                &poly_mask_data);

  /* Build selection lookup. */
  const uint *buf_iter = buf;
  BLI_bitmap *bitmap_buf = BLI_BITMAP_NEW(bitmap_len, __func__);
  int i = 0;
  while (buf_len--) {
    const uint index = *buf_iter - 1;
    if (index < bitmap_len && BLI_BITMAP_TEST(buf_mask, i)) {
      BLI_BITMAP_ENABLE(bitmap_buf, index);
    }
    buf_iter++;
    i++;
  }
  MEM_freeN((void *)buf);
  MEM_freeN(buf_mask);

  return bitmap_buf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Single Select ID's
 *
 * Given a buffer of select ID's, find the a single select id.
 *
 * \{ */

/**
 * Samples a single pixel.
 */
uint ED_select_buffer_sample_point(const int center[2])
{
  uint buf_len;
  uint *buf = ED_view3d_select_id_read(center[0], center[1], center[0], center[1], &buf_len);
  BLI_assert(0 != buf_len);
  uint ret = buf[0];
  MEM_freeN(buf);
  return ret;
}

/**
 * Find the selection id closest to \a center.
 * \param dist[in,out]: Use to initalize the distance,
 * when found, this value is set to the distance of the selection thats returned.
 */
uint ED_select_buffer_find_nearest_to_point(const int center[2],
                                            const uint id_min,
                                            const uint id_max,
                                            uint *dist)
{
  /* Smart function to sample a rect spiralling outside, nice for selection ID. */

  /* Create region around center (typically the mouse cursor).
   * This must be square and have an odd width,
   * the spiraling algorithm does not work with arbitrary rectangles. */
  rcti rect;
  BLI_rcti_init_pt_radius(&rect, center, *dist);
  rect.xmax += 1;
  rect.ymax += 1;

  int width = BLI_rcti_size_x(&rect);
  int height = width;
  BLI_assert(width == height);

  /* Read from selection framebuffer. */

  uint buf_len;
  const uint *buf = ED_view3d_select_id_read_rect(&rect, &buf_len);
  BLI_assert(width * height == buf_len);

  /* Spiral, starting from center of buffer. */
  int spiral_offset = height * (int)(width / 2) + (height / 2);
  int spiral_direction = 0;

  uint index = 0;

  for (int nr = 1; nr <= height; nr++) {
    for (int a = 0; a < 2; a++) {
      for (int b = 0; b < nr; b++) {
        /* Find hit within the specified range. */
        uint hit_id = buf[spiral_offset];

        if (hit_id && hit_id >= id_min && hit_id < id_max) {
          /* Get x/y from spiral offset. */
          int hit_x = spiral_offset % width;
          int hit_y = spiral_offset / width;

          int center_x = width / 2;
          int center_y = height / 2;

          /* Manhatten distance in keeping with other screen-based selection. */
          *dist = (uint)(abs(hit_x - center_x) + abs(hit_y - center_y));

          /* Indices start at 1 here. */
          index = (hit_id - id_min) + 1;
          goto exit;
        }

        /* Next spiral step. */
        if (spiral_direction == 0) {
          spiral_offset += 1; /* right */
        }
        else if (spiral_direction == 1) {
          spiral_offset -= width; /* down */
        }
        else if (spiral_direction == 2) {
          spiral_offset -= 1; /* left */
        }
        else {
          spiral_offset += width; /* up */
        }

        /* Stop if we are outside the buffer. */
        if (spiral_offset < 0 || spiral_offset >= buf_len) {
          goto exit;
        }
      }

      spiral_direction = (spiral_direction + 1) % 4;
    }
  }

exit:
  MEM_freeN((void *)buf);
  return index;
}

/** \} */
