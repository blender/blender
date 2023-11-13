/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_compat.h"
#include "BLI_sys_types.h" /* bool */
#include "DNA_vec_types.h"

struct rctf;
struct rcti;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determine if a `rect` is empty.
 * An empty `rect` is one with a zero (or negative) width or height.
 *
 * \return True if \a rect is empty.
 */
bool BLI_rcti_is_empty(const struct rcti *rect);
bool BLI_rctf_is_empty(const struct rctf *rect);
void BLI_rctf_init(struct rctf *rect, float xmin, float xmax, float ymin, float ymax);
void BLI_rcti_init(struct rcti *rect, int xmin, int xmax, int ymin, int ymax);
/**
 * Check if X-min and Y-min are less than or equal to X-max and Y-max, respectively.
 * If this returns false, #BLI_rctf_sanitize() can be called to address this.
 *
 * This is not a hard constraint or invariant for rectangles, in some cases it may be useful to
 * have max < min. Usually this is what you'd want though.
 */
bool BLI_rctf_is_valid(const struct rctf *rect);
bool BLI_rcti_is_valid(const struct rcti *rect);
/**
 * Ensure X-min and Y-min are less than or equal to X-max and Y-max, respectively.
 */
void BLI_rctf_sanitize(struct rctf *rect);
void BLI_rcti_sanitize(struct rcti *rect);
void BLI_rctf_init_pt_radius(struct rctf *rect, const float xy[2], float size);
void BLI_rcti_init_pt_radius(struct rcti *rect, const int xy[2], int size);
void BLI_rcti_init_minmax(struct rcti *rect);
void BLI_rctf_init_minmax(struct rctf *rect);
void BLI_rcti_do_minmax_v(struct rcti *rect, const int xy[2]);
void BLI_rctf_do_minmax_v(struct rctf *rect, const float xy[2]);
void BLI_rcti_do_minmax_rcti(struct rcti *rect, const struct rcti *other);

/**
 * Given 2 rectangles, transform a point from one to another.
 */
void BLI_rctf_transform_pt_v(const rctf *dst,
                             const rctf *src,
                             float xy_dst[2],
                             const float xy_src[2]);
/**
 * Calculate a 4x4 matrix representing the transformation between two rectangles.
 *
 * \note Multiplying a vector by this matrix does *not*
 * give the same value as #BLI_rctf_transform_pt_v.
 */
void BLI_rctf_transform_calc_m4_pivot_min_ex(
    const rctf *dst, const rctf *src, float matrix[4][4], uint x, uint y);
void BLI_rctf_transform_calc_m4_pivot_min(const rctf *dst, const rctf *src, float matrix[4][4]);

void BLI_rctf_translate(struct rctf *rect, float x, float y);
void BLI_rcti_translate(struct rcti *rect, int x, int y);
void BLI_rctf_mul(struct rctf *rect, float factor);
void BLI_rcti_mul(struct rcti *rect, int factor);
void BLI_rcti_recenter(struct rcti *rect, int x, int y);
void BLI_rctf_recenter(struct rctf *rect, float x, float y);
void BLI_rcti_resize(struct rcti *rect, int x, int y);
/**
 * Change width & height around the central X location.
 */
void BLI_rcti_resize_x(struct rcti *rect, int x);
/**
 * Change width & height around the central Y location.
 */
void BLI_rcti_resize_y(struct rcti *rect, int y);
void BLI_rcti_pad(struct rcti *rect, int pad_x, int pad_y);
void BLI_rctf_pad(struct rctf *rect, float pad_x, float pad_y);
void BLI_rctf_resize(struct rctf *rect, float x, float y);
void BLI_rctf_resize_x(struct rctf *rect, float x);
void BLI_rctf_resize_y(struct rctf *rect, float y);
void BLI_rcti_scale(rcti *rect, float scale);
void BLI_rctf_scale(rctf *rect, float scale);
void BLI_rctf_pad_y(struct rctf *rect, float boundary_size, float pad_min, float pad_max);
void BLI_rctf_interp(struct rctf *rect,
                     const struct rctf *rect_a,
                     const struct rctf *rect_b,
                     float fac);
// void BLI_rcti_interp(struct rctf *rect, struct rctf *rect_a, struct rctf *rect_b, float fac);
bool BLI_rctf_clamp_pt_v(const struct rctf *rect, float xy[2]);
bool BLI_rcti_clamp_pt_v(const struct rcti *rect, int xy[2]);
/**
 * Clamp \a rect within \a rect_bounds, setting \a r_xy to the offset.
 *
 * Keeps the top left corner within the bounds, which for user interface
 * elements is typically where the most important information is.
 *
 * \return true if a change is made.
 */
