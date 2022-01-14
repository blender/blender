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

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct MemArena;

/**
 * A version of #BLI_polyfill_calc that uses a memory arena to avoid re-allocations.
 */
void BLI_polyfill_calc_arena(const float (*coords)[2],
                             unsigned int coords_tot,
                             int coords_sign,
                             unsigned int (*r_tris)[3],

                             struct MemArena *arena);

/**
 * Triangulates the given (convex or concave) simple polygon to a list of triangle vertices.
 *
 * \param coords: 2D coordinates describing vertices of the polygon,
 * in either clockwise or counterclockwise order.
 * \param coords_tot: Total points in the array.
 * \param coords_sign: Pass this when we know the sign in advance to avoid extra calculations.
 *
 * \param r_tris: This array is filled in with triangle indices in clockwise order.
 * The length of the array must be `coords_tot - 2`.
 * Indices are guaranteed to be assigned to unique triangles, with valid indices,
 * even in the case of degenerate input (self intersecting polygons, zero area ears... etc).
 */
void BLI_polyfill_calc(const float (*coords)[2],
                       unsigned int coords_tot,
                       int coords_sign,
                       unsigned int (*r_tris)[3]);

/* default size of polyfill arena */
#define BLI_POLYFILL_ARENA_SIZE MEM_SIZE_OPTIMAL(1 << 14)

#ifdef __cplusplus
}
#endif
