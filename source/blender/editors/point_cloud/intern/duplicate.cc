/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"

#include "ED_point_cloud.hh"

#include "DNA_pointcloud_types.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

namespace blender::ed::point_cloud {

static void duplicate_points(PointCloud &point_cloud, const IndexMask &mask)
{
  PointCloud *new_point_cloud = BKE_pointcloud_new_nomain(point_cloud.totpoint + mask.size());
  bke::MutableAttributeAccessor dst_attributes = new_point_cloud->attributes_for_write();
  point_cloud.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
    const GVArray src = *iter.get();
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    array_utils::copy(src, dst.span.take_front(point_cloud.totpoint));
    array_utils::gather(src, mask, dst.span.take_back(mask.size()));
    dst.finish();
  });
  BKE_pointcloud_nomain_to_pointcloud(new_point_cloud, &point_cloud);
}

static int duplicate_exec(bContext *C, wmOperator * /*op*/)
{
  for (PointCloud *point_cloud : get_unique_editable_point_clouds(*C)) {
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_points(*point_cloud, memory);
    if (selection.is_empty()) {
      continue;
    }

    point_cloud->attributes_for_write().remove(".selection");

    duplicate_points(*point_cloud, selection);

    bke::SpanAttributeWriter selection_attr =
        point_cloud->attributes_for_write().lookup_or_add_for_write_span<bool>(
            ".selection", bke::AttrDomain::Point);
    selection_attr.span.take_back(selection.size()).fill(true);
    selection_attr.finish();

    DEG_id_tag_update(&point_cloud->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, point_cloud);
  }
  return OPERATOR_FINISHED;
}

void POINT_CLOUD_OT_duplicate(wmOperatorType *ot)
{
  ot->name = "Duplicate";
  ot->idname = "POINT_CLOUD_OT_duplicate";
  ot->description = "Copy selected points ";

  ot->exec = duplicate_exec;
  ot->poll = editable_point_cloud_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::point_cloud
