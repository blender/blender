/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_assert.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

namespace blender {

struct MemArena;

/**
 * A version of #BLI_polyfill_calc that uses a memory arena to avoid re-allocations.
 */
void BLI_polyfill_calc_arena(const float (*coords)[2],
                             unsigned int coords_num,
                             int coords_sign,
                             unsigned int (*r_tris)[3],

                             struct MemArena *arena);

/**
 * Triangulates the given (convex or concave) simple polygon to a list of triangle vertices.
 *
 * \param coords: 2D coordinates describing vertices of the polygon,
 * in either clockwise or counterclockwise order.
 * \param coords_num: Total points in the array.
 * \param coords_sign: Pass this when we know the sign in advance to avoid extra calculations.
 *
 * \param r_tris: This array is filled in with triangle indices in clockwise order.
 * The length of the array must be `coords_num - 2`.
 * Indices are guaranteed to be assigned to unique triangles, with valid indices,
 * even in the case of degenerate input (self intersecting polygons, zero area ears... etc).
 */
void BLI_polyfill_calc(const float (*coords)[2],
                       unsigned int coords_num,
                       int coords_sign,
                       unsigned int (*r_tris)[3]);

/**
 * C++ version of #BLI_polyfill_calc above.
 *
 * \param r_tris: Must have a size of `coords.size() - 2`.
 */
inline void BLI_polyfill_calc(const Span<float2> coords,
                              const int coords_sign,
                              MutableSpan<uint3> r_tris)
{
  BLI_assert(r_tris.size() == coords.size() - 2);
  BLI_polyfill_calc(reinterpret_cast<const float (*)[2]>(coords.data()),
                    uint(coords.size()),
                    coords_sign,
                    reinterpret_cast<uint(*)[3]>(r_tris.data()));
}

/* default size of polyfill arena */
#define BLI_POLYFILL_ARENA_SIZE MEM_SIZE_OPTIMAL(1 << 14)

}  // namespace blender
