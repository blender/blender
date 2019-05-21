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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 *
 * A minimalist lib for functions doing stuff with rectangle structs.
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <limits.h>
#include <float.h>

#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_vec_types.h"

/* avoid including BLI_math */
static void unit_m4(float m[4][4]);

/**
 * Determine if a rect is empty. An empty
 * rect is one with a zero (or negative)
 * width or height.
 *
 * \return True if \a rect is empty.
 */
bool BLI_rcti_is_empty(const rcti *rect)
{
  return ((rect->xmax <= rect->xmin) || (rect->ymax <= rect->ymin));
}

bool BLI_rctf_is_empty(const rctf *rect)
{
  return ((rect->xmax <= rect->xmin) || (rect->ymax <= rect->ymin));
}

bool BLI_rcti_isect_x(const rcti *rect, const int x)
{
  if (x < rect->xmin) {
    return false;
  }
  if (x > rect->xmax) {
    return false;
  }
  return true;
}

bool BLI_rcti_isect_y(const rcti *rect, const int y)
{
  if (y < rect->ymin) {
    return false;
  }
  if (y > rect->ymax) {
    return false;
  }
  return true;
}

bool BLI_rcti_isect_pt(const rcti *rect, const int x, const int y)
{
  if (x < rect->xmin) {
    return false;
  }
  if (x > rect->xmax) {
    return false;
  }
  if (y < rect->ymin) {
    return false;
  }
  if (y > rect->ymax) {
    return false;
  }
  return true;
}

bool BLI_rcti_isect_pt_v(const rcti *rect, const int xy[2])
{
  if (xy[0] < rect->xmin) {
    return false;
  }
  if (xy[0] > rect->xmax) {
    return false;
  }
  if (xy[1] < rect->ymin) {
    return false;
  }
  if (xy[1] > rect->ymax) {
    return false;
  }
  return true;
}

bool BLI_rctf_isect_x(const rctf *rect, const float x)
{
  if (x < rect->xmin) {
    return false;
  }
  if (x > rect->xmax) {
    return false;
  }
  return true;
}

bool BLI_rctf_isect_y(const rctf *rect, const float y)
{
  if (y < rect->ymin) {
    return false;
  }
  if (y > rect->ymax) {
    return false;
  }
  return true;
}

bool BLI_rctf_isect_pt(const rctf *rect, const float x, const float y)
{
  if (x < rect->xmin) {
    return false;
  }
  if (x > rect->xmax) {
    return false;
  }
  if (y < rect->ymin) {
    return false;
  }
  if (y > rect->ymax) {
    return false;
  }
  return true;
}

bool BLI_rctf_isect_pt_v(const rctf *rect, const float xy[2])
{
  if (xy[0] < rect->xmin) {
    return false;
  }
  if (xy[0] > rect->xmax) {
    return false;
  }
  if (xy[1] < rect->ymin) {
    return false;
  }
  if (xy[1] > rect->ymax) {
    return false;
  }
  return true;
}

/**
 * \returns shortest distance from \a rect to x/y (0 if inside)
 */

int BLI_rcti_length_x(const rcti *rect, const int x)
{
  if (x < rect->xmin) {
    return rect->xmin - x;
  }
  if (x > rect->xmax) {
    return x - rect->xmax;
  }
  return 0;
}

int BLI_rcti_length_y(const rcti *rect, const int y)
{
  if (y < rect->ymin) {
    return rect->ymin - y;
  }
  if (y > rect->ymax) {
    return y - rect->ymax;
  }
  return 0;
}

float BLI_rctf_length_x(const rctf *rect, const float x)
{
  if (x < rect->xmin) {
    return rect->xmin - x;
  }
  if (x > rect->xmax) {
    return x - rect->xmax;
  }
  return 0.0f;
}

float BLI_rctf_length_y(const rctf *rect, const float y)
{
  if (y < rect->ymin) {
    return rect->ymin - y;
  }
  if (y > rect->ymax) {
    return y - rect->ymax;
  }
  return 0.0f;
}

