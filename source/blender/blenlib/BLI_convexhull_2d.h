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
 * A.M. Andrew's monotone chain 2D convex hull algorithm.
 *
 * \param points: An array of 2D points presorted by increasing x and y-coords.
 * \param n: The number of points in points.
 * \param r_points: An array of the convex hull vertex indices (max is n).
 * \returns the number of points in r_points.
 */
int BLI_convexhull_2d_sorted(const float (*points)[2], int n, int r_points[]);
/**
 * A.M. Andrew's monotone chain 2D convex hull algorithm.
 *
 * \param points: An array of 2D points.
 * \param n: The number of points in points.
 * \param r_points: An array of the convex hull vertex indices (max is n).
 * _must_ be allocated as `n * 2` because of how its used internally,
 * even though the final result will be no more than \a n in size.
 * \returns the number of points in r_points.
 */
int BLI_convexhull_2d(const float (*points)[2], int n, int r_points[]);

/**
 * \return The best angle for fitting the convex hull to an axis aligned bounding box.
 *
 * Intended to be used with #BLI_convexhull_2d
 *
 * \param points_hull: Ordered hull points
 * (result of #BLI_convexhull_2d mapped to a contiguous array).
 *
 * \note we could return the index of the best edge too if its needed.
 */
float BLI_convexhull_aabb_fit_hull_2d(const float (*points_hull)[2], unsigned int n);
/**
 * Wrap #BLI_convexhull_aabb_fit_hull_2d and do the convex hull calculation.
 *
 * \param points: arbitrary 2d points.
 */
float BLI_convexhull_aabb_fit_points_2d(const float (*points)[2], unsigned int n);

#ifdef __cplusplus
}
#endif
