/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 * Operator for creating bézier splines in Grease Pencil.
 */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"
#include "BKE_report.hh"

#include "BLI_array_utils.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "DEG_depsgraph.hh"

#include "DNA_material_types.h"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "UI_resources.hh"

namespace blender::ed::greasepencil {

class GreasePencilPenToolOperation : public curves::pen_tool::PenToolOperation {
 public:
  GreasePencil *grease_pencil;
  Vector<MutableDrawingInfo> drawings;

  /* Helper class to project screen space coordinates to 3D. */
  DrawingPlacement placement;

  float3 project(const float2 &screen_co) const
  {
    return this->placement.project(screen_co);
  }

  IndexMask all_selected_points(const int curves_index, IndexMaskMemory &memory) const
  {
    const MutableDrawingInfo &info = this->drawings[curves_index];
    return ed::greasepencil::retrieve_editable_and_all_selected_points(
        *this->vc.obact,
        info.drawing,
        info.layer_index,
        this->vc.v3d->overlay.handle_display,
        memory);
  }

  IndexMask visible_bezier_handle_points(const int curves_index, IndexMaskMemory &memory) const
  {
    const MutableDrawingInfo &info = this->drawings[curves_index];
    return ed::greasepencil::retrieve_visible_bezier_handle_points(
        *this->vc.obact,
        info.drawing,
        info.layer_index,
        this->vc.v3d->overlay.handle_display,
        memory);
  }

  IndexMask editable_curves(const int curves_index, IndexMaskMemory &memory) const
  {
    const MutableDrawingInfo &info = this->drawings[curves_index];
    return ed::greasepencil::retrieve_editable_strokes(
        *this->vc.obact, info.drawing, info.layer_index, memory);
  }

  void tag_curve_changed(const int curves_index) const
  {
    const MutableDrawingInfo &info = this->drawings[curves_index];
    info.drawing.tag_topology_changed();
  }

  bke::CurvesGeometry &get_curves(const int curves_index) const
  {
    const MutableDrawingInfo &info = this->drawings[curves_index];
    return info.drawing.strokes_for_write();
  }

  IndexRange curves_range() const
  {
    return this->drawings.index_range();
  }

  void single_point_attributes(bke::CurvesGeometry &curves, const int curves_index) const
  {
    const MutableDrawingInfo &info = this->drawings[curves_index];
    info.drawing.opacities_for_write().last() = 1.0f;
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    bke::SpanAttributeWriter<float> aspect_ratios = attributes.lookup_or_add_for_write_span<float>(
        "aspect_ratio",
        bke::AttrDomain::Curve,
        bke::AttributeInitVArray(VArray<float>::from_single(0.0f, curves.curves_num())));
    aspect_ratios.span.last() = 1.0f;
    aspect_ratios.finish();

    bke::SpanAttributeWriter<float> u_scales = attributes.lookup_or_add_for_write_span<float>(
        "u_scale",
        bke::AttrDomain::Curve,
        bke::AttributeInitVArray(VArray<float>::from_single(0.0f, curves.curves_num())));
    u_scales.span.last() = 1.0f;
    u_scales.finish();
  }

  bool can_create_new_curve(wmOperator *op) const
  {
    if (!this->grease_pencil->has_active_layer()) {
      BKE_report(op->reports, RPT_ERROR, "No active Grease Pencil layer");
      return false;
    }

    bke::greasepencil::Layer &layer = *this->grease_pencil->get_active_layer();
    if (!layer.is_editable()) {
      BKE_report(op->reports, RPT_ERROR, "Active layer is locked or hidden");
      return false;
    }

    const int material_index = this->vc.obact->actcol - 1;
    Material *material = BKE_object_material_get(this->vc.obact, material_index + 1);
    /* The editable materials are unlocked and not hidden. */
    if (material != nullptr && material->gp_style != nullptr &&
        ((material->gp_style->flag & GP_MATERIAL_LOCKED) != 0 ||
         (material->gp_style->flag & GP_MATERIAL_HIDE) != 0))
    {
      BKE_report(op->reports, RPT_ERROR, "Active Material is locked or hidden");
      return false;
    }

    /* Ensure a drawing at the current keyframe. */
    bool inserted_keyframe = false;
    if (!ed::greasepencil::ensure_active_keyframe(
            *this->vc.scene, *this->grease_pencil, layer, false, inserted_keyframe))
    {
      BKE_report(op->reports, RPT_ERROR, "No Grease Pencil frame to draw on");
      return false;
    }

    /* We should insert the keyframe when initializing not here. */
    BLI_assert(!inserted_keyframe);
    BLI_assert(this->active_drawing_index != std::nullopt);

    return true;
  }

  void update_view(bContext *C) const
  {
    GreasePencil *grease_pencil = this->grease_pencil;

    DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, grease_pencil);

    ED_region_tag_redraw(this->vc.region);
  }