/**
 * is \a rct_b inside \a rct_a
 */
bool BLI_rctf_inside_rctf(const rctf *rct_a, const rctf *rct_b)
{
  return ((rct_a->xmin <= rct_b->xmin) && (rct_a->xmax >= rct_b->xmax) &&
          (rct_a->ymin <= rct_b->ymin) && (rct_a->ymax >= rct_b->ymax));
}
bool BLI_rcti_inside_rcti(const rcti *rct_a, const rcti *rct_b)
{
  return ((rct_a->xmin <= rct_b->xmin) && (rct_a->xmax >= rct_b->xmax) &&
          (rct_a->ymin <= rct_b->ymin) && (rct_a->ymax >= rct_b->ymax));
}

/* based closely on 'isect_seg_seg_v2_int',
 * but in modified so corner cases are treated as intersections */
static int isect_segments_i(const int v1[2], const int v2[2], const int v3[2], const int v4[2])
{
  const double div = (double)((v2[0] - v1[0]) * (v4[1] - v3[1]) -
                              (v2[1] - v1[1]) * (v4[0] - v3[0]));
  if (div == 0.0) {
    return 1; /* co-linear */
  }
  else {
    const double lambda = (double)((v1[1] - v3[1]) * (v4[0] - v3[0]) -
                                   (v1[0] - v3[0]) * (v4[1] - v3[1])) /
                          div;
    const double mu = (double)((v1[1] - v3[1]) * (v2[0] - v1[0]) -
                               (v1[0] - v3[0]) * (v2[1] - v1[1])) /
                      div;
    return (lambda >= 0.0 && lambda <= 1.0 && mu >= 0.0 && mu <= 1.0);
  }
}
static int isect_segments_fl(const float v1[2],
                             const float v2[2],
                             const float v3[2],
                             const float v4[2])
{
  const double div = (double)((v2[0] - v1[0]) * (v4[1] - v3[1]) -
                              (v2[1] - v1[1]) * (v4[0] - v3[0]));
  if (div == 0.0) {
    return 1; /* co-linear */
  }
  else {
    const double lambda = (double)((v1[1] - v3[1]) * (v4[0] - v3[0]) -
                                   (v1[0] - v3[0]) * (v4[1] - v3[1])) /
                          div;
    const double mu = (double)((v1[1] - v3[1]) * (v2[0] - v1[0]) -
                               (v1[0] - v3[0]) * (v2[1] - v1[1])) /
                      div;
    return (lambda >= 0.0 && lambda <= 1.0 && mu >= 0.0 && mu <= 1.0);
  }
}

bool BLI_rcti_isect_segment(const rcti *rect, const int s1[2], const int s2[2])
{
  /* first do outside-bounds check for both points of the segment */
  if (s1[0] < rect->xmin && s2[0] < rect->xmin) {
    return false;
  }
  if (s1[0] > rect->xmax && s2[0] > rect->xmax) {
    return false;
  }
  if (s1[1] < rect->ymin && s2[1] < rect->ymin) {
    return false;
  }
  if (s1[1] > rect->ymax && s2[1] > rect->ymax) {
    return false;
  }

  /* if either points intersect then we definetly intersect */
  if (BLI_rcti_isect_pt_v(rect, s1) || BLI_rcti_isect_pt_v(rect, s2)) {
    return true;
  }
  else {
    /* both points are outside but may insersect the rect */
    int tvec1[2];
    int tvec2[2];
    /* diagonal: [/] */
    tvec1[0] = rect->xmin;
    tvec1[1] = rect->ymin;
    tvec2[0] = rect->xmin;
    tvec2[1] = rect->ymax;
    if (isect_segments_i(s1, s2, tvec1, tvec2)) {
      return true;
    }

    /* diagonal: [\] */
    tvec1[0] = rect->xmin;
    tvec1[1] = rect->ymax;
    tvec2[0] = rect->xmax;
    tvec2[1] = rect->ymin;
    if (isect_segments_i(s1, s2, tvec1, tvec2)) {
      return true;
    }

    /* no intersection */
    return false;
  }
}

