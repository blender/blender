/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_report.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "mesh_intern.hh" /* own include */

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Screw Operator
 * \{ */

static int edbm_screw_exec(bContext *C, wmOperator *op)
{
  BMEdge *eed;
  BMVert *eve, *v1, *v2;
  BMIter iter, eiter;
  float dvec[3], nor[3], cent[3], axis[3], v1_co_global[3], v2_co_global[3];
  int steps, turns;
  int valence;
  uint objects_empty_len = 0;
  uint failed_axis_len = 0;
  uint failed_verts_len = 0;

  turns = RNA_int_get(op->ptr, "turns");
  steps = RNA_int_get(op->ptr, "steps");
  RNA_float_get_array(op->ptr, "center", cent);
  RNA_float_get_array(op->ptr, "axis", axis);

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    if (bm->totvertsel < 2) {
      if (bm->totvertsel == 0) {
        objects_empty_len++;
      }
      continue;
    }

    if (is_zero_v3(axis)) {
      failed_axis_len++;
      continue;
    }

    /* find two vertices with valence count == 1, more or less is wrong */
    v1 = nullptr;
    v2 = nullptr;

    BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
      valence = 0;
      BM_ITER_ELEM (eed, &eiter, eve, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          valence++;
        }
      }

      if (valence == 1) {
        if (v1 == nullptr) {
          v1 = eve;
        }
        else if (v2 == nullptr) {
          v2 = eve;
        }
        else {
          v1 = nullptr;
          break;
        }
      }
    }

    if (v1 == nullptr || v2 == nullptr) {
      failed_verts_len++;
      continue;
    }

    copy_v3_v3(nor, obedit->object_to_world[2]);

    /* calculate dvec */
    mul_v3_m4v3(v1_co_global, obedit->object_to_world, v1->co);
    mul_v3_m4v3(v2_co_global, obedit->object_to_world, v2->co);
    sub_v3_v3v3(dvec, v1_co_global, v2_co_global);
    mul_v3_fl(dvec, 1.0f / steps);

    if (dot_v3v3(nor, dvec) > 0.0f) {
      negate_v3(dvec);
    }

    BMOperator spinop;
    if (!EDBM_op_init(
            em,
            &spinop,
            op,
            "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 use_duplicate=%b",
            BM_ELEM_SELECT,
            cent,
            axis,
            dvec,
            turns * steps,
            DEG2RADF(360.0f * turns),
            obedit->object_to_world,
            false))
    {
      continue;
    }

    BMO_op_exec(bm, &spinop);
    EDBM_flag_disable_all(em, BM_ELEM_SELECT);
    BMO_slot_buffer_hflag_enable(
        bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);

    if (!EDBM_op_finish(em, &spinop, op, true)) {
      continue;
    }

    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = false;
    params.is_destructive = true;
    EDBM_update(static_cast<Mesh *>(obedit->data), &params);
  }

  if (failed_axis_len == objects.size() - objects_empty_len) {
    BKE_report(op->reports, RPT_ERROR, "Invalid/unset axis");
  }
  else if (failed_verts_len == objects.size() - objects_empty_len) {
    BKE_report(op->reports, RPT_ERROR, "You have to select a string of connected vertices too");
  }

  return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int edbm_screw_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_scene(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "center");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_set_array(op->ptr, prop, scene->cursor.location);
  }
  if (rv3d) {
    prop = RNA_struct_find_property(op->ptr, "axis");
    if (!RNA_property_is_set(op->ptr, prop)) {
      RNA_property_float_set_array(op->ptr, prop, rv3d->viewinv[1]);
    }
  }

  return edbm_screw_exec(C, op);
}

void MESH_OT_screw(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Screw";
  ot->description =
      "Extrude selected vertices in screw-shaped rotation around the cursor in indicated viewport";
  ot->idname = "MESH_OT_screw";

  /* api callbacks */
  ot->invoke = edbm_screw_invoke;
  ot->exec = edbm_screw_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "steps", 9, 1, 100000, "Steps", "Steps", 3, 256);
  RNA_def_int(ot->srna, "turns", 1, 1, 100000, "Turns", "Turns", 1, 256);

  RNA_def_float_vector_xyz(ot->srna,
                           "center",
                           3,
                           nullptr,
                           -1e12f,
                           1e12f,
                           "Center",
                           "Center in global view space",
                           -1e4f,
                           1e4f);
  RNA_def_float_vector(
      ot->srna, "axis", 3, nullptr, -1.0f, 1.0f, "Axis", "Axis in global view space", -1.0f, 1.0f);
}

/** \} */
