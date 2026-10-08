/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"

#include "BLI_listbase.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_c.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "BKE_annotations.h"
#include "DNA_gpencil_legacy_types.h"
#include "ED_gpencil_legacy.hh"

#include "mesh_intern.hh" /* own include */

namespace blender {

static const EnumPropertyItem prop_method_items[] = {
    {TO_ANNOTATION_SPREAD,
     "SPREAD",
     0,
     "Spread",
     "Distribute vertices along the full stroke, retaining relative distances between the "
     "vertices"},
    {TO_ANNOTATION_SPREAD_EVENLY,
     "SPREAD_EVENLY",
     0,
     "Spread Evenly",
     "Distribute vertices at regular distances along the full stroke "},
    {TO_ANNOTATION_PROJECT,
     "PROJECT",
     0,
     "Project",
     "Project vertices onto the stroke using vertex normals and connected edges. "
     "Requires more exact annotation placement, otherwise it may do nothing"},
    {0, nullptr},
};

static wmOperatorStatus edbm_fit_edge_loops_to_annotation_exec(bContext *C, wmOperator *op)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      *bmain, scene, view_layer, CTX_wm_view3d(C));

  const float factor = RNA_float_get(op->ptr, "factor");
  const int method = RNA_enum_get(op->ptr, "method");
  const bool delete_strokes = RNA_boolean_get(op->ptr, "delete_strokes");
  bool lock[3];
  RNA_boolean_get_array(op->ptr, "lock", lock);
  bool changed_multi = false;

  bGPdata *gpd = ED_annotation_data_get_active(C);
  bGPDlayer *gpl = gpd ? BKE_annotations_layer_active_get(gpd) : nullptr;
  bGPDframe *gpf = gpl ? gpl->actframe : nullptr;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  for (Object *obedit : objects) {
    BMesh *bm = BKE_editmesh_bmesh_get_for_write(obedit);

    Vector<Vector<float3>> all_points;
    if (gpf) {
      for (bGPDstroke *gps = gpf->strokes.first(); gps; gps = gps->next) {
        if (gps->totpoints < 2) {
          continue;
        }

        if (!(gps->flag & GP_STROKE_3DSPACE)) {
          continue;
        }

        Vector<float3> points;
        points.reserve(gps->totpoints);
        Object *obedit_eval = DEG_get_evaluated(depsgraph, obedit);
        float4x4 mat = obedit_eval->world_to_object();

        for (const int i : IndexRange(gps->totpoints)) {
          bGPDspoint *stroke_point = &gps->points[i];
          float3 local_co = math::transform_point(
              mat, float3(stroke_point->x, stroke_point->y, stroke_point->z));
          points.append(local_co);
        }

        all_points.append(std::move(points));
      }
    }

    Vector<Span<float3>> stroke_data;
    stroke_data.reserve(all_points.size());
    for (const Vector<float3> &points : all_points) {
      stroke_data.append(points);
    }
    if (!EDBM_op_callf(
            bm,
            op,
            "fit_edge_loops_to_annotation geom=%he factor=%f method=%i strokes=%p lock_x=%b "
            "lock_y=%b lock_z=%b",
            BM_ELEM_SELECT,
            factor,
            method,
            &stroke_data,
            lock[0],
            lock[1],
            lock[2]))
    {
      continue;
    }

    changed_multi = true;
    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = true;
    EDBM_update(id_cast<Mesh *>(obedit->data), &params);
  }

  if (changed_multi && delete_strokes && gpf) {
    bool deleted = false;
    bGPDstroke *gps_next;
    for (bGPDstroke *gps = gpf->strokes.first(); gps; gps = gps_next) {
      gps_next = gps->next;
      if (gps->totpoints >= 2 && (gps->flag & GP_STROKE_3DSPACE)) {
        BLI_remlink(&gpf->strokes, gps);
        BKE_annotations_free_stroke(gps);
        deleted = true;
      }
    }
    if (deleted) {
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
    }
  }

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void edbm_fit_edge_loops_to_annotation_ui(bContext * /*C*/, wmOperator *op)
{
  /* A custom UI function is needed to draw the axis locks (X/Y/Z) as a row of toggle buttons. */
  ui::Layout &layout = *op->layout;
  layout.use_property_split_set(true);

  layout.prop(op->ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(op->ptr, "method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(op->ptr, "delete_strokes", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  ui::Layout &lock_row = layout.row(true, IFACE_("Lock"));
  PropertyRNA *lock_prop = RNA_struct_find_property(op->ptr, "lock");
  lock_row.prop(op->ptr, lock_prop, 0, 0, ui::ITEM_R_TOGGLE, "X", ICON_NONE);
  lock_row.prop(op->ptr, lock_prop, 1, 0, ui::ITEM_R_TOGGLE, "Y", ICON_NONE);
  lock_row.prop(op->ptr, lock_prop, 2, 0, ui::ITEM_R_TOGGLE, "Z", ICON_NONE);
}

void MESH_OT_fit_edge_loops_to_annotation(wmOperatorType *ot)
{
  ot->name = "Fit Edge Loops To Annotation";
  ot->description = "Stretch selected vertices to active stroke";
  ot->idname = "MESH_OT_fit_edge_loops_to_annotation";

  ot->exec = edbm_fit_edge_loops_to_annotation_exec;
  ot->poll = ED_operator_editmesh;
  ot->ui = edbm_fit_edge_loops_to_annotation_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(
      ot->srna, "factor", 1.0f, 0.0f, 1.0f, "Factor", "Annotation factor", 0.0f, 1.0f);
  RNA_def_boolean(ot->srna,
                  "delete_strokes",
                  false,
                  "Delete Strokes",
                  "Remove strokes if they have been used");
  RNA_def_enum(ot->srna,
               "method",
               prop_method_items,
               0,
               "Method",
               "Method of distributing the vertices over the stroke");
  RNA_def_boolean_array(ot->srna, "lock", 3, nullptr, "Lock", "Lock editing of the axis");
}

}  // namespace blender