bool BLI_rctf_isect_segment(const rctf *rect, const float s1[2], const float s2[2])
{
  /* first do outside-bounds check for both points of the segment */
  if (s1[0] < rect->xmin && s2[0] < rect->xmin) {
    return false;
  }
  if (s1[0] > rect->xmax && s2[0] > rect->xmax) {
    return false;
  }
  if (s1[1] < rect->ymin && s2[1] < rect->ymin) {
    return false;
  }
  if (s1[1] > rect->ymax && s2[1] > rect->ymax) {
    return false;
  }

  /* if either points intersect then we definetly intersect */
  if (BLI_rctf_isect_pt_v(rect, s1) || BLI_rctf_isect_pt_v(rect, s2)) {
    return true;
  }
  else {
    /* both points are outside but may insersect the rect */
    float tvec1[2];
    float tvec2[2];
    /* diagonal: [/] */
    tvec1[0] = rect->xmin;
    tvec1[1] = rect->ymin;
    tvec2[0] = rect->xmin;
    tvec2[1] = rect->ymax;
    if (isect_segments_fl(s1, s2, tvec1, tvec2)) {
      return true;
    }

    /* diagonal: [\] */
    tvec1[0] = rect->xmin;
    tvec1[1] = rect->ymax;
    tvec2[0] = rect->xmax;
    tvec2[1] = rect->ymin;
    if (isect_segments_fl(s1, s2, tvec1, tvec2)) {
      return true;
    }

    /* no intersection */
    return false;
  }
}

bool BLI_rcti_isect_circle(const rcti *rect, const float xy[2], const float radius)
{
  float dx, dy;

  if (xy[0] >= rect->xmin && xy[0] <= rect->xmax) {
    dx = 0;
  }
  else {
    dx = (xy[0] < rect->xmin) ? (rect->xmin - xy[0]) : (xy[0] - rect->xmax);
  }

  if (xy[1] >= rect->ymin && xy[1] <= rect->ymax) {
    dy = 0;
  }
  else {
    dy = (xy[1] < rect->ymin) ? (rect->ymin - xy[1]) : (xy[1] - rect->ymax);
  }

  return dx * dx + dy * dy <= radius * radius;
}

bool BLI_rctf_isect_circle(const rctf *rect, const float xy[2], const float radius)
{
  float dx, dy;

  if (xy[0] >= rect->xmin && xy[0] <= rect->xmax) {
    dx = 0;
  }
  else {
    dx = (xy[0] < rect->xmin) ? (rect->xmin - xy[0]) : (xy[0] - rect->xmax);
  }

  if (xy[1] >= rect->ymin && xy[1] <= rect->ymax) {
    dy = 0;
  }
  else {
    dy = (xy[1] < rect->ymin) ? (rect->ymin - xy[1]) : (xy[1] - rect->ymax);
  }

  return dx * dx + dy * dy <= radius * radius;
}

void BLI_rctf_union(rctf *rct1, const rctf *rct2)
{
  if (rct1->xmin > rct2->xmin) {
    rct1->xmin = rct2->xmin;
  }
  if (rct1->xmax < rct2->xmax) {
    rct1->xmax = rct2->xmax;
  }
  if (rct1->ymin > rct2->ymin) {
    rct1->ymin = rct2->ymin;
  }
  if (rct1->ymax < rct2->ymax) {
    rct1->ymax = rct2->ymax;
  }
}

void BLI_rcti_union(rcti *rct1, const rcti *rct2)
{
  if (rct1->xmin > rct2->xmin) {
    rct1->xmin = rct2->xmin;
  }
  if (rct1->xmax < rct2->xmax) {
    rct1->xmax = rct2->xmax;
  }
  if (rct1->ymin > rct2->ymin) {
    rct1->ymin = rct2->ymin;
  }
  if (rct1->ymax < rct2->ymax) {
    rct1->ymax = rct2->ymax;
  }
}

