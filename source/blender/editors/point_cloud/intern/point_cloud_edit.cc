/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edpointcloud
 */

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"

#include "ED_point_cloud.hh"

namespace blender::ed::point_cloud {

PointCloud *copy_selection(const PointCloud &src, const IndexMask &mask)
{
  if (mask.size() == src.totpoint) {
    return BKE_pointcloud_copy_for_eval(&src);
  }
  PointCloud *dst = BKE_pointcloud_new_nomain(mask.size());
  bke::gather_attributes(src.attributes(),
                         bke::AttrDomain::Point,
                         bke::AttrDomain::Point,
                         {},
                         mask,
                         dst->attributes_for_write());
  pointcloud_copy_parameters(src, *dst);
  return dst;
}

bool remove_selection(PointCloud &point_cloud)
{
  const bke::AttributeAccessor attributes = point_cloud.attributes();
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools_inverse(selection, memory);
  if (mask.size() == point_cloud.totpoint) {
    return false;
  }

  PointCloud *point_cloud_new = copy_selection(point_cloud, mask);
  BKE_pointcloud_nomain_to_pointcloud(point_cloud_new, &point_cloud);
  return true;
}

}  // namespace blender::ed::point_cloud
