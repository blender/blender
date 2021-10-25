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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_BITMAP_DRAW_2D_H__
#define __BLI_BITMAP_DRAW_2D_H__

/** \file BLI_bitmap_draw_2d.h
 *  \ingroup bli
 */

void BLI_bitmap_draw_2d_line_v2v2i(
        const int p1[2], const int p2[2],
        bool (*callback)(int, int, void *), void *userData);

void BLI_bitmap_draw_2d_poly_v2i_n(
        const int xmin, const int ymin, const int xmax, const int ymax,
        const int polyXY[][2], const int polyCorners,
        void (*callback)(int x, int x_end, int y, void *), void *userData);

#endif  /* __BLI_BITMAP_DRAW_2D_H__ */