void BLI_rctf_init(rctf *rect, float xmin, float xmax, float ymin, float ymax)
{
  if (xmin <= xmax) {
    rect->xmin = xmin;
    rect->xmax = xmax;
  }
  else {
    rect->xmax = xmin;
    rect->xmin = xmax;
  }
  if (ymin <= ymax) {
    rect->ymin = ymin;
    rect->ymax = ymax;
  }
  else {
    rect->ymax = ymin;
    rect->ymin = ymax;
  }
}

void BLI_rcti_init(rcti *rect, int xmin, int xmax, int ymin, int ymax)
{
  if (xmin <= xmax) {
    rect->xmin = xmin;
    rect->xmax = xmax;
  }
  else {
    rect->xmax = xmin;
    rect->xmin = xmax;
  }
  if (ymin <= ymax) {
    rect->ymin = ymin;
    rect->ymax = ymax;
  }
  else {
    rect->ymax = ymin;
    rect->ymin = ymax;
  }
}

void BLI_rctf_init_pt_radius(rctf *rect, const float xy[2], float size)
{
  rect->xmin = xy[0] - size;
  rect->xmax = xy[0] + size;
  rect->ymin = xy[1] - size;
  rect->ymax = xy[1] + size;
}

void BLI_rcti_init_pt_radius(rcti *rect, const int xy[2], int size)
{
  rect->xmin = xy[0] - size;
  rect->xmax = xy[0] + size;
  rect->ymin = xy[1] - size;
  rect->ymax = xy[1] + size;
}

void BLI_rcti_init_minmax(rcti *rect)
{
  rect->xmin = rect->ymin = INT_MAX;
  rect->xmax = rect->ymax = INT_MIN;
}

void BLI_rctf_init_minmax(rctf *rect)
{
  rect->xmin = rect->ymin = FLT_MAX;
  rect->xmax = rect->ymax = -FLT_MAX;
}

void BLI_rcti_do_minmax_v(rcti *rect, const int xy[2])
{
  if (xy[0] < rect->xmin) {
    rect->xmin = xy[0];
  }
  if (xy[0] > rect->xmax) {
    rect->xmax = xy[0];
  }
  if (xy[1] < rect->ymin) {
    rect->ymin = xy[1];
  }
  if (xy[1] > rect->ymax) {
    rect->ymax = xy[1];
  }
}

void BLI_rctf_do_minmax_v(rctf *rect, const float xy[2])
{
  if (xy[0] < rect->xmin) {
    rect->xmin = xy[0];
  }
  if (xy[0] > rect->xmax) {
    rect->xmax = xy[0];
  }
  if (xy[1] < rect->ymin) {
    rect->ymin = xy[1];
  }
  if (xy[1] > rect->ymax) {
    rect->ymax = xy[1];
  }
}

/* given 2 rectangles - transform a point from one to another */
void BLI_rctf_transform_pt_v(const rctf *dst,
                             const rctf *src,
                             float xy_dst[2],
                             const float xy_src[2])
{
  xy_dst[0] = ((xy_src[0] - src->xmin) / (src->xmax - src->xmin));
  xy_dst[0] = dst->xmin + ((dst->xmax - dst->xmin) * xy_dst[0]);

  xy_dst[1] = ((xy_src[1] - src->ymin) / (src->ymax - src->ymin));
  xy_dst[1] = dst->ymin + ((dst->ymax - dst->ymin) * xy_dst[1]);
}

/**
 * Calculate a 4x4 matrix representing the transformation between two rectangles.
 *
 * \note Multiplying a vector by this matrix does *not*
 * give the same value as #BLI_rctf_transform_pt_v.
 */
