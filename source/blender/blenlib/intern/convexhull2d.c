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

/** \file blender/blenlib/intern/convexhull2d.c
 *  \ingroup bli
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_convexhull2d.h"
#include "BLI_math.h"
#include "BLI_strict_flags.h"
#include "BLI_utildefines.h"

/* Copyright 2001, softSurfer (www.softsurfer.com)
 * This code may be freely used and modified for any purpose
 * providing that this copyright notice is included with it.
 * SoftSurfer makes no warranty for this code, and cannot be held
 * liable for any real or imagined damage resulting from its use.
 * Users of this code must verify correctness for their application.
 * http://softsurfer.com/Archive/algorithm_0203/algorithm_0203.htm
 */

/** \name Main Convex-Hull Calculation
 * \{ */

/**
 * tests if a point is Left|On|Right of an infinite line.
 *    Input:  three points P0, P1, and P2
 * \returns > 0.0 for P2 left of the line through P0 and P1.
 *          = 0.0 for P2 on the line.
 *          < 0.0 for P2 right of the line.
 */
static float is_left(const float p0[2], const float p1[2], const float p2[2])
{
	return (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p2[0] - p0[0]) * (p1[1] - p0[1]);
}

/**
 * A.M. Andrew's monotone chain 2D convex hull algorithm
 *
 * \param  points  An array of 2D points presorted by increasing x and y-coords.
 * \param  n  The number of points in points.
 * \param  r_points  An array of the convex hull vertex indices (max is n).
 * \returns the number of points in r_points.
 */
int BLI_convexhull_2d_sorted(const float (*points)[2], const int n, int r_points[])
{
	/* the output array r_points[] will be used as the stack */
	int bot = 0;
	int top = -1;  /* indices for bottom and top of the stack */
	int i;  /* array scan index */
	int minmin, minmax;
	int maxmin, maxmax;
	float xmax;

	/* Get the indices of points with min x-coord and min|max y-coord */
	float xmin = points[0][0];
	for (i = 1; i < n; i++) {
		if (points[i][0] != xmin) {
			break;
		}
	}

	minmin = 0;
	minmax = i - 1;
	if (minmax == n - 1) {  /* degenerate case: all x-coords == xmin */
		r_points[++top] = minmin;
		if (points[minmax][1] != points[minmin][1])  /* a nontrivial segment */
			r_points[++top] = minmax;
		r_points[++top] = minmin;  /* add polygon endpoint */
		return top + 1;
	}

	/* Get the indices of points with max x-coord and min|max y-coord */

	maxmax = n - 1;
	xmax = points[n - 1][0];
	for (i = n - 2; i >= 0; i--) {
		if (points[i][0] != xmax) {
			break;
		}
	}
	maxmin = i + 1;

	/* Compute the lower hull on the stack r_points */
	r_points[++top] = minmin;  /* push minmin point onto stack */
	i = minmax;
	while (++i <= maxmin) {
		/* the lower line joins points[minmin] with points[maxmin] */
		if (is_left(points[minmin], points[maxmin], points[i]) >= 0 && i < maxmin) {
			continue;  /* ignore points[i] above or on the lower line */
		}

		while (top > 0) {  /* there are at least 2 points on the stack */
			/* test if points[i] is left of the line at the stack top */
			if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
				break;  /* points[i] is a new hull vertex */
			}
			else {
				top--;  /* pop top point off stack */
			}
		}

		r_points[++top] = i;  /* push points[i] onto stack */
	}

	/* Next, compute the upper hull on the stack r_points above the bottom hull */
	if (maxmax != maxmin) {  /* if distinct xmax points */
		r_points[++top] = maxmax;  /* push maxmax point onto stack */
	}

	bot = top;  /* the bottom point of the upper hull stack */
	i = maxmin;
	while (--i >= minmax) {
		/* the upper line joins points[maxmax] with points[minmax] */
		if (is_left(points[maxmax], points[minmax], points[i]) >= 0 && i > minmax) {
			continue;  /* ignore points[i] below or on the upper line */
		}

		while (top > bot) {  /* at least 2 points on the upper stack */
			/* test if points[i] is left of the line at the stack top */
			if (is_left(points[r_points[top - 1]], points[r_points[top]], points[i]) > 0.0f) {
				break;  /* points[i] is a new hull vertex */
			}
			else {
				top--;  /* pop top point off stack */
			}
		}

		if (points[i][0] == points[r_points[0]][0] && points[i][1] == points[r_points[0]][1]) {
			return top + 1;  /* special case (mgomes) */
		}

		r_points[++top] = i;  /* push points[i] onto stack */
	}

	if (minmax != minmin) {
		r_points[++top] = minmin;  /* push joining endpoint onto stack */
	}

	return top + 1;
}

struct PointRef {
	const float *pt;  /* 2d vector */
};

