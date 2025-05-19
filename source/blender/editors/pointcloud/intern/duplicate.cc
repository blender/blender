/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_pointcloud.hh"

#include "ED_pointcloud.hh"

#include "DNA_pointcloud_types.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

namespace blender::ed::pointcloud {

static void duplicate_points(PointCloud &pointcloud, const IndexMask &mask)
{
  PointCloud *new_pointcloud = BKE_pointcloud_new_nomain(pointcloud.totpoint + mask.size());
  bke::MutableAttributeAccessor dst_attributes = new_pointcloud->attributes_for_write();
  pointcloud.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
    const GVArray src = *iter.get();
    bke::GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    array_utils::copy(src, dst.span.take_front(pointcloud.totpoint));
    array_utils::gather(src, mask, dst.span.take_back(mask.size()));
    dst.finish();
  });
  BKE_pointcloud_nomain_to_pointcloud(new_pointcloud, &pointcloud);
}

static wmOperatorStatus duplicate_exec(bContext *C, wmOperator * /*op*/)
{
  for (PointCloud *pointcloud : get_unique_editable_pointclouds(*C)) {
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_points(*pointcloud, memory);
    if (selection.is_empty()) {
      continue;
    }

    pointcloud->attributes_for_write().remove(".selection");

    duplicate_points(*pointcloud, selection);

    bke::SpanAttributeWriter selection_attr =
        pointcloud->attributes_for_write().lookup_or_add_for_write_span<bool>(
            ".selection", bke::AttrDomain::Point);
    selection_attr.span.take_back(selection.size()).fill(true);
    selection_attr.finish();

    DEG_id_tag_update(&pointcloud->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, pointcloud);
  }
  return OPERATOR_FINISHED;
}

void POINTCLOUD_OT_duplicate(wmOperatorType *ot)
{
  ot->name = "Duplicate";
  ot->idname = "POINTCLOUD_OT_duplicate";
  ot->description = "Copy selected points";

  ot->exec = duplicate_exec;
  ot->poll = editable_pointcloud_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::pointcloud
