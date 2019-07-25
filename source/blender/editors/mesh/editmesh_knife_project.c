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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 */

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"

#include "BKE_mesh.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_editmesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "mesh_intern.h" /* own include */

static LinkNode *knifeproject_poly_from_object(const bContext *C,
                                               Scene *scene,
                                               Object *ob,
                                               LinkNode *polys)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ARegion *ar = CTX_wm_region(C);
  struct Mesh *me_eval;
  bool me_eval_needs_free;

  if (ob->type == OB_MESH || ob->runtime.mesh_eval) {
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    me_eval = ob_eval->runtime.mesh_eval;
    if (me_eval == NULL) {
      Scene *scene_eval = (Scene *)DEG_get_evaluated_id(depsgraph, &scene->id);
      me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &CD_MASK_BAREMESH);
    }
    me_eval_needs_free = false;
  }
  else if (ELEM(ob->type, OB_FONT, OB_CURVE, OB_SURF)) {
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    me_eval = BKE_mesh_new_nomain_from_curve(ob_eval);
    me_eval_needs_free = true;
  }
  else {
    me_eval = NULL;
  }

  if (me_eval) {
    ListBase nurbslist = {NULL, NULL};
    float projmat[4][4];

    BKE_mesh_to_curve_nurblist(me_eval, &nurbslist, 0); /* wire */
    BKE_mesh_to_curve_nurblist(me_eval, &nurbslist, 1); /* boundary */

    ED_view3d_ob_project_mat_get(ar->regiondata, ob, projmat);

    if (nurbslist.first) {
      Nurb *nu;
      for (nu = nurbslist.first; nu; nu = nu->next) {
        if (nu->bp) {
          int a;
          BPoint *bp;
          bool is_cyclic = (nu->flagu & CU_NURB_CYCLIC) != 0;
          float(*mval)[2] = MEM_mallocN(sizeof(*mval) * (nu->pntsu + is_cyclic), __func__);

          for (bp = nu->bp, a = 0; a < nu->pntsu; a++, bp++) {
            ED_view3d_project_float_v2_m4(ar, bp->vec, mval[a], projmat);
          }
          if (is_cyclic) {
            copy_v2_v2(mval[a], mval[0]);
          }

          BLI_linklist_prepend(&polys, mval);
        }
      }
    }

    BKE_nurbList_free(&nurbslist);

    if (me_eval_needs_free) {
      BKE_mesh_free(me_eval);
    }
  }

  return polys;
}

static int knifeproject_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool cut_through = RNA_boolean_get(op->ptr, "cut_through");

  LinkNode *polys = NULL;

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob != obedit) {
      polys = knifeproject_poly_from_object(C, scene, ob, polys);
    }
  }
  CTX_DATA_END;

  if (polys) {
    EDBM_mesh_knife(C, polys, true, cut_through);

    /* select only tagged faces */
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

    /* not essential, but switch out of vertex mode since the
     * selected regions wont be nicely isolated after flushing.
     * note: call after de-select to avoid selection flushing */
    EDBM_selectmode_disable(scene, em, SCE_SELECT_VERTEX, SCE_SELECT_EDGE);

    BM_mesh_elem_hflag_enable_test(em->bm, BM_FACE, BM_ELEM_SELECT, true, false, BM_ELEM_TAG);

    BM_mesh_select_mode_flush(em->bm);

    BLI_linklist_freeN(polys);

    return OPERATOR_FINISHED;
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No other selected objects found to use for projection");
    return OPERATOR_CANCELLED;
  }
}

void MESH_OT_knife_project(wmOperatorType *ot)
{
  /* description */
  ot->name = "Knife Project";
  ot->idname = "MESH_OT_knife_project";
  ot->description = "Use other objects outlines & boundaries to project knife cuts";

  /* callbacks */
  ot->exec = knifeproject_exec;
  ot->poll = ED_operator_editmesh_region_view3d;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* parameters */
  RNA_def_boolean(ot->srna,
                  "cut_through",
                  false,
                  "Cut through",
                  "Cut through all faces, not just visible ones");
}
