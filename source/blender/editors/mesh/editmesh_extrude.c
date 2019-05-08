/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 */

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_layer.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_types.h"
#include "WM_api.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Extrude Internal Utilities
 * \{ */

static void edbm_extrude_edge_exclude_mirror(
    Object *obedit, BMEditMesh *em, const char hflag, BMOperator *op, BMOpSlot *slot_edges_exclude)
{
  BMesh *bm = em->bm;
  ModifierData *md;

  /* If a mirror modifier with clipping is on, we need to adjust some
   * of the cases above to handle edges on the line of symmetry.
   */
  for (md = obedit->modifiers.first; md; md = md->next) {
    if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
      MirrorModifierData *mmd = (MirrorModifierData *)md;

      if (mmd->flag & MOD_MIR_CLIPPING) {
        BMIter iter;
        BMEdge *edge;

        float mtx[4][4];
        if (mmd->mirror_ob) {
          float imtx[4][4];
          invert_m4_m4(imtx, mmd->mirror_ob->obmat);
          mul_m4_m4m4(mtx, imtx, obedit->obmat);
        }

        BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(edge, hflag) && BM_edge_is_boundary(edge) &&
              BM_elem_flag_test(edge->l->f, hflag)) {
            float co1[3], co2[3];

            copy_v3_v3(co1, edge->v1->co);
            copy_v3_v3(co2, edge->v2->co);

            if (mmd->mirror_ob) {
              mul_v3_m4v3(co1, mtx, co1);
              mul_v3_m4v3(co2, mtx, co2);
            }

            if (mmd->flag & MOD_MIR_AXIS_X) {
              if ((fabsf(co1[0]) < mmd->tolerance) && (fabsf(co2[0]) < mmd->tolerance)) {
                BMO_slot_map_empty_insert(op, slot_edges_exclude, edge);
              }
            }
            if (mmd->flag & MOD_MIR_AXIS_Y) {
              if ((fabsf(co1[1]) < mmd->tolerance) && (fabsf(co2[1]) < mmd->tolerance)) {
                BMO_slot_map_empty_insert(op, slot_edges_exclude, edge);
              }
            }
            if (mmd->flag & MOD_MIR_AXIS_Z) {
              if ((fabsf(co1[2]) < mmd->tolerance) && (fabsf(co2[2]) < mmd->tolerance)) {
                BMO_slot_map_empty_insert(op, slot_edges_exclude, edge);
              }
            }
          }
        }
      }
    }
  }
}

/* individual face extrude */
/* will use vertex normals for extrusion directions, so *nor is unaffected */
static bool edbm_extrude_discrete_faces(BMEditMesh *em, wmOperator *op, const char hflag)
{
  BMOIter siter;
  BMIter liter;
  BMFace *f;
  BMLoop *l;
  BMOperator bmop;

  EDBM_op_init(
      em, &bmop, op, "extrude_discrete_faces faces=%hf use_select_history=%b", hflag, true);

  /* deselect original verts */
  EDBM_flag_disable_all(em, BM_ELEM_SELECT);

  BMO_op_exec(em->bm, &bmop);

  BMO_ITER (f, &siter, bmop.slots_out, "faces.out", BM_FACE) {
    BM_face_select_set(em->bm, f, true);

    /* set face vertex normals to face normal */
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      copy_v3_v3(l->v->no, f->no);
    }
  }

  if (!EDBM_op_finish(em, &bmop, op, true)) {
    return false;
  }

  return true;
}

/* extrudes individual edges */
static bool edbm_extrude_edges_indiv(BMEditMesh *em,
                                     wmOperator *op,
                                     const char hflag,
                                     const bool use_normal_flip)
{
  BMesh *bm = em->bm;
  BMOperator bmop;

  EDBM_op_init(em,
               &bmop,
               op,
               "extrude_edge_only edges=%he use_normal_flip=%b use_select_history=%b",
               hflag,
               use_normal_flip,
               true);

  /* deselect original verts */
  BM_SELECT_HISTORY_BACKUP(bm);
  EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  BM_SELECT_HISTORY_RESTORE(bm);

  BMO_op_exec(em->bm, &bmop);
  BMO_slot_buffer_hflag_enable(
      em->bm, bmop.slots_out, "geom.out", BM_VERT | BM_EDGE, BM_ELEM_SELECT, true);

  if (!EDBM_op_finish(em, &bmop, op, true)) {
    return false;
  }

  return true;
}

