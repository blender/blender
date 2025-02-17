/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edpointcloud
 */

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"

#include "DNA_node_types.h"

#include "ED_point_cloud.hh"

namespace blender::ed::point_cloud {

bool remove_selection(PointCloud &point_cloud)
{
  const bke::AttributeAccessor attributes = point_cloud.attributes();
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  const int domain_size_orig = point_cloud.totpoint;
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools_inverse(selection, memory);

  PointCloud *point_cloud_new = BKE_pointcloud_new_nomain(mask.size());
  bke::gather_attributes(attributes,
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         {},
                         mask,
                         point_cloud_new->attributes_for_write());
  pointcloud_copy_parameters(point_cloud, *point_cloud_new);
  BKE_pointcloud_nomain_to_pointcloud(point_cloud_new, &point_cloud);
  return point_cloud.totpoint != domain_size_orig;
}

}  // namespace blender::ed::point_cloud