void BLI_rctf_transform_calc_m4_pivot_min_ex(
    const rctf *dst, const rctf *src, float matrix[4][4], uint x, uint y)
{
  BLI_assert(x < 3 && y < 3);

  unit_m4(matrix);

  matrix[x][x] = BLI_rctf_size_x(src) / BLI_rctf_size_x(dst);
  matrix[y][y] = BLI_rctf_size_y(src) / BLI_rctf_size_y(dst);
  matrix[3][x] = (src->xmin - dst->xmin) * matrix[x][x];
  matrix[3][y] = (src->ymin - dst->ymin) * matrix[y][y];
}

void BLI_rctf_transform_calc_m4_pivot_min(const rctf *dst, const rctf *src, float matrix[4][4])
{
  BLI_rctf_transform_calc_m4_pivot_min_ex(dst, src, matrix, 0, 1);
}

void BLI_rcti_translate(rcti *rect, int x, int y)
{
  rect->xmin += x;
  rect->ymin += y;
  rect->xmax += x;
  rect->ymax += y;
}
void BLI_rctf_translate(rctf *rect, float x, float y)
{
  rect->xmin += x;
  rect->ymin += y;
  rect->xmax += x;
  rect->ymax += y;
}

void BLI_rcti_recenter(rcti *rect, int x, int y)
{
  const int dx = x - BLI_rcti_cent_x(rect);
  const int dy = y - BLI_rcti_cent_y(rect);
  BLI_rcti_translate(rect, dx, dy);
}
void BLI_rctf_recenter(rctf *rect, float x, float y)
{
  const float dx = x - BLI_rctf_cent_x(rect);
  const float dy = y - BLI_rctf_cent_y(rect);
  BLI_rctf_translate(rect, dx, dy);
}

/* change width & height around the central location */
void BLI_rcti_resize(rcti *rect, int x, int y)
{
  rect->xmin = BLI_rcti_cent_x(rect) - (x / 2);
  rect->ymin = BLI_rcti_cent_y(rect) - (y / 2);
  rect->xmax = rect->xmin + x;
  rect->ymax = rect->ymin + y;
}

void BLI_rctf_resize(rctf *rect, float x, float y)
{
  rect->xmin = BLI_rctf_cent_x(rect) - (x * 0.5f);
  rect->ymin = BLI_rctf_cent_y(rect) - (y * 0.5f);
  rect->xmax = rect->xmin + x;
  rect->ymax = rect->ymin + y;
}

void BLI_rcti_scale(rcti *rect, const float scale)
{
  const int cent_x = BLI_rcti_cent_x(rect);
  const int cent_y = BLI_rcti_cent_y(rect);
  const int size_x_half = BLI_rcti_size_x(rect) * (scale * 0.5f);
  const int size_y_half = BLI_rcti_size_y(rect) * (scale * 0.5f);
  rect->xmin = cent_x - size_x_half;
  rect->ymin = cent_y - size_y_half;
  rect->xmax = cent_x + size_x_half;
  rect->ymax = cent_y + size_y_half;
}

void BLI_rctf_scale(rctf *rect, const float scale)
{
  const float cent_x = BLI_rctf_cent_x(rect);
  const float cent_y = BLI_rctf_cent_y(rect);
  const float size_x_half = BLI_rctf_size_x(rect) * (scale * 0.5f);
  const float size_y_half = BLI_rctf_size_y(rect) * (scale * 0.5f);
  rect->xmin = cent_x - size_x_half;
  rect->ymin = cent_y - size_y_half;
  rect->xmax = cent_x + size_x_half;
  rect->ymax = cent_y + size_y_half;
}

