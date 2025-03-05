/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

namespace blender::geometry {

/**
 * Remove the points in the \a point_mask and split each curve at the points that are removed (if
 * necessary).
 */
bke::CurvesGeometry remove_points_and_split(const bke::CurvesGeometry &curves,
                                            const IndexMask &mask);

}  // namespace blender::geometry
