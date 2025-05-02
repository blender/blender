/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_index_mask.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"

#include "BKE_curves.hh"

#include "ED_curves.hh"
#include "ED_object.hh"

#include "DNA_layer_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "GEO_curves_remove_and_split.hh"

#include "WM_api.hh"

namespace blender::ed::curves {

static wmOperatorStatus separate_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
      scene, view_layer, CTX_wm_view3d(C));

  VectorSet<Curves *> src_curves;
  for (Base *base_src : bases) {
    src_curves.add(static_cast<Curves *>(base_src->object->data));
  }

  /* Modify new curves and generate new curves in parallel. */
  Array<std::optional<bke::CurvesGeometry>> dst_geometry(src_curves.size());
  threading::parallel_for(dst_geometry.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      Curves &src = *src_curves[i];
      IndexMaskMemory memory;
      switch (bke::AttrDomain(src.selection_domain)) {
        case bke::AttrDomain::Point: {
          const IndexMask selection = retrieve_selected_points(src, memory);
          if (selection.is_empty()) {
            continue;
          }
          bke::CurvesGeometry separated;
          bke::CurvesGeometry retained;
          separate_points(src.geometry.wrap(), selection, separated, retained);

          separated.calculate_bezier_auto_handles();
          retained.calculate_bezier_auto_handles();

          dst_geometry[i] = std::move(separated);
          src.geometry.wrap() = std::move(retained);
          break;
        }
        case bke::AttrDomain::Curve: {
          const IndexMask selection = retrieve_selected_curves(src, memory);
          if (selection.is_empty()) {
            continue;
          }
          dst_geometry[i] = bke::curves_copy_curve_selection(src.geometry.wrap(), selection, {});
          src.geometry.wrap().remove_curves(selection, {});
          break;
        }
        default:
          BLI_assert_unreachable();
          break;
      }
    }
  });

  /* Move new curves into main data-base. */
  Array<Curves *> dst_curves(src_curves.size(), nullptr);
  for (const int i : dst_curves.index_range()) {
    if (std::optional<bke::CurvesGeometry> &dst = dst_geometry[i]) {
      dst_curves[i] = BKE_curves_add(bmain, BKE_id_name(src_curves[i]->id));
      dst_curves[i]->geometry.wrap() = std::move(*dst);
      bke::curves_copy_parameters(*src_curves[i], *dst_curves[i]);
    }
  }

  /* Skip processing objects with no selected elements. */
  bases.remove_if([&](Base *base) {
    Curves *curves = static_cast<Curves *>(base->object->data);
    return dst_curves[src_curves.index_of(curves)] == nullptr;
  });

  if (bases.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Add new objects for the new curves. */
  for (Base *base_src : bases) {
    Curves *src = static_cast<Curves *>(base_src->object->data);
    Curves *dst = dst_curves[src_curves.index_of(src)];

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

void CURVES_OT_separate(wmOperatorType *ot)
{
  ot->name = "Separate";
  ot->idname = "CURVES_OT_separate";
  ot->description = "Separate selected geometry into a new object";

  ot->exec = separate_exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::curves
