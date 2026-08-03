/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"

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
    {RELAX_EDGE_LOOPS_INTERP_CUBIC, "CUBIC", 0, "Cubic", "Natural cubic spline, smooth results"},
    {RELAX_EDGE_LOOPS_INTERP_LINEAR,
     "LINEAR",
     0,
     "Linear",
     "Relax edge loops using linear interpolation"},
    {0, nullptr},
};

static wmOperatorStatus edbm_relax_edge_loops_exec(bContext *C, wmOperator *op)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      *bmain, scene, view_layer, CTX_wm_view3d(C));

  const int interpolation = RNA_enum_get(op->ptr, "interpolation");
  const int iterations = RNA_int_get(op->ptr, "iterations");
  const bool even_spacing = RNA_boolean_get(op->ptr, "even_spacing");
  bool changed_multi = false;
  bool has_edges_selected = false;
  bool has_faces_selected = false;

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totedgesel > 0) {
      has_edges_selected = true;
    }

    if (em->bm->totfacesel > 0) {
      has_faces_selected = true;
      continue;
    }

    if (em->bm->totedgesel < 2) {
      continue;
    }

    if (!EDBM_op_callf(em,
                       op,
                       "relax_edge_loops geom=%he interpolation=%i iterations=%i even_spacing=%b",
                       BM_ELEM_SELECT,
                       interpolation,
                       iterations,
                       even_spacing))
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

void MESH_OT_relax_edge_loops(wmOperatorType *ot)
{
  ot->name = "Relax Edge Loops";
  ot->description = "Relax the loop, so it is smoother";
  ot->idname = "MESH_OT_relax_edge_loops";

  ot->exec = edbm_relax_edge_loops_exec;
  ot->poll = ED_operator_editmesh;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  RNA_def_int(ot->srna,
              "iterations",
              1,
              1,
              25,
              "Iterations",
              "Number of times the loop is relaxed",
              1,
              25);
  RNA_def_boolean(ot->srna,
                  "even_spacing",
                  true,
                  "Space Evenly",
                  "Distribute vertices at constant distances along the loop");
  RNA_def_enum(ot->srna,
               "interpolation",
               prop_interpolation_items,
               0,
               "Interpolation",
               "Algorithm used for interpolation");
}

}  // namespace blender
