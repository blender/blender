/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "BKE_pointcloud.hh"

#include "DEG_depsgraph_build.hh"
#include "ED_object.hh"
#include "ED_pointcloud.hh"

#include "DNA_layer_types.h"
#include "DNA_pointcloud_types.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

namespace blender::ed::pointcloud {

static wmOperatorStatus separate_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
      scene, view_layer, CTX_wm_view3d(C));

  VectorSet<PointCloud *> src_pointclouds;
  for (Base *base_src : bases) {
    src_pointclouds.add(static_cast<PointCloud *>(base_src->object->data));
  }

  /* Modify new point clouds and generate new point clouds in parallel. */
  Array<PointCloud *> dst_pointclouds(src_pointclouds.size());
  threading::parallel_for(dst_pointclouds.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      IndexMaskMemory memory;
      const IndexMask selection = retrieve_selected_points(*src_pointclouds[i], memory);
      if (selection.is_empty()) {
        dst_pointclouds[i] = nullptr;
        continue;
      }
      dst_pointclouds[i] = copy_selection(*src_pointclouds[i], selection);
      const IndexMask inverse = selection.complement(IndexRange(src_pointclouds[i]->totpoint),
                                                     memory);
      BKE_pointcloud_nomain_to_pointcloud(copy_selection(*src_pointclouds[i], inverse),
                                          src_pointclouds[i]);
    }
  });

  /* Move new point clouds into main data-base. */
  for (const int i : dst_pointclouds.index_range()) {
    if (PointCloud *dst = dst_pointclouds[i]) {
      dst_pointclouds[i] = BKE_pointcloud_add(bmain, BKE_id_name(src_pointclouds[i]->id));
      pointcloud_copy_parameters(*src_pointclouds[i], *dst_pointclouds[i]);
      BKE_pointcloud_nomain_to_pointcloud(dst, dst_pointclouds[i]);
    }
  }

  /* Skip processing objects with no selected elements. */
  bases.remove_if([&](Base *base) {
    PointCloud *pointcloud = static_cast<PointCloud *>(base->object->data);
    return dst_pointclouds[src_pointclouds.index_of(pointcloud)] == nullptr;
  });

  if (bases.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Add new objects for the new point clouds. */
  for (Base *base_src : bases) {
    PointCloud *src = static_cast<PointCloud *>(base_src->object->data);
    PointCloud *dst = dst_pointclouds[src_pointclouds.index_of(src)];

    Base *base_dst = object::add_duplicate(
        bmain, scene, view_layer, base_src, eDupli_ID_Flags(U.dupflag) & USER_DUP_ACT);
    Object *object_dst = base_dst->object;
    object_dst->mode = OB_MODE_OBJECT;
    object_dst->data = dst;

    DEG_id_tag_update(&src->id, ID_RECALC_GEOMETRY);
    DEG_id_tag_update(&dst->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, base_src->object);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, object_dst);
  }

  DEG_relations_tag_update(bmain);
  return OPERATOR_FINISHED;
}

void POINTCLOUD_OT_separate(wmOperatorType *ot)
{
  ot->name = "Separate";
  ot->idname = "POINTCLOUD_OT_separate";
  ot->description = "Separate selected geometry into a new point cloud";

  ot->exec = separate_exec;
  ot->poll = editable_pointcloud_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::pointcloud