void BLI_rctf_padding_y(rctf *rect,
                        const float boundary_height,
                        const float padding_top,
                        const float padding_bottom)
{
  BLI_assert(padding_top >= 0.0f);
  BLI_assert(padding_bottom >= 0.0f);
  BLI_assert(boundary_height > 0.0f);

  float total_padding = padding_top + padding_bottom;
  if (total_padding == 0.0f) {
    return;
  }

  float total_extend = BLI_rctf_size_y(rect) * total_padding / (boundary_height - total_padding);
  rect->ymax += total_extend * (padding_top / total_padding);
  rect->ymin -= total_extend * (padding_bottom / total_padding);
}

void BLI_rctf_interp(rctf *rect, const rctf *rect_a, const rctf *rect_b, const float fac)
{
  const float ifac = 1.0f - fac;
  rect->xmin = (rect_a->xmin * ifac) + (rect_b->xmin * fac);
  rect->xmax = (rect_a->xmax * ifac) + (rect_b->xmax * fac);
  rect->ymin = (rect_a->ymin * ifac) + (rect_b->ymin * fac);
  rect->ymax = (rect_a->ymax * ifac) + (rect_b->ymax * fac);
}

/* BLI_rcti_interp() not needed yet */

bool BLI_rctf_clamp_pt_v(const rctf *rect, float xy[2])
{
  bool changed = false;
  if (xy[0] < rect->xmin) {
    xy[0] = rect->xmin;
    changed = true;
  }
  if (xy[0] > rect->xmax) {
    xy[0] = rect->xmax;
    changed = true;
  }
  if (xy[1] < rect->ymin) {
    xy[1] = rect->ymin;
    changed = true;
  }
  if (xy[1] > rect->ymax) {
    xy[1] = rect->ymax;
    changed = true;
  }
  return changed;
}

bool BLI_rcti_clamp_pt_v(const rcti *rect, int xy[2])
{
  bool changed = false;
  if (xy[0] < rect->xmin) {
    xy[0] = rect->xmin;
    changed = true;
  }
  if (xy[0] > rect->xmax) {
    xy[0] = rect->xmax;
    changed = true;
  }
  if (xy[1] < rect->ymin) {
    xy[1] = rect->ymin;
    changed = true;
  }
  if (xy[1] > rect->ymax) {
    xy[1] = rect->ymax;
    changed = true;
  }
  return changed;
}

/**
 * Clamp \a rect within \a rect_bounds, setting \a r_xy to the offset.
 *
 * Keeps the top left corner within the bounds, which for user interface
 * elements is typically where the most important information is.
 *
 * \return true if a change is made.
 */
bool BLI_rctf_clamp(rctf *rect, const rctf *rect_bounds, float r_xy[2])
{
  bool changed = false;

  r_xy[0] = 0.0f;
  r_xy[1] = 0.0f;

  if (rect->xmax > rect_bounds->xmax) {
    float ofs = rect_bounds->xmax - rect->xmax;
    rect->xmin += ofs;
    rect->xmax += ofs;
    r_xy[0] += ofs;
    changed = true;
  }

  if (rect->xmin < rect_bounds->xmin) {
    float ofs = rect_bounds->xmin - rect->xmin;
    rect->xmin += ofs;
    rect->xmax += ofs;
    r_xy[0] += ofs;
    changed = true;
  }

  if (rect->ymin < rect_bounds->ymin) {
    float ofs = rect_bounds->ymin - rect->ymin;
    rect->ymin += ofs;
    rect->ymax += ofs;
    r_xy[1] += ofs;
    changed = true;
  }

  if (rect->ymax > rect_bounds->ymax) {
    float ofs = rect_bounds->ymax - rect->ymax;
    rect->ymin += ofs;
    rect->ymax += ofs;
    r_xy[1] += ofs;
    changed = true;
  }

  return changed;
}

