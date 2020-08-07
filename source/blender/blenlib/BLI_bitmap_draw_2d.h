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

void BLI_bitmap_draw_2d_line_v2v2i(const int p1[2],
                                   const int p2[2],
                                   bool (*callback)(int, int, void *),
                                   void *user_data);

void BLI_bitmap_draw_2d_tri_v2i(const int p1[2],
                                const int p2[2],
                                const int p3[2],
                                void (*callback)(int x, int x_end, int y, void *),
                                void *user_data);

void BLI_bitmap_draw_2d_poly_v2i_n(const int xmin,
                                   const int ymin,
                                   const int xmax,
                                   const int ymax,
                                   const int verts[][2],
                                   const int verts_len,
                                   void (*callback)(int x, int x_end, int y, void *),
                                   void *user_data);

#ifdef __cplusplus
}
#endif