/* extrudes individual vertices */
static bool edbm_extrude_verts_indiv(BMEditMesh *em, wmOperator *op, const char hflag)
{
  BMOperator bmop;

  EDBM_op_init(em, &bmop, op, "extrude_vert_indiv verts=%hv use_select_history=%b", hflag, true);

  /* deselect original verts */
  BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

  BMO_op_exec(em->bm, &bmop);
  BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

  if (!EDBM_op_finish(em, &bmop, op, true)) {
    return false;
  }

  return true;
}

static char edbm_extrude_htype_from_em_select(BMEditMesh *em)
{
  char htype = BM_ALL_NOLOOP;

  if (em->selectmode & SCE_SELECT_VERTEX) {
    /* pass */
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    htype &= ~BM_VERT;
  }
  else {
    htype &= ~(BM_VERT | BM_EDGE);
  }

  if (em->bm->totedgesel == 0) {
    htype &= ~(BM_EDGE | BM_FACE);
  }
  else if (em->bm->totfacesel == 0) {
    htype &= ~BM_FACE;
  }

  return htype;
}

static bool edbm_extrude_ex(Object *obedit,
                            BMEditMesh *em,
                            char htype,
                            const char hflag,
                            const bool use_normal_flip,
                            const bool use_mirror,
                            const bool use_select_history)
{
  BMesh *bm = em->bm;
  BMOIter siter;
  BMOperator extop;
  BMElem *ele;

  /* needed to remove the faces left behind */
  if (htype & BM_FACE) {
    htype |= BM_EDGE;
  }

  BMO_op_init(bm, &extop, BMO_FLAG_DEFAULTS, "extrude_face_region");
  BMO_slot_bool_set(extop.slots_in, "use_normal_flip", use_normal_flip);
  BMO_slot_bool_set(extop.slots_in, "use_select_history", use_select_history);
  BMO_slot_buffer_from_enabled_hflag(bm, &extop, extop.slots_in, "geom", htype, hflag);

  if (use_mirror) {
    BMOpSlot *slot_edges_exclude;
    slot_edges_exclude = BMO_slot_get(extop.slots_in, "edges_exclude");

    edbm_extrude_edge_exclude_mirror(obedit, em, hflag, &extop, slot_edges_exclude);
  }

  BM_SELECT_HISTORY_BACKUP(bm);
  EDBM_flag_disable_all(em, BM_ELEM_SELECT);
  BM_SELECT_HISTORY_RESTORE(bm);

  BMO_op_exec(bm, &extop);

  BMO_ITER (ele, &siter, extop.slots_out, "geom.out", BM_ALL_NOLOOP) {
    BM_elem_select_set(bm, ele, true);
  }

  BMO_op_finish(bm, &extop);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Repeat Operator
 * \{ */

static int edbm_extrude_repeat_exec(bContext *C, wmOperator *op)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const int steps = RNA_int_get(op->ptr, "steps");
  const float offs = RNA_float_get(op->ptr, "offset");
  float dvec[3], tmat[3][3], bmat[3][3];
  short a;

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {

    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    /* dvec */
    normalize_v3_v3_length(dvec, rv3d->persinv[2], offs);

    /* base correction */
    copy_m3_m4(bmat, obedit->obmat);
    invert_m3_m3(tmat, bmat);
    mul_m3_v3(tmat, dvec);

    for (a = 0; a < steps; a++) {
      edbm_extrude_ex(obedit, em, BM_ALL_NOLOOP, BM_ELEM_SELECT, false, false, false);

      BMO_op_callf(em->bm, BMO_FLAG_DEFAULTS, "translate vec=%v verts=%hv", dvec, BM_ELEM_SELECT);
    }

    EDBM_mesh_normals_update(em);

    EDBM_update_generic(em, true, true);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void MESH_OT_extrude_repeat(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Repeat Mesh";
  ot->description = "Extrude selected vertices, edges or faces repeatedly";
  ot->idname = "MESH_OT_extrude_repeat";

  /* api callbacks */
  ot->exec = edbm_extrude_repeat_exec;
  ot->poll = ED_operator_editmesh_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_float_distance(ot->srna, "offset", 2.0f, 0.0f, 1e12f, "Offset", "", 0.0f, 100.0f);
  RNA_def_int(ot->srna, "steps", 10, 0, 1000000, "Steps", "", 0, 180);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Operator
 * \{ */

/* generic extern called extruder */
static bool edbm_extrude_mesh(Object *obedit, BMEditMesh *em, wmOperator *op)
{
  const bool use_normal_flip = RNA_boolean_get(op->ptr, "use_normal_flip");
  const char htype = edbm_extrude_htype_from_em_select(em);
  enum { NONE = 0, ELEM_FLAG, VERT_ONLY, EDGE_ONLY } nr;
  bool changed = false;

  if (em->selectmode & SCE_SELECT_VERTEX) {
    if (em->bm->totvertsel == 0) {
      nr = NONE;
    }
    else if (em->bm->totvertsel == 1) {
      nr = VERT_ONLY;
    }
    else if (em->bm->totedgesel == 0) {
      nr = VERT_ONLY;
    }
    else {
      nr = ELEM_FLAG;
    }
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    if (em->bm->totedgesel == 0) {
      nr = NONE;
    }
    else if (em->bm->totfacesel == 0) {
      nr = EDGE_ONLY;
    }
    else {
      nr = ELEM_FLAG;
    }
  }
  else {
    if (em->bm->totfacesel == 0) {
      nr = NONE;
    }
    else {
      nr = ELEM_FLAG;
    }
  }

  switch (nr) {
    case NONE:
      return false;
    case ELEM_FLAG:
      changed = edbm_extrude_ex(obedit, em, htype, BM_ELEM_SELECT, use_normal_flip, true, true);
      break;
    case VERT_ONLY:
      changed = edbm_extrude_verts_indiv(em, op, BM_ELEM_SELECT);
      break;
    case EDGE_ONLY:
      changed = edbm_extrude_edges_indiv(em, op, BM_ELEM_SELECT, use_normal_flip);
      break;
  }

  if (changed) {
    return true;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "Not a valid selection for extrude");
    return false;
  }
}

/* extrude without transform */
static int edbm_extrude_region_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totvertsel == 0) {
      continue;
    }

    if (!edbm_extrude_mesh(obedit, em, op)) {
      continue;
    }
    /* This normally happens when pushing undo but modal operators
     * like this one don't push undo data until after modal mode is
     * done.*/
    EDBM_mesh_normals_update(em);

    EDBM_update_generic(em, true, true);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void MESH_OT_extrude_region(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Region";
  ot->idname = "MESH_OT_extrude_region";
  ot->description = "Extrude region of faces";

  /* api callbacks */
  // ot->invoke = mesh_extrude_region_invoke;
  ot->exec = edbm_extrude_region_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "use_normal_flip", false, "Flip Normals", "");
  Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Context Operator
 *
 * Guess what to do based on selection.
 * \{ */

/* extrude without transform */
static int edbm_extrude_context_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totvertsel == 0) {
      continue;
    }

    edbm_extrude_mesh(obedit, em, op);
    /* This normally happens when pushing undo but modal operators
     * like this one don't push undo data until after modal mode is
     * done.*/

    EDBM_mesh_normals_update(em);

    EDBM_update_generic(em, true, true);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void MESH_OT_extrude_context(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Context";
  ot->idname = "MESH_OT_extrude_context";
  ot->description = "Extrude selection";

  /* api callbacks */
  ot->exec = edbm_extrude_context_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "use_normal_flip", false, "Flip Normals", "");
  Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Verts Operator
 * \{ */

static int edbm_extrude_verts_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totvertsel == 0) {
      continue;
    }

    edbm_extrude_verts_indiv(em, op, BM_ELEM_SELECT);

    EDBM_update_generic(em, true, true);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void MESH_OT_extrude_verts_indiv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Only Vertices";
  ot->idname = "MESH_OT_extrude_verts_indiv";
  ot->description = "Extrude individual vertices only";

  /* api callbacks */
  ot->exec = edbm_extrude_verts_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Edges Operator
 * \{ */

static int edbm_extrude_edges_exec(bContext *C, wmOperator *op)
{
  const bool use_normal_flip = RNA_boolean_get(op->ptr, "use_normal_flip");
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totedgesel == 0) {
      continue;
    }

    edbm_extrude_edges_indiv(em, op, BM_ELEM_SELECT, use_normal_flip);

    EDBM_update_generic(em, true, true);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void MESH_OT_extrude_edges_indiv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Only Edges";
  ot->idname = "MESH_OT_extrude_edges_indiv";
  ot->description = "Extrude individual edges only";

  /* api callbacks */
  ot->exec = edbm_extrude_edges_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  RNA_def_boolean(ot->srna, "use_normal_flip", false, "Flip Normals", "");
  Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extrude Faces Operator
 * \{ */

static int edbm_extrude_faces_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (em->bm->totfacesel == 0) {
      continue;
    }

    edbm_extrude_discrete_faces(em, op, BM_ELEM_SELECT);

    EDBM_update_generic(em, true, true);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void MESH_OT_extrude_faces_indiv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extrude Individual Faces";
  ot->idname = "MESH_OT_extrude_faces_indiv";
  ot->description = "Extrude individual faces only";

  /* api callbacks */
  ot->exec = edbm_extrude_faces_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  Transform_Properties(ot, P_NO_DEFAULTS | P_MIRROR_DUMMY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Extrude Operator
 *
 * Add-click-mesh (extrude) operator.
 * \{ */

static int edbm_dupli_extrude_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ViewContext vc;
  BMVert *v1;
  BMIter iter;
  float center[3];
  uint verts_len;

  em_setup_viewcontext(C, &vc);
  const Object *object_active = vc.obact;

  const bool rot_src = RNA_boolean_get(op->ptr, "rotate_source");
  const bool use_proj = ((vc.scene->toolsettings->snap_flag & SCE_SNAP) &&
                         (vc.scene->toolsettings->snap_mode == SCE_SNAP_MODE_FACE));

  /* First calculate the center of transformation. */
  zero_v3(center);
  verts_len = 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      vc.view_layer, vc.v3d, &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    ED_view3d_viewcontext_init_object(&vc, obedit);
    const int local_verts_len = vc.em->bm->totvertsel;

    if (vc.em->bm->totvertsel == 0) {
      continue;
    }

    float local_center[3];
    zero_v3(local_center);

    BM_ITER_MESH (v1, &iter, vc.em->bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v1, BM_ELEM_SELECT)) {
        add_v3_v3(local_center, v1->co);
      }
    }

    mul_v3_fl(local_center, 1.0f / (float)local_verts_len);
    mul_m4_v3(vc.obedit->obmat, local_center);
    mul_v3_fl(local_center, (float)local_verts_len);

    add_v3_v3(center, local_center);
    verts_len += local_verts_len;
  }

  if (verts_len != 0) {
    mul_v3_fl(center, 1.0f / (float)verts_len);
  }

  /* Then we process the meshes. */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    ED_view3d_viewcontext_init_object(&vc, obedit);

    if (verts_len != 0) {
      if (vc.em->bm->totvertsel == 0) {
        continue;
      }
    }
    else if (obedit != object_active) {
      continue;
    }

    invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
    ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

    float local_center[3];
    mul_v3_m4v3(local_center, vc.obedit->imat, center);

    /* call extrude? */
    if (verts_len != 0) {
      const char extrude_htype = edbm_extrude_htype_from_em_select(vc.em);
      BMEdge *eed;
      float mat[3][3];
      float vec[3], ofs[3];
      float nor[3] = {0.0, 0.0, 0.0};

      /* 2D normal calc */
      const float mval_f[2] = {(float)event->mval[0], (float)event->mval[1]};

      /* check for edges that are half selected, use for rotation */
      bool done = false;
      BM_ITER_MESH (eed, &iter, vc.em->bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          float co1[2], co2[2];

          if ((ED_view3d_project_float_object(vc.ar, eed->v1->co, co1, V3D_PROJ_TEST_NOP) ==
               V3D_PROJ_RET_OK) &&
              (ED_view3d_project_float_object(vc.ar, eed->v2->co, co2, V3D_PROJ_TEST_NOP) ==
               V3D_PROJ_RET_OK)) {
            /* 2D rotate by 90d while adding.
             *  (x, y) = (y, -x)
             *
             * accumulate the screenspace normal in 2D,
             * with screenspace edge length weighting the result. */
            if (line_point_side_v2(co1, co2, mval_f) >= 0.0f) {
              nor[0] += (co1[1] - co2[1]);
              nor[1] += -(co1[0] - co2[0]);
            }
            else {
              nor[0] += (co2[1] - co1[1]);
              nor[1] += -(co2[0] - co1[0]);
            }
            done = true;
          }
        }
      }

      if (done) {
        float view_vec[3], cross[3];

        /* convert the 2D normal into 3D */
        mul_mat3_m4_v3(vc.rv3d->viewinv, nor); /* worldspace */
        mul_mat3_m4_v3(vc.obedit->imat, nor);  /* local space */

        /* correct the normal to be aligned on the view plane */
        mul_v3_mat3_m4v3(view_vec, vc.obedit->imat, vc.rv3d->viewinv[2]);
        cross_v3_v3v3(cross, nor, view_vec);
        cross_v3_v3v3(nor, view_vec, cross);
        normalize_v3(nor);
      }

      /* center */
      copy_v3_v3(ofs, local_center);

      mul_m4_v3(vc.obedit->obmat, ofs); /* view space */
      ED_view3d_win_to_3d_int(vc.v3d, vc.ar, ofs, event->mval, ofs);
      mul_m4_v3(vc.obedit->imat, ofs);  // back in object space

      sub_v3_v3(ofs, local_center);

      /* calculate rotation */
      unit_m3(mat);
      if (done) {
        float angle;

        normalize_v3_v3(vec, ofs);

        angle = angle_normalized_v3v3(vec, nor);

        if (angle != 0.0f) {
          float axis[3];

          cross_v3_v3v3(axis, nor, vec);

          /* halve the rotation if its applied twice */
          if (rot_src) {
            angle *= 0.5f;
          }

          axis_angle_to_mat3(mat, axis, angle);
        }
      }

      if (rot_src) {
        EDBM_op_callf(
            vc.em, op, "rotate verts=%hv cent=%v matrix=%m3", BM_ELEM_SELECT, local_center, mat);

        /* also project the source, for retopo workflow */
        if (use_proj) {
          EDBM_project_snap_verts(C, vc.ar, vc.em);
        }
      }

      edbm_extrude_ex(vc.obedit, vc.em, extrude_htype, BM_ELEM_SELECT, false, true, true);
      EDBM_op_callf(
          vc.em, op, "rotate verts=%hv cent=%v matrix=%m3", BM_ELEM_SELECT, local_center, mat);
      EDBM_op_callf(vc.em, op, "translate verts=%hv vec=%v", BM_ELEM_SELECT, ofs);
    }
    else {
      /* This only runs for the active object. */
      const float *cursor = vc.scene->cursor.location;
      BMOperator bmop;
      BMOIter oiter;

      copy_v3_v3(local_center, cursor);
      ED_view3d_win_to_3d_int(vc.v3d, vc.ar, local_center, event->mval, local_center);

      mul_m4_v3(vc.obedit->imat, local_center);  // back in object space

      EDBM_op_init(vc.em, &bmop, op, "create_vert co=%v", local_center);
      BMO_op_exec(vc.em->bm, &bmop);

      BMO_ITER (v1, &oiter, bmop.slots_out, "vert.out", BM_VERT) {
        BM_vert_select_set(vc.em->bm, v1, true);
      }

      if (!EDBM_op_finish(vc.em, &bmop, op, true)) {
        continue;
      }
    }

    if (use_proj) {
      EDBM_project_snap_verts(C, vc.ar, vc.em);
    }

    /* This normally happens when pushing undo but modal operators
     * like this one don't push undo data until after modal mode is
     * done. */
    EDBM_mesh_normals_update(vc.em);

    EDBM_update_generic(vc.em, true, true);

    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

void MESH_OT_dupli_extrude_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate or Extrude to Cursor";
  ot->idname = "MESH_OT_dupli_extrude_cursor";
  ot->description =
      "Duplicate and extrude selected vertices, edges or faces towards the mouse cursor";

  /* api callbacks */
  ot->invoke = edbm_dupli_extrude_cursor_invoke;
  ot->poll = ED_operator_editmesh_region_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "rotate_source",
                  true,
                  "Rotate Source",
                  "Rotate initial selection giving better shape");
}

/** \} */
