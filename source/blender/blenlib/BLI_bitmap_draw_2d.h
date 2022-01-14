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

/**
 * Plot a line from \a p1 to \a p2 (inclusive).
 *
 * \note For clipped line drawing, see: http://stackoverflow.com/a/40902741/432509
 */
void BLI_bitmap_draw_2d_line_v2v2i(const int p1[2],
                                   const int p2[2],
                                   bool (*callback)(int, int, void *),
                                   void *user_data);

/**
 * \note Unclipped (clipped version can be added if needed).
 */
void BLI_bitmap_draw_2d_tri_v2i(const int p1[2],
                                const int p2[2],
                                const int p3[2],
                                void (*callback)(int x, int x_end, int y, void *),
                                void *user_data);

/**
 * Draws a filled polygon with support for self intersections.
 *
 * \param callback: Takes the x, y coords and x-span (\a x_end is not inclusive),
 * note that \a x_end will always be greater than \a x, so we can use:
 *
 * \code{.c}
 * do {
 *     func(x, y);
 * } while (++x != x_end);
 * \endcode
 */
void BLI_bitmap_draw_2d_poly_v2i_n(int xmin,
                                   int ymin,
                                   int xmax,
                                   int ymax,
                                   const int verts[][2],
                                   int verts_len,
                                   void (*callback)(int x, int x_end, int y, void *),
                                   void *user_data);

#ifdef __cplusplus
}
#endif
