/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_listbase.hh"
#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_report.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "ED_mesh.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "mesh_intern.hh" /* own include */

namespace blender {

static const EnumPropertyItem prop_interpolation_items[] = {
    {SPACE_EDGE_LOOPS_EVENLY_INTERP_CUBIC,
     "CUBIC",
     0,
     "Cubic",
     "Natural cubic spline, smooth results"},
    {SPACE_EDGE_LOOPS_EVENLY_INTERP_LINEAR,
     "LINEAR",
     0,
     "Linear",
     "Vertices are projected on existing edges"},
    {0, nullptr},
};

/** Return true if at least two selected edges form a chain. */
static bool has_connected_selected_edges(BMesh *bm)
{
  BMIter iter;
  BMEdge *eed;
  BM_ITER_MESH (eed, &iter, bm, BM_EDGES_OF_MESH) {
    if (!BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
      continue;
    }
    if (BM_iter_elem_count_flag(BM_EDGES_OF_VERT, eed->v1, BM_ELEM_SELECT, true) >= 2 ||
        BM_iter_elem_count_flag(BM_EDGES_OF_VERT, eed->v2, BM_ELEM_SELECT, true) >= 2)
    {
      return true;
    }
  }
  return false;
}

static wmOperatorStatus edbm_space_edge_loops_evenly_exec(bContext *C, wmOperator *op)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      *bmain, scene, view_layer, CTX_wm_view3d(C));
  const float factor = RNA_float_get(op->ptr, "factor");
  const int interpolation = RNA_enum_get(op->ptr, "interpolation");
  bool lock[3];
  RNA_boolean_get_array(op->ptr, "lock", lock);
  bool has_edges_selected = false;
  bool has_faces_selected = false;
  bool changed_multi = false;

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    if (bm->totedgesel > 0) {
      has_edges_selected = true;
    }
    if (bm->totfacesel > 0) {
      has_faces_selected = true;
      continue;
    }
    /* At least 2 connected edges are needed to form a chain.
     * This check isn't fool-proof since edges may be disconnected. */
    if (bm->totedgesel < 2) {
      continue;
    }

    if (!has_connected_selected_edges(bm)) {
      continue;
    }

    if (!EDBM_op_callf(em,
                       op,
                       "space_edge_loops_evenly geom=%he interpolation=%i factor=%f "
                       "lock_x=%b lock_y=%b lock_z=%b",
                       BM_ELEM_SELECT,
                       interpolation,
                       factor,
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

  if (!changed_multi) {
    if (has_faces_selected) {
      BKE_report(
          op->reports, RPT_WARNING, "Operator requires separate edge loops, selected faces found");
    }
    else if (!has_edges_selected) {
      BKE_report(op->reports, RPT_WARNING, "No edges selected");
    }
    else {
      BKE_report(op->reports, RPT_WARNING, "No edge loops found containing 2 or more edges");
    }
  }

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void edbm_space_edge_loops_evenly_ui(bContext * /*C*/, wmOperator *op)
{
  /* A custom UI function is needed to draw the axis locks (X/Y/Z) as a row of toggle buttons. */
  ui::Layout &layout = *op->layout;
  layout.use_property_split_set(true);

  layout.prop(op->ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(op->ptr, "interpolation", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  ui::Layout &lock_row = layout.row(true, IFACE_("Lock"));
  PropertyRNA *lock_prop = RNA_struct_find_property(op->ptr, "lock");
  lock_row.prop(op->ptr, lock_prop, 0, 0, ui::ITEM_R_TOGGLE, "X", ICON_NONE);
  lock_row.prop(op->ptr, lock_prop, 1, 0, ui::ITEM_R_TOGGLE, "Y", ICON_NONE);
  lock_row.prop(op->ptr, lock_prop, 2, 0, ui::ITEM_R_TOGGLE, "Z", ICON_NONE);
}

void MESH_OT_space_edge_loops_evenly(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Space Edge Loops Evenly";
  ot->description = "Space the vertices in a regular distribution on the loop";
  ot->idname = "MESH_OT_space_edge_loops_evenly";

  /* API callbacks */
  ot->exec = edbm_space_edge_loops_evenly_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  ot->ui = edbm_space_edge_loops_evenly_ui;

  RNA_def_float_factor(
      ot->srna, "factor", 1.0f, 0.0f, 1.0f, "Factor", "Spacing effect factor", 0.0f, 1.0f);
  RNA_def_enum(ot->srna,
               "interpolation",
               prop_interpolation_items,
               0,
               "Interpolation",
               "Algorithm used for interpolation");
  RNA_def_boolean_array(ot->srna, "lock", 3, nullptr, "Lock", "Lock editing of the axis");
}

}  // namespace blender
