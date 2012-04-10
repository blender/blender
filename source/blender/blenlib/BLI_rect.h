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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef __BLI_RECT_H__
#define __BLI_RECT_H__

/** \file BLI_rect.h
 *  \ingroup bli
 */

struct rctf;
struct rcti;

#ifdef __cplusplus
extern "C" {
#endif

/* BLI_rct.c */
/**
 * Determine if a rect is empty. An empty
 * rect is one with a zero (or negative)
 * width or height.
 *
 * \return True if \a rect is empty.
 */
int  BLI_rcti_is_empty(struct rcti *rect);
int  BLI_rctf_is_empty(struct rctf *rect);
void BLI_init_rctf(struct rctf *rect, float xmin, float xmax, float ymin, float ymax);
void BLI_init_rcti(struct rcti *rect, int xmin, int xmax, int ymin, int ymax);
void BLI_translate_rctf(struct rctf *rect, float x, float y);
void BLI_translate_rcti(struct rcti *rect, int x, int y);
void BLI_resize_rcti(struct rcti *rect, int x, int y);
void BLI_resize_rctf(struct rctf *rect, float x, float y);
int  BLI_in_rcti(struct rcti *rect, int x, int y);
int  BLI_in_rctf(struct rctf *rect, float x, float y);
int  BLI_segment_in_rcti(struct rcti *rect, int s1[2], int s2[2]);
// int  BLI_segment_in_rctf(struct rcti *rect, int s1[2], int s2[2]); // NOT NEEDED YET
int  BLI_isect_rctf(struct rctf *src1, struct rctf *src2, struct rctf *dest);
int  BLI_isect_rcti(struct rcti *src1, struct rcti *src2, struct rcti *dest);
void BLI_union_rctf(struct rctf *rcta, struct rctf *rctb);
void BLI_union_rcti(struct rcti *rcti1, struct rcti *rcti2);
void BLI_copy_rcti_rctf(struct rcti *tar, const struct rctf *src);

void print_rctf(const char *str, struct rctf *rect);
void print_rcti(const char *str, struct rcti *rect);

#ifdef __cplusplus
}
#endif

#endif
