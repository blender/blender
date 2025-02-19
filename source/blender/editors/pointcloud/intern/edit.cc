/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edpointcloud
 */

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"

#include "ED_pointcloud.hh"

namespace blender::ed::pointcloud {

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

bool remove_selection(PointCloud &pointcloud)
{
  const bke::AttributeAccessor attributes = pointcloud.attributes();
  const VArray<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", bke::AttrDomain::Point, true);
  IndexMaskMemory memory;
  const IndexMask mask = IndexMask::from_bools_inverse(selection, memory);
  if (mask.size() == pointcloud.totpoint) {
    return false;
  }

  PointCloud *pointcloud_new = copy_selection(pointcloud, mask);
  BKE_pointcloud_nomain_to_pointcloud(pointcloud_new, &pointcloud);
  return true;
}

}  // namespace blender::ed::pointcloud
