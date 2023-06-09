/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Heap;
struct MemArena;

/**
 * The intention is that this calculates the output of #BLI_polyfill_calc
 * \note assumes the \a coords form a boundary,
 * so any edges running along contiguous (wrapped) indices,
 * are ignored since the edges won't share 2 faces.
 */
void BLI_polyfill_beautify(const float (*coords)[2],
                           unsigned int coords_num,
                           unsigned int (*tris)[3],

                           /* structs for reuse */
                           struct MemArena *arena,
                           struct Heap *eheap);

/**
 * Assuming we have 2 triangles sharing an edge (2 - 4),
 * check if the edge running from (1 - 3) gives better results.
 *
 * \param lock_degenerate: Use to avoid rotating out of a degenerate state:
 * - When true, an existing zero area face on either side of the (2 - 4
 *   split will return a positive value.
 * - When false, the check must be non-biased towards either split direction.
 * \param r_area: Return the area of the quad,
 * This can be useful when comparing the return value with near zero epsilons.
 * In this case the epsilon can be scaled by the area to avoid the return value
 * of very large faces not having a reliable way to detect near-zero output.
 *
 * \return (negative number means the edge can be rotated, lager == better).
 */
float BLI_polyfill_beautify_quad_rotate_calc_ex(const float v1[2],
                                                const float v2[2],
                                                const float v3[2],
                                                const float v4[2],
                                                bool lock_degenerate,
                                                float *r_area);
#define BLI_polyfill_beautify_quad_rotate_calc(v1, v2, v3, v4) \
  BLI_polyfill_beautify_quad_rotate_calc_ex(v1, v2, v3, v4, false, NULL)

/* avoid realloc's when creating new structures for polyfill ngons */
#define BLI_POLYFILL_ALLOC_NGON_RESERVE 64

#ifdef __cplusplus
}
#endif
