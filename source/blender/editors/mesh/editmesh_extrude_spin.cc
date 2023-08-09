/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_object_types.h"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h" /* own include */

#define USE_GIZMO

/* -------------------------------------------------------------------- */
/** \name Spin Operator
 * \{ */

static int edbm_spin_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  float cent[3], axis[3];
  const float d[3] = {0.0f, 0.0f, 0.0f};

  RNA_float_get_array(op->ptr, "center", cent);
  RNA_float_get_array(op->ptr, "axis", axis);
  const int steps = RNA_int_get(op->ptr, "steps");
  const float angle = RNA_float_get(op->ptr, "angle");
  const bool use_normal_flip = RNA_boolean_get(op->ptr, "use_normal_flip");
  const bool dupli = RNA_boolean_get(op->ptr, "dupli");
  const bool use_auto_merge = (RNA_boolean_get(op->ptr, "use_auto_merge") && (dupli == false) &&
                               (steps >= 3) && fabsf(fabsf(angle) - float(M_PI * 2)) <= 1e-6f);

  if (is_zero_v3(axis)) {
    BKE_report(op->reports, RPT_ERROR, "Invalid/unset axis");
    return OPERATOR_CANCELLED;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    BMOperator spinop;

    /* Keep the values in world-space since we're passing the `obmat`. */
    if (!EDBM_op_init(em,
                      &spinop,
                      op,
                      "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 "
                      "use_normal_flip=%b use_duplicate=%b use_merge=%b",
                      BM_ELEM_SELECT,
                      cent,
                      axis,
                      d,
                      steps,
                      -angle,
                      obedit->object_to_world,
                      use_normal_flip,
                      dupli,
                      use_auto_merge))
    {
      continue;
    }
    BMO_op_exec(bm, &spinop);
    if (use_auto_merge == false) {
      EDBM_flag_disable_all(em, BM_ELEM_SELECT);
      BMO_slot_buffer_hflag_enable(
          bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
    }
    if (!EDBM_op_finish(em, &spinop, op, true)) {
      continue;
    }

    EDBMUpdate_Params params{};
    params.calc_looptri = true;
    params.calc_normals = false;
    params.is_destructive = true;
    EDBM_update(static_cast<Mesh *>(obedit->data), &params);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int edbm_spin_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);

  PropertyRNA *prop;
  prop = RNA_struct_find_property(op->ptr, "center");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_set_array(op->ptr, prop, scene->cursor.location);
  }
  if (rv3d) {
    prop = RNA_struct_find_property(op->ptr, "axis");
    if (!RNA_property_is_set(op->ptr, prop)) {
      RNA_property_float_set_array(op->ptr, prop, rv3d->viewinv[2]);
    }
  }

#ifdef USE_GIZMO
  /* Start with zero angle, drag out the value. */
  prop = RNA_struct_find_property(op->ptr, "angle");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_float_set(op->ptr, prop, 0.0f);
  }
#endif

  int ret = edbm_spin_exec(C, op);

#ifdef USE_GIZMO
  if (ret & OPERATOR_FINISHED) {
    /* Setup gizmos */
    if (v3d && ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0)) {
      wmGizmoGroupType *gzgt = WM_gizmogrouptype_find("MESH_GGT_spin_redo", false);
      if (!WM_gizmo_group_type_ensure_ptr(gzgt)) {
        Main *bmain = CTX_data_main(C);
        WM_gizmo_group_type_reinit_ptr(bmain, gzgt);
      }
    }
  }
#endif

  return ret;
}

static bool edbm_spin_poll_property(const bContext * /*C*/,
                                    wmOperator *op,
                                    const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  const bool dupli = RNA_boolean_get(op->ptr, "dupli");

  if (dupli) {
    if (STR_ELEM(prop_id, "use_auto_merge", "use_normal_flip")) {
      return false;
    }
  }
  return true;
}

void MESH_OT_spin(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Spin";
  ot->description =
      "Extrude selected vertices in a circle around the cursor in indicated viewport";
  ot->idname = "MESH_OT_spin";

  /* api callbacks */
  ot->invoke = edbm_spin_invoke;
  ot->exec = edbm_spin_exec;
  ot->poll = ED_operator_editmesh;
  ot->poll_property = edbm_spin_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "steps", 12, 0, 1000000, "Steps", "Steps", 0, 1000);

  prop = RNA_def_boolean(ot->srna, "dupli", false, "Use Duplicates", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_float(ot->srna,
                       "angle",
                       DEG2RADF(90.0f),
                       -1e12f,
                       1e12f,
                       "Angle",
                       "Rotation for each step",
                       DEG2RADF(-360.0f),
                       DEG2RADF(360.0f));
  RNA_def_property_subtype(prop, PROP_ANGLE);
  RNA_def_boolean(ot->srna,
                  "use_auto_merge",
                  true,
                  "Auto Merge",
                  "Merge first/last when the angle is a full revolution");
  RNA_def_boolean(ot->srna, "use_normal_flip", false, "Flip Normals", "");

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

  WM_gizmogrouptype_append(MESH_GGT_spin);
#ifdef USE_GIZMO
  WM_gizmogrouptype_append(MESH_GGT_spin_redo);
#endif
}

/** \} */
