/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extract 2D convex hull.
 *
 * \param points: An array of 2D points.
 * \param points_num: The number of points in points.
 * \param r_points: An array of the convex hull vertex indices (max is `points_num`).
 * Vertices are ordered counter clockwise, the polygons cross product is always negative (or zero).
 *
 * \return The number of indices in r_points.
 *
 * \note Performance is `O(points_num.log(points_num))`, same as `qsort`.
 */
int BLI_convexhull_2d(const float (*points)[2], int points_num, int r_points[/*points_num*/]);

/**
 * \return The best angle for fitting the points to an axis aligned bounding box.
 *
 * \note We could return the index of the best edge too if its needed.
 *
 * \param points: Arbitrary 2D points.
 */
float BLI_convexhull_aabb_fit_points_2d(const float (*points)[2], int points_num);

#ifdef __cplusplus
}
#endif