bool BLI_rcti_clamp(rcti *rect, const rcti *rect_bounds, int r_xy[2])
{
  bool changed = false;

  r_xy[0] = 0;
  r_xy[1] = 0;

  if (rect->xmax > rect_bounds->xmax) {
    int ofs = rect_bounds->xmax - rect->xmax;
    rect->xmin += ofs;
    rect->xmax += ofs;
    r_xy[0] += ofs;
    changed = true;
  }

  if (rect->xmin < rect_bounds->xmin) {
    int ofs = rect_bounds->xmin - rect->xmin;
    rect->xmin += ofs;
    rect->xmax += ofs;
    r_xy[0] += ofs;
    changed = true;
  }

  if (rect->ymin < rect_bounds->ymin) {
    int ofs = rect_bounds->ymin - rect->ymin;
    rect->ymin += ofs;
    rect->ymax += ofs;
    r_xy[1] += ofs;
    changed = true;
  }

  if (rect->ymax > rect_bounds->ymax) {
    int ofs = rect_bounds->ymax - rect->ymax;
    rect->ymin += ofs;
    rect->ymax += ofs;
    r_xy[1] += ofs;
    changed = true;
  }

  return changed;
}

bool BLI_rctf_compare(const rctf *rect_a, const rctf *rect_b, const float limit)
{
  if (fabsf(rect_a->xmin - rect_b->xmin) < limit) {
    if (fabsf(rect_a->xmax - rect_b->xmax) < limit) {
      if (fabsf(rect_a->ymin - rect_b->ymin) < limit) {
        if (fabsf(rect_a->ymax - rect_b->ymax) < limit) {
          return true;
        }
      }
    }
  }

  return false;
}

bool BLI_rcti_compare(const rcti *rect_a, const rcti *rect_b)
{
  if (rect_a->xmin == rect_b->xmin) {
    if (rect_a->xmax == rect_b->xmax) {
      if (rect_a->ymin == rect_b->ymin) {
        if (rect_a->ymax == rect_b->ymax) {
          return true;
        }
      }
    }
  }

  return false;
}

bool BLI_rctf_isect(const rctf *src1, const rctf *src2, rctf *dest)
{
  float xmin, xmax;
  float ymin, ymax;

  xmin = (src1->xmin) > (src2->xmin) ? (src1->xmin) : (src2->xmin);
  xmax = (src1->xmax) < (src2->xmax) ? (src1->xmax) : (src2->xmax);
  ymin = (src1->ymin) > (src2->ymin) ? (src1->ymin) : (src2->ymin);
  ymax = (src1->ymax) < (src2->ymax) ? (src1->ymax) : (src2->ymax);

  if (xmax >= xmin && ymax >= ymin) {
    if (dest) {
      dest->xmin = xmin;
      dest->xmax = xmax;
      dest->ymin = ymin;
      dest->ymax = ymax;
    }
    return true;
  }
  else {
    if (dest) {
      dest->xmin = 0;
      dest->xmax = 0;
      dest->ymin = 0;
      dest->ymax = 0;
    }
    return false;
  }
}

bool BLI_rcti_isect(const rcti *src1, const rcti *src2, rcti *dest)
{
  int xmin, xmax;
  int ymin, ymax;

  xmin = (src1->xmin) > (src2->xmin) ? (src1->xmin) : (src2->xmin);
  xmax = (src1->xmax) < (src2->xmax) ? (src1->xmax) : (src2->xmax);
  ymin = (src1->ymin) > (src2->ymin) ? (src1->ymin) : (src2->ymin);
  ymax = (src1->ymax) < (src2->ymax) ? (src1->ymax) : (src2->ymax);

  if (xmax >= xmin && ymax >= ymin) {
    if (dest) {
      dest->xmin = xmin;
      dest->xmax = xmax;
      dest->ymin = ymin;
      dest->ymax = ymax;
    }
    return true;
  }
  else {
    if (dest) {
      dest->xmin = 0;
      dest->xmax = 0;
      dest->ymin = 0;
      dest->ymax = 0;
    }
    return false;
  }
}

void BLI_rcti_rctf_copy(rcti *dst, const rctf *src)
{
  dst->xmin = floorf(src->xmin + 0.5f);
  dst->xmax = dst->xmin + floorf(BLI_rctf_size_x(src) + 0.5f);
  dst->ymin = floorf(src->ymin + 0.5f);
  dst->ymax = dst->ymin + floorf(BLI_rctf_size_y(src) + 0.5f);
}

