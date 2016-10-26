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
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

/** \file blender/blenlib/intern/bitmap_draw_2d.c
 *  \ingroup bli
 *
 * Utility functions for primitive drawing operations.
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"

#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

/**
 * Plot a line from \a p1 to \a p2 (inclusive).
 */
void plot_line_v2v2i(
        const int p1[2], const int p2[2],
        bool (*callback)(int, int, void *), void *userData)
{
	/* Bresenham's line algorithm. */
	int x1 = p1[0];
	int y1 = p1[1];
	int x2 = p2[0];
	int y2 = p2[1];

	int ix;
	int iy;

	/* if x1 == x2 or y1 == y2, then it does not matter what we set here */
	int delta_x = (x2 > x1 ? ((void)(ix = 1), x2 - x1) : ((void)(ix = -1), x1 - x2)) << 1;
	int delta_y = (y2 > y1 ? ((void)(iy = 1), y2 - y1) : ((void)(iy = -1), y1 - y2)) << 1;

	if (callback(x1, y1, userData) == 0) {
		return;
	}

	if (delta_x >= delta_y) {
		/* error may go below zero */
		int error = delta_y - (delta_x >> 1);

		while (x1 != x2) {
			if (error >= 0) {
				if (error || (ix > 0)) {
					y1 += iy;
					error -= delta_x;
				}
				/* else do nothing */
			}
			/* else do nothing */

			x1 += ix;
			error += delta_y;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
	else {
		/* error may go below zero */
		int error = delta_x - (delta_y >> 1);

		while (y1 != y2) {
			if (error >= 0) {
				if (error || (iy > 0)) {
					x1 += ix;
					error -= delta_y;
				}
				/* else do nothing */
			}
			/* else do nothing */

			y1 += iy;
			error += delta_x;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
}

/**
 * \param callback: Takes the x, y coords and x-span (\a x_end is not inclusive),
 * note that \a x_end will always be greater than \a x, so we can use:
 *
 * \code{.c}
 * do {
 *     func(x, y);
 * } while (++x != x_end);
 * \endcode
 */
void fill_poly_v2i_n(
        const int xmin, const int ymin, const int xmax, const int ymax,
        const int verts[][2], const int nr,
        void (*callback)(int x, int x_end, int y, void *), void *userData)
{
	/* Originally by Darel Rex Finley, 2007.
	 */

	int  nodes, pixel_y, i, j, swap;
	int *node_x = MEM_mallocN(sizeof(*node_x) * (size_t)(nr + 1), __func__);

	/* Loop through the rows of the image. */
	for (pixel_y = ymin; pixel_y < ymax; pixel_y++) {

		/* Build a list of nodes. */
		nodes = 0; j = nr - 1;
		for (i = 0; i < nr; i++) {
			if ((verts[i][1] < pixel_y && verts[j][1] >= pixel_y) ||
			    (verts[j][1] < pixel_y && verts[i][1] >= pixel_y))
			{
				node_x[nodes++] = (int)(verts[i][0] +
				                        ((double)(pixel_y - verts[i][1]) / (verts[j][1] - verts[i][1])) *
				                        (verts[j][0] - verts[i][0]));
			}
			j = i;
		}

		/* Sort the nodes, via a simple "Bubble" sort. */
		i = 0;
		while (i < nodes - 1) {
			if (node_x[i] > node_x[i + 1]) {
				SWAP_TVAL(swap, node_x[i], node_x[i + 1]);
				if (i) i--;
			}
			else {
				i++;
			}
		}

		/* Fill the pixels between node pairs. */
		for (i = 0; i < nodes; i += 2) {
			if (node_x[i] >= xmax) break;
			if (node_x[i + 1] >  xmin) {
				if (node_x[i    ] < xmin) node_x[i    ] = xmin;
				if (node_x[i + 1] > xmax) node_x[i + 1] = xmax;

#if 0
				/* for many x/y calls */
				for (j = node_x[i]; j < node_x[i + 1]; j++) {
					callback(j - xmin, pixel_y - ymin, userData);
				}
#else
				/* for single call per x-span */
				if (node_x[i] < node_x[i + 1]) {
					callback(node_x[i] - xmin, node_x[i + 1] - xmin, pixel_y - ymin, userData);
				}
#endif
			}
		}
	}
	MEM_freeN(node_x);
}
