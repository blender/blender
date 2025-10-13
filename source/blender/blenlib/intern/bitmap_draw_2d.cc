/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Utility functions for primitive drawing operations.
 */

#include <algorithm>
#include <climits>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"

#include "BLI_math_base.h"
#include "BLI_sort.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

using blender::int2;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Draw Line
 * \{ */

void BLI_bitmap_draw_2d_line_v2v2i(const int p1[2],
                                   const int p2[2],
                                   bool (*callback)(int, int, void *),
                                   void *user_data)
{
  /* Bresenham's line algorithm. */
  int x1 = p1[0];
  int y1 = p1[1];
  int x2 = p2[0];
  int y2 = p2[1];

  if (callback(x1, y1, user_data) == 0) {
    return;
  }

  /* if x1 == x2 or y1 == y2, then it does not matter what we set here */
  const int sign_x = (x2 > x1) ? 1 : -1;
  const int sign_y = (y2 > y1) ? 1 : -1;

  const int delta_x = (sign_x == 1) ? (x2 - x1) : (x1 - x2);
  const int delta_y = (sign_y == 1) ? (y2 - y1) : (y1 - y2);

  const int delta_x_step = delta_x * 2;
  const int delta_y_step = delta_y * 2;

  if (delta_x >= delta_y) {
    /* error may go below zero */
    int error = delta_y_step - delta_x;

    while (x1 != x2) {
      if (error >= 0) {
        if (error || (sign_x == 1)) {
          y1 += sign_y;
          error -= delta_x_step;
        }
        /* else do nothing */
      }
      /* else do nothing */

      x1 += sign_x;
      error += delta_y_step;

      if (callback(x1, y1, user_data) == 0) {
        return;
      }
    }
  }
  else {
    /* error may go below zero */
    int error = delta_x_step - delta_y;

    while (y1 != y2) {
      if (error >= 0) {
        if (error || (sign_y == 1)) {
          x1 += sign_x;
          error -= delta_y_step;
        }
        /* else do nothing */
      }
      /* else do nothing */

      y1 += sign_y;
      error += delta_x_step;

      if (callback(x1, y1, user_data) == 0) {
        return;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Filled Triangle
 * \{ */

/**
 * Fill a triangle
 *
 * Standard algorithm,
 * See: http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
 *
 * Changes to the basic implementation:
 *
 * - Reuse slope calculation when drawing the second triangle.
 * - Don't calculate the 4th point at all for the triangle split.
 * - Order line drawing from left to right (minor detail).
 * - 1-pixel offsets are applied so adjacent triangles don't overlap.
 *
 * This is not clipped, a clipped version can be added if needed.
 */

/* Macros could be moved to a shared location. */
#define ORDERED_SWAP(ty, a, b) \
  if (a > b) { \
    SWAP(ty, a, b); \
  } \
  ((void)0)

#define ORDERED_SWAP_BY(ty, a, b, by) \
  if ((a by) > (b by)) { \
    SWAP(ty, a, b); \
  } \
  ((void)0)

#define ORDER_VARS2(ty, a, b) \
  { \
    ORDERED_SWAP(ty, a, b); \
  } \
  ((void)0)

#define ORDER_VARS3_BY(ty, a, b, c, by) \
  { \
    ORDERED_SWAP_BY(ty, b, c, by); \
    ORDERED_SWAP_BY(ty, a, c, by); \
    ORDERED_SWAP_BY(ty, a, b, by); \
  } \
  ((void)0)

static float inv_slope(const int a[2], const int b[2])
{
  return float(a[0] - b[0]) / float(a[1] - b[1]);
}

/**
 * <pre>
 * *---*
 * \ /
 *   *
 * </pre>
 */
static void draw_tri_flat_max(const int p[2],
                              const int max_y,
                              const float inv_slope1,
                              const float inv_slope2,
                              void (*callback)(int x, int x_end, int y, void *),
                              void *user_data)
{
  float cur_x1 = float(p[0]);
  float cur_x2 = cur_x1;
  /* start-end inclusive */
  const int min_y = p[1];
  const int max_y_end = max_y + 1;
  for (int scanline_y = min_y; scanline_y != max_y_end; scanline_y += 1) {
    callback(int(cur_x1), 1 + int(cur_x2), scanline_y, user_data);
    cur_x1 += inv_slope1;
    cur_x2 += inv_slope2;
  }
}

/**
 * <pre>
 *   *
 *  / \
 * *---*
 * </pre>
 */
static void draw_tri_flat_min(const int p[2],
                              const int min_y,
                              const float inv_slope1,
                              const float inv_slope2,
                              void (*callback)(int x, int x_end, int y, void *),
                              void *user_data)
{
  float cur_x1 = float(p[0]);
  float cur_x2 = cur_x1;
  /* start-end inclusive */
  const int max_y = p[1];
  const int min_y_end = min_y - 1;
  for (int scanline_y = max_y; scanline_y != min_y_end; scanline_y -= 1) {
    callback(int(cur_x1), 1 + int(cur_x2), scanline_y, user_data);
    cur_x1 -= inv_slope1;
    cur_x2 -= inv_slope2;
  }
}

void BLI_bitmap_draw_2d_tri_v2i(
    /* all 2d */
    const int p1[2],
    const int p2[2],
    const int p3[2],
    void (*callback)(int x, int x_end, int y, void *),
    void *user_data)
{
  /* At first sort the three vertices by y-coordinate ascending so p1 is the top-most vertex */
  ORDER_VARS3_BY(const int *, p1, p2, p3, [1]);

  BLI_assert(p1[1] <= p2[1] && p2[1] <= p3[1]);

  /* Check for trivial case of bottom-flat triangle. */
  if (p2[1] == p3[1]) {
    float inv_slope1 = inv_slope(p2, p1);
    float inv_slope2 = inv_slope(p3, p1);
    ORDER_VARS2(float, inv_slope1, inv_slope2);
    BLI_assert(!(inv_slope1 > inv_slope2));
    draw_tri_flat_max(p1, p2[1], inv_slope1, inv_slope2, callback, user_data);
  }
  else if (p1[1] == p2[1]) {
    /* Check for trivial case of top-flat triangle. */
    float inv_slope1 = inv_slope(p3, p1);
    float inv_slope2 = inv_slope(p3, p2);
    ORDER_VARS2(float, inv_slope2, inv_slope1);
    BLI_assert(!(inv_slope1 < inv_slope2));
    draw_tri_flat_min(p3,
                      p2[1] + 1, /* avoid overlap */
                      inv_slope1,
                      inv_slope2,
                      callback,
                      user_data);
  }
  else {
    /* General case - split the triangle in a top-flat and bottom-flat one. */
    const float inv_slope_p21 = inv_slope(p2, p1);
    const float inv_slope_p31 = inv_slope(p3, p1);
    const float inv_slope_p32 = inv_slope(p3, p2);

    float inv_slope1_max, inv_slope2_max;
    float inv_slope2_min, inv_slope1_min;

    if (inv_slope_p21 < inv_slope_p31) {
      inv_slope1_max = inv_slope_p21;
      inv_slope2_max = inv_slope_p31;
      inv_slope2_min = inv_slope_p31;
      inv_slope1_min = inv_slope_p32;
    }
    else {
      inv_slope1_max = inv_slope_p31;
      inv_slope2_max = inv_slope_p21;
      inv_slope2_min = inv_slope_p32;
      inv_slope1_min = inv_slope_p31;
    }

    draw_tri_flat_max(p1, p2[1], inv_slope1_max, inv_slope2_max, callback, user_data);
    draw_tri_flat_min(p3,
                      p2[1] + 1, /* avoid overlap */
                      inv_slope1_min,
                      inv_slope2_min,
                      callback,
                      user_data);
  }
}

#undef ORDERED_SWAP
#undef ORDERED_SWAP_BY
#undef ORDER_VARS2
#undef ORDER_VARS3_BY

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Filled Polygon
 * \{ */

/* sort edge-segments on y, then x axis */
static int draw_poly_v2i_n__span_y_sort(const void *a_p, const void *b_p, void *verts_p)
{
  const int (*verts)[2] = static_cast<const int (*)[2]>(verts_p);
  const int *a = static_cast<const int *>(a_p);
  const int *b = static_cast<const int *>(b_p);
  const int *co_a = verts[a[0]];
  const int *co_b = verts[b[0]];

  if (co_a[1] < co_b[1]) {
    return -1;
  }
  if (co_a[1] > co_b[1]) {
    return 1;
  }
  if (co_a[0] < co_b[0]) {
    return -1;
  }
  if (co_a[0] > co_b[0]) {
    return 1;
  }
  /* co_a & co_b are identical, use the line closest to the x-min */
  const int *co = co_a;
  co_a = verts[a[1]];
  co_b = verts[b[1]];
  int ord = (((co_b[0] - co[0]) * (co_a[1] - co[1])) - ((co_a[0] - co[0]) * (co_b[1] - co[1])));
  if (ord > 0) {
    return -1;
  }
  if (ord < 0) {
    return 1;
  }
  return 0;
}

void BLI_bitmap_draw_2d_poly_v2i_n(const int xmin,
                                   const int ymin,
                                   const int xmax,
                                   const int ymax,
                                   const Span<int2> verts,
                                   void (*callback)(int x, int x_end, int y, void *),
                                   void *user_data)
{
  /* Originally by Darel Rex Finley, 2007.
   * Optimized by Campbell Barton, 2016 to track sorted intersections. */

  int (*span_y)[2] = MEM_malloc_arrayN<int[2]>(size_t(verts.size()), __func__);
  int span_y_len = 0;

  for (int i_curr = 0, i_prev = int(verts.size() - 1); i_curr < verts.size(); i_prev = i_curr++) {
    const int *co_prev = verts[i_prev];
    const int *co_curr = verts[i_curr];

    if (co_prev[1] != co_curr[1]) {
      /* Any segments entirely above or below the area of interest can be skipped. */
      if ((min_ii(co_prev[1], co_curr[1]) >= ymax) || (max_ii(co_prev[1], co_curr[1]) < ymin)) {
        continue;
      }

      int *s = span_y[span_y_len++];
      if (co_prev[1] < co_curr[1]) {
        s[0] = i_prev;
        s[1] = i_curr;
      }
      else {
        s[0] = i_curr;
        s[1] = i_prev;
      }
    }
  }

  BLI_qsort_r(span_y,
              size_t(span_y_len),
              sizeof(*span_y),
              draw_poly_v2i_n__span_y_sort,
              (void *)verts.data());

  struct NodeX {
    int span_y_index;
    int x;
  } *node_x = MEM_malloc_arrayN<NodeX>(size_t(verts.size() + 1), __func__);
  int node_x_len = 0;

  int span_y_index = 0;
  if (span_y_len != 0 && verts[span_y[0][0]][1] < ymin) {
    while ((span_y_index < span_y_len) && (verts[span_y[span_y_index][0]][1] < ymin)) {
      BLI_assert(verts[span_y[span_y_index][0]][1] < verts[span_y[span_y_index][1]][1]);
      if (verts[span_y[span_y_index][1]][1] >= ymin) {
        NodeX *n = &node_x[node_x_len++];
        n->span_y_index = span_y_index;
      }
      span_y_index += 1;
    }
  }

  /* Loop through the rows of the image. */
  for (int pixel_y = ymin; pixel_y < ymax; pixel_y++) {
    bool is_sorted = true;
    bool do_remove = false;

    for (int i = 0, x_ix_prev = INT_MIN; i < node_x_len; i++) {
      NodeX *n = &node_x[i];
      const int *s = span_y[n->span_y_index];
      const int *co_prev = verts[s[0]];
      const int *co_curr = verts[s[1]];

      BLI_assert(co_prev[1] < pixel_y && co_curr[1] >= pixel_y);

      const double x = (co_prev[0] - co_curr[0]);
      const double y = (co_prev[1] - co_curr[1]);
      const double y_px = (pixel_y - co_curr[1]);
      const int x_ix = int(double(co_curr[0]) + ((y_px / y) * x));
      n->x = x_ix;

      if (is_sorted && (x_ix_prev > x_ix)) {
        is_sorted = false;
      }
      if (do_remove == false && co_curr[1] == pixel_y) {
        do_remove = true;
      }
      x_ix_prev = x_ix;
    }

    /* Sort the nodes, via a simple "Bubble" sort. */
    if (is_sorted == false) {
      int i = 0;
      const int node_x_end = node_x_len - 1;
      while (i < node_x_end) {
        if (node_x[i].x > node_x[i + 1].x) {
          SWAP(NodeX, node_x[i], node_x[i + 1]);
          if (i != 0) {
            i -= 1;
          }
        }
        else {
          i += 1;
        }
      }
    }

    /* Fill the pixels between node pairs. */
    for (int i = 0; i < node_x_len; i += 2) {
      int x_src = node_x[i].x;
      int x_dst = node_x[i + 1].x;

      if (x_src >= xmax) {
        break;
      }

      if (x_dst > xmin) {
        x_src = std::max(x_src, xmin);
        x_dst = std::min(x_dst, xmax);
        /* for single call per x-span */
        if (x_src < x_dst) {
          callback(x_src - xmin, x_dst - xmin, pixel_y - ymin, user_data);
        }
      }
    }

    /* Clear finalized nodes in one pass, only when needed
     * (avoids excessive array-resizing). */
    if (do_remove == true) {
      int i_dst = 0;
      for (int i_src = 0; i_src < node_x_len; i_src += 1) {
        const int *s = span_y[node_x[i_src].span_y_index];
        const int *co = verts[s[1]];
        if (co[1] != pixel_y) {
          if (i_dst != i_src) {
            /* x is initialized for the next pixel_y (no need to adjust here) */
            node_x[i_dst].span_y_index = node_x[i_src].span_y_index;
          }
          i_dst += 1;
        }
      }
      node_x_len = i_dst;
    }

    /* Scan for new x-nodes */
    while ((span_y_index < span_y_len) && (verts[span_y[span_y_index][0]][1] == pixel_y)) {
      /* NOTE: node_x these are just added at the end,
       * not ideal but sorting once will resolve. */

      /* x is initialized for the next pixel_y */
      NodeX *n = &node_x[node_x_len++];
      n->span_y_index = span_y_index;
      span_y_index += 1;
    }
  }

  MEM_freeN(span_y);
  MEM_freeN(node_x);
}

/** \} */
