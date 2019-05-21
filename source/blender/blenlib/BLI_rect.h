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

#ifndef __BLI_RECT_H__
#define __BLI_RECT_H__

/** \file
 * \ingroup bli
 */

#include "DNA_vec_types.h"
#include "BLI_sys_types.h" /* bool */
#include "BLI_compiler_compat.h"

struct rctf;
struct rcti;

#ifdef __cplusplus
extern "C" {
#endif

bool BLI_rcti_is_empty(const struct rcti *rect);
bool BLI_rctf_is_empty(const struct rctf *rect);
void BLI_rctf_init(struct rctf *rect, float xmin, float xmax, float ymin, float ymax);
void BLI_rcti_init(struct rcti *rect, int xmin, int xmax, int ymin, int ymax);
void BLI_rctf_init_pt_radius(struct rctf *rect, const float xy[2], float size);
void BLI_rcti_init_pt_radius(struct rcti *rect, const int xy[2], int size);
void BLI_rcti_init_minmax(struct rcti *rect);
void BLI_rctf_init_minmax(struct rctf *rect);
void BLI_rcti_do_minmax_v(struct rcti *rect, const int xy[2]);
void BLI_rctf_do_minmax_v(struct rctf *rect, const float xy[2]);

void BLI_rctf_transform_pt_v(const rctf *dst,
                             const rctf *src,
                             float xy_dst[2],
                             const float xy_src[2]);
void BLI_rctf_transform_calc_m4_pivot_min_ex(
    const rctf *dst, const rctf *src, float matrix[4][4], uint x, uint y);
void BLI_rctf_transform_calc_m4_pivot_min(const rctf *dst, const rctf *src, float matrix[4][4]);

void BLI_rctf_translate(struct rctf *rect, float x, float y);
void BLI_rcti_translate(struct rcti *rect, int x, int y);
void BLI_rcti_recenter(struct rcti *rect, int x, int y);
void BLI_rctf_recenter(struct rctf *rect, float x, float y);
void BLI_rcti_resize(struct rcti *rect, int x, int y);
void BLI_rctf_resize(struct rctf *rect, float x, float y);
void BLI_rcti_scale(rcti *rect, const float scale);
void BLI_rctf_scale(rctf *rect, const float scale);
void BLI_rctf_pad_y(struct rctf *rect,
                    const float boundary_size,
                    const float pad_min,
                    const float pad_max);
void BLI_rctf_interp(struct rctf *rect,
                     const struct rctf *rect_a,
                     const struct rctf *rect_b,
                     const float fac);
// void BLI_rcti_interp(struct rctf *rect, struct rctf *rect_a, struct rctf *rect_b, float fac);
bool BLI_rctf_clamp_pt_v(const struct rctf *rect, float xy[2]);
bool BLI_rcti_clamp_pt_v(const struct rcti *rect, int xy[2]);
bool BLI_rctf_clamp(struct rctf *rect, const struct rctf *rect_bounds, float r_xy[2]);
bool BLI_rcti_clamp(struct rcti *rect, const struct rcti *rect_bounds, int r_xy[2]);
bool BLI_rctf_compare(const struct rctf *rect_a, const struct rctf *rect_b, const float limit);
bool BLI_rcti_compare(const struct rcti *rect_a, const struct rcti *rect_b);
bool BLI_rctf_isect(const struct rctf *src1, const struct rctf *src2, struct rctf *dest);
bool BLI_rcti_isect(const struct rcti *src1, const struct rcti *src2, struct rcti *dest);
bool BLI_rcti_isect_x(const rcti *rect, const int x);
bool BLI_rcti_isect_y(const rcti *rect, const int y);
bool BLI_rcti_isect_pt(const struct rcti *rect, const int x, const int y);
bool BLI_rcti_isect_pt_v(const struct rcti *rect, const int xy[2]);
bool BLI_rctf_isect_x(const rctf *rect, const float x);
bool BLI_rctf_isect_y(const rctf *rect, const float y);
bool BLI_rctf_isect_pt(const struct rctf *rect, const float x, const float y);
bool BLI_rctf_isect_pt_v(const struct rctf *rect, const float xy[2]);
int BLI_rcti_length_x(const rcti *rect, const int x);
int BLI_rcti_length_y(const rcti *rect, const int y);
float BLI_rctf_length_x(const rctf *rect, const float x);
float BLI_rctf_length_y(const rctf *rect, const float y);
bool BLI_rcti_isect_segment(const struct rcti *rect, const int s1[2], const int s2[2]);
bool BLI_rctf_isect_segment(const struct rctf *rect, const float s1[2], const float s2[2]);
bool BLI_rcti_isect_circle(const struct rcti *rect, const float xy[2], const float radius);
bool BLI_rctf_isect_circle(const struct rctf *rect, const float xy[2], const float radius);
bool BLI_rcti_inside_rcti(const rcti *rct_a, const rcti *rct_b);
bool BLI_rctf_inside_rctf(const rctf *rct_a, const rctf *rct_b);
void BLI_rcti_union(struct rcti *rcti1, const struct rcti *rcti2);
void BLI_rctf_union(struct rctf *rctf1, const struct rctf *rctf2);
void BLI_rcti_rctf_copy(struct rcti *dst, const struct rctf *src);
void BLI_rctf_rcti_copy(struct rctf *dst, const struct rcti *src);
void BLI_rcti_rctf_copy_floor(struct rcti *dst, const struct rctf *src);
void BLI_rcti_rctf_copy_round(struct rcti *dst, const struct rctf *src);

void BLI_rctf_rotate_expand(rctf *dst, const rctf *src, const float angle);

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

#endif /* __BLI_RECT_H__ */