void BLI_rcti_rctf_copy_floor(rcti *dst, const rctf *src)
{
  dst->xmin = floorf(src->xmin);
  dst->xmax = floorf(src->xmax);
  dst->ymin = floorf(src->ymin);
  dst->ymax = floorf(src->ymax);
}

void BLI_rcti_rctf_copy_round(rcti *dst, const rctf *src)
{
  dst->xmin = floorf(src->xmin + 0.5f);
  dst->xmax = floorf(src->xmax + 0.5f);
  dst->ymin = floorf(src->ymin + 0.5f);
  dst->ymax = floorf(src->ymax + 0.5f);
}

void BLI_rctf_rcti_copy(rctf *dst, const rcti *src)
{
  dst->xmin = src->xmin;
  dst->xmax = src->xmax;
  dst->ymin = src->ymin;
  dst->ymax = src->ymax;
}

void print_rctf(const char *str, const rctf *rect)
{
  printf("%s: xmin %.8f, xmax %.8f, ymin %.8f, ymax %.8f (%.12fx%.12f)\n",
         str,
         rect->xmin,
         rect->xmax,
         rect->ymin,
         rect->ymax,
         BLI_rctf_size_x(rect),
         BLI_rctf_size_y(rect));
}

void print_rcti(const char *str, const rcti *rect)
{
  printf("%s: xmin %d, xmax %d, ymin %d, ymax %d (%dx%d)\n",
         str,
         rect->xmin,
         rect->xmax,
         rect->ymin,
         rect->ymax,
         BLI_rcti_size_x(rect),
         BLI_rcti_size_y(rect));
}

/* -------------------------------------------------------------------- */
/* Comprehensive math (float only) */

/** \name Rect math functions
 * \{ */

#define ROTATE_SINCOS(r_vec, mat2, vec) \
  { \
    (r_vec)[0] = (mat2)[1] * (vec)[0] + (+(mat2)[0]) * (vec)[1]; \
    (r_vec)[1] = (mat2)[0] * (vec)[0] + (-(mat2)[1]) * (vec)[1]; \
  } \
  ((void)0)

/**
 * Expand the rectangle to fit a rotated \a src.
 */
void BLI_rctf_rotate_expand(rctf *dst, const rctf *src, const float angle)
{
  const float mat2[2] = {sinf(angle), cosf(angle)};
  const float cent[2] = {BLI_rctf_cent_x(src), BLI_rctf_cent_y(src)};
  float corner[2], corner_rot[2], corder_max[2];

  /* x is same for both corners */
  corner[0] = src->xmax - cent[0];
  corner[1] = src->ymax - cent[1];
  ROTATE_SINCOS(corner_rot, mat2, corner);
  corder_max[0] = fabsf(corner_rot[0]);
  corder_max[1] = fabsf(corner_rot[1]);

  corner[1] *= -1;
  ROTATE_SINCOS(corner_rot, mat2, corner);
  corder_max[0] = MAX2(corder_max[0], fabsf(corner_rot[0]));
  corder_max[1] = MAX2(corder_max[1], fabsf(corner_rot[1]));

  dst->xmin = cent[0] - corder_max[0];
  dst->xmax = cent[0] + corder_max[0];
  dst->ymin = cent[1] - corder_max[1];
  dst->ymax = cent[1] + corder_max[1];
}

#undef ROTATE_SINCOS

/** \} */

static void unit_m4(float m[4][4])
{
  m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
  m[0][1] = m[0][2] = m[0][3] = 0.0f;
  m[1][0] = m[1][2] = m[1][3] = 0.0f;
  m[2][0] = m[2][1] = m[2][3] = 0.0f;
  m[3][0] = m[3][1] = m[3][2] = 0.0f;
}
