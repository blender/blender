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
 * \ingroup render
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Span fill in method, is also used to localize data for Z-buffering. */
typedef struct ZSpan {
  int rectx, recty; /* range for clipping */

  int miny1, maxy1, miny2, maxy2;             /* actual filled in range */
  const float *minp1, *maxp1, *minp2, *maxp2; /* vertex pointers detect min/max range in */
  float *span1, *span2;
} ZSpan;

/**
 * Each Z-buffer has coordinates transformed to local rect coordinates, so we can simply clip.
 */
void zbuf_alloc_span(struct ZSpan *zspan, int rectx, int recty);
void zbuf_free_span(struct ZSpan *zspan);

/**
 * Scan-convert for strand triangles, calls function for each x, y coordinate
 * and gives UV barycentrics and z.
 */
void zspan_scanconvert(struct ZSpan *zspan,
                       void *handle,
                       float *v1,
                       float *v2,
                       float *v3,
                       void (*func)(void *, int, int, float, float));

#ifdef __cplusplus
}
#endif
