/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

namespace blender {

/** Span fill in method, is also used to localize data for Z-buffering. */
struct ZSpan {
  int rectx, recty; /* range for clipping */

  int miny1, maxy1, miny2, maxy2;             /* actual filled in range */
  const float *minp1, *maxp1, *minp2, *maxp2; /* vertex pointers detect min/max range in */
  float *span1, *span2;
};

/**
 * Each Z-buffer has coordinates transformed to local rect coordinates, so we can simply clip.
 */
void zbuf_alloc_span(struct ZSpan *zspan, int rectx, int recty);
void zbuf_free_span(struct ZSpan *zspan);

/**
 * Scan-convert for strand triangles, calls function for each x, y coordinate
 * and gives UV barycentrics.
 */
void zspan_rasterize_triangle(struct ZSpan *zspan,
                              void *handle,
                              const float *v1,
                              const float *v2,
                              const float *v3,
                              void (*func)(void *, int, int, float, float));

/**
 * Conservatively rasterize triangle wireframe, calls function for each x, y coordinate
 * and gives UV barycentrics.
 */
void zspan_rasterize_conservative_wireframe(ZSpan *zspan,
                                            void *handle,
                                            const float *v1,
                                            const float *v2,
                                            const float *v3,
                                            void (*func)(void *, int, int, float, float));

}  // namespace blender