  std::optional<wmOperatorStatus> initialize(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
  {
    if (this->vc.scene->toolsettings->gpencil_selectmode_edit != GP_SELECTMODE_POINT) {
      BKE_report(op->reports, RPT_ERROR, "Selection Mode must be Points");
      return OPERATOR_CANCELLED;
    }

    GreasePencil *grease_pencil = static_cast<GreasePencil *>(this->vc.obact->data);
    this->grease_pencil = grease_pencil;
    View3D *view3d = CTX_wm_view3d(C);

    /* Initialize helper class for projecting screen space coordinates. */
    DrawingPlacement placement = DrawingPlacement(*this->vc.scene,
                                                  *this->vc.region,
                                                  *view3d,
                                                  *this->vc.obact,
                                                  grease_pencil->get_active_layer());
    if (placement.use_project_to_surface()) {
      placement.cache_viewport_depths(CTX_data_depsgraph_pointer(C), this->vc.region, view3d);
    }
    else if (placement.use_project_to_stroke()) {
      placement.cache_viewport_depths(CTX_data_depsgraph_pointer(C), this->vc.region, view3d);
    }

    bool inserted_keyframe = false;
    /* For the pen tool, we don't want the auto-key to create an empty keyframe, so we
     * duplicate the previous key. */
    const bool use_duplicate_previous_key = true;
    for (bke::greasepencil::Layer *layer : grease_pencil->layers_for_write()) {
      if (layer->is_editable()) {
        ed::greasepencil::ensure_active_keyframe(*this->vc.scene,
                                                 *grease_pencil,
                                                 *layer,
                                                 use_duplicate_previous_key,
                                                 inserted_keyframe);
      }
    }

    /* Update the view. */
    if (inserted_keyframe) {
      WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
    }

    this->placement = placement;
    this->drawings = retrieve_editable_drawings(*this->vc.scene, *this->grease_pencil);

    for (const int drawing_index : this->drawings.index_range()) {
      const MutableDrawingInfo &info = this->drawings[drawing_index];
      const bke::greasepencil::Layer &layer = this->grease_pencil->layer(info.layer_index);
      this->layer_to_object_per_curves.append(layer.local_transform());
      this->layer_to_world_per_curves.append(layer.to_world_space(*this->vc.obact));
    }

    this->active_drawing_index = std::nullopt;
    const bke::greasepencil::Layer *active_layer = this->grease_pencil->get_active_layer();

    if (active_layer != nullptr) {
      const bke::greasepencil::Drawing *active_drawing =
          this->grease_pencil->get_editable_drawing_at(*active_layer, this->vc.scene->r.cfra);

      for (const int drawing_index : this->drawings.index_range()) {
        const MutableDrawingInfo &info = this->drawings[drawing_index];

        if (active_drawing == &info.drawing) {
          BLI_assert(this->active_drawing_index == std::nullopt);
          this->active_drawing_index = drawing_index;
        }
      }
    }

    return std::nullopt;
  }
};

/* Exit and free memory. */
static void grease_pencil_pen_exit(bContext *C, wmOperator *op)
{
  GreasePencilPenToolOperation *ptd = static_cast<GreasePencilPenToolOperation *>(op->customdata);

  /* Clear status message area. */
  ED_workspace_status_text(C, nullptr);

  WM_cursor_modal_restore(ptd->vc.win);

  ptd->update_view(C);

  MEM_delete(ptd);
  /* Clear pointer. */
  op->customdata = nullptr;
}

/* Invoke handler: Initialize the operator. */
static wmOperatorStatus grease_pencil_pen_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Allocate new data. */
  GreasePencilPenToolOperation *ptd_pointer = MEM_new<GreasePencilPenToolOperation>(__func__);
  op->customdata = ptd_pointer;
  GreasePencilPenToolOperation &ptd = *ptd_pointer;

  const wmOperatorStatus result = ptd.invoke(C, op, event);
  if (result != OPERATOR_RUNNING_MODAL) {
    grease_pencil_pen_exit(C, op);
  }
  return result;
}

/* Modal handler: Events handling during interactive part. */
static wmOperatorStatus grease_pencil_pen_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  GreasePencilPenToolOperation &ptd = *reinterpret_cast<GreasePencilPenToolOperation *>(
      op->customdata);

  const wmOperatorStatus result = ptd.modal(C, op, event);
  if (result != OPERATOR_RUNNING_MODAL) {
    grease_pencil_pen_exit(C, op);
  }
  return result;
}

static void GREASE_PENCIL_OT_pen(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Grease Pencil Pen";
  ot->idname = "GREASE_PENCIL_OT_pen";
  ot->description = "Construct and edit splines";

  /* Callbacks. */
  ot->invoke = grease_pencil_pen_invoke;
  ot->modal = grease_pencil_pen_modal;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  curves::pen_tool::pen_tool_common_props(ot);
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_pen()
{
  WM_operatortype_append(blender::ed::greasepencil::GREASE_PENCIL_OT_pen);
}

void ED_grease_pencil_pentool_modal_keymap(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = blender::ed::curves::pen_tool::ensure_keymap(keyconf);
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_pen");
}