static int pointref_cmp_yx(const void *a_, const void *b_)
{
	const struct PointRef *a = a_;
	const struct PointRef *b = b_;

	if      (a->pt[1] > b->pt[1]) return  1;
	else if (a->pt[1] < b->pt[1]) return -1;

	if      (a->pt[0] > b->pt[0]) return  1;
	else if (a->pt[0] < b->pt[0]) return -1;

	else                          return  0;
}

/**
 * A.M. Andrew's monotone chain 2D convex hull algorithm
 *
 * \param  points  An array of 2D points.
 * \param  n  The number of points in points.
 * \param  r_points  An array of the convex hull vertex indices (max is n).
 * _must_ be allocated as ``n * 2`` because of how its used internally,
 * even though the final result will be no more than \a n in size.
 * \returns the number of points in r_points.
 */
int BLI_convexhull_2d(const float (*points)[2], const int n, int r_points[])
{
	struct PointRef *points_ref = MEM_mallocN(sizeof(*points_ref) * (size_t)n, __func__);
	float (*points_sort)[2] = MEM_mallocN(sizeof(*points_sort) * (size_t)n, __func__);
	int *points_map;
	int tot, i;

	for (i = 0; i < n; i++) {
		points_ref[i].pt = points[i];
	}

	/* Sort the points by X, then by Y (required by the algorithm) */
	qsort(points_ref, (size_t)n, sizeof(struct PointRef), pointref_cmp_yx);

	for (i = 0; i < n; i++) {
		memcpy(points_sort[i], points_ref[i].pt, sizeof(float[2]));
	}

	tot = BLI_convexhull_2d_sorted((const float (*)[2])points_sort, n, r_points);

	/* map back to the original index values */
	points_map = (int *)points_sort;  /* abuse float array for temp storage */
	for (i = 0; i < tot; i++) {
		points_map[i] = (int)((const float(*)[2])points_ref[r_points[i]].pt - points);
	}

	memcpy(r_points, points_map, (size_t)tot * sizeof(*points_map));

	MEM_freeN(points_ref);
	MEM_freeN(points_sort);

	return tot;
}

/** \} */


/* -------------------------------------------------------------------- */
/* Helper functions */

/** \name Utility Convex-Hull Functions
 * \{ */

/**
 * \return The best angle for fitting the convex hull to an axis aligned bounding box.
 *
 * Intended to be used with #BLI_convexhull_2d
 *
 * \param points  Orded hull points
 * (result of #BLI_convexhull_2d mapped to a contiguous array).
 *
 * \note we could return the index of the best edge too if its needed.
 */
float BLI_convexhull_aabb_fit_hull_2d(const float (*points_hull)[2], unsigned int n)
{
	unsigned int i, i_prev;
	float area_best = FLT_MAX;
	float dvec_best[2];  /* best angle, delay atan2 */

	i_prev = n - 1;
	for (i = 0; i < n; i++) {
		const float *ev_a = points_hull[i];
		const float *ev_b = points_hull[i_prev];
		float dvec[2];  /* 2d rotation matrix */

		sub_v2_v2v2(dvec, ev_a, ev_b);
		if (normalize_v2(dvec) != 0.0f) {
			/* rotation matrix */
			float min[2] = {FLT_MAX, FLT_MAX}, max[2] = {-FLT_MAX, -FLT_MAX};
			unsigned int j;
			float area;

			for (j = 0; j < n; j++) {
				float tvec[2];
				mul_v2_v2_cw(tvec, dvec, points_hull[j]);

				min[0] = min_ff(min[0], tvec[0]);
				min[1] = min_ff(min[1], tvec[1]);

				max[0] = max_ff(max[0], tvec[0]);
				max[1] = max_ff(max[1], tvec[1]);

				area = (max[0] - min[0]) * (max[1] - min[1]);
				if (area > area_best) {
					break;
				}
			}

			if (area < area_best) {
				area_best = area;
				copy_v2_v2(dvec_best, dvec);
			}
		}

		i_prev = i;
	}

	return (area_best != FLT_MAX) ? atan2f(dvec_best[0], dvec_best[1]) : 0.0f;
}

/**
 * Wrap #BLI_convexhull_aabb_fit_hull_2d and do the convex hull calculation.
 *
 * \param points  arbitrary 2d points.
 */
float BLI_convexhull_aabb_fit_points_2d(const float (*points)[2], unsigned int n)
{
	int *index_map;
	int tot;

	float angle;

	index_map = MEM_mallocN(sizeof(*index_map) * n * 2, __func__);

	tot = BLI_convexhull_2d((const float (*)[2])points, (int)n, index_map);

	if (tot) {
		float (*points_hull)[2];
		int j;

		points_hull = MEM_mallocN(sizeof(*points_hull) * (size_t)tot, __func__);
		for (j = 0; j < tot; j++) {
			copy_v2_v2(points_hull[j], points[index_map[j]]);
		}

		angle = BLI_convexhull_aabb_fit_hull_2d((const float (*)[2])points_hull, (unsigned int)tot);
		MEM_freeN(points_hull);
	}
	else {
		angle = 0.0f;
	}

	MEM_freeN(index_map);

	return angle;
}

/** \} */
