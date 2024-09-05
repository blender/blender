/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_index_mask.hh"

#include "BKE_attribute_filter.hh"

struct PointCloud;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

/**
 * Merge selected points into other selected points within the \a merge_distance. The merged
 * indices favor speed over accuracy, since the results will depend on the order of the points.
 */
PointCloud *point_merge_by_distance(const PointCloud &src_points,
                                    const float merge_distance,
                                    const IndexMask &selection,
                                    const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
