/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_layer.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curves.hh"
#include "ED_view3d.hh"

namespace blender::ed::curves {

struct ClosestCurveDataBlock {
  Curves *curves_id = nullptr;
  FindClosestData elem = {};
};
static ClosestCurveDataBlock find_closest_curve(const Depsgraph &depsgraph,
                                                const ViewContext &vc,
                                                const Span<Base *> bases,
                                                const int2 &mval)
{
  return threading::parallel_reduce(
      bases.index_range(),
      1L,
      ClosestCurveDataBlock(),
      [&](const IndexRange range, const ClosestCurveDataBlock &init) {
        ClosestCurveDataBlock new_closest = init;
        for (Base *base : bases.slice(range)) {
          Object &curves_ob = *base->object;
          Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
          bke::crazyspace::GeometryDeformation deformation =
              bke::crazyspace::get_evaluated_curves_deformation(depsgraph, curves_ob);
          const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
          const float4x4 projection = ED_view3d_ob_project_mat_get(vc.rv3d, &curves_ob);
          foreach_selectable_curve_range(
              curves,
              deformation,
              eHandleDisplay(vc.v3d->overlay.handle_display),
              [&](IndexRange range, Span<float3> positions, StringRef /*selection_name*/) {
                std::optional<FindClosestData> new_closest_elem = closest_elem_find_screen_space(
                    vc,
                    curves.points_by_curve(),
                    positions,
                    curves.cyclic(),
                    projection,
                    range,
                    bke::AttrDomain::Curve,
                    mval,
                    new_closest.elem);
                if (new_closest_elem) {
                  new_closest.elem = *new_closest_elem;
                  new_closest.curves_id = &curves_id;
                }
              });
        }
        return new_closest;
      },
      [](const ClosestCurveDataBlock &a, const ClosestCurveDataBlock &b) {
        return (a.elem.distance_sq < b.elem.distance_sq) ? a : b;
      });
}

static bool select_linked_pick(bContext &C, const int2 &mval, const SelectPick_Params &params)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(&C);
  const ViewContext vc = ED_view3d_viewcontext_init(&C, depsgraph);
  const Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d);

  const ClosestCurveDataBlock closest = find_closest_curve(*depsgraph, vc, bases, mval);
  if (!closest.curves_id) {
    return false;
  }

  bke::CurvesGeometry &closest_curves = closest.curves_id->geometry.wrap();
  const bke::AttrDomain selection_domain = bke::AttrDomain(closest.curves_id->selection_domain);

  if (selection_domain == bke::AttrDomain::Point) {
    const OffsetIndices points_by_curve = closest_curves.points_by_curve();
    foreach_selection_attribute_writer(
        closest_curves, bke::AttrDomain::Point, [&](bke::GSpanAttributeWriter &selection) {
          for (const int point : points_by_curve[closest.elem.index]) {
            apply_selection_operation_at_index(selection.span, point, params.sel_op);
          }
        });
  }
  else if (selection_domain == bke::AttrDomain::Curve) {
    bke::GSpanAttributeWriter selection = ensure_selection_attribute(
        closest_curves, bke::AttrDomain::Curve, CD_PROP_BOOL);
    apply_selection_operation_at_index(selection.span, closest.elem.index, params.sel_op);
    selection.finish();
  }

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
   * generic attribute for now. */
  DEG_id_tag_update(&closest.curves_id->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(&C, NC_GEOM | ND_DATA, closest.curves_id);

  return true;
}

static int select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SelectPick_Params params{};
  params.sel_op = RNA_boolean_get(op->ptr, "deselect") ? SEL_OP_SUB : SEL_OP_ADD;
  params.deselect_all = false;
  params.select_passthrough = false;

  if (!select_linked_pick(*C, event->mval, params)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void CURVES_OT_select_linked_pick(wmOperatorType *ot)
{
  ot->name = "Select Linked";
  ot->idname = "CURVES_OT_select_linked_pick";
  ot->description = "Select all points in the curve under the cursor";

  ot->invoke = select_linked_pick_invoke;
  ot->poll = editable_curves_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "deselect",
                  false,
                  "Deselect",
                  "Deselect linked control points rather than selecting them");
}

}  // namespace blender::ed::curves