bool BLI_rctf_clamp(struct rctf *rect, const struct rctf *rect_bounds, float r_xy[2]);
bool BLI_rcti_clamp(struct rcti *rect, const struct rcti *rect_bounds, int r_xy[2]);
bool BLI_rctf_compare(const struct rctf *rect_a, const struct rctf *rect_b, float limit);
bool BLI_rcti_compare(const struct rcti *rect_a, const struct rcti *rect_b);
bool BLI_rctf_isect(const struct rctf *src1, const struct rctf *src2, struct rctf *dest);
bool BLI_rcti_isect(const struct rcti *src1, const struct rcti *src2, struct rcti *dest);
bool BLI_rctf_isect_rect_x(const struct rctf *src1, const struct rctf *src2, float range_x[2]);
bool BLI_rctf_isect_rect_y(const struct rctf *src1, const struct rctf *src2, float range_y[2]);
bool BLI_rcti_isect_rect_x(const struct rcti *src1, const struct rcti *src2, int range_x[2]);
bool BLI_rcti_isect_rect_y(const struct rcti *src1, const struct rcti *src2, int range_y[2]);
bool BLI_rcti_isect_x(const rcti *rect, int x);
bool BLI_rcti_isect_y(const rcti *rect, int y);
bool BLI_rcti_isect_pt(const struct rcti *rect, int x, int y);
bool BLI_rcti_isect_pt_v(const struct rcti *rect, const int xy[2]);
bool BLI_rctf_isect_x(const rctf *rect, float x);
bool BLI_rctf_isect_y(const rctf *rect, float y);
bool BLI_rctf_isect_pt(const struct rctf *rect, float x, float y);
bool BLI_rctf_isect_pt_v(const struct rctf *rect, const float xy[2]);
/**
 * \returns shortest distance from \a rect to x (0 if inside)
 */
int BLI_rcti_length_x(const rcti *rect, int x);
/**
 * \returns shortest distance from \a rect to y (0 if inside)
 */
int BLI_rcti_length_y(const rcti *rect, int y);
float BLI_rctf_length_x(const rctf *rect, float x);
float BLI_rctf_length_y(const rctf *rect, float y);
bool BLI_rcti_isect_segment(const struct rcti *rect, const int s1[2], const int s2[2]);
bool BLI_rctf_isect_segment(const struct rctf *rect, const float s1[2], const float s2[2]);
bool BLI_rcti_isect_circle(const struct rcti *rect, const float xy[2], float radius);
bool BLI_rctf_isect_circle(const struct rctf *rect, const float xy[2], float radius);
bool BLI_rcti_inside_rcti(const rcti *rct_a, const rcti *rct_b);
/**
 * is \a rct_b inside \a rct_a
 */
bool BLI_rctf_inside_rctf(const rctf *rct_a, const rctf *rct_b);
void BLI_rcti_union(struct rcti *rct_a, const struct rcti *rct_b);
void BLI_rctf_union(struct rctf *rct_a, const struct rctf *rct_b);
void BLI_rcti_rctf_copy(struct rcti *dst, const struct rctf *src);
void BLI_rctf_rcti_copy(struct rctf *dst, const struct rcti *src);
void BLI_rcti_rctf_copy_floor(struct rcti *dst, const struct rctf *src);
void BLI_rcti_rctf_copy_round(struct rcti *dst, const struct rctf *src);

/**
 * Expand the rectangle to fit a rotated \a src.
 */
void BLI_rctf_rotate_expand(rctf *dst, const rctf *src, float angle);

void print_rctf(const char *str, const struct rctf *rect);
void print_rcti(const char *str, const struct rcti *rect);

#define print_rctf_id(rect) print_rctf(STRINGIFY(rect), rect)
#define print_rcti_id(rect) print_rcti(STRINGIFY(rect), rect)

BLI_INLINE float BLI_rcti_cent_x_fl(const struct rcti *rct)
{
  return (float)(rct->xmin + rct->xmax) / 2.0f;
}
BLI_INLINE float BLI_rcti_cent_y_fl(const struct rcti *rct)
{
  return (float)(rct->ymin + rct->ymax) / 2.0f;
}
BLI_INLINE int BLI_rcti_cent_x(const struct rcti *rct)
{
  return (rct->xmin + rct->xmax) / 2;
}
BLI_INLINE int BLI_rcti_cent_y(const struct rcti *rct)
{
  return (rct->ymin + rct->ymax) / 2;
}
BLI_INLINE float BLI_rctf_cent_x(const struct rctf *rct)
{
  return (rct->xmin + rct->xmax) / 2.0f;
}
BLI_INLINE float BLI_rctf_cent_y(const struct rctf *rct)
{
  return (rct->ymin + rct->ymax) / 2.0f;
}

BLI_INLINE int BLI_rcti_size_x(const struct rcti *rct)
{
  return (rct->xmax - rct->xmin);
}
BLI_INLINE int BLI_rcti_size_y(const struct rcti *rct)
{
  return (rct->ymax - rct->ymin);
}
BLI_INLINE float BLI_rctf_size_x(const struct rctf *rct)
{
  return (rct->xmax - rct->xmin);
}
BLI_INLINE float BLI_rctf_size_y(const struct rctf *rct)
{
  return (rct->ymax - rct->ymin);
}

#ifdef __cplusplus
}
#endif
