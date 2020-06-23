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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spview3d
 */

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transverts.h"

#include "view3d_intern.h"

static bool snap_curs_to_sel_ex(bContext *C, float cursor[3]);
static bool snap_calc_active_center(bContext *C, const bool select_only, float r_center[3]);

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Grid Operator
 * \{ */

/** Snaps every individual object center to its nearest point on the grid. */
static int snap_sel_to_grid_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  Object *obact = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  RegionView3D *rv3d = CTX_wm_region_data(C);
  View3D *v3d = CTX_wm_view3d(C);
  TransVertStore tvs = {NULL};
  TransVert *tv;
  float gridf, imat[3][3], bmat[3][3], vec[3];
  int a;

  gridf = ED_view3d_grid_view_scale(scene, v3d, rv3d, NULL);

  if (OBEDIT_FROM_OBACT(obact)) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    uint objects_len = 0;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        view_layer, CTX_wm_view3d(C), &objects_len);
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *obedit = objects[ob_index];

      if (obedit->type == OB_MESH) {
        BMEditMesh *em = BKE_editmesh_from_object(obedit);

        if (em->bm->totvertsel == 0) {
          continue;
        }
      }

      if (ED_transverts_check_obedit(obedit)) {
        ED_transverts_create_from_obedit(&tvs, obedit, 0);
      }

      if (tvs.transverts_tot != 0) {
        copy_m3_m4(bmat, obedit->obmat);
        invert_m3_m3(imat, bmat);

        tv = tvs.transverts;
        for (a = 0; a < tvs.transverts_tot; a++, tv++) {
          copy_v3_v3(vec, tv->loc);
          mul_m3_v3(bmat, vec);
          add_v3_v3(vec, obedit->obmat[3]);
          vec[0] = gridf * floorf(0.5f + vec[0] / gridf);
          vec[1] = gridf * floorf(0.5f + vec[1] / gridf);
          vec[2] = gridf * floorf(0.5f + vec[2] / gridf);
          sub_v3_v3(vec, obedit->obmat[3]);

          mul_m3_v3(imat, vec);
          copy_v3_v3(tv->loc, vec);
        }
        ED_transverts_update_obedit(&tvs, obedit);
      }
      ED_transverts_free(&tvs);
    }
    MEM_freeN(objects);
  }
  else if (OBPOSE_FROM_OBACT(obact)) {
    struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
    uint objects_len = 0;
    Object **objects_eval = BKE_object_pose_array_get(view_layer_eval, v3d, &objects_len);
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *ob_eval = objects_eval[ob_index];
      Object *ob = DEG_get_original_object(ob_eval);
      bPoseChannel *pchan_eval;
      bArmature *arm_eval = ob_eval->data;

      invert_m4_m4(ob_eval->imat, ob_eval->obmat);

      for (pchan_eval = ob_eval->pose->chanbase.first; pchan_eval; pchan_eval = pchan_eval->next) {
        if (pchan_eval->bone->flag & BONE_SELECTED) {
          if (pchan_eval->bone->layer & arm_eval->layer) {
            if ((pchan_eval->bone->flag & BONE_CONNECTED) == 0) {
              float nLoc[3];

              /* get nearest grid point to snap to */
              copy_v3_v3(nLoc, pchan_eval->pose_mat[3]);
              /* We must operate in world space! */
              mul_m4_v3(ob_eval->obmat, nLoc);
              vec[0] = gridf * floorf(0.5f + nLoc[0] / gridf);
              vec[1] = gridf * floorf(0.5f + nLoc[1] / gridf);
              vec[2] = gridf * floorf(0.5f + nLoc[2] / gridf);
              /* Back in object space... */
              mul_m4_v3(ob_eval->imat, vec);

              /* Get location of grid point in pose space. */
              BKE_armature_loc_pose_to_bone(pchan_eval, vec, vec);

              /* adjust location on the original pchan*/
              bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, pchan_eval->name);
              if ((pchan->protectflag & OB_LOCK_LOCX) == 0) {
                pchan->loc[0] = vec[0];
              }
              if ((pchan->protectflag & OB_LOCK_LOCY) == 0) {
                pchan->loc[1] = vec[1];
              }
              if ((pchan->protectflag & OB_LOCK_LOCZ) == 0) {
                pchan->loc[2] = vec[2];
              }

              /* auto-keyframing */
              ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
            }
            /* if the bone has a parent and is connected to the parent,
             * don't do anything - will break chain unless we do auto-ik.
             */
          }
        }
      }
      ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
    MEM_freeN(objects_eval);
  }
  else {
    /* Object mode. */
    Main *bmain = CTX_data_main(C);

    struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);

    const bool use_transform_skip_children = (scene->toolsettings->transform_flag &
                                              SCE_XFORM_SKIP_CHILDREN);
    const bool use_transform_data_origin = (scene->toolsettings->transform_flag &
                                            SCE_XFORM_DATA_ORIGIN);
    struct XFormObjectSkipChild_Container *xcs = NULL;
    struct XFormObjectData_Container *xds = NULL;

    /* Build object array. */
    Object **objects_eval = NULL;
    uint objects_eval_len;
    {
      BLI_array_declare(objects_eval);
      FOREACH_SELECTED_EDITABLE_OBJECT_BEGIN (view_layer_eval, v3d, ob_eval) {
        BLI_array_append(objects_eval, ob_eval);
      }
      FOREACH_SELECTED_EDITABLE_OBJECT_END;
      objects_eval_len = BLI_array_len(objects_eval);
    }

    if (use_transform_skip_children) {
      ViewLayer *view_layer = CTX_data_view_layer(C);

      Object **objects = MEM_malloc_arrayN(objects_eval_len, sizeof(*objects), __func__);

      for (int ob_index = 0; ob_index < objects_eval_len; ob_index++) {
        Object *ob_eval = objects_eval[ob_index];
        objects[ob_index] = DEG_get_original_object(ob_eval);
      }
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
      xcs = ED_object_xform_skip_child_container_create();
      ED_object_xform_skip_child_container_item_ensure_from_array(
          xcs, view_layer, objects, objects_eval_len);
      MEM_freeN(objects);
    }
    if (use_transform_data_origin) {
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
      xds = ED_object_data_xform_container_create();
    }

    for (int ob_index = 0; ob_index < objects_eval_len; ob_index++) {
      Object *ob_eval = objects_eval[ob_index];
      Object *ob = DEG_get_original_object(ob_eval);
      vec[0] = -ob_eval->obmat[3][0] + gridf * floorf(0.5f + ob_eval->obmat[3][0] / gridf);
      vec[1] = -ob_eval->obmat[3][1] + gridf * floorf(0.5f + ob_eval->obmat[3][1] / gridf);
      vec[2] = -ob_eval->obmat[3][2] + gridf * floorf(0.5f + ob_eval->obmat[3][2] / gridf);

      if (ob->parent) {
        float originmat[3][3];
        BKE_object_where_is_calc_ex(depsgraph, scene, NULL, ob, originmat);

        invert_m3_m3(imat, originmat);
        mul_m3_v3(imat, vec);
      }
      if ((ob->protectflag & OB_LOCK_LOCX) == 0) {
        ob->loc[0] = ob_eval->loc[0] + vec[0];
      }
      if ((ob->protectflag & OB_LOCK_LOCY) == 0) {
        ob->loc[1] = ob_eval->loc[1] + vec[1];
      }
      if ((ob->protectflag & OB_LOCK_LOCZ) == 0) {
        ob->loc[2] = ob_eval->loc[2] + vec[2];
      }

      /* auto-keyframing */
      ED_autokeyframe_object(C, scene, ob, ks);

      if (use_transform_data_origin) {
        ED_object_data_xform_container_item_ensure(xds, ob);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }

    if (objects_eval) {
      MEM_freeN(objects_eval);
    }

    if (use_transform_skip_children) {
      ED_object_xform_skip_child_container_update_all(xcs, bmain, depsgraph);
      ED_object_xform_skip_child_container_destroy(xcs);
    }
    if (use_transform_data_origin) {
      ED_object_data_xform_container_update_all(xds, bmain, depsgraph);
      ED_object_data_xform_container_destroy(xds);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_selected_to_grid(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Grid";
  ot->description = "Snap selected item(s) to their nearest grid division";
  ot->idname = "VIEW3D_OT_snap_selected_to_grid";

  /* api callbacks */
  ot->exec = snap_sel_to_grid_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Location (Utility)
 * \{ */

/**
 * Snaps the selection as a whole (use_offset=true) or each selected object to the given location.
 *
 * \param snap_target_global: a location in global space to snap to
 * (eg. 3D cursor or active object).
 * \param use_offset: if the selected objects should maintain their relative offsets
 * and be snapped by the selection pivot point (median, active),
 * or if every object origin should be snapped to the given location.
 */
static int snap_selected_to_location(bContext *C,
                                     const float snap_target_global[3],
                                     const bool use_offset)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  Object *obact = CTX_data_active_object(C);
  View3D *v3d = CTX_wm_view3d(C);
  TransVertStore tvs = {NULL};
  TransVert *tv;
  float imat[3][3], bmat[3][3];
  float center_global[3];
  float offset_global[3];
  int a;

  if (use_offset) {
    if ((v3d && scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) &&
        snap_calc_active_center(C, true, center_global)) {
      /* pass */
    }
    else {
      snap_curs_to_sel_ex(C, center_global);
    }
    sub_v3_v3v3(offset_global, snap_target_global, center_global);
  }

  if (obedit) {
    float snap_target_local[3];
    ViewLayer *view_layer = CTX_data_view_layer(C);
    uint objects_len = 0;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        view_layer, CTX_wm_view3d(C), &objects_len);
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      obedit = objects[ob_index];

      if (obedit->type == OB_MESH) {
        BMEditMesh *em = BKE_editmesh_from_object(obedit);

        if (em->bm->totvertsel == 0) {
          continue;
        }
      }

      if (ED_transverts_check_obedit(obedit)) {
        ED_transverts_create_from_obedit(&tvs, obedit, 0);
      }

      if (tvs.transverts_tot != 0) {
        copy_m3_m4(bmat, obedit->obmat);
        invert_m3_m3(imat, bmat);

        /* get the cursor in object space */
        sub_v3_v3v3(snap_target_local, snap_target_global, obedit->obmat[3]);
        mul_m3_v3(imat, snap_target_local);

        if (use_offset) {
          float offset_local[3];

          mul_v3_m3v3(offset_local, imat, offset_global);

          tv = tvs.transverts;
          for (a = 0; a < tvs.transverts_tot; a++, tv++) {
            add_v3_v3(tv->loc, offset_local);
          }
        }
        else {
          tv = tvs.transverts;
          for (a = 0; a < tvs.transverts_tot; a++, tv++) {
            copy_v3_v3(tv->loc, snap_target_local);
          }
        }
        ED_transverts_update_obedit(&tvs, obedit);
      }
      ED_transverts_free(&tvs);
    }
    MEM_freeN(objects);
  }
  else if (OBPOSE_FROM_OBACT(obact)) {
    struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    uint objects_len = 0;
    Object **objects = BKE_object_pose_array_get(view_layer, v3d, &objects_len);

    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *ob = objects[ob_index];
      bPoseChannel *pchan;
      bArmature *arm = ob->data;
      float snap_target_local[3];

      invert_m4_m4(ob->imat, ob->obmat);
      mul_v3_m4v3(snap_target_local, ob->imat, snap_target_global);

      for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
        if ((pchan->bone->flag & BONE_SELECTED) && (PBONE_VISIBLE(arm, pchan->bone)) &&
            /* if the bone has a parent and is connected to the parent,
             * don't do anything - will break chain unless we do auto-ik.
             */
            (pchan->bone->flag & BONE_CONNECTED) == 0) {
          pchan->bone->flag |= BONE_TRANSFORM;
        }
        else {
          pchan->bone->flag &= ~BONE_TRANSFORM;
        }
      }

      for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
        if ((pchan->bone->flag & BONE_TRANSFORM) &&
            /* check that our parents not transformed (if we have one) */
            ((pchan->bone->parent &&
              BKE_armature_bone_flag_test_recursive(pchan->bone->parent, BONE_TRANSFORM)) == 0)) {
          /* Get position in pchan (pose) space. */
          float cursor_pose[3];

          if (use_offset) {
            mul_v3_m4v3(cursor_pose, ob->obmat, pchan->pose_mat[3]);
            add_v3_v3(cursor_pose, offset_global);

            mul_m4_v3(ob->imat, cursor_pose);
            BKE_armature_loc_pose_to_bone(pchan, cursor_pose, cursor_pose);
          }
          else {
            BKE_armature_loc_pose_to_bone(pchan, snap_target_local, cursor_pose);
          }

          /* copy new position */
          if ((pchan->protectflag & OB_LOCK_LOCX) == 0) {
            pchan->loc[0] = cursor_pose[0];
          }
          if ((pchan->protectflag & OB_LOCK_LOCY) == 0) {
            pchan->loc[1] = cursor_pose[1];
          }
          if ((pchan->protectflag & OB_LOCK_LOCZ) == 0) {
            pchan->loc[2] = cursor_pose[2];
          }

          /* auto-keyframing */
          ED_autokeyframe_pchan(C, scene, ob, pchan, ks);
        }
      }

      for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
        pchan->bone->flag &= ~BONE_TRANSFORM;
      }

      ob->pose->flag |= (POSE_LOCKED | POSE_DO_UNLOCK);

      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
    MEM_freeN(objects);
  }
  else {
    struct KeyingSet *ks = ANIM_get_keyingset_for_autokeying(scene, ANIM_KS_LOCATION_ID);
    Main *bmain = CTX_data_main(C);
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

    /* Reset flags. */
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      ob->flag &= ~OB_DONE;
    }

    /* Build object array, tag objects we're transforming. */
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Object **objects = NULL;
    uint objects_len;
    {
      BLI_array_declare(objects);
      FOREACH_SELECTED_EDITABLE_OBJECT_BEGIN (view_layer, v3d, ob) {
        BLI_array_append(objects, ob);
        ob->flag |= OB_DONE;
      }
      FOREACH_SELECTED_EDITABLE_OBJECT_END;
      objects_len = BLI_array_len(objects);
    }

    const bool use_transform_skip_children = (scene->toolsettings->transform_flag &
                                              SCE_XFORM_SKIP_CHILDREN);
    const bool use_transform_data_origin = (scene->toolsettings->transform_flag &
                                            SCE_XFORM_DATA_ORIGIN);
    struct XFormObjectSkipChild_Container *xcs = NULL;
    struct XFormObjectData_Container *xds = NULL;

    if (use_transform_skip_children) {
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
      xcs = ED_object_xform_skip_child_container_create();
      ED_object_xform_skip_child_container_item_ensure_from_array(
          xcs, view_layer, objects, objects_len);
    }
    if (use_transform_data_origin) {
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
      xds = ED_object_data_xform_container_create();

      /* Initialize the transform data in a separate loop because the depsgraph
       * may be evaluated while setting the locations. */
      for (int ob_index = 0; ob_index < objects_len; ob_index++) {
        Object *ob = objects[ob_index];
        ED_object_data_xform_container_item_ensure(xds, ob);
      }
    }

    for (int ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *ob = objects[ob_index];

      if ((ob->parent && BKE_object_flag_test_recursive(ob->parent, OB_DONE)) == 0) {

        float cursor_parent[3]; /* parent-relative */

        if (use_offset) {
          add_v3_v3v3(cursor_parent, ob->obmat[3], offset_global);
        }
        else {
          copy_v3_v3(cursor_parent, snap_target_global);
        }

        sub_v3_v3(cursor_parent, ob->obmat[3]);

        if (ob->parent) {
          float originmat[3][3], parentmat[4][4];
          /* Use the evaluated object here because sometimes
           * `ob->parent->runtime.curve_cache` is required. */
          BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
          Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

          BKE_object_get_parent_matrix(ob_eval, ob_eval->parent, parentmat);
          mul_m3_m4m4(originmat, parentmat, ob->parentinv);
          invert_m3_m3(imat, originmat);
          mul_m3_v3(imat, cursor_parent);
        }
        if ((ob->protectflag & OB_LOCK_LOCX) == 0) {
          ob->loc[0] += cursor_parent[0];
        }
        if ((ob->protectflag & OB_LOCK_LOCY) == 0) {
          ob->loc[1] += cursor_parent[1];
        }
        if ((ob->protectflag & OB_LOCK_LOCZ) == 0) {
          ob->loc[2] += cursor_parent[2];
        }

        /* auto-keyframing */
        ED_autokeyframe_object(C, scene, ob, ks);

        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
      }
    }

    if (objects) {
      MEM_freeN(objects);
    }

    if (use_transform_skip_children) {
      ED_object_xform_skip_child_container_update_all(xcs, bmain, depsgraph);
      ED_object_xform_skip_child_container_destroy(xcs);
    }
    if (use_transform_data_origin) {
      ED_object_data_xform_container_update_all(xds, bmain, depsgraph);
      ED_object_data_xform_container_destroy(xds);
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Cursor Operator
 * \{ */

static int snap_selected_to_cursor_exec(bContext *C, wmOperator *op)
{
  const bool use_offset = RNA_boolean_get(op->ptr, "use_offset");

  Scene *scene = CTX_data_scene(C);

  const float *snap_target_global = scene->cursor.location;

  return snap_selected_to_location(C, snap_target_global, use_offset);
}

void VIEW3D_OT_snap_selected_to_cursor(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Cursor";
  ot->description = "Snap selected item(s) to the 3D cursor";
  ot->idname = "VIEW3D_OT_snap_selected_to_cursor";

  /* api callbacks */
  ot->exec = snap_selected_to_cursor_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  RNA_def_boolean(ot->srna,
                  "use_offset",
                  1,
                  "Offset",
                  "If the selection should be snapped as a whole or by each object center");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection to Active Operator
 * \{ */

/** Snaps each selected object to the location of the active selected object. */
static int snap_selected_to_active_exec(bContext *C, wmOperator *op)
{
  float snap_target_global[3];

  if (snap_calc_active_center(C, false, snap_target_global) == false) {
    BKE_report(op->reports, RPT_ERROR, "No active element found!");
    return OPERATOR_CANCELLED;
  }

  return snap_selected_to_location(C, snap_target_global, false);
}

void VIEW3D_OT_snap_selected_to_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Selection to Active";
  ot->description = "Snap selected item(s) to the active item";
  ot->idname = "VIEW3D_OT_snap_selected_to_active";

  /* api callbacks */
  ot->exec = snap_selected_to_active_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Grid Operator
 * \{ */

/** Snaps the 3D cursor location to its nearest point on the grid. */
static int snap_curs_to_grid_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  RegionView3D *rv3d = CTX_wm_region_data(C);
  View3D *v3d = CTX_wm_view3d(C);
  float gridf, *curs;

  gridf = ED_view3d_grid_view_scale(scene, v3d, rv3d, NULL);
  curs = scene->cursor.location;

  curs[0] = gridf * floorf(0.5f + curs[0] / gridf);
  curs[1] = gridf * floorf(0.5f + curs[1] / gridf);
  curs[2] = gridf * floorf(0.5f + curs[2] / gridf);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d); /* hrm */
  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_grid(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Grid";
  ot->description = "Snap 3D cursor to the nearest grid division";
  ot->idname = "VIEW3D_OT_snap_cursor_to_grid";

  /* api callbacks */
  ot->exec = snap_curs_to_grid_exec;
  ot->poll = ED_operator_region_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Selection Operator
 * \{ */

/**
 * Returns the center position of a tracking marker visible on the viewport
 * (useful to snap to).
 */
static void bundle_midpoint(Scene *scene, Object *ob, float r_vec[3])
{
  MovieClip *clip = BKE_object_movieclip_get(scene, ob, false);
  MovieTracking *tracking;
  MovieTrackingObject *object;
  bool ok = false;
  float min[3], max[3], mat[4][4], pos[3], cammat[4][4];

  if (!clip) {
    return;
  }

  tracking = &clip->tracking;

  copy_m4_m4(cammat, ob->obmat);

  BKE_tracking_get_camera_object_matrix(ob, mat);

  INIT_MINMAX(min, max);

  for (object = tracking->objects.first; object; object = object->next) {
    ListBase *tracksbase = BKE_tracking_object_get_tracks(tracking, object);
    MovieTrackingTrack *track = tracksbase->first;
    float obmat[4][4];

    if (object->flag & TRACKING_OBJECT_CAMERA) {
      copy_m4_m4(obmat, mat);
    }
    else {
      float imat[4][4];

      BKE_tracking_camera_get_reconstructed_interpolate(tracking, object, scene->r.cfra, imat);
      invert_m4(imat);

      mul_m4_m4m4(obmat, cammat, imat);
    }

    while (track) {
      if ((track->flag & TRACK_HAS_BUNDLE) && TRACK_SELECTED(track)) {
        ok = 1;
        mul_v3_m4v3(pos, obmat, track->bundle_pos);
        minmax_v3v3_v3(min, max, pos);
      }

      track = track->next;
    }
  }

  if (ok) {
    mid_v3_v3v3(r_vec, min, max);
  }
}

/** Snaps the 3D cursor location to the median point of the selection. */
static bool snap_curs_to_sel_ex(bContext *C, float cursor[3])
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer_eval = DEG_get_evaluated_view_layer(depsgraph);
  Object *obedit = CTX_data_edit_object(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  TransVertStore tvs = {NULL};
  TransVert *tv;
  float bmat[3][3], vec[3], min[3], max[3], centroid[3];
  int count = 0;

  INIT_MINMAX(min, max);
  zero_v3(centroid);

  if (obedit) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    uint objects_len = 0;
    Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
        view_layer, CTX_wm_view3d(C), &objects_len);
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      obedit = objects[ob_index];

      /* We can do that quick check for meshes only... */
      if (obedit->type == OB_MESH) {
        BMEditMesh *em = BKE_editmesh_from_object(obedit);

        if (em->bm->totvertsel == 0) {
          continue;
        }
      }

      if (ED_transverts_check_obedit(obedit)) {
        ED_transverts_create_from_obedit(&tvs, obedit, TM_ALL_JOINTS | TM_SKIP_HANDLES);
      }

      count += tvs.transverts_tot;
      if (tvs.transverts_tot != 0) {
        Object *obedit_eval = DEG_get_evaluated_object(depsgraph, obedit);
        copy_m3_m4(bmat, obedit_eval->obmat);

        tv = tvs.transverts;
        for (int i = 0; i < tvs.transverts_tot; i++, tv++) {
          copy_v3_v3(vec, tv->loc);
          mul_m3_v3(bmat, vec);
          add_v3_v3(vec, obedit_eval->obmat[3]);
          add_v3_v3(centroid, vec);
          minmax_v3v3_v3(min, max, vec);
        }
      }
      ED_transverts_free(&tvs);
    }
    MEM_freeN(objects);
  }
  else {
    Object *obact = CTX_data_active_object(C);

    if (obact && (obact->mode & OB_MODE_POSE)) {
      Object *obact_eval = DEG_get_evaluated_object(depsgraph, obact);
      bArmature *arm = obact_eval->data;
      bPoseChannel *pchan;
      for (pchan = obact_eval->pose->chanbase.first; pchan; pchan = pchan->next) {
        if (arm->layer & pchan->bone->layer) {
          if (pchan->bone->flag & BONE_SELECTED) {
            copy_v3_v3(vec, pchan->pose_head);
            mul_m4_v3(obact_eval->obmat, vec);
            add_v3_v3(centroid, vec);
            minmax_v3v3_v3(min, max, vec);
            count++;
          }
        }
      }
    }
    else {
      FOREACH_SELECTED_OBJECT_BEGIN (view_layer_eval, v3d, ob_eval) {
        copy_v3_v3(vec, ob_eval->obmat[3]);

        /* special case for camera -- snap to bundles */
        if (ob_eval->type == OB_CAMERA) {
          /* snap to bundles should happen only when bundles are visible */
          if (v3d->flag2 & V3D_SHOW_RECONSTRUCTION) {
            bundle_midpoint(scene, DEG_get_original_object(ob_eval), vec);
          }
        }

        add_v3_v3(centroid, vec);
        minmax_v3v3_v3(min, max, vec);
        count++;
      }
      FOREACH_SELECTED_OBJECT_END;
    }
  }

  if (count == 0) {
    return false;
  }

  if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CENTER_BOUNDS) {
    mid_v3_v3v3(cursor, min, max);
  }
  else {
    mul_v3_fl(centroid, 1.0f / (float)count);
    copy_v3_v3(cursor, centroid);
  }
  return true;
}

static int snap_curs_to_sel_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  if (snap_curs_to_sel_ex(C, scene->cursor.location)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void VIEW3D_OT_snap_cursor_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Selected";
  ot->description = "Snap 3D cursor to the middle of the selected item(s)";
  ot->idname = "VIEW3D_OT_snap_cursor_to_selected";

  /* api callbacks */
  ot->exec = snap_curs_to_sel_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Active Operator
 * \{ */

/**
 * Calculates the center position of the active object in global space.
 *
 * Note: this could be exported to be a generic function.
 * see: #calculateCenterActive
 */
static bool snap_calc_active_center(bContext *C, const bool select_only, float r_center[3])
{
  Object *ob = CTX_data_active_object(C);
  if (ob == NULL) {
    return false;
  }
  return ED_object_calc_active_center(ob, select_only, r_center);
}

static int snap_curs_to_active_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  if (snap_calc_active_center(C, false, scene->cursor.location)) {
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void VIEW3D_OT_snap_cursor_to_active(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to Active";
  ot->description = "Snap 3D cursor to the active item";
  ot->idname = "VIEW3D_OT_snap_cursor_to_active";

  /* api callbacks */
  ot->exec = snap_curs_to_active_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor to Center Operator
 * \{ */

/** Snaps the 3D cursor location to the origin and clears cursor rotation. */
static int snap_curs_to_center_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  float mat3[3][3];
  unit_m3(mat3);

  zero_v3(scene->cursor.location);
  BKE_scene_cursor_mat3_to_rot(&scene->cursor, mat3, false);

  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  return OPERATOR_FINISHED;
}

void VIEW3D_OT_snap_cursor_to_center(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Cursor to World Origin";
  ot->description = "Snap 3D cursor to the world origin";
  ot->idname = "VIEW3D_OT_snap_cursor_to_center";

  /* api callbacks */
  ot->exec = snap_curs_to_center_exec;
  ot->poll = ED_operator_view3d_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Min/Max Object Vertices Utility
 * \{ */

/**
 * Calculates the bounding box corners (min and max) for \a obedit.
 * The returned values are in global space.
 */
bool ED_view3d_minmax_verts(Object *obedit, float r_min[3], float r_max[3])
{
  TransVertStore tvs = {NULL};
  TransVert *tv;
  float centroid[3], vec[3], bmat[3][3];

  /* Metaballs are an exception. */
  if (obedit->type == OB_MBALL) {
    float ob_min[3], ob_max[3];
    bool changed;

    changed = BKE_mball_minmax_ex(obedit->data, ob_min, ob_max, obedit->obmat, SELECT);
    if (changed) {
      minmax_v3v3_v3(r_min, r_max, ob_min);
      minmax_v3v3_v3(r_min, r_max, ob_max);
    }
    return changed;
  }

  if (ED_transverts_check_obedit(obedit)) {
    ED_transverts_create_from_obedit(&tvs, obedit, TM_ALL_JOINTS);
  }

  if (tvs.transverts_tot == 0) {
    return false;
  }

  copy_m3_m4(bmat, obedit->obmat);

  tv = tvs.transverts;
  for (int a = 0; a < tvs.transverts_tot; a++, tv++) {
    copy_v3_v3(vec, (tv->flag & TX_VERT_USE_MAPLOC) ? tv->maploc : tv->loc);
    mul_m3_v3(bmat, vec);
    add_v3_v3(vec, obedit->obmat[3]);
    add_v3_v3(centroid, vec);
    minmax_v3v3_v3(r_min, r_max, vec);
  }

  ED_transverts_free(&tvs);

  return true;
}

/** \} */
