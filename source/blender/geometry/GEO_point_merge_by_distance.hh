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

#include "BLI_index_mask.hh"

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
PointCloud *point_merge_by_distance(const PointCloudComponent &src_points,
                                    const float merge_distance,
                                    const IndexMask selection);

}  // namespace blender::geometry
