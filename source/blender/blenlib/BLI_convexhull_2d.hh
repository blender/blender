/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

namespace blender {

/** \file
 * \ingroup bli
 */

/**
 * Extract 2D convex hull.
 *
 * \param points: An array of 2D points.
 * \param points_num: The number of points in points.
 * \param r_points: An array of the convex hull vertex indices (max is `points_num`).
 * - Points are ordered counter clockwise.
 * - The first point in `r_points` will be the lowest Y value
 *   (lowest (X, Y) when there are multiple Y aligned vertices).
 * - The polygons cross product is always positive (or zero).
 *
 * \return The number of indices in `r_points`.
 *
 * \note Performance is `O(points_num.log(points_num))`, same as `qsort`.
 */
int BLI_convexhull_2d(Span<float2> points, int r_points[/*points_num*/]);

/**
 * \return The best angle for fitting the points to an axis aligned bounding box.
 *
 * \note We could return the index of the best edge too if its needed.
 *
 * \param points: Arbitrary 2D points.
 */
float BLI_convexhull_aabb_fit_points_2d(Span<float2> points);

}  // namespace blender
