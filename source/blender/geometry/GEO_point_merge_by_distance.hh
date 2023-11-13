/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask.hh"

#include "BKE_attribute.hh"

#pragma once

struct PointCloud;
class PointCloudComponent;

/** \file
 * \ingroup geo
 */

namespace blender::geometry {

/**
 * Merge selected points into other selected points within the \a merge_distance. The merged
 * indices favor speed over accuracy, since the results will depend on the order of the points.
 */
PointCloud *point_merge_by_distance(
    const PointCloud &src_points,
    const float merge_distance,
    const IndexMask &selection,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
