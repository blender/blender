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

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap_draw_2d.h"

#include "BLI_math_base.h"
#include "BLI_sort.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/* Draw Line */

/**
 * Plot a line from \a p1 to \a p2 (inclusive).
 *
 * \note For clipped line drawing, see: http://stackoverflow.com/a/40902741/432509
 */
void BLI_bitmap_draw_2d_line_v2v2i(
        const int p1[2], const int p2[2],
        bool (*callback)(int, int, void *), void *userData)
{
	/* Bresenham's line algorithm. */
	int x1 = p1[0];
	int y1 = p1[1];
	int x2 = p2[0];
	int y2 = p2[1];

	if (callback(x1, y1, userData) == 0) {
		return;
	}

	/* if x1 == x2 or y1 == y2, then it does not matter what we set here */
	const int sign_x = (x2 > x1) ? 1 : -1;
	const int sign_y = (y2 > y1) ? 1 : -1;

	const int delta_x = (sign_x == 1) ? (x2 - x1) : (x1 - x2);
	const int delta_y = (sign_y == 1) ? (y2 - y1) : (y1 - y2);

	const int delta_x_step = delta_x * 2;
	const int delta_y_step = delta_y * 2;

	if (delta_x >= delta_y) {
		/* error may go below zero */
		int error = delta_y_step - delta_x;

		while (x1 != x2) {
			if (error >= 0) {
				if (error || (sign_x == 1)) {
					y1 += sign_y;
					error -= delta_x_step;
				}
				/* else do nothing */
			}
			/* else do nothing */

			x1 += sign_x;
			error += delta_y_step;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
	else {
		/* error may go below zero */
		int error = delta_x_step - delta_y;

		while (y1 != y2) {
			if (error >= 0) {
				if (error || (sign_y == 1)) {
					x1 += sign_x;
					error -= delta_y_step;
				}
				/* else do nothing */
			}
			/* else do nothing */

			y1 += sign_y;
			error += delta_x_step;

			if (callback(x1, y1, userData) == 0) {
				return;
			}
		}
	}
}


/* -------------------------------------------------------------------- */
/* Draw Filled Polygon */

/* sort edge-segments on y, then x axis */
static int draw_poly_v2i_n__span_y_sort(const void *a_p, const void *b_p, void *verts_p)
{
	const int (*verts)[2] = verts_p;
	const int *a = a_p;
	const int *b = b_p;
	const int *co_a = verts[a[0]];
	const int *co_b = verts[b[0]];

	if (co_a[1] < co_b[1]) {
		return -1;
	}
	else if (co_a[1] > co_b[1]) {
		return 1;
	}
	else if (co_a[0] < co_b[0]) {
		return -1;
	}
	else if (co_a[0] > co_b[0]) {
		return 1;
	}
	else {
		/* co_a & co_b are identical, use the line closest to the x-min */
		const int *co = co_a;
		co_a = verts[a[1]];
		co_b = verts[b[1]];
		int ord = (((co_b[0] - co[0]) * (co_a[1] - co[1])) -
		           ((co_a[0] - co[0]) * (co_b[1] - co[1])));
		if (ord > 0) {
			return -1;
		}
		if (ord < 0) {
			return 1;
		}
	}
	return 0;
}

/**
 * Draws a filled polyon with support for self intersections.
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
void BLI_bitmap_draw_2d_poly_v2i_n(
        const int xmin, const int ymin, const int xmax, const int ymax,
        const int verts[][2], const int nr,
        void (*callback)(int x, int x_end, int y, void *), void *userData)
{
	/* Originally by Darel Rex Finley, 2007.
	 * Optimized by Campbell Barton, 2016 to track sorted intersections. */

	int (*span_y)[2] = MEM_mallocN(sizeof(*span_y) * (size_t)nr, __func__);
	int span_y_len = 0;

	for (int i_curr = 0, i_prev = nr - 1; i_curr < nr; i_prev = i_curr++) {
		const int *co_prev = verts[i_prev];
		const int *co_curr = verts[i_curr];

		if (co_prev[1] != co_curr[1]) {
			/* Any segments entirely above or below the area of interest can be skipped. */
			if ((min_ii(co_prev[1], co_curr[1]) >= ymax) ||
			    (max_ii(co_prev[1], co_curr[1]) <  ymin))
			{
				continue;
			}

			int *s = span_y[span_y_len++];
			if (co_prev[1] < co_curr[1]) {
				s[0] = i_prev;
				s[1] = i_curr;
			}
			else {
				s[0] = i_curr;
				s[1] = i_prev;
			}
		}
	}

	BLI_qsort_r(span_y, (size_t)span_y_len, sizeof(*span_y), draw_poly_v2i_n__span_y_sort, (void *)verts);

	struct NodeX {
		int span_y_index;
		int x;
	} *node_x = MEM_mallocN(sizeof(*node_x) * (size_t)(nr + 1), __func__);
	int node_x_len = 0;

	int span_y_index = 0;
	if (span_y_len != 0 && verts[span_y[0][0]][1] < ymin) {
		while ((span_y_index < span_y_len) &&
		       (verts[span_y[span_y_index][0]][1] < ymin))
		{
			BLI_assert(verts[span_y[span_y_index][0]][1] <
			           verts[span_y[span_y_index][1]][1]);
			if (verts[span_y[span_y_index][1]][1] >= ymin) {
				struct NodeX *n = &node_x[node_x_len++];
				n->span_y_index = span_y_index;
			}
			span_y_index += 1;
		}
	}

	/* Loop through the rows of the image. */
	for (int pixel_y = ymin; pixel_y < ymax; pixel_y++) {
		bool is_sorted = true;
		bool do_remove = false;

		for (int i = 0, x_ix_prev = INT_MIN; i < node_x_len; i++) {
			struct NodeX *n = &node_x[i];
			const int *s = span_y[n->span_y_index];
			const int *co_prev = verts[s[0]];
			const int *co_curr = verts[s[1]];

			BLI_assert(co_prev[1] < pixel_y && co_curr[1] >= pixel_y);

			const double x    = (co_prev[0] - co_curr[0]);
			const double y    = (co_prev[1] - co_curr[1]);
			const double y_px = (pixel_y    - co_curr[1]);
			const int    x_ix = (int)((double)co_curr[0] + ((y_px / y) * x));
			n->x = x_ix;

			if (is_sorted && (x_ix_prev > x_ix)) {
				is_sorted = false;
			}
			if (do_remove == false && co_curr[1] == pixel_y) {
				do_remove = true;
			}
			x_ix_prev = x_ix;
		}

		/* Sort the nodes, via a simple "Bubble" sort. */
		if (is_sorted == false) {
			int i = 0;
			const int node_x_end = node_x_len - 1;
			while (i < node_x_end) {
				if (node_x[i].x > node_x[i + 1].x) {
					SWAP(struct NodeX, node_x[i], node_x[i + 1]);
					if (i != 0) {
						i -= 1;
					}
				}
				else {
					i += 1;
				}
			}
		}

		/* Fill the pixels between node pairs. */
		for (int i = 0; i < node_x_len; i += 2) {
			int x_src = node_x[i].x;
			int x_dst = node_x[i + 1].x;

			if (x_src >= xmax) {
				break;
			}

			if (x_dst > xmin) {
				if (x_src < xmin) {
					x_src = xmin;
				}
				if (x_dst > xmax) {
					x_dst = xmax;
				}
				/* for single call per x-span */
				if (x_src < x_dst) {
					callback(x_src - xmin, x_dst - xmin, pixel_y - ymin, userData);
				}
			}
		}

		/* Clear finalized nodes in one pass, only when needed
		 * (avoids excessive array-resizing). */
		if (do_remove == true) {
			int i_dst = 0;
			for (int i_src = 0; i_src < node_x_len; i_src += 1) {
				const int *s = span_y[node_x[i_src].span_y_index];
				const int *co = verts[s[1]];
				if (co[1] != pixel_y) {
					if (i_dst != i_src) {
						/* x is initialized for the next pixel_y (no need to adjust here) */
						node_x[i_dst].span_y_index = node_x[i_src].span_y_index;
					}
					i_dst += 1;
				}
			}
			node_x_len = i_dst;
		}

		/* Scan for new x-nodes */
		while ((span_y_index < span_y_len) &&
		       (verts[span_y[span_y_index][0]][1] == pixel_y))
		{
			/* note, node_x these are just added at the end,
			 * not ideal but sorting once will resolve. */

			/* x is initialized for the next pixel_y */
			struct NodeX *n = &node_x[node_x_len++];
			n->span_y_index = span_y_index;
			span_y_index += 1;
		}
	}

	MEM_freeN(span_y);
	MEM_freeN(node_x);
}
