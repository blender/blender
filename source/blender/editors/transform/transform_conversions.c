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
 * \ingroup edtransform
 */

#include <string.h>
#include <math.h>
#include <limits.h>

#include "DNA_anim_types.h"
#include "DNA_brush_types.h"
#include "DNA_armature_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_view3d_types.h"
#include "DNA_constraint_types.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_mask_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_linklist_stack.h"
#include "BLI_string.h"
#include "BLI_bitmap.h"
#include "BLI_rect.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_curve.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_layer.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_nla.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_paint.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_editmesh.h"
#include "BKE_tracking.h"
#include "BKE_mask.h"
#include "BKE_colortools.h"

#include "BIK_api.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_object.h"
#include "ED_markers.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_uvedit.h"
#include "ED_clip.h"
#include "ED_mask.h"
#include "ED_gpencil.h"

#include "WM_api.h" /* for WM_event_add_notifier to deal with stabilization nodes */
#include "WM_types.h"

#include "UI_view2d.h"
#include "UI_interface.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "transform.h"
#include "bmesh.h"

/**
 * Transforming around ourselves is no use, fallback to individual origins,
 * useful for curve/armatures.
 */
static void transform_around_single_fallback(TransInfo *t)
{
  if ((t->data_len_all == 1) &&
      (ELEM(t->around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN, V3D_AROUND_ACTIVE)) &&
      (ELEM(t->mode, TFM_RESIZE, TFM_ROTATION, TFM_TRACKBALL))) {
    t->around = V3D_AROUND_LOCAL_ORIGINS;
  }
}

/* when transforming islands */
struct TransIslandData {
  float co[3];
  float axismtx[3][3];
};

/* local function prototype - for Object/Bone Constraints */
static bool constraints_list_needinv(TransInfo *t, ListBase *list);

/* ************************** Functions *************************** */

static int trans_data_compare_dist(const void *a, const void *b)
{
  const TransData *td_a = (const TransData *)a;
  const TransData *td_b = (const TransData *)b;

  if (td_a->dist < td_b->dist) {
    return -1;
  }
  else if (td_a->dist > td_b->dist) {
    return 1;
  }
  else {
    return 0;
  }
}

static int trans_data_compare_rdist(const void *a, const void *b)
{
  const TransData *td_a = (const TransData *)a;
  const TransData *td_b = (const TransData *)b;

  if (td_a->rdist < td_b->rdist) {
    return -1;
  }
  else if (td_a->rdist > td_b->rdist) {
    return 1;
  }
  else {
    return 0;
  }
}

static void sort_trans_data_dist_container(const TransInfo *t, TransDataContainer *tc)
{
  TransData *start = tc->data;
  int i;

  for (i = 0; i < tc->data_len && start->flag & TD_SELECTED; i++) {
    start++;
  }

  if (i < tc->data_len) {
    if (t->flag & T_PROP_CONNECTED) {
      qsort(start, tc->data_len - i, sizeof(TransData), trans_data_compare_dist);
    }
    else {
      qsort(start, tc->data_len - i, sizeof(TransData), trans_data_compare_rdist);
    }
  }
}
void sort_trans_data_dist(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sort_trans_data_dist_container(t, tc);
  }
}

/**
 * Make #TD_SELECTED first in the array.
 */
static void sort_trans_data_selected_first_container(TransDataContainer *tc)
{
  TransData *sel, *unsel;
  TransData temp;
  unsel = tc->data;
  sel = tc->data;
  sel += tc->data_len - 1;
  while (sel > unsel) {
    while (unsel->flag & TD_SELECTED) {
      unsel++;
      if (unsel == sel) {
        return;
      }
    }
    while (!(sel->flag & TD_SELECTED)) {
      sel--;
      if (unsel == sel) {
        return;
      }
    }
    temp = *unsel;
    *unsel = *sel;
    *sel = temp;
    sel--;
    unsel++;
  }
}
static void sort_trans_data_selected_first(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sort_trans_data_selected_first_container(tc);
  }
}

/* distance calculated from not-selected vertex to nearest selected vertex
 * warning; this is loops inside loop, has minor N^2 issues, but by sorting list it is OK */
static void set_prop_dist(TransInfo *t, const bool with_dist)
{
  int a;

  float _proj_vec[3];
  const float *proj_vec = NULL;

  /* support for face-islands */
  const bool use_island = transdata_check_local_islands(t, t->around);

  if (t->flag & T_PROP_PROJECTED) {
    if (t->spacetype == SPACE_VIEW3D && t->ar && t->ar->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = t->ar->regiondata;
      normalize_v3_v3(_proj_vec, rv3d->viewinv[2]);
      proj_vec = _proj_vec;
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *tob = tc->data;
    for (a = 0; a < tc->data_len; a++, tob++) {

      tob->rdist = 0.0f;  // init, it was mallocced

      if ((tob->flag & TD_SELECTED) == 0) {
        TransData *td;
        int i;
        float dist_sq, vec[3];

        tob->rdist = -1.0f;  // signal for next loop

        for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
          if (td->flag & TD_SELECTED) {
            if (use_island) {
              sub_v3_v3v3(vec, tob->iloc, td->iloc);
            }
            else {
              sub_v3_v3v3(vec, tob->center, td->center);
            }
            mul_m3_v3(tob->mtx, vec);

            if (proj_vec) {
              float vec_p[3];
              project_v3_v3v3(vec_p, vec, proj_vec);
              sub_v3_v3(vec, vec_p);
            }

            dist_sq = len_squared_v3(vec);
            if ((tob->rdist == -1.0f) || (dist_sq < SQUARE(tob->rdist))) {
              tob->rdist = sqrtf(dist_sq);
              if (use_island) {
                copy_v3_v3(tob->center, td->center);
                copy_m3_m3(tob->axismtx, td->axismtx);
              }
            }
          }
          else {
            break; /* by definition transdata has selected items in beginning */
          }
        }
        if (with_dist) {
          tob->dist = tob->rdist;
        }
      }
    }
  }
}

/* ************************** CONVERSIONS ************************* */

/* ********************* texture space ********* */

static void createTransTexspace(TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  TransData *td;
  Object *ob;
  ID *id;
  short *texflag;

  ob = OBACT(view_layer);

  if (ob == NULL) {  // Shouldn't logically happen, but still...
    return;
  }

  id = ob->data;
  if (id == NULL || !ELEM(GS(id->name), ID_ME, ID_CU, ID_MB)) {
    BKE_report(t->reports, RPT_ERROR, "Unsupported object type for text-space transform");
    return;
  }

  if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(t->reports, RPT_ERROR, "Linked data can't text-space transform");
    return;
  }

  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransTexspace");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, ob->obmat[3]);
  td->ob = ob;

  copy_m3_m4(td->mtx, ob->obmat);
  copy_m3_m4(td->axismtx, ob->obmat);
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  if (BKE_object_obdata_texspace_get(ob, &texflag, &td->loc, &td->ext->size, &td->ext->rot)) {
    ob->dtx |= OB_TEXSPACE;
    *texflag &= ~ME_AUTOSPACE;
  }

  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->ext->irot, td->ext->rot);
  copy_v3_v3(td->ext->isize, td->ext->size);
}

/* -------------------------------------------------------------------- */
/** \name Cursor Transform Creation
 *
 * Instead of transforming the selection, move the 2D/3D cursor.
 *
 * \{ */

static void createTransCursor_image(TransInfo *t)
{
  TransData *td;
  SpaceImage *sima = t->sa->spacedata.first;
  float *cursor_location = sima->cursor;

  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransTexspace");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, cursor_location);
  td->ob = NULL;

  unit_m3(td->mtx);
  unit_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  td->loc = cursor_location;
  copy_v3_v3(td->iloc, cursor_location);
}

static void createTransCursor_view3d(TransInfo *t)
{
  TransData *td;

  Scene *scene = t->scene;
  if (ID_IS_LINKED(scene)) {
    BKE_report(t->reports, RPT_ERROR, "Linked data can't text-space transform");
    return;
  }

  View3DCursor *cursor = &scene->cursor;
  {
    BLI_assert(t->data_container_len == 1);
    TransDataContainer *tc = t->data_container;
    tc->data_len = 1;
    td = tc->data = MEM_callocN(sizeof(TransData), "TransTexspace");
    td->ext = tc->data_ext = MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
  }

  td->flag = TD_SELECTED;
  copy_v3_v3(td->center, cursor->location);
  td->ob = NULL;

  unit_m3(td->mtx);
  BKE_scene_cursor_rot_to_mat3(cursor, td->axismtx);
  normalize_m3(td->axismtx);
  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  td->loc = cursor->location;
  copy_v3_v3(td->iloc, cursor->location);

  if (cursor->rotation_mode > 0) {
    td->ext->rot = cursor->rotation_euler;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = NULL;

    copy_v3_v3(td->ext->irot, cursor->rotation_euler);
  }
  else if (cursor->rotation_mode == ROT_MODE_AXISANGLE) {
    td->ext->rot = NULL;
    td->ext->rotAxis = cursor->rotation_axis;
    td->ext->rotAngle = &cursor->rotation_angle;
    td->ext->quat = NULL;

    td->ext->irotAngle = cursor->rotation_angle;
    copy_v3_v3(td->ext->irotAxis, cursor->rotation_axis);
  }
  else {
    td->ext->rot = NULL;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = cursor->rotation_quaternion;

    copy_qt_qt(td->ext->iquat, cursor->rotation_quaternion);
  }
  td->ext->rotOrder = cursor->rotation_mode;
}

/** \} */

/* ********************* edge (for crease) ***** */

static void createTransEdge(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    TransData *td = NULL;
    BMEdge *eed;
    BMIter iter;
    float mtx[3][3], smtx[3][3];
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    int cd_edge_float_offset;

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          countsel++;
        }
        if (is_prop_edit) {
          count++;
        }
      }
    }

    if (countsel == 0) {
      tc->data_len = 0;
      continue;
    }

    if (is_prop_edit) {
      tc->data_len = count;
    }
    else {
      tc->data_len = countsel;
    }

    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransCrease");

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    /* create data we need */
    if (t->mode == TFM_BWEIGHT) {
      BM_mesh_cd_flag_ensure(em->bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_EDGE_BWEIGHT);
      cd_edge_float_offset = CustomData_get_offset(&em->bm->edata, CD_BWEIGHT);
    }
    else {  // if (t->mode == TFM_CREASE) {
      BLI_assert(t->mode == TFM_CREASE);
      BM_mesh_cd_flag_ensure(em->bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_EDGE_CREASE);
      cd_edge_float_offset = CustomData_get_offset(&em->bm->edata, CD_CREASE);
    }

    BLI_assert(cd_edge_float_offset != -1);

    BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN) &&
          (BM_elem_flag_test(eed, BM_ELEM_SELECT) || is_prop_edit)) {
        float *fl_ptr;
        /* need to set center for center calculations */
        mid_v3_v3v3(td->center, eed->v1->co, eed->v2->co);

        td->loc = NULL;
        if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
          td->flag = TD_SELECTED;
        }
        else {
          td->flag = 0;
        }

        copy_m3_m3(td->smtx, smtx);
        copy_m3_m3(td->mtx, mtx);

        td->ext = NULL;

        fl_ptr = BM_ELEM_CD_GET_VOID_P(eed, cd_edge_float_offset);
        td->val = fl_ptr;
        td->ival = *fl_ptr;

        td++;
      }
    }
  }
}

/* ********************* pose mode ************* */

static bKinematicConstraint *has_targetless_ik(bPoseChannel *pchan)
{
  bConstraint *con = pchan->constraints.first;

  for (; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->enforce != 0.0f)) {
      bKinematicConstraint *data = con->data;

      if (data->tar == NULL) {
        return data;
      }
      if (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0) {
        return data;
      }
    }
  }
  return NULL;
}

static short apply_targetless_ik(Object *ob)
{
  bPoseChannel *pchan, *parchan, *chanlist[256];
  bKinematicConstraint *data;
  int segcount, apply = 0;

  /* now we got a difficult situation... we have to find the
   * target-less IK pchans, and apply transformation to the all
   * pchans that were in the chain */

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    data = has_targetless_ik(pchan);
    if (data && (data->flag & CONSTRAINT_IK_AUTO)) {

      /* fill the array with the bones of the chain (armature.c does same, keep it synced) */
      segcount = 0;

      /* exclude tip from chain? */
      if (!(data->flag & CONSTRAINT_IK_TIP)) {
        parchan = pchan->parent;
      }
      else {
        parchan = pchan;
      }

      /* Find the chain's root & count the segments needed */
      for (; parchan; parchan = parchan->parent) {
        chanlist[segcount] = parchan;
        segcount++;

        if (segcount == data->rootbone || segcount > 255) {
          break;  // 255 is weak
        }
      }
      for (; segcount; segcount--) {
        Bone *bone;
        float rmat[4][4] /*, tmat[4][4], imat[4][4]*/;

        /* pose_mat(b) = pose_mat(b-1) * offs_bone * channel * constraint * IK  */
        /* we put in channel the entire result of rmat = (channel * constraint * IK) */
        /* pose_mat(b) = pose_mat(b-1) * offs_bone * rmat  */
        /* rmat = pose_mat(b) * inv(pose_mat(b-1) * offs_bone ) */

        parchan = chanlist[segcount - 1];
        bone = parchan->bone;
        bone->flag |= BONE_TRANSFORM; /* ensures it gets an auto key inserted */

        BKE_armature_mat_pose_to_bone(parchan, parchan->pose_mat, rmat);

        /* apply and decompose, doesn't work for constraints or non-uniform scale well */
        {
          float rmat3[3][3], qrmat[3][3], imat3[3][3], smat[3][3];
          copy_m3_m4(rmat3, rmat);

          /* rotation */
          /* [#22409] is partially caused by this, as slight numeric error introduced during
           * the solving process leads to locked-axis values changing. However, we cannot modify
           * the values here, or else there are huge discrepancies between IK-solver (interactive)
           * and applied poses. */
          BKE_pchan_mat3_to_rot(parchan, rmat3, false);

          /* for size, remove rotation */
          /* causes problems with some constraints (so apply only if needed) */
          if (data->flag & CONSTRAINT_IK_STRETCH) {
            BKE_pchan_rot_to_mat3(parchan, qrmat);
            invert_m3_m3(imat3, qrmat);
            mul_m3_m3m3(smat, rmat3, imat3);
            mat3_to_size(parchan->size, smat);
          }

          /* causes problems with some constraints (e.g. childof), so disable this */
          /* as it is IK shouldn't affect location directly */
          /* copy_v3_v3(parchan->loc, rmat[3]); */
        }
      }

      apply = 1;
      data->flag &= ~CONSTRAINT_IK_AUTO;
    }
  }

  return apply;
}

static void add_pose_transdata(
    TransInfo *t, bPoseChannel *pchan, Object *ob, TransDataContainer *tc, TransData *td)
{
  Bone *bone = pchan->bone;
  float pmat[3][3], omat[3][3];
  float cmat[3][3], tmat[3][3];
  float vec[3];

  copy_v3_v3(vec, pchan->pose_mat[3]);
  copy_v3_v3(td->center, vec);

  td->ob = ob;
  td->flag = TD_SELECTED;
  if (bone->flag & BONE_HINGE_CHILD_TRANSFORM) {
    td->flag |= TD_NOCENTER;
  }

  if (bone->flag & BONE_TRANSFORM_CHILD) {
    td->flag |= TD_NOCENTER;
    td->flag |= TD_NO_LOC;
  }

  td->protectflag = pchan->protectflag;

  td->loc = pchan->loc;
  copy_v3_v3(td->iloc, pchan->loc);

  td->ext->size = pchan->size;
  copy_v3_v3(td->ext->isize, pchan->size);

  if (pchan->rotmode > 0) {
    td->ext->rot = pchan->eul;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = NULL;

    copy_v3_v3(td->ext->irot, pchan->eul);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    td->ext->rot = NULL;
    td->ext->rotAxis = pchan->rotAxis;
    td->ext->rotAngle = &pchan->rotAngle;
    td->ext->quat = NULL;

    td->ext->irotAngle = pchan->rotAngle;
    copy_v3_v3(td->ext->irotAxis, pchan->rotAxis);
  }
  else {
    td->ext->rot = NULL;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = pchan->quat;

    copy_qt_qt(td->ext->iquat, pchan->quat);
  }
  td->ext->rotOrder = pchan->rotmode;

  /* proper way to get parent transform + own transform + constraints transform */
  copy_m3_m4(omat, ob->obmat);

  /* New code, using "generic" BKE_bone_parent_transform_calc_from_pchan(). */
  {
    BoneParentTransform bpt;
    float rpmat[3][3];

    BKE_bone_parent_transform_calc_from_pchan(pchan, &bpt);
    if (t->mode == TFM_TRANSLATION) {
      copy_m3_m4(pmat, bpt.loc_mat);
    }
    else {
      copy_m3_m4(pmat, bpt.rotscale_mat);
    }

    /* Grrr! Exceptional case: When translating pose bones that are either Hinge or NoLocal,
     * and want align snapping, we just need both loc_mat and rotscale_mat.
     * So simply always store rotscale mat in td->ext, and always use it to apply rotations...
     * Ugly to need such hacks! :/ */
    copy_m3_m4(rpmat, bpt.rotscale_mat);

    if (constraints_list_needinv(t, &pchan->constraints)) {
      copy_m3_m4(tmat, pchan->constinv);
      invert_m3_m3(cmat, tmat);
      mul_m3_series(td->mtx, cmat, omat, pmat);
      mul_m3_series(td->ext->r_mtx, cmat, omat, rpmat);
    }
    else {
      mul_m3_series(td->mtx, omat, pmat);
      mul_m3_series(td->ext->r_mtx, omat, rpmat);
    }
    invert_m3_m3(td->ext->r_smtx, td->ext->r_mtx);
  }

  pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);

  /* exceptional case: rotate the pose bone which also applies transformation
   * when a parentless bone has BONE_NO_LOCAL_LOCATION [] */
  if (!ELEM(t->mode, TFM_TRANSLATION, TFM_RESIZE) &&
      (pchan->bone->flag & BONE_NO_LOCAL_LOCATION)) {
    if (pchan->parent) {
      /* same as td->smtx but without pchan->bone->bone_mat */
      td->flag |= TD_PBONE_LOCAL_MTX_C;
      mul_m3_m3m3(td->ext->l_smtx, pchan->bone->bone_mat, td->smtx);
    }
    else {
      td->flag |= TD_PBONE_LOCAL_MTX_P;
    }
  }

  /* for axismat we use bone's own transform */
  copy_m3_m4(pmat, pchan->pose_mat);
  mul_m3_m3m3(td->axismtx, omat, pmat);
  normalize_m3(td->axismtx);

  if (ELEM(t->mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
    bArmature *arm = tc->poseobj->data;

    if ((t->mode == TFM_BONE_ENVELOPE_DIST) || (arm->drawtype == ARM_ENVELOPE)) {
      td->loc = NULL;
      td->val = &bone->dist;
      td->ival = bone->dist;
    }
    else {
      // abusive storage of scale in the loc pointer :)
      td->loc = &bone->xwidth;
      copy_v3_v3(td->iloc, td->loc);
      td->val = NULL;
    }
  }

  /* in this case we can do target-less IK grabbing */
  if (t->mode == TFM_TRANSLATION) {
    bKinematicConstraint *data = has_targetless_ik(pchan);
    if (data) {
      if (data->flag & CONSTRAINT_IK_TIP) {
        copy_v3_v3(data->grabtarget, pchan->pose_tail);
      }
      else {
        copy_v3_v3(data->grabtarget, pchan->pose_head);
      }
      td->loc = data->grabtarget;
      copy_v3_v3(td->iloc, td->loc);
      data->flag |= CONSTRAINT_IK_AUTO;

      /* only object matrix correction */
      copy_m3_m3(td->mtx, omat);
      pseudoinverse_m3_m3(td->smtx, td->mtx, PSEUDOINVERSE_EPSILON);
    }
  }

  /* store reference to first constraint */
  td->con = pchan->constraints.first;
}

static void bone_children_clear_transflag(int mode, short around, ListBase *lb)
{
  Bone *bone = lb->first;

  for (; bone; bone = bone->next) {
    if ((bone->flag & BONE_HINGE) && (bone->flag & BONE_CONNECTED)) {
      bone->flag |= BONE_HINGE_CHILD_TRANSFORM;
    }
    else if ((bone->flag & BONE_TRANSFORM) && (mode == TFM_ROTATION || mode == TFM_TRACKBALL) &&
             (around == V3D_AROUND_LOCAL_ORIGINS)) {
      bone->flag |= BONE_TRANSFORM_CHILD;
    }
    else {
      bone->flag &= ~(BONE_TRANSFORM | BONE_TRANSFORM_MIRROR);
    }

    bone_children_clear_transflag(mode, around, &bone->childbase);
  }
}

/* sets transform flags in the bones
 * returns total number of bones with BONE_TRANSFORM */
int count_set_pose_transflags(Object *ob,
                              const int mode,
                              const short around,
                              bool has_translate_rotate[2])
{
  bArmature *arm = ob->data;
  bPoseChannel *pchan;
  Bone *bone;
  int total = 0;

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    bone = pchan->bone;
    if (PBONE_VISIBLE(arm, bone)) {
      if ((bone->flag & BONE_SELECTED)) {
        bone->flag |= BONE_TRANSFORM;
      }
      else {
        bone->flag &= ~(BONE_TRANSFORM | BONE_TRANSFORM_MIRROR);
      }

      bone->flag &= ~BONE_HINGE_CHILD_TRANSFORM;
      bone->flag &= ~BONE_TRANSFORM_CHILD;
    }
    else {
      bone->flag &= ~(BONE_TRANSFORM | BONE_TRANSFORM_MIRROR);
    }
  }

  /* make sure no bone can be transformed when a parent is transformed */
  /* since pchans are depsgraph sorted, the parents are in beginning of list */
  if (!ELEM(mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
    for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      bone = pchan->bone;
      if (bone->flag & BONE_TRANSFORM) {
        bone_children_clear_transflag(mode, around, &bone->childbase);
      }
    }
  }
  /* now count, and check if we have autoIK or have to switch from translate to rotate */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    bone = pchan->bone;
    if (bone->flag & BONE_TRANSFORM) {
      total++;

      if (has_translate_rotate != NULL) {
        if (has_targetless_ik(pchan) == NULL) {
          if (pchan->parent && (pchan->bone->flag & BONE_CONNECTED)) {
            if (pchan->bone->flag & BONE_HINGE_CHILD_TRANSFORM) {
              has_translate_rotate[0] = true;
            }
          }
          else {
            if ((pchan->protectflag & OB_LOCK_LOC) != OB_LOCK_LOC) {
              has_translate_rotate[0] = true;
            }
          }
          if ((pchan->protectflag & OB_LOCK_ROT) != OB_LOCK_ROT) {
            has_translate_rotate[1] = true;
          }
        }
        else {
          has_translate_rotate[0] = true;
        }
      }
    }
  }

  return total;
}

/* -------- Auto-IK ---------- */

/* adjust pose-channel's auto-ik chainlen */
static bool pchan_autoik_adjust(bPoseChannel *pchan, short chainlen)
{
  bConstraint *con;
  bool changed = false;

  /* don't bother to search if no valid constraints */
  if ((pchan->constflag & (PCHAN_HAS_IK | PCHAN_HAS_TARGET)) == 0) {
    return changed;
  }

  /* check if pchan has ik-constraint */
  for (con = pchan->constraints.first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->enforce != 0.0f)) {
      bKinematicConstraint *data = con->data;

      /* only accept if a temporary one (for auto-ik) */
      if (data->flag & CONSTRAINT_IK_TEMP) {
        /* chainlen is new chainlen, but is limited by maximum chainlen */
        const int old_rootbone = data->rootbone;
        if ((chainlen == 0) || (chainlen > data->max_rootbone)) {
          data->rootbone = data->max_rootbone;
        }
        else {
          data->rootbone = chainlen;
        }
        changed |= (data->rootbone != old_rootbone);
      }
    }
  }

  return changed;
}

/* change the chain-length of auto-ik */
void transform_autoik_update(TransInfo *t, short mode)
{
  Main *bmain = CTX_data_main(t->context);

  short *chainlen = &t->settings->autoik_chainlen;
  bPoseChannel *pchan;

  /* mode determines what change to apply to chainlen */
  if (mode == 1) {
    /* mode=1 is from WHEELMOUSEDOWN... increases len */
    (*chainlen)++;
  }
  else if (mode == -1) {
    /* mode==-1 is from WHEELMOUSEUP... decreases len */
    if (*chainlen > 0) {
      (*chainlen)--;
    }
    else {
      /* IK length did not change, skip updates. */
      return;
    }
  }

  /* apply to all pose-channels */
  bool changed = false;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    /* sanity checks (don't assume t->poseobj is set, or that it is an armature) */
    if (ELEM(NULL, tc->poseobj, tc->poseobj->pose)) {
      continue;
    }

    for (pchan = tc->poseobj->pose->chanbase.first; pchan; pchan = pchan->next) {
      changed |= pchan_autoik_adjust(pchan, *chainlen);
    }
  }

  if (changed) {
    /* TODO(sergey): Consider doing partial update only. */
    DEG_relations_tag_update(bmain);
  }
}

/* frees temporal IKs */
static void pose_grab_with_ik_clear(Main *bmain, Object *ob)
{
  bKinematicConstraint *data;
  bPoseChannel *pchan;
  bConstraint *con, *next;
  bool relations_changed = false;

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    /* clear all temporary lock flags */
    pchan->ikflag &= ~(BONE_IK_NO_XDOF_TEMP | BONE_IK_NO_YDOF_TEMP | BONE_IK_NO_ZDOF_TEMP);

    pchan->constflag &= ~(PCHAN_HAS_IK | PCHAN_HAS_TARGET);

    /* remove all temporary IK-constraints added */
    for (con = pchan->constraints.first; con; con = next) {
      next = con->next;
      if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
        data = con->data;
        if (data->flag & CONSTRAINT_IK_TEMP) {
          relations_changed = true;

          /* iTaSC needs clear for removed constraints */
          BIK_clear_data(ob->pose);

          BLI_remlink(&pchan->constraints, con);
          MEM_freeN(con->data);
          MEM_freeN(con);
          continue;
        }
        pchan->constflag |= PCHAN_HAS_IK;
        if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0)) {
          pchan->constflag |= PCHAN_HAS_TARGET;
        }
      }
    }
  }

  if (relations_changed) {
    /* TODO(sergey): Consider doing partial update only. */
    DEG_relations_tag_update(bmain);
  }
}

/* adds the IK to pchan - returns if added */
static short pose_grab_with_ik_add(bPoseChannel *pchan)
{
  bKinematicConstraint *targetless = NULL;
  bKinematicConstraint *data;
  bConstraint *con;

  /* Sanity check */
  if (pchan == NULL) {
    return 0;
  }

  /* Rule: not if there's already an IK on this channel */
  for (con = pchan->constraints.first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
      data = con->data;

      if (data->tar == NULL || (data->tar->type == OB_ARMATURE && data->subtarget[0] == '\0')) {
        /* make reference to constraint to base things off later
         * (if it's the last targetless constraint encountered) */
        targetless = (bKinematicConstraint *)con->data;

        /* but, if this is a targetless IK, we make it auto anyway (for the children loop) */
        if (con->enforce != 0.0f) {
          data->flag |= CONSTRAINT_IK_AUTO;

          /* if no chain length has been specified,
           * just make things obey standard rotation locks too */
          if (data->rootbone == 0) {
            for (; pchan; pchan = pchan->parent) {
              /* here, we set ik-settings for bone from pchan->protectflag */
              // XXX: careful with quats/axis-angle rotations where we're locking 4d components
              if (pchan->protectflag & OB_LOCK_ROTX) {
                pchan->ikflag |= BONE_IK_NO_XDOF_TEMP;
              }
              if (pchan->protectflag & OB_LOCK_ROTY) {
                pchan->ikflag |= BONE_IK_NO_YDOF_TEMP;
              }
              if (pchan->protectflag & OB_LOCK_ROTZ) {
                pchan->ikflag |= BONE_IK_NO_ZDOF_TEMP;
              }
            }
          }

          return 0;
        }
      }

      if ((con->flag & CONSTRAINT_DISABLE) == 0 && (con->enforce != 0.0f)) {
        return 0;
      }
    }
  }

  con = BKE_constraint_add_for_pose(NULL, pchan, "TempConstraint", CONSTRAINT_TYPE_KINEMATIC);

  /* for draw, but also for detecting while pose solving */
  pchan->constflag |= (PCHAN_HAS_IK | PCHAN_HAS_TARGET);

  data = con->data;
  if (targetless) {
    /* if exists, use values from last targetless (but disabled) IK-constraint as base */
    *data = *targetless;
  }
  else {
    data->flag = CONSTRAINT_IK_TIP;
  }
  data->flag |= CONSTRAINT_IK_TEMP | CONSTRAINT_IK_AUTO | CONSTRAINT_IK_POS;
  copy_v3_v3(data->grabtarget, pchan->pose_tail);

  /* watch-it! has to be 0 here, since we're still on the
   * same bone for the first time through the loop T25885. */
  data->rootbone = 0;

  /* we only include bones that are part of a continual connected chain */
  do {
    /* here, we set ik-settings for bone from pchan->protectflag */
    // XXX: careful with quats/axis-angle rotations where we're locking 4d components
    if (pchan->protectflag & OB_LOCK_ROTX) {
      pchan->ikflag |= BONE_IK_NO_XDOF_TEMP;
    }
    if (pchan->protectflag & OB_LOCK_ROTY) {
      pchan->ikflag |= BONE_IK_NO_YDOF_TEMP;
    }
    if (pchan->protectflag & OB_LOCK_ROTZ) {
      pchan->ikflag |= BONE_IK_NO_ZDOF_TEMP;
    }

    /* now we count this pchan as being included */
    data->rootbone++;

    /* continue to parent, but only if we're connected to it */
    if (pchan->bone->flag & BONE_CONNECTED) {
      pchan = pchan->parent;
    }
    else {
      pchan = NULL;
    }
  } while (pchan);

  /* make a copy of maximum chain-length */
  data->max_rootbone = data->rootbone;

  return 1;
}

/* bone is a candidate to get IK, but we don't do it if it has children connected */
static short pose_grab_with_ik_children(bPose *pose, Bone *bone)
{
  Bone *bonec;
  short wentdeeper = 0, added = 0;

  /* go deeper if children & children are connected */
  for (bonec = bone->childbase.first; bonec; bonec = bonec->next) {
    if (bonec->flag & BONE_CONNECTED) {
      wentdeeper = 1;
      added += pose_grab_with_ik_children(pose, bonec);
    }
  }
  if (wentdeeper == 0) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(pose, bone->name);
    if (pchan) {
      added += pose_grab_with_ik_add(pchan);
    }
  }

  return added;
}

/* main call which adds temporal IK chains */
static short pose_grab_with_ik(Main *bmain, Object *ob)
{
  bArmature *arm;
  bPoseChannel *pchan, *parent;
  Bone *bonec;
  short tot_ik = 0;

  if ((ob == NULL) || (ob->pose == NULL) || (ob->mode & OB_MODE_POSE) == 0) {
    return 0;
  }

  arm = ob->data;

  /* Rule: allow multiple Bones
   * (but they must be selected, and only one ik-solver per chain should get added) */
  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->bone->layer & arm->layer) {
      if (pchan->bone->flag & BONE_SELECTED) {
        /* Rule: no IK for solitatry (unconnected) bones */
        for (bonec = pchan->bone->childbase.first; bonec; bonec = bonec->next) {
          if (bonec->flag & BONE_CONNECTED) {
            break;
          }
        }
        if ((pchan->bone->flag & BONE_CONNECTED) == 0 && (bonec == NULL)) {
          continue;
        }

        /* rule: if selected Bone is not a root bone, it gets a temporal IK */
        if (pchan->parent) {
          /* only adds if there's no IK yet (and no parent bone was selected) */
          for (parent = pchan->parent; parent; parent = parent->parent) {
            if (parent->bone->flag & BONE_SELECTED) {
              break;
            }
          }
          if (parent == NULL) {
            tot_ik += pose_grab_with_ik_add(pchan);
          }
        }
        else {
          /* rule: go over the children and add IK to the tips */
          tot_ik += pose_grab_with_ik_children(ob->pose, pchan->bone);
        }
      }
    }
  }

  /* iTaSC needs clear for new IK constraints */
  if (tot_ik) {
    BIK_clear_data(ob->pose);
    /* TODO(sergey): Consider doing partial update only. */
    DEG_relations_tag_update(bmain);
  }

  return (tot_ik) ? 1 : 0;
}

static void pose_mirror_info_init(PoseInitData_Mirror *pid,
                                  bPoseChannel *pchan,
                                  bPoseChannel *pchan_orig,
                                  bool is_mirror_relative)
{
  pid->pchan = pchan;
  copy_v3_v3(pid->orig.loc, pchan->loc);
  copy_v3_v3(pid->orig.size, pchan->size);
  pid->orig.curve_in_x = pchan->curve_in_x;
  pid->orig.curve_out_x = pchan->curve_out_x;
  pid->orig.roll1 = pchan->roll1;
  pid->orig.roll2 = pchan->roll2;

  if (pchan->rotmode > 0) {
    copy_v3_v3(pid->orig.eul, pchan->eul);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    copy_v3_v3(pid->orig.axis_angle, pchan->rotAxis);
    pid->orig.axis_angle[3] = pchan->rotAngle;
  }
  else {
    copy_qt_qt(pid->orig.quat, pchan->quat);
  }

  if (is_mirror_relative) {
    float pchan_mtx[4][4];
    float pchan_mtx_mirror[4][4];

    float flip_mtx[4][4];
    unit_m4(flip_mtx);
    flip_mtx[0][0] = -1;

    BKE_pchan_to_mat4(pchan_orig, pchan_mtx_mirror);
    BKE_pchan_to_mat4(pchan, pchan_mtx);

    mul_m4_m4m4(pchan_mtx_mirror, pchan_mtx_mirror, flip_mtx);
    mul_m4_m4m4(pchan_mtx_mirror, flip_mtx, pchan_mtx_mirror);

    invert_m4(pchan_mtx_mirror);
    mul_m4_m4m4(pid->offset_mtx, pchan_mtx, pchan_mtx_mirror);
  }
  else {
    unit_m4(pid->offset_mtx);
  }
}

static void pose_mirror_info_restore(const PoseInitData_Mirror *pid)
{
  bPoseChannel *pchan = pid->pchan;
  copy_v3_v3(pchan->loc, pid->orig.loc);
  copy_v3_v3(pchan->size, pid->orig.size);
  pchan->curve_in_x = pid->orig.curve_in_x;
  pchan->curve_out_x = pid->orig.curve_out_x;
  pchan->roll1 = pid->orig.roll1;
  pchan->roll2 = pid->orig.roll2;

  if (pchan->rotmode > 0) {
    copy_v3_v3(pchan->eul, pid->orig.eul);
  }
  else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
    copy_v3_v3(pchan->rotAxis, pid->orig.axis_angle);
    pchan->rotAngle = pid->orig.axis_angle[3];
  }
  else {
    copy_qt_qt(pchan->quat, pid->orig.quat);
  }
}

/**
 * When objects array is NULL, use 't->data_container' as is.
 */
static void createTransPose(TransInfo *t)
{
  Main *bmain = CTX_data_main(t->context);

  t->data_len_all = 0;

  bool has_translate_rotate_buf[2] = {false, false};
  bool *has_translate_rotate = (t->mode == TFM_TRANSLATION) ? has_translate_rotate_buf : NULL;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Object *ob = tc->poseobj;
    bPose *pose = ob->pose;

    bArmature *arm;

    /* check validity of state */
    arm = BKE_armature_from_object(tc->poseobj);
    if ((arm == NULL) || (pose == NULL)) {
      continue;
    }

    const bool mirror = ((pose->flag & POSE_MIRROR_EDIT) != 0);

    /* set flags and count total */
    tc->data_len = count_set_pose_transflags(ob, t->mode, t->around, has_translate_rotate);
    if (tc->data_len == 0) {
      continue;
    }

    if (arm->flag & ARM_RESTPOS) {
      if (ELEM(t->mode, TFM_DUMMY, TFM_BONESIZE) == 0) {
        BKE_report(t->reports, RPT_ERROR, "Cannot change Pose when 'Rest Position' is enabled");
        tc->data_len = 0;
        continue;
      }
    }

    /* do we need to add temporal IK chains? */
    if ((pose->flag & POSE_AUTO_IK) && t->mode == TFM_TRANSLATION) {
      if (pose_grab_with_ik(bmain, ob)) {
        t->flag |= T_AUTOIK;
        has_translate_rotate[0] = true;
      }
    }

    if (mirror) {
      int total_mirrored = 0;
      for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
        if ((pchan->bone->flag & BONE_TRANSFORM) &&
            BKE_pose_channel_get_mirrored(ob->pose, pchan->name)) {
          total_mirrored++;
        }
      }

      PoseInitData_Mirror *pid = MEM_mallocN((total_mirrored + 1) * sizeof(PoseInitData_Mirror),
                                             "PoseInitData_Mirror");

      /* Trick to terminate iteration. */
      pid[total_mirrored].pchan = NULL;

      tc->custom.type.data = pid;
      tc->custom.type.use_free = true;
    }
  }

  /* if there are no translatable bones, do rotation */
  if ((t->mode == TFM_TRANSLATION) && !has_translate_rotate[0]) {
    if (has_translate_rotate[1]) {
      t->mode = TFM_ROTATION;
    }
    else {
      t->mode = TFM_RESIZE;
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len == 0) {
      continue;
    }
    Object *ob = tc->poseobj;
    TransData *td;
    TransDataExtension *tdx;
    int i;

    PoseInitData_Mirror *pid = tc->custom.type.data;
    int pid_index = 0;
    bPose *pose = ob->pose;

    if (pose == NULL) {
      continue;
    }

    const bool mirror = ((pose->flag & POSE_MIRROR_EDIT) != 0);
    const bool is_mirror_relative = ((pose->flag & POSE_MIRROR_RELATIVE) != 0);

    tc->poseobj = ob; /* we also allow non-active objects to be transformed, in weightpaint */

    /* init trans data */
    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransPoseBone");
    tdx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                     "TransPoseBoneExt");
    for (i = 0; i < tc->data_len; i++, td++, tdx++) {
      td->ext = tdx;
      td->val = NULL;
    }

    /* use pose channels to fill trans data */
    td = tc->data;
    for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->bone->flag & BONE_TRANSFORM) {
        add_pose_transdata(t, pchan, ob, tc, td);

        if (mirror) {
          bPoseChannel *pchan_mirror = BKE_pose_channel_get_mirrored(ob->pose, pchan->name);
          if (pchan_mirror) {
            pose_mirror_info_init(&pid[pid_index], pchan_mirror, pchan, is_mirror_relative);
            pid_index++;
          }
        }

        td++;
      }
    }

    if (td != (tc->data + tc->data_len)) {
      BKE_report(t->reports, RPT_DEBUG, "Bone selection count error");
    }

    /* initialize initial auto=ik chainlen's? */
    if (t->flag & T_AUTOIK) {
      transform_autoik_update(t, 0);
    }
  }

  t->flag |= T_POSE;
  /* disable PET, its not usable in pose mode yet [#32444] */
  t->flag &= ~T_PROP_EDIT_ALL;
}

void restoreMirrorPoseBones(TransDataContainer *tc)
{
  bPose *pose = tc->poseobj->pose;

  if (!(pose->flag & POSE_MIRROR_EDIT)) {
    return;
  }

  for (PoseInitData_Mirror *pid = tc->custom.type.data; pid->pchan; pid++) {
    pose_mirror_info_restore(pid);
  }
}

void restoreBones(TransDataContainer *tc)
{
  bArmature *arm;
  BoneInitData *bid = tc->custom.type.data;
  EditBone *ebo;

  if (tc->obedit) {
    arm = tc->obedit->data;
  }
  else {
    BLI_assert(tc->poseobj != NULL);
    arm = tc->poseobj->data;
  }

  while (bid->bone) {
    ebo = bid->bone;

    ebo->dist = bid->dist;
    ebo->rad_tail = bid->rad_tail;
    ebo->roll = bid->roll;
    ebo->xwidth = bid->xwidth;
    ebo->zwidth = bid->zwidth;
    copy_v3_v3(ebo->head, bid->head);
    copy_v3_v3(ebo->tail, bid->tail);

    if (arm->flag & ARM_MIRROR_EDIT) {
      EditBone *ebo_child;

      /* Also move connected ebo_child, in case ebo_child's name aren't mirrored properly */
      for (ebo_child = arm->edbo->first; ebo_child; ebo_child = ebo_child->next) {
        if ((ebo_child->flag & BONE_CONNECTED) && (ebo_child->parent == ebo)) {
          copy_v3_v3(ebo_child->head, ebo->tail);
          ebo_child->rad_head = ebo->rad_tail;
        }
      }

      /* Also move connected parent, in case parent's name isn't mirrored properly */
      if ((ebo->flag & BONE_CONNECTED) && ebo->parent) {
        EditBone *parent = ebo->parent;
        copy_v3_v3(parent->tail, ebo->head);
        parent->rad_tail = ebo->rad_head;
      }
    }

    bid++;
  }
}

/* ********************* armature ************** */
static void createTransArmatureVerts(TransInfo *t)
{
  t->data_len_all = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    EditBone *ebo, *eboflip;
    bArmature *arm = tc->obedit->data;
    ListBase *edbo = arm->edbo;
    bool mirror = ((arm->flag & ARM_MIRROR_EDIT) != 0);
    int total_mirrored = 0;

    tc->data_len = 0;
    for (ebo = edbo->first; ebo; ebo = ebo->next) {
      const int data_len_prev = tc->data_len;

      if (EBONE_VISIBLE(arm, ebo) && !(ebo->flag & BONE_EDITMODE_LOCKED)) {
        if (ELEM(t->mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
          if (ebo->flag & BONE_SELECTED) {
            tc->data_len++;
          }
        }
        else if (t->mode == TFM_BONE_ROLL) {
          if (ebo->flag & BONE_SELECTED) {
            tc->data_len++;
          }
        }
        else {
          if (ebo->flag & BONE_TIPSEL) {
            tc->data_len++;
          }
          if (ebo->flag & BONE_ROOTSEL) {
            tc->data_len++;
          }
        }
      }

      if (mirror && (data_len_prev < tc->data_len)) {
        eboflip = ED_armature_ebone_get_mirrored(arm->edbo, ebo);
        if (eboflip) {
          total_mirrored++;
        }
      }
    }
    if (!tc->data_len) {
      continue;
    }

    if (mirror) {
      BoneInitData *bid = MEM_mallocN((total_mirrored + 1) * sizeof(BoneInitData), "BoneInitData");

      /* trick to terminate iteration */
      bid[total_mirrored].bone = NULL;

      tc->custom.type.data = bid;
      tc->custom.type.use_free = true;
    }
    t->data_len_all += tc->data_len;
  }

  transform_around_single_fallback(t);
  t->data_len_all = -1;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (!tc->data_len) {
      continue;
    }

    EditBone *ebo, *eboflip;
    bArmature *arm = tc->obedit->data;
    ListBase *edbo = arm->edbo;
    TransData *td, *td_old;
    float mtx[3][3], smtx[3][3], bonemat[3][3];
    bool mirror = ((arm->flag & ARM_MIRROR_EDIT) != 0);
    BoneInitData *bid = tc->custom.type.data;

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransEditBone");
    int i = 0;

    for (ebo = edbo->first; ebo; ebo = ebo->next) {
      td_old = td;
      ebo->oldlength =
          ebo->length;  // length==0.0 on extrude, used for scaling radius of bone points

      if (EBONE_VISIBLE(arm, ebo) && !(ebo->flag & BONE_EDITMODE_LOCKED)) {
        if (t->mode == TFM_BONE_ENVELOPE) {
          if (ebo->flag & BONE_ROOTSEL) {
            td->val = &ebo->rad_head;
            td->ival = *td->val;

            copy_v3_v3(td->center, ebo->head);
            td->flag = TD_SELECTED;

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            td->loc = NULL;
            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
          if (ebo->flag & BONE_TIPSEL) {
            td->val = &ebo->rad_tail;
            td->ival = *td->val;
            copy_v3_v3(td->center, ebo->tail);
            td->flag = TD_SELECTED;

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            td->loc = NULL;
            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
        else if (ELEM(t->mode, TFM_BONESIZE, TFM_BONE_ENVELOPE_DIST)) {
          if (ebo->flag & BONE_SELECTED) {
            if ((t->mode == TFM_BONE_ENVELOPE_DIST) || (arm->drawtype == ARM_ENVELOPE)) {
              td->loc = NULL;
              td->val = &ebo->dist;
              td->ival = ebo->dist;
            }
            else {
              // abusive storage of scale in the loc pointer :)
              td->loc = &ebo->xwidth;
              copy_v3_v3(td->iloc, td->loc);
              td->val = NULL;
            }
            copy_v3_v3(td->center, ebo->head);
            td->flag = TD_SELECTED;

            /* use local bone matrix */
            ED_armature_ebone_to_mat3(ebo, bonemat);
            mul_m3_m3m3(td->mtx, mtx, bonemat);
            invert_m3_m3(td->smtx, td->mtx);

            copy_m3_m3(td->axismtx, td->mtx);
            normalize_m3(td->axismtx);

            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
        else if (t->mode == TFM_BONE_ROLL) {
          if (ebo->flag & BONE_SELECTED) {
            td->loc = NULL;
            td->val = &(ebo->roll);
            td->ival = ebo->roll;

            copy_v3_v3(td->center, ebo->head);
            td->flag = TD_SELECTED;

            td->ext = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
        else {
          if (ebo->flag & BONE_TIPSEL) {
            copy_v3_v3(td->iloc, ebo->tail);

            /* Don't allow single selected tips to have a modified center,
             * causes problem with snapping (see T45974).
             * However, in rotation mode, we want to keep that 'rotate bone around root with
             * only its tip selected' behavior (see T46325). */
            if ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                ((t->mode == TFM_ROTATION) || (ebo->flag & BONE_ROOTSEL))) {
              copy_v3_v3(td->center, ebo->head);
            }
            else {
              copy_v3_v3(td->center, td->iloc);
            }

            td->loc = ebo->tail;
            td->flag = TD_SELECTED;
            if (ebo->flag & BONE_EDITMODE_LOCKED) {
              td->protectflag = OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE;
            }

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            ED_armature_ebone_to_mat3(ebo, td->axismtx);

            if ((ebo->flag & BONE_ROOTSEL) == 0) {
              td->extra = ebo;
              td->ival = ebo->roll;
            }

            td->ext = NULL;
            td->val = NULL;
            td->ob = tc->obedit;

            td++;
          }
          if (ebo->flag & BONE_ROOTSEL) {
            copy_v3_v3(td->iloc, ebo->head);
            copy_v3_v3(td->center, td->iloc);
            td->loc = ebo->head;
            td->flag = TD_SELECTED;
            if (ebo->flag & BONE_EDITMODE_LOCKED) {
              td->protectflag = OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE;
            }

            copy_m3_m3(td->smtx, smtx);
            copy_m3_m3(td->mtx, mtx);

            ED_armature_ebone_to_mat3(ebo, td->axismtx);

            td->extra = ebo; /* to fix roll */
            td->ival = ebo->roll;

            td->ext = NULL;
            td->val = NULL;
            td->ob = tc->obedit;

            td++;
          }
        }
      }

      if (mirror && (td_old != td)) {
        eboflip = ED_armature_ebone_get_mirrored(arm->edbo, ebo);
        if (eboflip) {
          bid[i].bone = eboflip;
          bid[i].dist = eboflip->dist;
          bid[i].rad_tail = eboflip->rad_tail;
          bid[i].roll = eboflip->roll;
          bid[i].xwidth = eboflip->xwidth;
          bid[i].zwidth = eboflip->zwidth;
          copy_v3_v3(bid[i].head, eboflip->head);
          copy_v3_v3(bid[i].tail, eboflip->tail);
          i++;
        }
      }
    }

    if (mirror) {
      /* trick to terminate iteration */
      BLI_assert(i + 1 == (MEM_allocN_len(bid) / sizeof(*bid)));
      bid[i].bone = NULL;
    }
  }
}

/* ********************* meta elements ********* */

static void createTransMBallVerts(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    MetaBall *mb = (MetaBall *)tc->obedit->data;
    MetaElem *ml;
    TransData *td;
    TransDataExtension *tx;
    float mtx[3][3], smtx[3][3];
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

    /* count totals */
    for (ml = mb->editelems->first; ml; ml = ml->next) {
      if (ml->flag & SELECT) {
        countsel++;
      }
      if (is_prop_edit) {
        count++;
      }
    }

    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
      continue;
    }

    if (is_prop_edit) {
      tc->data_len = count;
    }
    else {
      tc->data_len = countsel;
    }

    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(MBall EditMode)");
    tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                    "MetaElement_TransExtension");

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    for (ml = mb->editelems->first; ml; ml = ml->next) {
      if (is_prop_edit || (ml->flag & SELECT)) {
        td->loc = &ml->x;
        copy_v3_v3(td->iloc, td->loc);
        copy_v3_v3(td->center, td->loc);

        quat_to_mat3(td->axismtx, ml->quat);

        if (ml->flag & SELECT) {
          td->flag = TD_SELECTED | TD_USEQUAT | TD_SINGLESIZE;
        }
        else {
          td->flag = TD_USEQUAT;
        }

        copy_m3_m3(td->smtx, smtx);
        copy_m3_m3(td->mtx, mtx);

        td->ext = tx;

        /* Radius of MetaElem (mass of MetaElem influence) */
        if (ml->flag & MB_SCALE_RAD) {
          td->val = &ml->rad;
          td->ival = ml->rad;
        }
        else {
          td->val = &ml->s;
          td->ival = ml->s;
        }

        /* expx/expy/expz determine "shape" of some MetaElem types */
        tx->size = &ml->expx;
        tx->isize[0] = ml->expx;
        tx->isize[1] = ml->expy;
        tx->isize[2] = ml->expz;

        /* quat is used for rotation of MetaElem */
        tx->quat = ml->quat;
        copy_qt_qt(tx->iquat, ml->quat);

        tx->rot = NULL;

        td++;
        tx++;
      }
    }
  }
}

/* ********************* curve/surface ********* */

static void calc_distanceCurveVerts(TransData *head, TransData *tail)
{
  TransData *td, *td_near = NULL;
  for (td = head; td <= tail; td++) {
    if (td->flag & TD_SELECTED) {
      td_near = td;
      td->dist = 0.0f;
    }
    else if (td_near) {
      float dist;
      float vec[3];

      sub_v3_v3v3(vec, td_near->center, td->center);
      mul_m3_v3(head->mtx, vec);
      dist = len_v3(vec);

      if (dist < (td - 1)->dist) {
        td->dist = (td - 1)->dist;
      }
      else {
        td->dist = dist;
      }
    }
    else {
      td->dist = FLT_MAX;
      td->flag |= TD_NOTCONNECTED;
    }
  }
  td_near = NULL;
  for (td = tail; td >= head; td--) {
    if (td->flag & TD_SELECTED) {
      td_near = td;
      td->dist = 0.0f;
    }
    else if (td_near) {
      float dist;
      float vec[3];

      sub_v3_v3v3(vec, td_near->center, td->center);
      mul_m3_v3(head->mtx, vec);
      dist = len_v3(vec);

      if (td->flag & TD_NOTCONNECTED || dist < td->dist || (td + 1)->dist < td->dist) {
        td->flag &= ~TD_NOTCONNECTED;
        if (dist < (td + 1)->dist) {
          td->dist = (td + 1)->dist;
        }
        else {
          td->dist = dist;
        }
      }
    }
  }
}

/* Utility function for getting the handle data from bezier's */
static TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, struct BezTriple *bezt)
{
  TransDataCurveHandleFlags *hdata;
  td->flag |= TD_BEZTRIPLE;
  hdata = td->hdata = MEM_mallocN(sizeof(TransDataCurveHandleFlags), "CuHandle Data");
  hdata->ih1 = bezt->h1;
  hdata->h1 = &bezt->h1;
  hdata->ih2 = bezt->h2; /* in case the second is not selected */
  hdata->h2 = &bezt->h2;
  return hdata;
}

/**
 * For the purpose of transform code we need to behave as if handles are selected,
 * even when they aren't (see special case below).
 */
static int bezt_select_to_transform_triple_flag(const BezTriple *bezt, const bool hide_handles)
{
  int flag = 0;

  if (hide_handles) {
    if (bezt->f2 & SELECT) {
      flag = (1 << 0) | (1 << 1) | (1 << 2);
    }
  }
  else {
    flag = (((bezt->f1 & SELECT) ? (1 << 0) : 0) | ((bezt->f2 & SELECT) ? (1 << 1) : 0) |
            ((bezt->f3 & SELECT) ? (1 << 2) : 0));
  }

  /* Special case for auto & aligned handles:
   * When a center point is being moved without the handles,
   * leaving the handles stationary makes no sense and only causes strange behavior,
   * where one handle is arbitrarily anchored, the other one is aligned and lengthened
   * based on where the center point is moved. Also a bug when cancelling, see: T52007.
   *
   * A more 'correct' solution could be to store handle locations in 'TransDataCurveHandleFlags'.
   * However that doesn't resolve odd behavior, so best transform the handles in this case.
   */
  if ((flag != ((1 << 0) | (1 << 1) | (1 << 2))) && (flag & (1 << 1))) {
    if (ELEM(bezt->h1, HD_AUTO, HD_ALIGN) && ELEM(bezt->h2, HD_AUTO, HD_ALIGN)) {
      flag = (1 << 0) | (1 << 1) | (1 << 2);
    }
  }

  return flag;
}

static void createTransCurveVerts(TransInfo *t)
{

#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

  t->data_len_all = 0;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Curve *cu = tc->obedit->data;
    BLI_assert(cu->editnurb != NULL);
    BezTriple *bezt;
    BPoint *bp;
    int a;
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    View3D *v3d = t->view;
    short hide_handles = (v3d != NULL) ?
                             ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_CU_HANDLES) == 0) :
                             false;

    /* count total of vertices, check identical as in 2nd loop for making transdata! */
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    for (Nurb *nu = nurbs->first; nu; nu = nu->next) {
      if (nu->type == CU_BEZIER) {
        for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
          if (bezt->hide == 0) {
            const int bezt_tx = bezt_select_to_transform_triple_flag(bezt, hide_handles);
            if (bezt_tx & SEL_F1) {
              countsel++;
            }
            if (bezt_tx & SEL_F2) {
              countsel++;
            }
            if (bezt_tx & SEL_F3) {
              countsel++;
            }
            if (is_prop_edit) {
              count += 3;
            }
          }
        }
      }
      else {
        for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a > 0; a--, bp++) {
          if (bp->hide == 0) {
            if (is_prop_edit) {
              count++;
            }
            if (bp->f1 & SELECT) {
              countsel++;
            }
          }
        }
      }
    }
    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
      tc->data_len = 0;
      continue;
    }

    if (is_prop_edit) {
      tc->data_len = count;
    }
    else {
      tc->data_len = countsel;
    }
    tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Curve EditMode)");

    t->data_len_all += tc->data_len;
  }

  transform_around_single_fallback(t);
  t->data_len_all = -1;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len == 0) {
      continue;
    }

    Curve *cu = tc->obedit->data;
    BezTriple *bezt;
    BPoint *bp;
    int a;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
    View3D *v3d = t->view;
    short hide_handles = (v3d != NULL) ?
                             ((v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_CU_HANDLES) == 0) :
                             false;

    float mtx[3][3], smtx[3][3];

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    TransData *td = tc->data;
    ListBase *nurbs = BKE_curve_editNurbs_get(cu);
    for (Nurb *nu = nurbs->first; nu; nu = nu->next) {
      if (nu->type == CU_BEZIER) {
        TransData *head, *tail;
        head = tail = td;
        for (a = 0, bezt = nu->bezt; a < nu->pntsu; a++, bezt++) {
          if (bezt->hide == 0) {
            TransDataCurveHandleFlags *hdata = NULL;
            float axismtx[3][3];

            if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
              float normal[3], plane[3];

              BKE_nurb_bezt_calc_normal(nu, bezt, normal);
              BKE_nurb_bezt_calc_plane(nu, bezt, plane);

              if (createSpaceNormalTangent(axismtx, normal, plane)) {
                /* pass */
              }
              else {
                normalize_v3(normal);
                axis_dominant_v3_to_m3(axismtx, normal);
                invert_m3(axismtx);
              }
            }

            /* Elements that will be transform (not always a match to selection). */
            const int bezt_tx = bezt_select_to_transform_triple_flag(bezt, hide_handles);

            if (is_prop_edit || bezt_tx & SEL_F1) {
              copy_v3_v3(td->iloc, bezt->vec[0]);
              td->loc = bezt->vec[0];
              copy_v3_v3(td->center,
                         bezt->vec[(hide_handles || (t->around == V3D_AROUND_LOCAL_ORIGINS) ||
                                    (bezt->f2 & SELECT)) ?
                                       1 :
                                       0]);
              if (hide_handles) {
                if (bezt->f2 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              else {
                if (bezt->f1 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              td->ext = NULL;
              td->val = NULL;

              hdata = initTransDataCurveHandles(td, bezt);

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                copy_m3_m3(td->axismtx, axismtx);
              }

              td++;
              tail++;
            }

            /* This is the Curve Point, the other two are handles */
            if (is_prop_edit || bezt_tx & SEL_F2) {
              copy_v3_v3(td->iloc, bezt->vec[1]);
              td->loc = bezt->vec[1];
              copy_v3_v3(td->center, td->loc);
              if (bezt->f2 & SELECT) {
                td->flag = TD_SELECTED;
              }
              else {
                td->flag = 0;
              }
              td->ext = NULL;

              /* TODO - make points scale */
              if (t->mode == TFM_CURVE_SHRINKFATTEN) { /* || t->mode==TFM_RESIZE) {*/
                td->val = &(bezt->radius);
                td->ival = bezt->radius;
              }
              else if (t->mode == TFM_TILT) {
                td->val = &(bezt->tilt);
                td->ival = bezt->tilt;
              }
              else {
                td->val = NULL;
              }

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                copy_m3_m3(td->axismtx, axismtx);
              }

              if ((bezt_tx & SEL_F1) == 0 && (bezt_tx & SEL_F3) == 0) {
                /* If the middle is selected but the sides arnt, this is needed */
                if (hdata == NULL) {
                  /* if the handle was not saved by the previous handle */
                  hdata = initTransDataCurveHandles(td, bezt);
                }
              }

              td++;
              tail++;
            }
            if (is_prop_edit || bezt_tx & SEL_F3) {
              copy_v3_v3(td->iloc, bezt->vec[2]);
              td->loc = bezt->vec[2];
              copy_v3_v3(td->center,
                         bezt->vec[(hide_handles || (t->around == V3D_AROUND_LOCAL_ORIGINS) ||
                                    (bezt->f2 & SELECT)) ?
                                       1 :
                                       2]);
              if (hide_handles) {
                if (bezt->f2 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              else {
                if (bezt->f3 & SELECT) {
                  td->flag = TD_SELECTED;
                }
                else {
                  td->flag = 0;
                }
              }
              td->ext = NULL;
              td->val = NULL;

              if (hdata == NULL) {
                /* if the handle was not saved by the previous handle */
                hdata = initTransDataCurveHandles(td, bezt);
              }

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                copy_m3_m3(td->axismtx, axismtx);
              }

              td++;
              tail++;
            }

            (void)hdata; /* quiet warning */
          }
          else if (is_prop_edit && head != tail) {
            calc_distanceCurveVerts(head, tail - 1);
            head = tail;
          }
        }
        if (is_prop_edit && head != tail) {
          calc_distanceCurveVerts(head, tail - 1);
        }

        /* TODO - in the case of tilt and radius we can also avoid allocating the
         * initTransDataCurveHandles but for now just don't change handle types */
        if (ELEM(t->mode, TFM_CURVE_SHRINKFATTEN, TFM_TILT, TFM_DUMMY) == 0) {
          /* sets the handles based on their selection,
           * do this after the data is copied to the TransData */
          BKE_nurb_handles_test(nu, !hide_handles);
        }
      }
      else {
        TransData *head, *tail;
        head = tail = td;
        for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a > 0; a--, bp++) {
          if (bp->hide == 0) {
            if (is_prop_edit || (bp->f1 & SELECT)) {
              float axismtx[3][3];

              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                if (nu->pntsv == 1) {
                  float normal[3], plane[3];

                  BKE_nurb_bpoint_calc_normal(nu, bp, normal);
                  BKE_nurb_bpoint_calc_plane(nu, bp, plane);

                  if (createSpaceNormalTangent(axismtx, normal, plane)) {
                    /* pass */
                  }
                  else {
                    normalize_v3(normal);
                    axis_dominant_v3_to_m3(axismtx, normal);
                    invert_m3(axismtx);
                  }
                }
              }

              copy_v3_v3(td->iloc, bp->vec);
              td->loc = bp->vec;
              copy_v3_v3(td->center, td->loc);
              if (bp->f1 & SELECT) {
                td->flag = TD_SELECTED;
              }
              else {
                td->flag = 0;
              }
              td->ext = NULL;

              if (t->mode == TFM_CURVE_SHRINKFATTEN || t->mode == TFM_RESIZE) {
                td->val = &(bp->radius);
                td->ival = bp->radius;
              }
              else {
                td->val = &(bp->tilt);
                td->ival = bp->tilt;
              }

              copy_m3_m3(td->smtx, smtx);
              copy_m3_m3(td->mtx, mtx);
              if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
                if (nu->pntsv == 1) {
                  copy_m3_m3(td->axismtx, axismtx);
                }
              }

              td++;
              tail++;
            }
          }
          else if (is_prop_edit && head != tail) {
            calc_distanceCurveVerts(head, tail - 1);
            head = tail;
          }
        }
        if (is_prop_edit && head != tail) {
          calc_distanceCurveVerts(head, tail - 1);
        }
      }
    }
  }
#undef SEL_F1
#undef SEL_F2
#undef SEL_F3
}

/* ********************* lattice *************** */

static void createTransLatticeVerts(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    Lattice *latt = ((Lattice *)tc->obedit->data)->editlatt->latt;
    TransData *td = NULL;
    BPoint *bp;
    float mtx[3][3], smtx[3][3];
    int a;
    int count = 0, countsel = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

    bp = latt->def;
    a = latt->pntsu * latt->pntsv * latt->pntsw;
    while (a--) {
      if (bp->hide == 0) {
        if (bp->f1 & SELECT) {
          countsel++;
        }
        if (is_prop_edit) {
          count++;
        }
      }
      bp++;
    }

    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
      return;
    }

    if (is_prop_edit) {
      tc->data_len = count;
    }
    else {
      tc->data_len = countsel;
    }
    tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Lattice EditMode)");

    copy_m3_m4(mtx, tc->obedit->obmat);
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    td = tc->data;
    bp = latt->def;
    a = latt->pntsu * latt->pntsv * latt->pntsw;
    while (a--) {
      if (is_prop_edit || (bp->f1 & SELECT)) {
        if (bp->hide == 0) {
          copy_v3_v3(td->iloc, bp->vec);
          td->loc = bp->vec;
          copy_v3_v3(td->center, td->loc);
          if (bp->f1 & SELECT) {
            td->flag = TD_SELECTED;
          }
          else {
            td->flag = 0;
          }
          copy_m3_m3(td->smtx, smtx);
          copy_m3_m3(td->mtx, mtx);

          td->ext = NULL;
          td->val = NULL;

          td++;
        }
      }
      bp++;
    }
  }
}

/* ******************* particle edit **************** */
static void createTransParticleVerts(bContext *C, TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = NULL;
    TransDataExtension *tx;
    Object *ob = CTX_data_active_object(C);
    ParticleEditSettings *pset = PE_settings(t->scene);
    PTCacheEdit *edit = PE_get_current(t->scene, ob);
    ParticleSystem *psys = NULL;
    PTCacheEditPoint *point;
    PTCacheEditKey *key;
    float mat[4][4];
    int i, k, transformparticle;
    int count = 0, hasselected = 0;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

    if (edit == NULL || t->settings->particle.selectmode == SCE_SELECT_PATH) {
      return;
    }

    psys = edit->psys;

    for (i = 0, point = edit->points; i < edit->totpoint; i++, point++) {
      point->flag &= ~PEP_TRANSFORM;
      transformparticle = 0;

      if ((point->flag & PEP_HIDE) == 0) {
        for (k = 0, key = point->keys; k < point->totkey; k++, key++) {
          if ((key->flag & PEK_HIDE) == 0) {
            if (key->flag & PEK_SELECT) {
              hasselected = 1;
              transformparticle = 1;
            }
            else if (is_prop_edit) {
              transformparticle = 1;
            }
          }
        }
      }

      if (transformparticle) {
        count += point->totkey;
        point->flag |= PEP_TRANSFORM;
      }
    }

    /* note: in prop mode we need at least 1 selected */
    if (hasselected == 0) {
      return;
    }

    tc->data_len = count;
    td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Particle Mode)");

    if (t->mode == TFM_BAKE_TIME) {
      tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                      "Particle_TransExtension");
    }
    else {
      tx = tc->data_ext = NULL;
    }

    unit_m4(mat);

    invert_m4_m4(ob->imat, ob->obmat);

    for (i = 0, point = edit->points; i < edit->totpoint; i++, point++) {
      TransData *head, *tail;
      head = tail = td;

      if (!(point->flag & PEP_TRANSFORM)) {
        continue;
      }

      if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
        ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
        psys_mat_hair_to_global(
            ob, psmd_eval->mesh_final, psys->part->from, psys->particles + i, mat);
      }

      for (k = 0, key = point->keys; k < point->totkey; k++, key++) {
        if (key->flag & PEK_USE_WCO) {
          copy_v3_v3(key->world_co, key->co);
          mul_m4_v3(mat, key->world_co);
          td->loc = key->world_co;
        }
        else {
          td->loc = key->co;
        }

        copy_v3_v3(td->iloc, td->loc);
        copy_v3_v3(td->center, td->loc);

        if (key->flag & PEK_SELECT) {
          td->flag |= TD_SELECTED;
        }
        else if (!is_prop_edit) {
          td->flag |= TD_SKIP;
        }

        unit_m3(td->mtx);
        unit_m3(td->smtx);

        /* don't allow moving roots */
        if (k == 0 && pset->flag & PE_LOCK_FIRST && (!psys || !(psys->flag & PSYS_GLOBAL_HAIR))) {
          td->protectflag |= OB_LOCK_LOC;
        }

        td->ob = ob;
        td->ext = tx;
        if (t->mode == TFM_BAKE_TIME) {
          td->val = key->time;
          td->ival = *(key->time);
          /* abuse size and quat for min/max values */
          td->flag |= TD_NO_EXT;
          if (k == 0) {
            tx->size = NULL;
          }
          else {
            tx->size = (key - 1)->time;
          }

          if (k == point->totkey - 1) {
            tx->quat = NULL;
          }
          else {
            tx->quat = (key + 1)->time;
          }
        }

        td++;
        if (tx) {
          tx++;
        }
        tail++;
      }
      if (is_prop_edit && head != tail) {
        calc_distanceCurveVerts(head, tail - 1);
      }
    }
  }
}

void flushTransParticles(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    PTCacheEdit *edit = PE_get_current(scene, ob);
    ParticleSystem *psys = edit->psys;
    PTCacheEditPoint *point;
    PTCacheEditKey *key;
    TransData *td;
    float mat[4][4], imat[4][4], co[3];
    int i, k;
    const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

    /* we do transform in world space, so flush world space position
     * back to particle local space (only for hair particles) */
    td = tc->data;
    for (i = 0, point = edit->points; i < edit->totpoint; i++, point++, td++) {
      if (!(point->flag & PEP_TRANSFORM)) {
        continue;
      }

      if (psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
        ParticleSystemModifierData *psmd_eval = edit->psmd_eval;
        psys_mat_hair_to_global(
            ob, psmd_eval->mesh_final, psys->part->from, psys->particles + i, mat);
        invert_m4_m4(imat, mat);

        for (k = 0, key = point->keys; k < point->totkey; k++, key++) {
          copy_v3_v3(co, key->world_co);
          mul_m4_v3(imat, co);

          /* optimization for proportional edit */
          if (!is_prop_edit || !compare_v3v3(key->co, co, 0.0001f)) {
            copy_v3_v3(key->co, co);
            point->flag |= PEP_EDIT_RECALC;
          }
        }
      }
      else {
        point->flag |= PEP_EDIT_RECALC;
      }
    }

    PE_update_object(t->depsgraph, scene, OBACT(view_layer), 1);
    BKE_particle_batch_cache_dirty_tag(psys, BKE_PARTICLE_BATCH_DIRTY_ALL);
    DEG_id_tag_update(&ob->id, ID_RECALC_PSYS_REDO);
  }
}

/* ********************* mesh ****************** */

static bool bmesh_test_dist_add(BMVert *v,
                                BMVert *v_other,
                                float *dists,
                                const float *dists_prev,
                                /* optionally track original index */
                                int *index,
                                const int *index_prev,
                                float mtx[3][3])
{
  if ((BM_elem_flag_test(v_other, BM_ELEM_SELECT) == 0) &&
      (BM_elem_flag_test(v_other, BM_ELEM_HIDDEN) == 0)) {
    const int i = BM_elem_index_get(v);
    const int i_other = BM_elem_index_get(v_other);
    float vec[3];
    float dist_other;
    sub_v3_v3v3(vec, v->co, v_other->co);
    mul_m3_v3(mtx, vec);

    dist_other = dists_prev[i] + len_v3(vec);
    if (dist_other < dists[i_other]) {
      dists[i_other] = dist_other;
      if (index != NULL) {
        index[i_other] = index_prev[i];
      }
      return true;
    }
  }

  return false;
}

/**
 * \param mtx: Measure distance in this space.
 * \param dists: Store the closest connected distance to selected vertices.
 * \param index: Optionally store the original index we're measuring the distance to (can be NULL).
 */
static void editmesh_set_connectivity_distance(BMesh *bm,
                                               float mtx[3][3],
                                               float *dists,
                                               int *index)
{
  BLI_LINKSTACK_DECLARE(queue, BMVert *);

  /* any BM_ELEM_TAG'd vertex is in 'queue_next', so we don't add in twice */
  BLI_LINKSTACK_DECLARE(queue_next, BMVert *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  {
    BMIter viter;
    BMVert *v;
    int i;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      float dist;
      BM_elem_index_set(v, i); /* set_inline */
      BM_elem_flag_disable(v, BM_ELEM_TAG);

      if (BM_elem_flag_test(v, BM_ELEM_SELECT) == 0 || BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        dist = FLT_MAX;
        if (index != NULL) {
          index[i] = i;
        }
      }
      else {
        BLI_LINKSTACK_PUSH(queue, v);
        dist = 0.0f;
        if (index != NULL) {
          index[i] = i;
        }
      }

      dists[i] = dist;
    }
    bm->elem_index_dirty &= ~BM_VERT;
  }

  /* need to be very careful of feedback loops here, store previous dist's to avoid feedback */
  float *dists_prev = MEM_dupallocN(dists);
  int *index_prev = MEM_dupallocN(index); /* may be NULL */

  do {
    BMVert *v;
    LinkNode *lnk;

    /* this is correct but slow to do each iteration,
     * instead sync the dist's while clearing BM_ELEM_TAG (below) */
#if 0
    memcpy(dists_prev, dists, sizeof(float) * bm->totvert);
#endif

    while ((v = BLI_LINKSTACK_POP(queue))) {
      BLI_assert(dists[BM_elem_index_get(v)] != FLT_MAX);

      /* connected edge-verts */
      if (v->e != NULL) {
        BMEdge *e_iter, *e_first;

        e_iter = e_first = v->e;

        /* would normally use BM_EDGES_OF_VERT, but this runs so often,
         * its faster to iterate on the data directly */
        do {

          if (BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN) == 0) {

            /* edge distance */
            {
              BMVert *v_other = BM_edge_other_vert(e_iter, v);
              if (bmesh_test_dist_add(v, v_other, dists, dists_prev, index, index_prev, mtx)) {
                if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == 0) {
                  BM_elem_flag_enable(v_other, BM_ELEM_TAG);
                  BLI_LINKSTACK_PUSH(queue_next, v_other);
                }
              }
            }

            /* face distance */
            if (e_iter->l) {
              BMLoop *l_iter_radial, *l_first_radial;
              /**
               * imaginary edge diagonally across quad,
               * \note, this takes advantage of the rules of winding that we
               * know 2 or more of a verts edges wont reference the same face twice.
               * Also, if the edge is hidden, the face will be hidden too.
               */
              l_iter_radial = l_first_radial = e_iter->l;

              do {
                if ((l_iter_radial->v == v) && (l_iter_radial->f->len == 4) &&
                    (BM_elem_flag_test(l_iter_radial->f, BM_ELEM_HIDDEN) == 0)) {
                  BMVert *v_other = l_iter_radial->next->next->v;
                  if (bmesh_test_dist_add(v, v_other, dists, dists_prev, index, index_prev, mtx)) {
                    if (BM_elem_flag_test(v_other, BM_ELEM_TAG) == 0) {
                      BM_elem_flag_enable(v_other, BM_ELEM_TAG);
                      BLI_LINKSTACK_PUSH(queue_next, v_other);
                    }
                  }
                }
              } while ((l_iter_radial = l_iter_radial->radial_next) != l_first_radial);
            }
          }
        } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);
      }
    }

    /* clear for the next loop */
    for (lnk = queue_next; lnk; lnk = lnk->next) {
      BMVert *v_link = lnk->link;
      const int i = BM_elem_index_get(v_link);

      BM_elem_flag_disable(v_link, BM_ELEM_TAG);

      /* keep in sync, avoid having to do full memcpy each iteration */
      dists_prev[i] = dists[i];
      if (index != NULL) {
        index_prev[i] = index[i];
      }
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

    /* none should be tagged now since 'queue_next' is empty */
    BLI_assert(BM_iter_mesh_count_flag(BM_VERTS_OF_MESH, bm, BM_ELEM_TAG, true) == 0);

  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);

  MEM_freeN(dists_prev);
  if (index_prev != NULL) {
    MEM_freeN(index_prev);
  }
}

static struct TransIslandData *editmesh_islands_info_calc(BMEditMesh *em,
                                                          int *r_island_tot,
                                                          int **r_island_vert_map,
                                                          bool calc_single_islands)
{
  BMesh *bm = em->bm;
  struct TransIslandData *trans_islands;
  char htype;
  char itype;
  int i;

  /* group vars */
  int *groups_array;
  int(*group_index)[2];
  int group_tot;
  void **ele_array;

  int *vert_map;

  if (em->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
    groups_array = MEM_mallocN(sizeof(*groups_array) * bm->totedgesel, __func__);
    group_tot = BM_mesh_calc_edge_groups(
        bm, groups_array, &group_index, NULL, NULL, BM_ELEM_SELECT);

    htype = BM_EDGE;
    itype = BM_VERTS_OF_EDGE;
  }
  else { /* (bm->selectmode & SCE_SELECT_FACE) */
    groups_array = MEM_mallocN(sizeof(*groups_array) * bm->totfacesel, __func__);
    group_tot = BM_mesh_calc_face_groups(
        bm, groups_array, &group_index, NULL, NULL, BM_ELEM_SELECT, BM_VERT);

    htype = BM_FACE;
    itype = BM_VERTS_OF_FACE;
  }

  trans_islands = MEM_mallocN(sizeof(*trans_islands) * group_tot, __func__);

  vert_map = MEM_mallocN(sizeof(*vert_map) * bm->totvert, __func__);
  /* we shouldn't need this, but with incorrect selection flushing
   * its possible we have a selected vertex that's not in a face,
   * for now best not crash in that case. */
  copy_vn_i(vert_map, bm->totvert, -1);

  BM_mesh_elem_table_ensure(bm, htype);
  ele_array = (htype == BM_FACE) ? (void **)bm->ftable : (void **)bm->etable;

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  /* may be an edge OR a face array */
  for (i = 0; i < group_tot; i++) {
    BMEditSelection ese = {NULL};

    const int fg_sta = group_index[i][0];
    const int fg_len = group_index[i][1];
    float co[3], no[3], tangent[3];
    int j;

    zero_v3(co);
    zero_v3(no);
    zero_v3(tangent);

    ese.htype = htype;

    /* loop on each face in this group:
     * - assign r_vert_map
     * - calculate (co, no)
     */
    for (j = 0; j < fg_len; j++) {
      float tmp_co[3], tmp_no[3], tmp_tangent[3];

      ese.ele = ele_array[groups_array[fg_sta + j]];

      BM_editselection_center(&ese, tmp_co);
      BM_editselection_normal(&ese, tmp_no);
      BM_editselection_plane(&ese, tmp_tangent);

      add_v3_v3(co, tmp_co);
      add_v3_v3(no, tmp_no);
      add_v3_v3(tangent, tmp_tangent);

      {
        /* setup vertex map */
        BMIter iter;
        BMVert *v;

        /* connected edge-verts */
        BM_ITER_ELEM (v, &iter, ese.ele, itype) {
          vert_map[BM_elem_index_get(v)] = i;
        }
      }
    }

    mul_v3_v3fl(trans_islands[i].co, co, 1.0f / (float)fg_len);

    if (createSpaceNormalTangent(trans_islands[i].axismtx, no, tangent)) {
      /* pass */
    }
    else {
      if (normalize_v3(no) != 0.0f) {
        axis_dominant_v3_to_m3(trans_islands[i].axismtx, no);
        invert_m3(trans_islands[i].axismtx);
      }
      else {
        unit_m3(trans_islands[i].axismtx);
      }
    }
  }

  MEM_freeN(groups_array);
  MEM_freeN(group_index);

  /* for PET we need islands of 1 so connected vertices can use it with V3D_AROUND_LOCAL_ORIGINS */
  if (calc_single_islands) {
    BMIter viter;
    BMVert *v;
    int group_tot_single = 0;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (vert_map[i] == -1)) {
        group_tot_single += 1;
      }
    }

    if (group_tot_single != 0) {
      trans_islands = MEM_reallocN(trans_islands,
                                   sizeof(*trans_islands) * (group_tot + group_tot_single));

      BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (vert_map[i] == -1)) {
          struct TransIslandData *v_island = &trans_islands[group_tot];
          vert_map[i] = group_tot;

          copy_v3_v3(v_island->co, v->co);

          if (is_zero_v3(v->no) != 0.0f) {
            axis_dominant_v3_to_m3(v_island->axismtx, v->no);
            invert_m3(v_island->axismtx);
          }
          else {
            unit_m3(v_island->axismtx);
          }

          group_tot += 1;
        }
      }
    }
  }

  *r_island_tot = group_tot;
  *r_island_vert_map = vert_map;

  return trans_islands;
}

/* way to overwrite what data is edited with transform */
static void VertsToTransData(TransInfo *t,
                             TransData *td,
                             TransDataExtension *tx,
                             BMEditMesh *em,
                             BMVert *eve,
                             float *bweight,
                             struct TransIslandData *v_island,
                             const bool no_island_center)
{
  float *no, _no[3];
  BLI_assert(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) == 0);

  td->flag = 0;
  // if (key)
  //  td->loc = key->co;
  // else
  td->loc = eve->co;
  copy_v3_v3(td->iloc, td->loc);

  if ((t->mode == TFM_SHRINKFATTEN) && (em->selectmode & SCE_SELECT_FACE) &&
      BM_elem_flag_test(eve, BM_ELEM_SELECT) &&
      (BM_vert_calc_normal_ex(eve, BM_ELEM_SELECT, _no))) {
    no = _no;
  }
  else {
    no = eve->no;
  }

  if (v_island) {
    if (no_island_center) {
      copy_v3_v3(td->center, td->loc);
    }
    else {
      copy_v3_v3(td->center, v_island->co);
    }
    copy_m3_m3(td->axismtx, v_island->axismtx);
  }
  else if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
    copy_v3_v3(td->center, td->loc);
    createSpaceNormal(td->axismtx, no);
  }
  else {
    copy_v3_v3(td->center, td->loc);

    /* Setting normals */
    copy_v3_v3(td->axismtx[2], no);
    td->axismtx[0][0] = td->axismtx[0][1] = td->axismtx[0][2] = td->axismtx[1][0] =
        td->axismtx[1][1] = td->axismtx[1][2] = 0.0f;
  }

  td->ext = NULL;
  td->val = NULL;
  td->extra = NULL;
  if (t->mode == TFM_BWEIGHT) {
    td->val = bweight;
    td->ival = *bweight;
  }
  else if (t->mode == TFM_SKIN_RESIZE) {
    MVertSkin *vs = CustomData_bmesh_get(&em->bm->vdata, eve->head.data, CD_MVERT_SKIN);
    if (vs) {
      /* skin node size */
      td->ext = tx;
      copy_v3_v3(tx->isize, vs->radius);
      tx->size = vs->radius;
      td->val = vs->radius;
    }
    else {
      td->flag |= TD_SKIP;
    }
  }
  else if (t->mode == TFM_SHRINKFATTEN) {
    td->ext = tx;
    tx->isize[0] = BM_vert_calc_shell_factor_ex(eve, no, BM_ELEM_SELECT);
  }
}

static void createTransEditVerts(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *tob = NULL;
    TransDataExtension *tx = NULL;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    Mesh *me = tc->obedit->data;
    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;
    float(*mappedcos)[3] = NULL, (*quats)[4] = NULL;
    float mtx[3][3], smtx[3][3], (*defmats)[3][3] = NULL, (*defcos)[3] = NULL;
    float *dists = NULL;
    int a;
    const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;
    int mirror = 0;
    int cd_vert_bweight_offset = -1;
    bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;

    struct TransIslandData *island_info = NULL;
    int island_info_tot;
    int *island_vert_map = NULL;

    /* Snap rotation along normal needs a common axis for whole islands,
     * otherwise one get random crazy results, see T59104.
     * However, we do not want to use the island center for the pivot/translation reference. */
    const bool is_snap_rotate = ((t->mode == TFM_TRANSLATION) &&
                                 /* There is not guarantee that snapping
                                  * is initialized yet at this point... */
                                 (usingSnappingNormal(t) ||
                                  (t->settings->snap_flag & SCE_SNAP_ROTATE) != 0) &&
                                 (t->around != V3D_AROUND_LOCAL_ORIGINS));
    /* Even for translation this is needed because of island-orientation, see: T51651. */
    const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS) || is_snap_rotate;
    /* Original index of our connected vertex when connected distances are calculated.
     * Optional, allocate if needed. */
    int *dists_index = NULL;

    if (tc->mirror.axis_flag) {
      EDBM_verts_mirror_cache_begin(em, 0, false, (t->flag & T_PROP_EDIT) == 0, use_topology);
      mirror = 1;
    }

    /**
     * Quick check if we can transform.
     *
     * \note ignore modes here, even in edge/face modes,
     * transform data is created by selected vertices.
     * \note in prop mode we need at least 1 selected.
     */
    if (bm->totvertsel == 0) {
      goto cleanup;
    }

    if (t->mode == TFM_BWEIGHT) {
      BM_mesh_cd_flag_ensure(bm, BKE_mesh_from_object(tc->obedit), ME_CDFLAG_VERT_BWEIGHT);
      cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
    }

    if (prop_mode) {
      unsigned int count = 0;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          count++;
        }
      }

      tc->data_len = count;

      /* allocating scratch arrays */
      if (prop_mode & T_PROP_CONNECTED) {
        dists = MEM_mallocN(em->bm->totvert * sizeof(float), __func__);
        if (is_island_center) {
          dists_index = MEM_mallocN(em->bm->totvert * sizeof(int), __func__);
        }
      }
    }
    else {
      tc->data_len = bm->totvertsel;
    }

    tob = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Mesh EditMode)");
    if (ELEM(t->mode, TFM_SKIN_RESIZE, TFM_SHRINKFATTEN)) {
      /* warning, this is overkill, we only need 2 extra floats,
       * but this stores loads of extra stuff, for TFM_SHRINKFATTEN its even more overkill
       * since we may not use the 'alt' transform mode to maintain shell thickness,
       * but with generic transform code its hard to lazy init vars */
      tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension),
                                      "TransObData ext");
    }

    copy_m3_m4(mtx, tc->obedit->obmat);
    /* we use a pseudo-inverse so that when one of the axes is scaled to 0,
     * matrix inversion still works and we can still moving along the other */
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    if (prop_mode & T_PROP_CONNECTED) {
      editmesh_set_connectivity_distance(em->bm, mtx, dists, dists_index);
    }

    if (is_island_center) {
      /* In this specific case, near-by vertices will need to know
       * the island of the nearest connected vertex. */
      const bool calc_single_islands = ((prop_mode & T_PROP_CONNECTED) &&
                                        (t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                        (em->selectmode & SCE_SELECT_VERTEX));

      island_info = editmesh_islands_info_calc(
          em, &island_info_tot, &island_vert_map, calc_single_islands);
    }

    /* detect CrazySpace [tm] */
    if (modifiers_getCageIndex(t->scene, tc->obedit, NULL, 1) != -1) {
      int totleft = -1;
      if (modifiers_isCorrectableDeformed(t->scene, tc->obedit)) {
        BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

        /* Use evaluated state because we need b-bone cache. */
        Scene *scene_eval = (Scene *)DEG_get_evaluated_id(t->depsgraph, &t->scene->id);
        Object *obedit_eval = (Object *)DEG_get_evaluated_id(t->depsgraph, &tc->obedit->id);
        BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
        /* check if we can use deform matrices for modifier from the
         * start up to stack, they are more accurate than quats */
        totleft = BKE_crazyspace_get_first_deform_matrices_editbmesh(
            t->depsgraph, scene_eval, obedit_eval, em_eval, &defmats, &defcos);
      }

      /* if we still have more modifiers, also do crazyspace
       * correction with quats, relative to the coordinates after
       * the modifiers that support deform matrices (defcos) */

#if 0 /* TODO, fix crazyspace+extrude so it can be enabled for general use - campbell */
      if ((totleft > 0) || (totleft == -1))
#else
      if (totleft > 0)
#endif
      {
        mappedcos = BKE_crazyspace_get_mapped_editverts(t->depsgraph, tc->obedit);
        quats = MEM_mallocN(em->bm->totvert * sizeof(*quats), "crazy quats");
        BKE_crazyspace_set_quats_editmesh(em, defcos, mappedcos, quats, !prop_mode);
        if (mappedcos) {
          MEM_freeN(mappedcos);
        }
      }

      if (defcos) {
        MEM_freeN(defcos);
      }
    }

    /* find out which half we do */
    if (mirror) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT) && eve->co[0] != 0.0f) {
          if (eve->co[0] < 0.0f) {
            tc->mirror.sign = -1.0f;
            mirror = -1;
          }
          break;
        }
      }
    }

    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
      if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          struct TransIslandData *v_island = NULL;
          float *bweight = (cd_vert_bweight_offset != -1) ?
                               BM_ELEM_CD_GET_VOID_P(eve, cd_vert_bweight_offset) :
                               NULL;

          if (island_info) {
            const int connected_index = (dists_index && dists_index[a] != -1) ? dists_index[a] : a;
            v_island = (island_vert_map[connected_index] != -1) ?
                           &island_info[island_vert_map[connected_index]] :
                           NULL;
          }

          /* Do not use the island center in case we are using islands
           * only to get axis for snap/rotate to normal... */
          VertsToTransData(t, tob, tx, em, eve, bweight, v_island, is_snap_rotate);
          if (tx) {
            tx++;
          }

          /* selected */
          if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            tob->flag |= TD_SELECTED;
          }

          if (prop_mode) {
            if (prop_mode & T_PROP_CONNECTED) {
              tob->dist = dists[a];
            }
            else {
              tob->flag |= TD_NOTCONNECTED;
              tob->dist = FLT_MAX;
            }
          }

          /* CrazySpace */
          const bool use_quats = quats && BM_elem_flag_test(eve, BM_ELEM_TAG);
          if (use_quats || defmats) {
            float mat[3][3], qmat[3][3], imat[3][3];

            /* Use both or either quat and defmat correction. */
            if (use_quats) {
              quat_to_mat3(qmat, quats[BM_elem_index_get(eve)]);

              if (defmats) {
                mul_m3_series(mat, defmats[a], qmat, mtx);
              }
              else {
                mul_m3_m3m3(mat, mtx, qmat);
              }
            }
            else {
              mul_m3_m3m3(mat, mtx, defmats[a]);
            }

            invert_m3_m3(imat, mat);

            copy_m3_m3(tob->smtx, imat);
            copy_m3_m3(tob->mtx, mat);
          }
          else {
            copy_m3_m3(tob->smtx, smtx);
            copy_m3_m3(tob->mtx, mtx);
          }

          /* Mirror? */
          if ((mirror > 0 && tob->iloc[0] > 0.0f) || (mirror < 0 && tob->iloc[0] < 0.0f)) {
            BMVert *vmir = EDBM_verts_mirror_get(em, eve);  // t->obedit, em, eve, tob->iloc, a);
            if (vmir && vmir != eve) {
              tob->extra = vmir;
            }
          }
          tob++;
        }
      }
    }

    if (island_info) {
      MEM_freeN(island_info);
      MEM_freeN(island_vert_map);
    }

    if (mirror != 0) {
      tob = tc->data;
      for (a = 0; a < tc->data_len; a++, tob++) {
        if (ABS(tob->loc[0]) <= 0.00001f) {
          tob->flag |= TD_MIRROR_EDGE;
        }
      }
    }

  cleanup:
    /* crazy space free */
    if (quats) {
      MEM_freeN(quats);
    }
    if (defmats) {
      MEM_freeN(defmats);
    }
    if (dists) {
      MEM_freeN(dists);
    }
    if (dists_index) {
      MEM_freeN(dists_index);
    }

    if (tc->mirror.axis_flag) {
      EDBM_verts_mirror_cache_end(em);
    }
  }
}

/* *** NODE EDITOR *** */
void flushTransNodes(TransInfo *t)
{
  const float dpi_fac = UI_DPI_FAC;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    int a;
    TransData *td;
    TransData2D *td2d;

    applyGridAbsolute(t);

    /* flush to 2d vector from internally used 3d vector */
    for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {
      bNode *node = td->extra;
      float locx, locy;

      /* weirdo - but the node system is a mix of free 2d elements and dpi sensitive UI */
#ifdef USE_NODE_CENTER
      locx = (td2d->loc[0] - (BLI_rctf_size_x(&node->totr)) * +0.5f) / dpi_fac;
      locy = (td2d->loc[1] - (BLI_rctf_size_y(&node->totr)) * -0.5f) / dpi_fac;
#else
      locx = td2d->loc[0] / dpi_fac;
      locy = td2d->loc[1] / dpi_fac;
#endif

      /* account for parents (nested nodes) */
      if (node->parent) {
        nodeFromView(node->parent, locx, locy, &node->locx, &node->locy);
      }
      else {
        node->locx = locx;
        node->locy = locy;
      }
    }

    /* handle intersection with noodles */
    if (tc->data_len == 1) {
      ED_node_link_intersect_test(t->sa, 1);
    }
  }
}

/* *** SEQUENCE EDITOR *** */

/* commented _only_ because the meta may have animation data which
 * needs moving too [#28158] */

#define SEQ_TX_NESTED_METAS

BLI_INLINE void trans_update_seq(Scene *sce, Sequence *seq, int old_start, int sel_flag)
{
  if (seq->depth == 0) {
    /* Calculate this strip and all nested strips.
     * Children are ALWAYS transformed first so we don't need to do this in another loop.
     */
    BKE_sequence_calc(sce, seq);
  }
  else {
    BKE_sequence_calc_disp(sce, seq);
  }

  if (sel_flag == SELECT) {
    BKE_sequencer_offset_animdata(sce, seq, seq->start - old_start);
  }
}

void flushTransSeq(TransInfo *t)
{
  /* Editing null check already done */
  ListBase *seqbasep = BKE_sequencer_editing_get(t->scene, false)->seqbasep;

  int a, new_frame;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataSeq *tdsq = NULL;
  Sequence *seq;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* prevent updating the same seq twice
   * if the transdata order is changed this will mess up
   * but so will TransDataSeq */
  Sequence *seq_prev = NULL;
  int old_start_prev = 0, sel_flag_prev = 0;

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {
    int old_start;
    tdsq = (TransDataSeq *)td->extra;
    seq = tdsq->seq;
    old_start = seq->start;
    new_frame = round_fl_to_int(td2d->loc[0]);

    switch (tdsq->sel_flag) {
      case SELECT:
#ifdef SEQ_TX_NESTED_METAS
        if ((seq->depth != 0 || BKE_sequence_tx_test(seq))) {
          /* for meta's, their children move */
          seq->start = new_frame - tdsq->start_offset;
        }
#else
        if (seq->type != SEQ_TYPE_META && (seq->depth != 0 || seq_tx_test(seq))) {
          /* for meta's, their children move */
          seq->start = new_frame - tdsq->start_offset;
        }
#endif
        if (seq->depth == 0) {
          seq->machine = round_fl_to_int(td2d->loc[1]);
          CLAMP(seq->machine, 1, MAXSEQ);
        }
        break;
      case SEQ_LEFTSEL: /* no vertical transform  */
        BKE_sequence_tx_set_final_left(seq, new_frame);
        BKE_sequence_tx_handle_xlimits(seq, tdsq->flag & SEQ_LEFTSEL, tdsq->flag & SEQ_RIGHTSEL);

        /* todo - move this into aftertrans update? - old seq tx needed it anyway */
        BKE_sequence_single_fix(seq);
        break;
      case SEQ_RIGHTSEL: /* no vertical transform  */
        BKE_sequence_tx_set_final_right(seq, new_frame);
        BKE_sequence_tx_handle_xlimits(seq, tdsq->flag & SEQ_LEFTSEL, tdsq->flag & SEQ_RIGHTSEL);

        /* todo - move this into aftertrans update? - old seq tx needed it anyway */
        BKE_sequence_single_fix(seq);
        break;
    }

    /* Update *previous* seq! Else, we would update a seq after its first transform,
     * and if it has more than one (like e.g. SEQ_LEFTSEL and SEQ_RIGHTSEL),
     * the others are not updated! See T38469.
     */
    if (seq != seq_prev) {
      if (seq_prev) {
        trans_update_seq(t->scene, seq_prev, old_start_prev, sel_flag_prev);
      }

      seq_prev = seq;
      old_start_prev = old_start;
      sel_flag_prev = tdsq->sel_flag;
    }
    else {
      /* We want to accumulate *all* sel_flags for this seq! */
      sel_flag_prev |= tdsq->sel_flag;
    }
  }

  /* Don't forget to update the last seq! */
  if (seq_prev) {
    trans_update_seq(t->scene, seq_prev, old_start_prev, sel_flag_prev);
  }

  /* originally TFM_TIME_EXTEND, transform changes */
  if (ELEM(t->mode, TFM_SEQ_SLIDE, TFM_TIME_TRANSLATE)) {
    /* Special annoying case here, need to calc metas with TFM_TIME_EXTEND only */

    /* calc all meta's then effects [#27953] */
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->type == SEQ_TYPE_META && seq->flag & SELECT) {
        BKE_sequence_calc(t->scene, seq);
      }
    }
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->seq1 || seq->seq2 || seq->seq3) {
        BKE_sequence_calc(t->scene, seq);
      }
    }

    /* update effects inside meta's */
    for (a = 0, seq_prev = NULL, td = tc->data, td2d = tc->data_2d; a < tc->data_len;
         a++, td++, td2d++, seq_prev = seq) {
      tdsq = (TransDataSeq *)td->extra;
      seq = tdsq->seq;
      if ((seq != seq_prev) && (seq->depth != 0)) {
        if (seq->seq1 || seq->seq2 || seq->seq3) {
          BKE_sequence_calc(t->scene, seq);
        }
      }
    }
  }

  /* need to do the overlap check in a new loop otherwise adjacent strips
   * will not be updated and we'll get false positives */
  seq_prev = NULL;
  for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {

    tdsq = (TransDataSeq *)td->extra;
    seq = tdsq->seq;

    if (seq != seq_prev) {
      if (seq->depth == 0) {
        /* test overlap, displays red outline */
        seq->flag &= ~SEQ_OVERLAP;
        if (BKE_sequence_test_overlap(seqbasep, seq)) {
          seq->flag |= SEQ_OVERLAP;
        }
      }
    }
    seq_prev = seq;
  }
}

/* ********************* UV ****************** */

static void UVsToTransData(const float aspect[2],
                           TransData *td,
                           TransData2D *td2d,
                           float *uv,
                           const float *center,
                           bool selected)
{
  /* uv coords are scaled by aspects. this is needed for rotations and
   * proportional editing to be consistent with the stretched uv coords
   * that are displayed. this also means that for display and numinput,
   * and when the uv coords are flushed, these are converted each time */
  td2d->loc[0] = uv[0] * aspect[0];
  td2d->loc[1] = uv[1] * aspect[1];
  td2d->loc[2] = 0.0f;
  td2d->loc2d = uv;

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v2_v2(td->center, center ? center : td->loc);
  td->center[2] = 0.0f;
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  if (selected) {
    td->flag |= TD_SELECTED;
    td->dist = 0.0;
  }
  else {
    td->dist = FLT_MAX;
  }
  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

static void createTransUVs(bContext *C, TransInfo *t)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = CTX_data_edit_image(C);
  Scene *scene = t->scene;
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = NULL;
    TransData2D *td2d = NULL;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMFace *efa;
    BMIter iter, liter;
    UvElementMap *elementmap = NULL;
    BLI_bitmap *island_enabled = NULL;
    struct {
      float co[2];
      int co_num;
    } *island_center = NULL;
    int count = 0, countsel = 0, count_rejected = 0;
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (!ED_space_image_show_uvedit(sima, tc->obedit)) {
      continue;
    }

    /* count */
    if (is_prop_connected || is_island_center) {
      /* create element map with island information */
      const bool use_facesel = (ts->uv_flag & UV_SYNC_SELECTION) == 0;
      elementmap = BM_uv_element_map_create(em->bm, use_facesel, false, true);
      if (elementmap == NULL) {
        return;
      }

      if (is_prop_connected) {
        island_enabled = BLI_BITMAP_NEW(elementmap->totalIslands, "TransIslandData(UV Editing)");
      }

      if (is_island_center) {
        island_center = MEM_callocN(sizeof(*island_center) * elementmap->totalIslands, __func__);
      }
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!uvedit_face_visible_test(scene, tc->obedit, ima, efa)) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        continue;
      }

      BM_elem_flag_enable(efa, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          countsel++;

          if (is_prop_connected || island_center) {
            UvElement *element = BM_uv_element_get(elementmap, efa, l);

            if (is_prop_connected) {
              BLI_BITMAP_ENABLE(island_enabled, element->island);
            }

            if (is_island_center) {
              if (element->flag == false) {
                MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
                add_v2_v2(island_center[element->island].co, luv->uv);
                island_center[element->island].co_num++;
                element->flag = true;
              }
            }
          }
        }

        if (is_prop_edit) {
          count++;
        }
      }
    }

    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
      goto finally;
    }

    if (is_island_center) {
      int i;

      for (i = 0; i < elementmap->totalIslands; i++) {
        mul_v2_fl(island_center[i].co, 1.0f / island_center[i].co_num);
        mul_v2_v2(island_center[i].co, t->aspect);
      }
    }

    tc->data_len = (is_prop_edit) ? count : countsel;
    tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(UV Editing)");
    /* for each 2d uv coord a 3d vector is allocated, so that they can be
     * treated just as if they were 3d verts */
    tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransObData2D(UV Editing)");

    if (sima->flag & SI_CLIP_UV) {
      t->flag |= T_CLIP_UV;
    }

    td = tc->data;
    td2d = tc->data_2d;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const bool selected = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
        MLoopUV *luv;
        const float *center = NULL;

        if (!is_prop_edit && !selected) {
          continue;
        }

        if (is_prop_connected || is_island_center) {
          UvElement *element = BM_uv_element_get(elementmap, efa, l);

          if (is_prop_connected) {
            if (!BLI_BITMAP_TEST(island_enabled, element->island)) {
              count_rejected++;
              continue;
            }
          }

          if (is_island_center) {
            center = island_center[element->island].co;
          }
        }

        BM_elem_flag_enable(l, BM_ELEM_TAG);
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        UVsToTransData(t->aspect, td++, td2d++, luv->uv, center, selected);
      }
    }

    if (is_prop_connected) {
      tc->data_len -= count_rejected;
    }

    if (sima->flag & SI_LIVE_UNWRAP) {
      ED_uvedit_live_unwrap_begin(t->scene, tc->obedit);
    }

  finally:
    if (is_prop_connected || is_island_center) {
      BM_uv_element_map_free(elementmap);

      if (is_prop_connected) {
        MEM_freeN(island_enabled);
      }

      if (island_center) {
        MEM_freeN(island_center);
      }
    }
  }
}

void flushTransUVs(TransInfo *t)
{
  SpaceImage *sima = t->sa->spacedata.first;
  const bool use_pixel_snap = ((sima->pixel_snap_mode != SI_PIXEL_SNAP_DISABLED) &&
                               (t->state != TRANS_CANCEL));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData2D *td;
    int a;
    float aspect_inv[2], size[2];

    aspect_inv[0] = 1.0f / t->aspect[0];
    aspect_inv[1] = 1.0f / t->aspect[1];

    if (use_pixel_snap) {
      int size_i[2];
      ED_space_image_get_size(sima, &size_i[0], &size_i[1]);
      size[0] = size_i[0];
      size[1] = size_i[1];
    }

    /* flush to 2d vector from internally used 3d vector */
    for (a = 0, td = tc->data_2d; a < tc->data_len; a++, td++) {
      td->loc2d[0] = td->loc[0] * aspect_inv[0];
      td->loc2d[1] = td->loc[1] * aspect_inv[1];

      if (use_pixel_snap) {
        td->loc2d[0] *= size[0];
        td->loc2d[1] *= size[1];

        switch (sima->pixel_snap_mode) {
          case SI_PIXEL_SNAP_CENTER:
            td->loc2d[0] = roundf(td->loc2d[0] - 0.5f) + 0.5f;
            td->loc2d[1] = roundf(td->loc2d[1] - 0.5f) + 0.5f;
            break;
          case SI_PIXEL_SNAP_CORNER:
            td->loc2d[0] = roundf(td->loc2d[0]);
            td->loc2d[1] = roundf(td->loc2d[1]);
            break;
        }

        td->loc2d[0] /= size[0];
        td->loc2d[1] /= size[1];
      }
    }
  }
}

bool clipUVTransform(TransInfo *t, float vec[2], const bool resize)
{
  bool clipx = true, clipy = true;
  float min[2], max[2];

  min[0] = min[1] = 0.0f;
  max[0] = t->aspect[0];
  max[1] = t->aspect[1];

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td;
    int a;

    for (a = 0, td = tc->data; a < tc->data_len; a++, td++) {
      minmax_v2v2_v2(min, max, td->loc);
    }
  }

  if (resize) {
    if (min[0] < 0.0f && t->center_global[0] > 0.0f && t->center_global[0] < t->aspect[0] * 0.5f) {
      vec[0] *= t->center_global[0] / (t->center_global[0] - min[0]);
    }
    else if (max[0] > t->aspect[0] && t->center_global[0] < t->aspect[0]) {
      vec[0] *= (t->center_global[0] - t->aspect[0]) / (t->center_global[0] - max[0]);
    }
    else {
      clipx = 0;
    }

    if (min[1] < 0.0f && t->center_global[1] > 0.0f && t->center_global[1] < t->aspect[1] * 0.5f) {
      vec[1] *= t->center_global[1] / (t->center_global[1] - min[1]);
    }
    else if (max[1] > t->aspect[1] && t->center_global[1] < t->aspect[1]) {
      vec[1] *= (t->center_global[1] - t->aspect[1]) / (t->center_global[1] - max[1]);
    }
    else {
      clipy = 0;
    }
  }
  else {
    if (min[0] < 0.0f) {
      vec[0] -= min[0];
    }
    else if (max[0] > t->aspect[0]) {
      vec[0] -= max[0] - t->aspect[0];
    }
    else {
      clipx = 0;
    }

    if (min[1] < 0.0f) {
      vec[1] -= min[1];
    }
    else if (max[1] > t->aspect[1]) {
      vec[1] -= max[1] - t->aspect[1];
    }
    else {
      clipy = 0;
    }
  }

  return (clipx || clipy);
}

void clipUVData(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (int a = 0; a < tc->data_len; a++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if ((td->flag & TD_SKIP) || (!td->loc)) {
        continue;
      }

      td->loc[0] = min_ff(max_ff(0.0f, td->loc[0]), t->aspect[0]);
      td->loc[1] = min_ff(max_ff(0.0f, td->loc[1]), t->aspect[1]);
    }
  }
}

/* ********************* ANIMATION EDITORS (GENERAL) ************************* */

/* This function tests if a point is on the "mouse" side of the cursor/frame-marking */
static bool FrameOnMouseSide(char side, float frame, float cframe)
{
  /* both sides, so it doesn't matter */
  if (side == 'B') {
    return true;
  }

  /* only on the named side */
  if (side == 'R') {
    return (frame >= cframe);
  }
  else {
    return (frame <= cframe);
  }
}

/* ********************* NLA EDITOR ************************* */

static void createTransNlaData(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  SpaceNla *snla = NULL;
  TransData *td = NULL;
  TransDataNla *tdn = NULL;

  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  int count = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* determine what type of data we are operating on */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }
  snla = (SpaceNla *)ac.sl;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  if (t->mode == TFM_TIME_EXTEND) {
    /* only side on which mouse is gets transformed */
    float xmouse, ymouse;

    UI_view2d_region_to_view(&ac.ar->v2d, t->mouse.imval[0], t->mouse.imval[1], &xmouse, &ymouse);
    t->frame_side = (xmouse > CFRA) ? 'R' : 'L';
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

  /* loop 1: count how many strips are selected (consider each strip as 2 points) */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    /* make some meta-strips for chains of selected strips */
    BKE_nlastrips_make_metas(&nlt->strips, 1);

    /* only consider selected strips */
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      // TODO: we can make strips have handles later on...
      /* transition strips can't get directly transformed */
      if (strip->type != NLASTRIP_TYPE_TRANSITION) {
        if (strip->flag & NLASTRIP_FLAG_SELECT) {
          if (FrameOnMouseSide(t->frame_side, strip->start, (float)CFRA)) {
            count++;
          }
          if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA)) {
            count++;
          }
        }
      }
    }
  }

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    /* clear temp metas that may have been created but aren't needed now
     * because they fell on the wrong side of CFRA
     */
    for (ale = anim_data.first; ale; ale = ale->next) {
      NlaTrack *nlt = (NlaTrack *)ale->data;
      BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
    }

    /* cleanup temp list */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  /* allocate memory for data */
  tc->data_len = count;

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(NLA Editor)");
  td = tc->data;
  tc->custom.type.data = tdn = MEM_callocN(tc->data_len * sizeof(TransDataNla),
                                           "TransDataNla (NLA Editor)");
  tc->custom.type.use_free = true;

  /* loop 2: build transdata array */
  for (ale = anim_data.first; ale; ale = ale->next) {
    /* only if a real NLA-track */
    if (ale->type == ANIMTYPE_NLATRACK) {
      AnimData *adt = ale->adt;
      NlaTrack *nlt = (NlaTrack *)ale->data;
      NlaStrip *strip;

      /* only consider selected strips */
      for (strip = nlt->strips.first; strip; strip = strip->next) {
        // TODO: we can make strips have handles later on...
        /* transition strips can't get directly transformed */
        if (strip->type != NLASTRIP_TYPE_TRANSITION) {
          if (strip->flag & NLASTRIP_FLAG_SELECT) {
            /* our transform data is constructed as follows:
             * - only the handles on the right side of the current-frame get included
             * - td structs are transform-elements operated on by the transform system
             *   and represent a single handle. The storage/pointer used (val or loc) depends on
             *   whether we're scaling or transforming. Ultimately though, the handles
             *   the td writes to will simply be a dummy in tdn
             * - for each strip being transformed, a single tdn struct is used, so in some
             *   cases, there will need to be 1 of these tdn elements in the array skipped...
             */
            float center[3], yval;

            /* firstly, init tdn settings */
            tdn->id = ale->id;
            tdn->oldTrack = tdn->nlt = nlt;
            tdn->strip = strip;
            tdn->trackIndex = BLI_findindex(&adt->nla_tracks, nlt);

            yval = (float)(tdn->trackIndex * NLACHANNEL_STEP(snla));

            tdn->h1[0] = strip->start;
            tdn->h1[1] = yval;
            tdn->h2[0] = strip->end;
            tdn->h2[1] = yval;

            center[0] = (float)CFRA;
            center[1] = yval;
            center[2] = 0.0f;

            /* set td's based on which handles are applicable */
            if (FrameOnMouseSide(t->frame_side, strip->start, (float)CFRA)) {
              /* just set tdn to assume that it only has one handle for now */
              tdn->handle = -1;

              /* now, link the transform data up to this data */
              if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
                td->loc = tdn->h1;
                copy_v3_v3(td->iloc, tdn->h1);

                /* store all the other gunk that is required by transform */
                copy_v3_v3(td->center, center);
                memset(td->axismtx, 0, sizeof(td->axismtx));
                td->axismtx[2][2] = 1.0f;

                td->ext = NULL;
                td->val = NULL;

                td->flag |= TD_SELECTED;
                td->dist = 0.0f;

                unit_m3(td->mtx);
                unit_m3(td->smtx);
              }
              else {
                /* time scaling only needs single value */
                td->val = &tdn->h1[0];
                td->ival = tdn->h1[0];
              }

              td->extra = tdn;
              td++;
            }
            if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA)) {
              /* if tdn is already holding the start handle,
               * then we're doing both, otherwise, only end */
              tdn->handle = (tdn->handle) ? 2 : 1;

              /* now, link the transform data up to this data */
              if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
                td->loc = tdn->h2;
                copy_v3_v3(td->iloc, tdn->h2);

                /* store all the other gunk that is required by transform */
                copy_v3_v3(td->center, center);
                memset(td->axismtx, 0, sizeof(td->axismtx));
                td->axismtx[2][2] = 1.0f;

                td->ext = NULL;
                td->val = NULL;

                td->flag |= TD_SELECTED;
                td->dist = 0.0f;

                unit_m3(td->mtx);
                unit_m3(td->smtx);
              }
              else {
                /* time scaling only needs single value */
                td->val = &tdn->h2[0];
                td->ival = tdn->h2[0];
              }

              td->extra = tdn;
              td++;
            }

            /* If both handles were used, skip the next tdn (i.e. leave it blank)
             * since the counting code is dumb.
             * Otherwise, just advance to the next one.
             */
            if (tdn->handle == 2) {
              tdn += 2;
            }
            else {
              tdn++;
            }
          }
        }
      }
    }
  }

  /* cleanup temp list */
  ANIM_animdata_freelist(&anim_data);
}

/* ********************* ACTION EDITOR ****************** */

static int gpf_cmp_frame(void *thunk, const void *a, const void *b)
{
  const bGPDframe *frame_a = a;
  const bGPDframe *frame_b = b;

  if (frame_a->framenum < frame_b->framenum) {
    return -1;
  }
  if (frame_a->framenum > frame_b->framenum) {
    return 1;
  }
  *((bool *)thunk) = true;
  /* selected last */
  if ((frame_a->flag & GP_FRAME_SELECT) && ((frame_b->flag & GP_FRAME_SELECT) == 0)) {
    return 1;
  }
  return 0;
}

static int masklay_shape_cmp_frame(void *thunk, const void *a, const void *b)
{
  const MaskLayerShape *frame_a = a;
  const MaskLayerShape *frame_b = b;

  if (frame_a->frame < frame_b->frame) {
    return -1;
  }
  if (frame_a->frame > frame_b->frame) {
    return 1;
  }
  *((bool *)thunk) = true;
  /* selected last */
  if ((frame_a->flag & MASK_SHAPE_SELECT) && ((frame_b->flag & MASK_SHAPE_SELECT) == 0)) {
    return 1;
  }
  return 0;
}

/* Called by special_aftertrans_update to make sure selected gp-frames replace
 * any other gp-frames which may reside on that frame (that are not selected).
 * It also makes sure gp-frames are still stored in chronological order after
 * transform.
 */
static void posttrans_gpd_clean(bGPdata *gpd)
{
  bGPDlayer *gpl;

  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    bGPDframe *gpf, *gpfn;
    bool is_double = false;

    BLI_listbase_sort_r(&gpl->frames, gpf_cmp_frame, &is_double);

    if (is_double) {
      for (gpf = gpl->frames.first; gpf; gpf = gpfn) {
        gpfn = gpf->next;
        if (gpfn && gpf->framenum == gpfn->framenum) {
          BKE_gpencil_layer_delframe(gpl, gpf);
        }
      }
    }

#ifdef DEBUG
    for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
      BLI_assert(!gpf->next || gpf->framenum < gpf->next->framenum);
    }
#endif
  }
  /* set cache flag to dirty */
  DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
}

static void posttrans_mask_clean(Mask *mask)
{
  MaskLayer *masklay;

  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskLayerShape *masklay_shape, *masklay_shape_next;
    bool is_double = false;

    BLI_listbase_sort_r(&masklay->splines_shapes, masklay_shape_cmp_frame, &is_double);

    if (is_double) {
      for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
           masklay_shape = masklay_shape_next) {
        masklay_shape_next = masklay_shape->next;
        if (masklay_shape_next && masklay_shape->frame == masklay_shape_next->frame) {
          BKE_mask_layer_shape_unlink(masklay, masklay_shape);
        }
      }
    }

#ifdef DEBUG
    for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
         masklay_shape = masklay_shape->next) {
      BLI_assert(!masklay_shape->next || masklay_shape->frame < masklay_shape->next->frame);
    }
#endif
  }
}

/* Time + Average value */
typedef struct tRetainedKeyframe {
  struct tRetainedKeyframe *next, *prev;
  float frame; /* frame to cluster around */
  float val;   /* average value */

  size_t tot_count; /* number of keyframes that have been averaged */
  size_t del_count; /* number of keyframes of this sort that have been deleted so far */
} tRetainedKeyframe;

/* Called during special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 */
static void posttrans_fcurve_clean(FCurve *fcu, const bool use_handle)
{
  /* NOTE: We assume that all keys are sorted */
  ListBase retained_keys = {NULL, NULL};
  const bool can_average_points = ((fcu->flag & (FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES)) ==
                                   0);

  /* sanity checks */
  if ((fcu->totvert == 0) || (fcu->bezt == NULL)) {
    return;
  }

  /* 1) Identify selected keyframes, and average the values on those
   * in case there are collisions due to multiple keys getting scaled
   * to all end up on the same frame
   */
  for (int i = 0; i < fcu->totvert; i++) {
    BezTriple *bezt = &fcu->bezt[i];

    if (BEZT_ISSEL_ANY(bezt)) {
      bool found = false;

      /* If there's another selected frame here, merge it */
      for (tRetainedKeyframe *rk = retained_keys.last; rk; rk = rk->prev) {
        if (IS_EQT(rk->frame, bezt->vec[1][0], BEZT_BINARYSEARCH_THRESH)) {
          rk->val += bezt->vec[1][1];
          rk->tot_count++;

          found = true;
          break;
        }
        else if (rk->frame < bezt->vec[1][0]) {
          /* Terminate early if have passed the supposed insertion point? */
          break;
        }
      }

      /* If nothing found yet, create a new one */
      if (found == false) {
        tRetainedKeyframe *rk = MEM_callocN(sizeof(tRetainedKeyframe), "tRetainedKeyframe");

        rk->frame = bezt->vec[1][0];
        rk->val = bezt->vec[1][1];
        rk->tot_count = 1;

        BLI_addtail(&retained_keys, rk);
      }
    }
  }

  if (BLI_listbase_is_empty(&retained_keys)) {
    /* This may happen if none of the points were selected... */
    if (G.debug & G_DEBUG) {
      printf("%s: nothing to do for FCurve %p (rna_path = '%s')\n", __func__, fcu, fcu->rna_path);
    }
    return;
  }
  else {
    /* Compute the average values for each retained keyframe */
    for (tRetainedKeyframe *rk = retained_keys.first; rk; rk = rk->next) {
      rk->val = rk->val / (float)rk->tot_count;
    }
  }

  /* 2) Delete all keyframes duplicating the "retained keys" found above
   *   - Most of these will be unselected keyframes
   *   - Some will be selected keyframes though. For those, we only keep the last one
   *     (or else everything is gone), and replace its value with the averaged value.
   */
  for (int i = fcu->totvert - 1; i >= 0; i--) {
    BezTriple *bezt = &fcu->bezt[i];

    /* Is this keyframe a candidate for deletion? */
    /* TODO: Replace loop with an O(1) lookup instead */
    for (tRetainedKeyframe *rk = retained_keys.last; rk; rk = rk->prev) {
      if (IS_EQT(bezt->vec[1][0], rk->frame, BEZT_BINARYSEARCH_THRESH)) {
        /* Selected keys are treated with greater care than unselected ones... */
        if (BEZT_ISSEL_ANY(bezt)) {
          /* - If this is the last selected key left (based on rk->del_count) ==> UPDATE IT
           *   (or else we wouldn't have any keyframe left here)
           * - Otherwise, there are still other selected keyframes on this frame
           *   to be merged down still ==> DELETE IT
           */
          if (rk->del_count == rk->tot_count - 1) {
            /* Update keyframe... */
            if (can_average_points) {
              /* TODO: update handles too? */
              bezt->vec[1][1] = rk->val;
            }
          }
          else {
            /* Delete Keyframe */
            delete_fcurve_key(fcu, i, 0);
          }

          /* Update count of how many we've deleted
           * - It should only matter that we're doing this for all but the last one
           */
          rk->del_count++;
        }
        else {
          /* Always delete - Unselected keys don't matter */
          delete_fcurve_key(fcu, i, 0);
        }

        /* Stop the RK search... we've found our match now */
        break;
      }
    }
  }

  /* 3) Recalculate handles */
  testhandles_fcurve(fcu, use_handle);

  /* cleanup */
  BLI_freelistN(&retained_keys);
}

/* Called by special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 * remake_action_ipos should have already been called
 */
static void posttrans_action_clean(bAnimContext *ac, bAction *act)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/);
  ANIM_animdata_filter(ac, &anim_data, filter, act, ANIMCONT_ACTION);

  /* loop through relevant data, removing keyframes as appropriate
   *      - all keyframes are converted in/out of global time
   */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(ac, ale);

    if (adt) {
      ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 0);
      posttrans_fcurve_clean(ale->key_data, false); /* only use handles in graph editor */
      ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0);
    }
    else {
      posttrans_fcurve_clean(ale->key_data, false); /* only use handles in graph editor */
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);
}

/* ----------------------------- */

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_fcurve_keys(FCurve *fcu, char side, float cfra, bool is_prop_edit)
{
  BezTriple *bezt;
  int i, count = 0, count_all = 0;

  if (ELEM(NULL, fcu, fcu->bezt)) {
    return count;
  }

  /* only include points that occur on the right side of cfra */
  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
      /* no need to adjust the handle selection since they are assumed
       * selected (like graph editor with SIPO_NOHANDLES) */
      if (bezt->f2 & SELECT) {
        count++;
      }

      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  else {
    return count;
  }
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_gplayer_frames(bGPDlayer *gpl, char side, float cfra, bool is_prop_edit)
{
  bGPDframe *gpf;
  int count = 0, count_all = 0;

  if (gpl == NULL) {
    return count;
  }

  /* only include points that occur on the right side of cfra */
  for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
    if (FrameOnMouseSide(side, (float)gpf->framenum, cfra)) {
      if (gpf->flag & GP_FRAME_SELECT) {
        count++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  else {
    return count;
  }
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_masklayer_frames(MaskLayer *masklay, char side, float cfra, bool is_prop_edit)
{
  MaskLayerShape *masklayer_shape;
  int count = 0, count_all = 0;

  if (masklay == NULL) {
    return count;
  }

  /* only include points that occur on the right side of cfra */
  for (masklayer_shape = masklay->splines_shapes.first; masklayer_shape;
       masklayer_shape = masklayer_shape->next) {
    if (FrameOnMouseSide(side, (float)masklayer_shape->frame, cfra)) {
      if (masklayer_shape->flag & MASK_SHAPE_SELECT) {
        count++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  else {
    return count;
  }
}

/* This function assigns the information to transdata */
static void TimeToTransData(TransData *td, float *time, AnimData *adt, float ypos)
{
  /* memory is calloc'ed, so that should zero everything nicely for us */
  td->val = time;
  td->ival = *(time);

  td->center[0] = td->ival;
  td->center[1] = ypos;

  /* store the AnimData where this keyframe exists as a keyframe of the
   * active action as td->extra.
   */
  td->extra = adt;
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = IcuToTransData(td, icu, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used.
 */
static TransData *ActionFCurveToTransData(TransData *td,
                                          TransData2D **td2dv,
                                          FCurve *fcu,
                                          AnimData *adt,
                                          char side,
                                          float cfra,
                                          bool is_prop_edit,
                                          float ypos)
{
  BezTriple *bezt;
  TransData2D *td2d = *td2dv;
  int i;

  if (ELEM(NULL, fcu, fcu->bezt)) {
    return td;
  }

  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    /* only add selected keyframes (for now, proportional edit is not enabled) */
    if (is_prop_edit || (bezt->f2 & SELECT)) { /* note this MUST match count_fcurve_keys(),
                                                * so can't use BEZT_ISSEL_ANY() macro */
      /* only add if on the right 'side' of the current frame */
      if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
        TimeToTransData(td, bezt->vec[1], adt, ypos);

        if (bezt->f2 & SELECT) {
          td->flag |= TD_SELECTED;
        }

        /*set flags to move handles as necessary*/
        td->flag |= TD_MOVEHANDLE1 | TD_MOVEHANDLE2;
        td2d->h1 = bezt->vec[0];
        td2d->h2 = bezt->vec[2];

        copy_v2_v2(td2d->ih1, td2d->h1);
        copy_v2_v2(td2d->ih2, td2d->h2);

        td++;
        td2d++;
      }
    }
  }

  *td2dv = td2d;

  return td;
}

/* helper struct for gp-frame transforms (only used here) */
typedef struct tGPFtransdata {
  float val;  /* where transdata writes transform */
  int *sdata; /* pointer to gpf->framenum */
} tGPFtransdata;

/* This function helps flush transdata written to tempdata into the gp-frames  */
void flushTransIntFrameActionData(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tGPFtransdata *tfd = tc->custom.type.data;

  /* flush data! */
  for (int i = 0; i < tc->data_len; i++, tfd++) {
    *(tfd->sdata) = round_fl_to_int(tfd->val);
  }
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = GPLayerToTransData(td, ipo, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used.
 */
static int GPLayerToTransData(TransData *td,
                              tGPFtransdata *tfd,
                              bGPDlayer *gpl,
                              char side,
                              float cfra,
                              bool is_prop_edit,
                              float ypos)
{
  bGPDframe *gpf;
  int count = 0;

  /* check for select frames on right side of current frame */
  for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
    if (is_prop_edit || (gpf->flag & GP_FRAME_SELECT)) {
      if (FrameOnMouseSide(side, (float)gpf->framenum, cfra)) {
        /* memory is calloc'ed, so that should zero everything nicely for us */
        td->val = &tfd->val;
        td->ival = (float)gpf->framenum;

        td->center[0] = td->ival;
        td->center[1] = ypos;

        tfd->val = (float)gpf->framenum;
        tfd->sdata = &gpf->framenum;

        /* advance td now */
        td++;
        tfd++;
        count++;
      }
    }
  }

  return count;
}

/* refer to comment above #GPLayerToTransData, this is the same but for masks */
static int MaskLayerToTransData(TransData *td,
                                tGPFtransdata *tfd,
                                MaskLayer *masklay,
                                char side,
                                float cfra,
                                bool is_prop_edit,
                                float ypos)
{
  MaskLayerShape *masklay_shape;
  int count = 0;

  /* check for select frames on right side of current frame */
  for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
       masklay_shape = masklay_shape->next) {
    if (is_prop_edit || (masklay_shape->flag & MASK_SHAPE_SELECT)) {
      if (FrameOnMouseSide(side, (float)masklay_shape->frame, cfra)) {
        /* memory is calloc'ed, so that should zero everything nicely for us */
        td->val = &tfd->val;
        td->ival = (float)masklay_shape->frame;

        td->center[0] = td->ival;
        td->center[1] = ypos;

        tfd->val = (float)masklay_shape->frame;
        tfd->sdata = &masklay_shape->frame;

        /* advance td now */
        td++;
        tfd++;
        count++;
      }
    }
  }

  return count;
}

static void createTransActionData(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  tGPFtransdata *tfd = NULL;

  rcti *mask = &t->ar->v2d.mask;
  rctf *datamask = &t->ar->v2d.cur;

  float xsize = BLI_rctf_size_x(datamask);
  float ysize = BLI_rctf_size_y(datamask);
  float xmask = BLI_rcti_size_x(mask);
  float ymask = BLI_rcti_size_y(mask);

  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

  int count = 0;
  float cfra;
  float ypos = 1.0f / ((ysize / xsize) * (xmask / ymask)) * BLI_rctf_cent_y(&t->ar->v2d.cur);

  /* determine what type of data we are operating on */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  /* filter data */
  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT);
  }
  else {
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/);
  }
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  if (t->mode == TFM_TIME_EXTEND) {
    /* only side on which mouse is gets transformed */
    float xmouse, ymouse;

    UI_view2d_region_to_view(&ac.ar->v2d, t->mouse.imval[0], t->mouse.imval[1], &xmouse, &ymouse);
    t->frame_side = (xmouse > CFRA) ? 'R' : 'L';  // XXX use t->frame_side
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

  /* loop 1: fully select ipo-keys and count how many BezTriples are selected */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    int adt_count = 0;
    /* convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points)
     */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = (float)CFRA;
    }

    if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
      adt_count = count_fcurve_keys(ale->key_data, t->frame_side, cfra, is_prop_edit);
    }
    else if (ale->type == ANIMTYPE_GPLAYER) {
      adt_count = count_gplayer_frames(ale->data, t->frame_side, cfra, is_prop_edit);
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      adt_count = count_masklayer_frames(ale->data, t->frame_side, cfra, is_prop_edit);
    }
    else {
      BLI_assert(0);
    }

    if (adt_count > 0) {
      count += adt_count;
      ale->tag = true;
    }
  }

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    /* cleanup temp list */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* allocate memory for data */
  tc->data_len = count;

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(Action Editor)");
  tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "transdata2d");
  td = tc->data;
  td2d = tc->data_2d;

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    tc->custom.type.data = tfd = MEM_callocN(sizeof(tGPFtransdata) * count, "tGPFtransdata");
    tc->custom.type.use_free = true;
  }

  /* loop 2: build transdata array */
  for (ale = anim_data.first; ale; ale = ale->next) {

    if (is_prop_edit && !ale->tag) {
      continue;
    }

    cfra = (float)CFRA;

    {
      AnimData *adt;
      adt = ANIM_nla_mapping_get(&ac, ale);
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
      }
    }

    if (ale->type == ANIMTYPE_GPLAYER) {
      bGPDlayer *gpl = (bGPDlayer *)ale->data;
      int i;

      i = GPLayerToTransData(td, tfd, gpl, t->frame_side, cfra, is_prop_edit, ypos);
      td += i;
      tfd += i;
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      MaskLayer *masklay = (MaskLayer *)ale->data;
      int i;

      i = MaskLayerToTransData(td, tfd, masklay, t->frame_side, cfra, is_prop_edit, ypos);
      td += i;
      tfd += i;
    }
    else {
      AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
      FCurve *fcu = (FCurve *)ale->key_data;

      td = ActionFCurveToTransData(td, &td2d, fcu, adt, t->frame_side, cfra, is_prop_edit, ypos);
    }
  }

  /* calculate distances for proportional editing */
  if (is_prop_edit) {
    td = tc->data;

    for (ale = anim_data.first; ale; ale = ale->next) {
      AnimData *adt;

      /* F-Curve may not have any keyframes */
      if (!ale->tag) {
        continue;
      }

      adt = ANIM_nla_mapping_get(&ac, ale);
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
      }
      else {
        cfra = (float)CFRA;
      }

      if (ale->type == ANIMTYPE_GPLAYER) {
        bGPDlayer *gpl = (bGPDlayer *)ale->data;
        bGPDframe *gpf;

        for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
          if (gpf->flag & GP_FRAME_SELECT) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            bGPDframe *gpf_iter;
            int min = INT_MAX;
            for (gpf_iter = gpl->frames.first; gpf_iter; gpf_iter = gpf_iter->next) {
              if (gpf_iter->flag & GP_FRAME_SELECT) {
                if (FrameOnMouseSide(t->frame_side, (float)gpf_iter->framenum, cfra)) {
                  int val = abs(gpf->framenum - gpf_iter->framenum);
                  if (val < min) {
                    min = val;
                  }
                }
              }
            }
            td->dist = td->rdist = min;
          }
          td++;
        }
      }
      else if (ale->type == ANIMTYPE_MASKLAYER) {
        MaskLayer *masklay = (MaskLayer *)ale->data;
        MaskLayerShape *masklay_shape;

        for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
             masklay_shape = masklay_shape->next) {
          if (FrameOnMouseSide(t->frame_side, (float)masklay_shape->frame, cfra)) {
            if (masklay_shape->flag & MASK_SHAPE_SELECT) {
              td->dist = td->rdist = 0.0f;
            }
            else {
              MaskLayerShape *masklay_iter;
              int min = INT_MAX;
              for (masklay_iter = masklay->splines_shapes.first; masklay_iter;
                   masklay_iter = masklay_iter->next) {
                if (masklay_iter->flag & MASK_SHAPE_SELECT) {
                  if (FrameOnMouseSide(t->frame_side, (float)masklay_iter->frame, cfra)) {
                    int val = abs(masklay_shape->frame - masklay_iter->frame);
                    if (val < min) {
                      min = val;
                    }
                  }
                }
              }
              td->dist = td->rdist = min;
            }
            td++;
          }
        }
      }
      else {
        FCurve *fcu = (FCurve *)ale->key_data;
        BezTriple *bezt;
        int i;

        for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
          if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
            if (bezt->f2 & SELECT) {
              td->dist = td->rdist = 0.0f;
            }
            else {
              BezTriple *bezt_iter;
              int j;
              float min = FLT_MAX;
              for (j = 0, bezt_iter = fcu->bezt; j < fcu->totvert; j++, bezt_iter++) {
                if (bezt_iter->f2 & SELECT) {
                  if (FrameOnMouseSide(t->frame_side, (float)bezt_iter->vec[1][0], cfra)) {
                    float val = fabs(bezt->vec[1][0] - bezt_iter->vec[1][0]);
                    if (val < min) {
                      min = val;
                    }
                  }
                }
              }
              td->dist = td->rdist = min;
            }
            td++;
          }
        }
      }
    }
  }

  /* cleanup temp list */
  ANIM_animdata_freelist(&anim_data);
}

/* ********************* GRAPH EDITOR ************************* */

typedef struct TransDataGraph {
  float unit_scale;
  float offset;
} TransDataGraph;

/* Helper function for createTransGraphEditData, which is responsible for associating
 * source data with transform data
 */
static void bezt_to_transdata(TransData *td,
                              TransData2D *td2d,
                              TransDataGraph *tdg,
                              AnimData *adt,
                              BezTriple *bezt,
                              int bi,
                              bool selected,
                              bool ishandle,
                              bool intvals,
                              float mtx[3][3],
                              float smtx[3][3],
                              float unit_scale,
                              float offset)
{
  float *loc = bezt->vec[bi];
  const float *cent = bezt->vec[1];

  /* New location from td gets dumped onto the old-location of td2d, which then
   * gets copied to the actual data at td2d->loc2d (bezt->vec[n])
   *
   * Due to NLA mapping, we apply NLA mapping to some of the verts here,
   * and then that mapping will be undone after transform is done.
   */

  if (adt) {
    td2d->loc[0] = BKE_nla_tweakedit_remap(adt, loc[0], NLATIME_CONVERT_MAP);
    td2d->loc[1] = (loc[1] + offset) * unit_scale;
    td2d->loc[2] = 0.0f;
    td2d->loc2d = loc;

    td->loc = td2d->loc;
    td->center[0] = BKE_nla_tweakedit_remap(adt, cent[0], NLATIME_CONVERT_MAP);
    td->center[1] = (cent[1] + offset) * unit_scale;
    td->center[2] = 0.0f;

    copy_v3_v3(td->iloc, td->loc);
  }
  else {
    td2d->loc[0] = loc[0];
    td2d->loc[1] = (loc[1] + offset) * unit_scale;
    td2d->loc[2] = 0.0f;
    td2d->loc2d = loc;

    td->loc = td2d->loc;
    copy_v3_v3(td->center, cent);
    td->center[1] = (td->center[1] + offset) * unit_scale;
    copy_v3_v3(td->iloc, td->loc);
  }

  if (!ishandle) {
    td2d->h1 = bezt->vec[0];
    td2d->h2 = bezt->vec[2];
    copy_v2_v2(td2d->ih1, td2d->h1);
    copy_v2_v2(td2d->ih2, td2d->h2);
  }
  else {
    td2d->h1 = NULL;
    td2d->h2 = NULL;
  }

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  /* store AnimData info in td->extra, for applying mapping when flushing */
  td->extra = adt;

  if (selected) {
    td->flag |= TD_SELECTED;
    td->dist = 0.0f;
  }
  else {
    td->dist = FLT_MAX;
  }

  if (ishandle) {
    td->flag |= TD_NOTIMESNAP;
  }
  if (intvals) {
    td->flag |= TD_INTVALUES;
  }

  /* copy space-conversion matrices for dealing with non-uniform scales */
  copy_m3_m3(td->mtx, mtx);
  copy_m3_m3(td->smtx, smtx);

  tdg->unit_scale = unit_scale;
  tdg->offset = offset;
}

static bool graph_edit_is_translation_mode(TransInfo *t)
{
  return ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_TRANSLATE, TFM_TIME_SLIDE, TFM_TIME_DUPLICATE);
}

static bool graph_edit_use_local_center(TransInfo *t)
{
  return ((t->around == V3D_AROUND_LOCAL_ORIGINS) && (graph_edit_is_translation_mode(t) == false));
}

static void graph_key_shortest_dist(
    TransInfo *t, FCurve *fcu, TransData *td_start, TransData *td, int cfra, bool use_handle)
{
  int j = 0;
  TransData *td_iter = td_start;

  td->dist = FLT_MAX;
  for (; j < fcu->totvert; j++) {
    BezTriple *bezt = fcu->bezt + j;
    if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
      const bool sel2 = (bezt->f2 & SELECT) != 0;
      const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
      const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

      if (sel1 || sel2 || sel3) {
        td->dist = td->rdist = min_ff(td->dist, fabs(td_iter->center[0] - td->center[0]));
      }

      td_iter += 3;
    }
  }
}

static void createTransGraphEditData(bContext *C, TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  Scene *scene = t->scene;
  ARegion *ar = t->ar;
  View2D *v2d = &ar->v2d;

  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataGraph *tdg = NULL;

  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  BezTriple *bezt;
  int count = 0, i;
  float mtx[3][3], smtx[3][3];
  const bool is_translation_mode = graph_edit_is_translation_mode(t);
  const bool use_handle = !(sipo->flag & SIPO_NOHANDLES);
  const bool use_local_center = graph_edit_use_local_center(t);
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  short anim_map_flag = ANIM_UNITCONV_ONLYSEL | ANIM_UNITCONV_SELVERTS;

  /* determine what type of data we are operating on */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  anim_map_flag |= ANIM_get_normalization_flags(&ac);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  // XXX we still want this mode, but how to get this using standard transform too?
  if (t->mode == TFM_TIME_EXTEND) {
    /* only side on which mouse is gets transformed */
    float xmouse, ymouse;

    UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[1], &xmouse, &ymouse);
    t->frame_side = (xmouse > CFRA) ? 'R' : 'L';  // XXX use t->frame_side
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

  /* Loop 1: count how many BezTriples (specifically their verts)
   * are selected (or should be edited). */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    FCurve *fcu = (FCurve *)ale->key_data;
    float cfra;
    int curvecount = 0;
    bool selected = false;

    /* F-Curve may not have any keyframes */
    if (fcu->bezt == NULL) {
      continue;
    }

    /* convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points)
     */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = (float)CFRA;
    }

    /* Only include BezTriples whose 'keyframe'
     * occurs on the same side of the current frame as mouse. */
    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
        const bool sel2 = (bezt->f2 & SELECT) != 0;
        const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
        const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

        if (is_prop_edit) {
          curvecount += 3;
          if (sel2 || sel1 || sel3) {
            selected = true;
          }
        }
        else {
          if (!is_translation_mode || !(sel2)) {
            if (sel1) {
              count++;
            }

            if (sel3) {
              count++;
            }
          }

          /* only include main vert if selected */
          if (sel2 && !use_local_center) {
            count++;
          }
        }
      }
    }

    if (is_prop_edit) {
      if (selected) {
        count += curvecount;
        ale->tag = true;
      }
    }
  }

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    /* cleanup temp list */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* allocate memory for data */
  tc->data_len = count;

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData (Graph Editor)");
  /* For each 2d vert a 3d vector is allocated,
   * so that they can be treated just as if they were 3d verts. */
  tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransData2D (Graph Editor)");
  tc->custom.type.data = MEM_callocN(tc->data_len * sizeof(TransDataGraph), "TransDataGraph");
  tc->custom.type.use_free = true;

  td = tc->data;
  td2d = tc->data_2d;
  tdg = tc->custom.type.data;

  /* precompute space-conversion matrices for dealing with non-uniform scaling of Graph Editor */
  unit_m3(mtx);
  unit_m3(smtx);

  if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
    float xscale, yscale;

    /* apply scale factors to x and y axes of space-conversion matrices */
    UI_view2d_scale_get(v2d, &xscale, &yscale);

    /* mtx is data to global (i.e. view) conversion */
    mul_v3_fl(mtx[0], xscale);
    mul_v3_fl(mtx[1], yscale);

    /* smtx is global (i.e. view) to data conversion */
    if (IS_EQF(xscale, 0.0f) == 0) {
      mul_v3_fl(smtx[0], 1.0f / xscale);
    }
    if (IS_EQF(yscale, 0.0f) == 0) {
      mul_v3_fl(smtx[1], 1.0f / yscale);
    }
  }

  /* loop 2: build transdata arrays */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    FCurve *fcu = (FCurve *)ale->key_data;
    bool intvals = (fcu->flag & FCURVE_INT_VALUES) != 0;
    float unit_scale, offset;
    float cfra;

    /* F-Curve may not have any keyframes */
    if (fcu->bezt == NULL || (is_prop_edit && ale->tag == 0)) {
      continue;
    }

    /* convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points)
     */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = (float)CFRA;
    }

    unit_scale = ANIM_unit_mapping_get_factor(
        ac.scene, ale->id, ale->key_data, anim_map_flag, &offset);

    /* only include BezTriples whose 'keyframe' occurs on the same side
     * of the current frame as mouse (if applicable) */
    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
        const bool sel2 = (bezt->f2 & SELECT) != 0;
        const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
        const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

        TransDataCurveHandleFlags *hdata = NULL;
        /* short h1=1, h2=1; */ /* UNUSED */

        if (is_prop_edit) {
          bool is_sel = (sel2 || sel1 || sel3);
          /* we always select all handles for proportional editing if central handle is selected */
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            adt,
                            bezt,
                            0,
                            is_sel,
                            true,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            adt,
                            bezt,
                            1,
                            is_sel,
                            false,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            adt,
                            bezt,
                            2,
                            is_sel,
                            true,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
        }
        else {
          /* only include handles if selected, irrespective of the interpolation modes.
           * also, only treat handles specially if the center point isn't selected.
           */
          if (!is_translation_mode || !(sel2)) {
            if (sel1) {
              hdata = initTransDataCurveHandles(td, bezt);
              bezt_to_transdata(td++,
                                td2d++,
                                tdg++,
                                adt,
                                bezt,
                                0,
                                sel1,
                                true,
                                intvals,
                                mtx,
                                smtx,
                                unit_scale,
                                offset);
            }
            else {
              /* h1 = 0; */ /* UNUSED */
            }

            if (sel3) {
              if (hdata == NULL) {
                hdata = initTransDataCurveHandles(td, bezt);
              }
              bezt_to_transdata(td++,
                                td2d++,
                                tdg++,
                                adt,
                                bezt,
                                2,
                                sel3,
                                true,
                                intvals,
                                mtx,
                                smtx,
                                unit_scale,
                                offset);
            }
            else {
              /* h2 = 0; */ /* UNUSED */
            }
          }

          /* only include main vert if selected */
          if (sel2 && !use_local_center) {
            /* move handles relative to center */
            if (is_translation_mode) {
              if (sel1) {
                td->flag |= TD_MOVEHANDLE1;
              }
              if (sel3) {
                td->flag |= TD_MOVEHANDLE2;
              }
            }

            /* if handles were not selected, store their selection status */
            if (!(sel1) || !(sel3)) {
              if (hdata == NULL) {
                hdata = initTransDataCurveHandles(td, bezt);
              }
            }

            bezt_to_transdata(td++,
                              td2d++,
                              tdg++,
                              adt,
                              bezt,
                              1,
                              sel2,
                              false,
                              intvals,
                              mtx,
                              smtx,
                              unit_scale,
                              offset);
          }
          /* Special hack (must be done after #initTransDataCurveHandles(),
           * as that stores handle settings to restore...):
           *
           * - Check if we've got entire BezTriple selected and we're scaling/rotating that point,
           *   then check if we're using auto-handles.
           * - If so, change them auto-handles to aligned handles so that handles get affected too
           */
          if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM) && ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM) &&
              ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
            if (hdata && (sel1) && (sel3)) {
              bezt->h1 = HD_ALIGN;
              bezt->h2 = HD_ALIGN;
            }
          }
        }
      }
    }

    /* Sets handles based on the selection */
    testhandles_fcurve(fcu, use_handle);
  }

  if (is_prop_edit) {
    /* loop 2: build transdata arrays */
    td = tc->data;

    for (ale = anim_data.first; ale; ale = ale->next) {
      AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
      FCurve *fcu = (FCurve *)ale->key_data;
      TransData *td_start = td;
      float cfra;

      /* F-Curve may not have any keyframes */
      if (fcu->bezt == NULL || (ale->tag == 0)) {
        continue;
      }

      /* convert current-frame to action-time (slightly less accurate, especially under
       * higher scaling ratios, but is faster than converting all points)
       */
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
      }
      else {
        cfra = (float)CFRA;
      }

      /* only include BezTriples whose 'keyframe' occurs on the
       * same side of the current frame as mouse (if applicable) */
      for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
        if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
          const bool sel2 = (bezt->f2 & SELECT) != 0;
          const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
          const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

          if (sel1 || sel2) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }
          td++;

          if (sel2) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }
          td++;

          if (sel3 || sel2) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }
          td++;
        }
      }
    }
  }

  /* cleanup temp list */
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------------ */

/* struct for use in re-sorting BezTriples during Graph Editor transform */
typedef struct BeztMap {
  BezTriple *bezt;
  unsigned int oldIndex; /* index of bezt in fcu->bezt array before sorting */
  unsigned int newIndex; /* index of bezt in fcu->bezt array after sorting */
  short swapHs;          /* swap order of handles (-1=clear; 0=not checked, 1=swap) */
  char pipo, cipo;       /* interpolation of current and next segments */
} BeztMap;

/* This function converts an FCurve's BezTriple array to a BeztMap array
 * NOTE: this allocates memory that will need to get freed later
 */
static BeztMap *bezt_to_beztmaps(BezTriple *bezts, int totvert, const short UNUSED(use_handle))
{
  BezTriple *bezt = bezts;
  BezTriple *prevbezt = NULL;
  BeztMap *bezm, *bezms;
  int i;

  /* allocate memory for this array */
  if (totvert == 0 || bezts == NULL) {
    return NULL;
  }
  bezm = bezms = MEM_callocN(sizeof(BeztMap) * totvert, "BeztMaps");

  /* assign beztriples to beztmaps */
  for (i = 0; i < totvert; i++, bezm++, prevbezt = bezt, bezt++) {
    bezm->bezt = bezt;

    bezm->oldIndex = i;
    bezm->newIndex = i;

    bezm->pipo = (prevbezt) ? prevbezt->ipo : bezt->ipo;
    bezm->cipo = bezt->ipo;
  }

  return bezms;
}

/* This function copies the code of sort_time_ipocurve, but acts on BeztMap structs instead */
static void sort_time_beztmaps(BeztMap *bezms, int totvert, const short UNUSED(use_handle))
{
  BeztMap *bezm;
  int i, ok = 1;

  /* keep repeating the process until nothing is out of place anymore */
  while (ok) {
    ok = 0;

    bezm = bezms;
    i = totvert;
    while (i--) {
      /* is current bezm out of order (i.e. occurs later than next)? */
      if (i > 0) {
        if (bezm->bezt->vec[1][0] > (bezm + 1)->bezt->vec[1][0]) {
          bezm->newIndex++;
          (bezm + 1)->newIndex--;

          SWAP(BeztMap, *bezm, *(bezm + 1));

          ok = 1;
        }
      }

      /* do we need to check if the handles need to be swapped?
       * optimization: this only needs to be performed in the first loop
       */
      if (bezm->swapHs == 0) {
        if ((bezm->bezt->vec[0][0] > bezm->bezt->vec[1][0]) &&
            (bezm->bezt->vec[2][0] < bezm->bezt->vec[1][0])) {
          /* handles need to be swapped */
          bezm->swapHs = 1;
        }
        else {
          /* handles need to be cleared */
          bezm->swapHs = -1;
        }
      }

      bezm++;
    }
  }
}

/* This function firstly adjusts the pointers that the transdata has to each BezTriple */
static void beztmap_to_data(
    TransInfo *t, FCurve *fcu, BeztMap *bezms, int totvert, const short UNUSED(use_handle))
{
  BezTriple *bezts = fcu->bezt;
  BeztMap *bezm;
  TransData2D *td2d;
  TransData *td;
  int i, j;
  char *adjusted;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* dynamically allocate an array of chars to mark whether an TransData's
   * pointers have been fixed already, so that we don't override ones that are
   * already done
   */
  adjusted = MEM_callocN(tc->data_len, "beztmap_adjusted_map");

  /* for each beztmap item, find if it is used anywhere */
  bezm = bezms;
  for (i = 0; i < totvert; i++, bezm++) {
    /* loop through transdata, testing if we have a hit
     * for the handles (vec[0]/vec[2]), we must also check if they need to be swapped...
     */
    td2d = tc->data_2d;
    td = tc->data;
    for (j = 0; j < tc->data_len; j++, td2d++, td++) {
      /* skip item if already marked */
      if (adjusted[j] != 0) {
        continue;
      }

      /* update all transdata pointers, no need to check for selections etc,
       * since only points that are really needed were created as transdata
       */
      if (td2d->loc2d == bezm->bezt->vec[0]) {
        if (bezm->swapHs == 1) {
          td2d->loc2d = (bezts + bezm->newIndex)->vec[2];
        }
        else {
          td2d->loc2d = (bezts + bezm->newIndex)->vec[0];
        }
        adjusted[j] = 1;
      }
      else if (td2d->loc2d == bezm->bezt->vec[2]) {
        if (bezm->swapHs == 1) {
          td2d->loc2d = (bezts + bezm->newIndex)->vec[0];
        }
        else {
          td2d->loc2d = (bezts + bezm->newIndex)->vec[2];
        }
        adjusted[j] = 1;
      }
      else if (td2d->loc2d == bezm->bezt->vec[1]) {
        td2d->loc2d = (bezts + bezm->newIndex)->vec[1];

        /* if only control point is selected, the handle pointers need to be updated as well */
        if (td2d->h1) {
          td2d->h1 = (bezts + bezm->newIndex)->vec[0];
        }
        if (td2d->h2) {
          td2d->h2 = (bezts + bezm->newIndex)->vec[2];
        }

        adjusted[j] = 1;
      }

      /* the handle type pointer has to be updated too */
      if (adjusted[j] && td->flag & TD_BEZTRIPLE && td->hdata) {
        if (bezm->swapHs == 1) {
          td->hdata->h1 = &(bezts + bezm->newIndex)->h2;
          td->hdata->h2 = &(bezts + bezm->newIndex)->h1;
        }
        else {
          td->hdata->h1 = &(bezts + bezm->newIndex)->h1;
          td->hdata->h2 = &(bezts + bezm->newIndex)->h2;
        }
      }
    }
  }

  /* free temp memory used for 'adjusted' array */
  MEM_freeN(adjusted);
}

/* This function is called by recalcData during the Transform loop to recalculate
 * the handles of curves and sort the keyframes so that the curves draw correctly.
 * It is only called if some keyframes have moved out of order.
 *
 * anim_data is the list of channels (F-Curves) retrieved already containing the
 * channels to work on. It should not be freed here as it may still need to be used.
 */
void remake_graph_transdata(TransInfo *t, ListBase *anim_data)
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  bAnimListElem *ale;
  const bool use_handle = (sipo->flag & SIPO_NOHANDLES) == 0;

  /* sort and reassign verts */
  for (ale = anim_data->first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;

    if (fcu->bezt) {
      BeztMap *bezm;

      /* adjust transform-data pointers */
      /* note, none of these functions use 'use_handle', it could be removed */
      bezm = bezt_to_beztmaps(fcu->bezt, fcu->totvert, use_handle);
      sort_time_beztmaps(bezm, fcu->totvert, use_handle);
      beztmap_to_data(t, fcu, bezm, fcu->totvert, use_handle);

      /* free mapping stuff */
      MEM_freeN(bezm);

      /* re-sort actual beztriples (perhaps this could be done using the beztmaps to save time?) */
      sort_time_fcurve(fcu);

      /* make sure handles are all set correctly */
      testhandles_fcurve(fcu, use_handle);
    }
  }
}

/* this function is called on recalcData to apply the transforms applied
 * to the transdata on to the actual keyframe data
 */
void flushTransGraphData(TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  TransData *td;
  TransData2D *td2d;
  TransDataGraph *tdg;
  Scene *scene = t->scene;
  double secf = FPS;
  int a;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data, td2d = tc->data_2d, tdg = tc->custom.type.data; a < tc->data_len;
       a++, td++, td2d++, tdg++) {
    /* pointers to relevant AnimData blocks are stored in the td->extra pointers */
    AnimData *adt = (AnimData *)td->extra;

    float inv_unit_scale = 1.0f / tdg->unit_scale;

    /* handle snapping for time values
     * - we should still be in NLA-mapping timespace
     * - only apply to keyframes (but never to handles)
     * - don't do this when canceling, or else these changes won't go away
     */
    if ((t->state != TRANS_CANCEL) && (td->flag & TD_NOTIMESNAP) == 0) {
      switch (sipo->autosnap) {
        case SACTSNAP_FRAME: /* snap to nearest frame */
          td2d->loc[0] = floor((double)td2d->loc[0] + 0.5);
          break;

        case SACTSNAP_SECOND: /* snap to nearest second */
          td2d->loc[0] = floor(((double)td2d->loc[0] / secf) + 0.5) * secf;
          break;

        case SACTSNAP_MARKER: /* snap to nearest marker */
          td2d->loc[0] = (float)ED_markers_find_nearest_marker_time(&t->scene->markers,
                                                                    td2d->loc[0]);
          break;
      }
    }

    /* we need to unapply the nla-mapping from the time in some situations */
    if (adt) {
      td2d->loc2d[0] = BKE_nla_tweakedit_remap(adt, td2d->loc[0], NLATIME_CONVERT_UNMAP);
    }
    else {
      td2d->loc2d[0] = td2d->loc[0];
    }

    /** Time-stepping auto-snapping modes don't get applied for Graph Editor transforms,
     * as these use the generic transform modes which don't account for this sort of thing.
     * These ones aren't affected by NLA mapping, so we do this after the conversion...
     *
     * \note We also have to apply to td->loc,
     * as that's what the handle-adjustment step below looks to,
     * otherwise we get "swimming handles".
     *
     * \note We don't do this when canceling transforms, or else these changes don't go away.
     */
    if ((t->state != TRANS_CANCEL) && (td->flag & TD_NOTIMESNAP) == 0 &&
        ELEM(sipo->autosnap, SACTSNAP_STEP, SACTSNAP_TSTEP)) {
      switch (sipo->autosnap) {
        case SACTSNAP_STEP: /* frame step */
          td2d->loc2d[0] = floor((double)td2d->loc[0] + 0.5);
          td->loc[0] = floor((double)td->loc[0] + 0.5);
          break;

        case SACTSNAP_TSTEP: /* second step */
          /* XXX: the handle behavior in this case is still not quite right... */
          td2d->loc[0] = floor(((double)td2d->loc[0] / secf) + 0.5) * secf;
          td->loc[0] = floor(((double)td->loc[0] / secf) + 0.5) * secf;
          break;
      }
    }

    /* if int-values only, truncate to integers */
    if (td->flag & TD_INTVALUES) {
      td2d->loc2d[1] = floorf(td2d->loc[1] * inv_unit_scale - tdg->offset + 0.5f);
    }
    else {
      td2d->loc2d[1] = td2d->loc[1] * inv_unit_scale - tdg->offset;
    }

    if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
      td2d->h1[0] = td2d->ih1[0] + td->loc[0] - td->iloc[0];
      td2d->h1[1] = td2d->ih1[1] + (td->loc[1] - td->iloc[1]) * inv_unit_scale;
    }

    if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
      td2d->h2[0] = td2d->ih2[0] + td->loc[0] - td->iloc[0];
      td2d->h2[1] = td2d->ih2[1] + (td->loc[1] - td->iloc[1]) * inv_unit_scale;
    }
  }
}

/* ******************* Sequencer Transform data ******************* */

/* This function applies the rules for transforming a strip so duplicate
 * checks don't need to be added in multiple places.
 *
 * recursive, count and flag MUST be set.
 *
 * seq->depth must be set before running this function so we know if the strips
 * are root level or not
 */
static void SeqTransInfo(TransInfo *t, Sequence *seq, int *recursive, int *count, int *flag)
{
  /* for extend we need to do some tricks */
  if (t->mode == TFM_TIME_EXTEND) {

    /* *** Extend Transform *** */

    Scene *scene = t->scene;
    int cfra = CFRA;
    int left = BKE_sequence_tx_get_final_left(seq, true);
    int right = BKE_sequence_tx_get_final_right(seq, true);

    if (seq->depth == 0 && ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK))) {
      *recursive = false;
      *count = 0;
      *flag = 0;
    }
    else if (seq->type == SEQ_TYPE_META) {

      /* for meta's we only ever need to extend their children, no matter what depth
       * just check the meta's are in the bounds */
      if (t->frame_side == 'R' && right <= cfra) {
        *recursive = false;
      }
      else if (t->frame_side == 'L' && left >= cfra) {
        *recursive = false;
      }
      else {
        *recursive = true;
      }

      *count = 1;
      *flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
    }
    else {

      *recursive = false; /* not a meta, so no thinking here */
      *count = 1;         /* unless its set to 0, extend will never set 2 handles at once */
      *flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);

      if (t->frame_side == 'R') {
        if (right <= cfra) {
          *count = *flag = 0;
        } /* ignore */
        else if (left > cfra) {
        } /* keep the selection */
        else {
          *flag |= SEQ_RIGHTSEL;
        }
      }
      else {
        if (left >= cfra) {
          *count = *flag = 0;
        } /* ignore */
        else if (right < cfra) {
        } /* keep the selection */
        else {
          *flag |= SEQ_LEFTSEL;
        }
      }
    }
  }
  else {

    t->frame_side = 'B';

    /* *** Normal Transform *** */

    if (seq->depth == 0) {

      /* Count */

      /* Non nested strips (resect selection and handles) */
      if ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK)) {
        *recursive = false;
        *count = 0;
        *flag = 0;
      }
      else {
        if ((seq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
          *flag = seq->flag;
          *count = 2; /* we need 2 transdata's */
        }
        else {
          *flag = seq->flag;
          *count = 1; /* selected or with a handle selected */
        }

        /* Recursive */

        if ((seq->type == SEQ_TYPE_META) && ((seq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == 0)) {
          /* if any handles are selected, don't recurse */
          *recursive = true;
        }
        else {
          *recursive = false;
        }
      }
    }
    else {
      /* Nested, different rules apply */

#ifdef SEQ_TX_NESTED_METAS
      *flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
      *count = 1; /* ignore the selection for nested */
      *recursive = (seq->type == SEQ_TYPE_META);
#else
      if (seq->type == SEQ_TYPE_META) {
        /* Meta's can only directly be moved between channels since they
         * don't have their start and length set directly (children affect that)
         * since this Meta is nested we don't need any of its data in fact.
         * BKE_sequence_calc() will update its settings when run on the toplevel meta */
        *flag = 0;
        *count = 0;
        *recursive = true;
      }
      else {
        *flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
        *count = 1; /* ignore the selection for nested */
        *recursive = false;
      }
#endif
    }
  }
}

static int SeqTransCount(TransInfo *t, Sequence *parent, ListBase *seqbase, int depth)
{
  Sequence *seq;
  int tot = 0, recursive, count, flag;

  for (seq = seqbase->first; seq; seq = seq->next) {
    seq->depth = depth;

    /* 'seq->tmp' is used by seq_tx_get_final_{left, right}
     * to check sequence's range and clamp to it if needed.
     * It's first place where digging into sequences tree, so store link to parent here. */
    seq->tmp = parent;

    SeqTransInfo(t, seq, &recursive, &count, &flag); /* ignore the flag */
    tot += count;

    if (recursive) {
      tot += SeqTransCount(t, seq, &seq->seqbase, depth + 1);
    }
  }

  return tot;
}

static TransData *SeqToTransData(
    TransData *td, TransData2D *td2d, TransDataSeq *tdsq, Sequence *seq, int flag, int sel_flag)
{
  int start_left;

  switch (sel_flag) {
    case SELECT:
      /* Use seq_tx_get_final_left() and an offset here
       * so transform has the left hand location of the strip.
       * tdsq->start_offset is used when flushing the tx data back */
      start_left = BKE_sequence_tx_get_final_left(seq, false);
      td2d->loc[0] = start_left;
      tdsq->start_offset = start_left - seq->start; /* use to apply the original location */
      break;
    case SEQ_LEFTSEL:
      start_left = BKE_sequence_tx_get_final_left(seq, false);
      td2d->loc[0] = start_left;
      break;
    case SEQ_RIGHTSEL:
      td2d->loc[0] = BKE_sequence_tx_get_final_right(seq, false);
      break;
  }

  td2d->loc[1] = seq->machine; /* channel - Y location */
  td2d->loc[2] = 0.0f;
  td2d->loc2d = NULL;

  tdsq->seq = seq;

  /* Use instead of seq->flag for nested strips and other
   * cases where the selection may need to be modified */
  tdsq->flag = flag;
  tdsq->sel_flag = sel_flag;

  td->extra = (void *)tdsq; /* allow us to update the strip from here */

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  /* Time Transform (extend) */
  td->val = td2d->loc;
  td->ival = td2d->loc[0];

  return td;
}

static int SeqToTransData_Recursive(
    TransInfo *t, ListBase *seqbase, TransData *td, TransData2D *td2d, TransDataSeq *tdsq)
{
  Sequence *seq;
  int recursive, count, flag;
  int tot = 0;

  for (seq = seqbase->first; seq; seq = seq->next) {

    SeqTransInfo(t, seq, &recursive, &count, &flag);

    /* add children first so recalculating metastrips does nested strips first */
    if (recursive) {
      int tot_children = SeqToTransData_Recursive(t, &seq->seqbase, td, td2d, tdsq);

      td = td + tot_children;
      td2d = td2d + tot_children;
      tdsq = tdsq + tot_children;

      tot += tot_children;
    }

    /* use 'flag' which is derived from seq->flag but modified for special cases */
    if (flag & SELECT) {
      if (flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        if (flag & SEQ_LEFTSEL) {
          SeqToTransData(td++, td2d++, tdsq++, seq, flag, SEQ_LEFTSEL);
          tot++;
        }
        if (flag & SEQ_RIGHTSEL) {
          SeqToTransData(td++, td2d++, tdsq++, seq, flag, SEQ_RIGHTSEL);
          tot++;
        }
      }
      else {
        SeqToTransData(td++, td2d++, tdsq++, seq, flag, SELECT);
        tot++;
      }
    }
  }
  return tot;
}

static void SeqTransDataBounds(TransInfo *t, ListBase *seqbase, TransSeq *ts)
{
  Sequence *seq;
  int recursive, count, flag;
  int max = INT32_MIN, min = INT32_MAX;

  for (seq = seqbase->first; seq; seq = seq->next) {

    /* just to get the flag since there are corner cases where this isn't totally obvious */
    SeqTransInfo(t, seq, &recursive, &count, &flag);

    /* use 'flag' which is derived from seq->flag but modified for special cases */
    if (flag & SELECT) {
      if (flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        if (flag & SEQ_LEFTSEL) {
          min = min_ii(seq->startdisp, min);
          max = max_ii(seq->startdisp, max);
        }
        if (flag & SEQ_RIGHTSEL) {
          min = min_ii(seq->enddisp, min);
          max = max_ii(seq->enddisp, max);
        }
      }
      else {
        min = min_ii(seq->startdisp, min);
        max = max_ii(seq->enddisp, max);
      }
    }
  }

  if (ts) {
    ts->max = max;
    ts->min = min;
  }
}

static void freeSeqData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  Editing *ed = BKE_sequencer_editing_get(t->scene, false);

  if (ed != NULL) {

    ListBase *seqbasep = ed->seqbasep;
    TransData *td = tc->data;
    int a;

    /* prevent updating the same seq twice
     * if the transdata order is changed this will mess up
     * but so will TransDataSeq */
    Sequence *seq_prev = NULL;
    Sequence *seq;

    if (!(t->state == TRANS_CANCEL)) {

#if 0  // default 2.4 behavior

      /* flush to 2d vector from internally used 3d vector */
      for (a = 0; a < t->total; a++, td++) {
        if ((seq != seq_prev) && (seq->depth == 0) && (seq->flag & SEQ_OVERLAP)) {
          seq = ((TransDataSeq *)td->extra)->seq;
          BKE_sequence_base_shuffle(seqbasep, seq, t->scene);
        }

        seq_prev = seq;
      }

#else  // durian hack
      {
        int overlap = 0;

        for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
          seq = ((TransDataSeq *)td->extra)->seq;
          if ((seq != seq_prev) && (seq->depth == 0) && (seq->flag & SEQ_OVERLAP)) {
            overlap = 1;
            break;
          }
        }

        if (overlap) {
          bool has_effect_root = false, has_effect_any = false;
          for (seq = seqbasep->first; seq; seq = seq->next) {
            seq->tmp = NULL;
          }

          td = tc->data;
          for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
            seq = ((TransDataSeq *)td->extra)->seq;
            if ((seq != seq_prev)) {
              /* check effects strips, we cant change their time */
              if ((seq->type & SEQ_TYPE_EFFECT) && seq->seq1) {
                has_effect_any = true;
                if (seq->depth == 0) {
                  has_effect_root = true;
                }
              }
              else {
                /* Tag seq with a non zero value, used by
                 * BKE_sequence_base_shuffle_time to identify the ones to shuffle */
                if (seq->depth == 0) {
                  seq->tmp = (void *)1;
                }
              }
            }
          }

          if (t->flag & T_ALT_TRANSFORM) {
            int minframe = MAXFRAME;
            td = tc->data;
            for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
              seq = ((TransDataSeq *)td->extra)->seq;
              if ((seq != seq_prev) && (seq->depth == 0)) {
                minframe = min_ii(minframe, seq->startdisp);
              }
            }

            for (seq = seqbasep->first; seq; seq = seq->next) {
              if (!(seq->flag & SELECT)) {
                if (seq->startdisp >= minframe) {
                  seq->machine += MAXSEQ * 2;
                }
              }
            }

            BKE_sequence_base_shuffle_time(seqbasep, t->scene);

            for (seq = seqbasep->first; seq; seq = seq->next) {
              if (seq->machine >= MAXSEQ * 2) {
                seq->machine -= MAXSEQ * 2;
                seq->tmp = (void *)1;
              }
              else {
                seq->tmp = NULL;
              }
            }

            BKE_sequence_base_shuffle_time(seqbasep, t->scene);
          }
          else {
            BKE_sequence_base_shuffle_time(seqbasep, t->scene);
          }

          if (has_effect_any) {
            /* update effects strips based on strips just moved in time */
            td = tc->data;
            for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
              seq = ((TransDataSeq *)td->extra)->seq;
              if ((seq != seq_prev)) {
                if ((seq->type & SEQ_TYPE_EFFECT) && seq->seq1) {
                  BKE_sequence_calc(t->scene, seq);
                }
              }
            }
          }

          if (has_effect_root) {
            /* now if any effects _still_ overlap, we need to move them up */
            td = tc->data;
            for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
              seq = ((TransDataSeq *)td->extra)->seq;
              if ((seq != seq_prev) && (seq->depth == 0)) {
                if ((seq->type & SEQ_TYPE_EFFECT) && seq->seq1) {
                  if (BKE_sequence_test_overlap(seqbasep, seq)) {
                    BKE_sequence_base_shuffle(seqbasep, seq, t->scene);
                  }
                }
              }
            }
            /* done with effects */
          }
        }
      }
#endif

      for (seq = seqbasep->first; seq; seq = seq->next) {
        /* We might want to build a list of effects that need to be updated during transform */
        if (seq->type & SEQ_TYPE_EFFECT) {
          if (seq->seq1 && seq->seq1->flag & SELECT) {
            BKE_sequence_calc(t->scene, seq);
          }
          else if (seq->seq2 && seq->seq2->flag & SELECT) {
            BKE_sequence_calc(t->scene, seq);
          }
          else if (seq->seq3 && seq->seq3->flag & SELECT) {
            BKE_sequence_calc(t->scene, seq);
          }
        }
      }

      BKE_sequencer_sort(t->scene);
    }
    else {
      /* Canceled, need to update the strips display */
      for (a = 0; a < tc->data_len; a++, td++) {
        seq = ((TransDataSeq *)td->extra)->seq;
        if ((seq != seq_prev) && (seq->depth == 0)) {
          if (seq->flag & SEQ_OVERLAP) {
            BKE_sequence_base_shuffle(seqbasep, seq, t->scene);
          }

          BKE_sequence_calc_disp(t->scene, seq);
        }
        seq_prev = seq;
      }
    }
  }

  if ((custom_data->data != NULL) && custom_data->use_free) {
    TransSeq *ts = custom_data->data;
    MEM_freeN(ts->tdseq);
    MEM_freeN(custom_data->data);
    custom_data->data = NULL;
  }

  DEG_id_tag_update(&t->scene->id, ID_RECALC_SEQUENCER_STRIPS);
}

static void createTransSeqData(bContext *C, TransInfo *t)
{
#define XXX_DURIAN_ANIM_TX_HACK

  View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = t->scene;
  Editing *ed = BKE_sequencer_editing_get(t->scene, false);
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataSeq *tdsq = NULL;
  TransSeq *ts = NULL;
  int xmouse;

  int count = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  if (ed == NULL) {
    tc->data_len = 0;
    return;
  }

  tc->custom.type.free_cb = freeSeqData;

  xmouse = (int)UI_view2d_region_to_view_x(v2d, t->mouse.imval[0]);

  /* which side of the current frame should be allowed */
  if (t->mode == TFM_TIME_EXTEND) {
    /* only side on which mouse is gets transformed */
    t->frame_side = (xmouse > CFRA) ? 'R' : 'L';
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

#ifdef XXX_DURIAN_ANIM_TX_HACK
  {
    Sequence *seq;
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      /* hack */
      if ((seq->flag & SELECT) == 0 && seq->type & SEQ_TYPE_EFFECT) {
        Sequence *seq_user;
        int i;
        for (i = 0; i < 3; i++) {
          seq_user = *((&seq->seq1) + i);
          if (seq_user && (seq_user->flag & SELECT) && !(seq_user->flag & SEQ_LOCK) &&
              !(seq_user->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL))) {
            seq->flag |= SELECT;
          }
        }
      }
    }
  }
#endif

  count = SeqTransCount(t, NULL, ed->seqbasep, 0);

  /* allocate memory for data */
  tc->data_len = count;

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    return;
  }

  tc->custom.type.data = ts = MEM_callocN(sizeof(TransSeq), "transseq");
  tc->custom.type.use_free = true;
  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransSeq TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransSeq TransData2D");
  ts->tdseq = tdsq = MEM_callocN(tc->data_len * sizeof(TransDataSeq), "TransSeq TransDataSeq");

  /* loop 2: build transdata array */
  SeqToTransData_Recursive(t, ed->seqbasep, td, td2d, tdsq);
  SeqTransDataBounds(t, ed->seqbasep, ts);

  /* set the snap mode based on how close the mouse is at the end/start points */
  if (abs(xmouse - ts->max) > abs(xmouse - ts->min)) {
    ts->snap_left = true;
  }

#undef XXX_DURIAN_ANIM_TX_HACK
}

/* *********************** Object Transform data ******************* */

/* Little helper function for ObjectToTransData used to give certain
 * constraints (ChildOf, FollowPath, and others that may be added)
 * inverse corrections for transform, so that they aren't in CrazySpace.
 * These particular constraints benefit from this, but others don't, hence
 * this semi-hack ;-)    - Aligorith
 */
static bool constraints_list_needinv(TransInfo *t, ListBase *list)
{
  bConstraint *con;

  /* loop through constraints, checking if there's one of the mentioned
   * constraints needing special crazyspace corrections
   */
  if (list) {
    for (con = list->first; con; con = con->next) {
      /* only consider constraint if it is enabled, and has influence on result */
      if ((con->flag & CONSTRAINT_DISABLE) == 0 && (con->enforce != 0.0f)) {
        /* (affirmative) returns for specific constraints here... */
        /* constraints that require this regardless  */
        if (ELEM(con->type,
                 CONSTRAINT_TYPE_FOLLOWPATH,
                 CONSTRAINT_TYPE_CLAMPTO,
                 CONSTRAINT_TYPE_ARMATURE,
                 CONSTRAINT_TYPE_OBJECTSOLVER,
                 CONSTRAINT_TYPE_FOLLOWTRACK)) {
          return true;
        }

        /* constraints that require this only under special conditions */
        if (con->type == CONSTRAINT_TYPE_CHILDOF) {
          /* ChildOf constraint only works when using all location components, see T42256. */
          bChildOfConstraint *data = (bChildOfConstraint *)con->data;

          if ((data->flag & CHILDOF_LOCX) && (data->flag & CHILDOF_LOCY) &&
              (data->flag & CHILDOF_LOCZ)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_ROTLIKE) {
          /* CopyRot constraint only does this when rotating, and offset is on */
          bRotateLikeConstraint *data = (bRotateLikeConstraint *)con->data;

          if ((data->flag & ROTLIKE_OFFSET) && (t->mode == TFM_ROTATION)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
          /* Transform constraint needs it for rotation at least (r.57309),
           * but doing so when translating may also mess things up [#36203]
           */

          if (t->mode == TFM_ROTATION) {
            return true;
          }
          /* ??? (t->mode == TFM_SCALE) ? */
        }
      }
    }
  }

  /* no appropriate candidates found */
  return false;
}

/* transcribe given object into TransData for Transforming */
static void ObjectToTransData(TransInfo *t, TransData *td, Object *ob)
{
  Scene *scene = t->scene;
  bool constinv;
  bool skip_invert = false;

  if (t->mode != TFM_DUMMY && ob->rigidbody_object) {
    float rot[3][3], scale[3];
    float ctime = BKE_scene_frame_get(scene);

    /* only use rigid body transform if simulation is running,
     * avoids problems with initial setup of rigid bodies */
    if (BKE_rigidbody_check_sim_running(scene->rigidbody_world, ctime)) {

      /* save original object transform */
      copy_v3_v3(td->ext->oloc, ob->loc);

      if (ob->rotmode > 0) {
        copy_v3_v3(td->ext->orot, ob->rot);
      }
      else if (ob->rotmode == ROT_MODE_AXISANGLE) {
        td->ext->orotAngle = ob->rotAngle;
        copy_v3_v3(td->ext->orotAxis, ob->rotAxis);
      }
      else {
        copy_qt_qt(td->ext->oquat, ob->quat);
      }
      /* update object's loc/rot to get current rigid body transform */
      mat4_to_loc_rot_size(ob->loc, rot, scale, ob->obmat);
      sub_v3_v3(ob->loc, ob->dloc);
      BKE_object_mat3_to_rot(ob, rot, false); /* drot is already corrected here */
    }
  }

  /* axismtx has the real orientation */
  copy_m3_m4(td->axismtx, ob->obmat);
  normalize_m3(td->axismtx);

  td->con = ob->constraints.first;

  /* hack: temporarily disable tracking and/or constraints when getting
   * object matrix, if tracking is on, or if constraints don't need
   * inverse correction to stop it from screwing up space conversion
   * matrix later
   */
  constinv = constraints_list_needinv(t, &ob->constraints);

  /* disable constraints inversion for dummy pass */
  if (t->mode == TFM_DUMMY) {
    skip_invert = true;
  }

  /* NOTE: This is not really following copy-on-write design and we should not
   * be re-evaluating the evaluated object. But as the comment above mentioned
   * this is part of a hack.
   * More proper solution would be to make a shallow copy of the object  and
   * evaluate that, and access matrix of that evaluated copy of the object.
   * Might be more tricky than it sounds, if some logic later on accesses the
   * object matrix via td->ob->obmat. */
  Object *object_eval = DEG_get_evaluated_object(t->depsgraph, ob);
  if (skip_invert == false && constinv == false) {
    object_eval->transflag |= OB_NO_CONSTRAINTS; /* BKE_object_where_is_calc checks this */
    /* It is possible to have transform data initialization prior to a
     * complete dependency graph evaluated. Happens, for example, when
     * changing transformation mode. */
    BKE_object_tfm_copy(object_eval, ob);
    BKE_object_where_is_calc(t->depsgraph, t->scene, object_eval);
    object_eval->transflag &= ~OB_NO_CONSTRAINTS;
  }
  else {
    BKE_object_where_is_calc(t->depsgraph, t->scene, object_eval);
  }
  /* Copy newly evaluated fields to the original object, similar to how
   * active dependency graph will do it. */
  copy_m4_m4(ob->obmat, object_eval->obmat);
  /* Only copy negative scale flag, this is the only flag which is modified by
   * the BKE_object_where_is_calc(). The rest of the flags we need to keep,
   * otherwise we might loose dupli flags  (see T61787). */
  ob->transflag &= ~OB_NEG_SCALE;
  ob->transflag |= (object_eval->transflag & OB_NEG_SCALE);

  td->ob = ob;

  td->loc = ob->loc;
  copy_v3_v3(td->iloc, td->loc);

  if (ob->rotmode > 0) {
    td->ext->rot = ob->rot;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = NULL;

    copy_v3_v3(td->ext->irot, ob->rot);
    copy_v3_v3(td->ext->drot, ob->drot);
  }
  else if (ob->rotmode == ROT_MODE_AXISANGLE) {
    td->ext->rot = NULL;
    td->ext->rotAxis = ob->rotAxis;
    td->ext->rotAngle = &ob->rotAngle;
    td->ext->quat = NULL;

    td->ext->irotAngle = ob->rotAngle;
    copy_v3_v3(td->ext->irotAxis, ob->rotAxis);
    // td->ext->drotAngle = ob->drotAngle;          // XXX, not implemented
    // copy_v3_v3(td->ext->drotAxis, ob->drotAxis); // XXX, not implemented
  }
  else {
    td->ext->rot = NULL;
    td->ext->rotAxis = NULL;
    td->ext->rotAngle = NULL;
    td->ext->quat = ob->quat;

    copy_qt_qt(td->ext->iquat, ob->quat);
    copy_qt_qt(td->ext->dquat, ob->dquat);
  }
  td->ext->rotOrder = ob->rotmode;

  td->ext->size = ob->scale;
  copy_v3_v3(td->ext->isize, ob->scale);
  copy_v3_v3(td->ext->dscale, ob->dscale);

  copy_v3_v3(td->center, ob->obmat[3]);

  copy_m4_m4(td->ext->obmat, ob->obmat);

  /* is there a need to set the global<->data space conversion matrices? */
  if (ob->parent || constinv) {
    float obmtx[3][3], totmat[3][3], obinv[3][3];

    /* Get the effect of parenting, and/or certain constraints.
     * NOTE: some Constraints, and also Tracking should never get this
     *       done, as it doesn't work well.
     */
    BKE_object_to_mat3(ob, obmtx);
    copy_m3_m4(totmat, ob->obmat);
    invert_m3_m3(obinv, totmat);
    mul_m3_m3m3(td->smtx, obmtx, obinv);
    invert_m3_m3(td->mtx, td->smtx);
  }
  else {
    /* no conversion to/from dataspace */
    unit_m3(td->smtx);
    unit_m3(td->mtx);
  }
}

static void trans_object_base_deps_flag_prepare(ViewLayer *view_layer)
{
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    base->object->id.tag &= ~LIB_TAG_DOIT;
  }
}

static void set_trans_object_base_deps_flag_cb(ID *id,
                                               eDepsObjectComponentType component,
                                               void *UNUSED(user_data))
{
  /* Here we only handle object IDs. */
  if (GS(id->name) != ID_OB) {
    return;
  }
  if (!ELEM(component, DEG_OB_COMP_TRANSFORM, DEG_OB_COMP_GEOMETRY)) {
    return;
  }
  id->tag |= LIB_TAG_DOIT;
}

static void flush_trans_object_base_deps_flag(Depsgraph *depsgraph, Object *object)
{
  object->id.tag |= LIB_TAG_DOIT;
  DEG_foreach_dependent_ID_component(
      depsgraph, &object->id, DEG_OB_COMP_TRANSFORM, set_trans_object_base_deps_flag_cb, NULL);
}

static void trans_object_base_deps_flag_finish(ViewLayer *view_layer)
{
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    if (base->object->id.tag & LIB_TAG_DOIT) {
      base->flag_legacy |= BA_SNAP_FIX_DEPS_FIASCO;
    }
  }
}

/* sets flags in Bases to define whether they take part in transform */
/* it deselects Bases, so we have to call the clear function always after */
static void set_trans_object_base_flags(TransInfo *t)
{
  Main *bmain = CTX_data_main(t->context);
  ViewLayer *view_layer = t->view_layer;
  View3D *v3d = t->view;
  Scene *scene = t->scene;
  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
  /* NOTE: if Base selected and has parent selected:
   *   base->flag_legacy = BA_WAS_SEL
   */
  /* Don't do it if we're not actually going to recalculate anything. */
  if (t->mode == TFM_DUMMY) {
    return;
  }
  /* Makes sure base flags and object flags are identical. */
  BKE_scene_base_flag_to_objects(t->view_layer);
  /* Make sure depsgraph is here. */
  DEG_graph_relations_update(depsgraph, bmain, scene, view_layer);
  /* Clear all flags we need. It will be used to detect dependencies. */
  trans_object_base_deps_flag_prepare(view_layer);
  /* Traverse all bases and set all possible flags. */
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    base->flag_legacy &= ~BA_WAS_SEL;
    if (BASE_SELECTED_EDITABLE(v3d, base)) {
      Object *ob = base->object;
      Object *parsel = ob->parent;
      /* If parent selected, deselect. */
      while (parsel != NULL) {
        if (parsel->base_flag & BASE_SELECTED) {
          Base *parbase = BKE_view_layer_base_find(view_layer, parsel);
          if (parbase != NULL) { /* in rare cases this can fail */
            if (BASE_SELECTED_EDITABLE(v3d, parbase)) {
              break;
            }
          }
        }
        parsel = parsel->parent;
      }
      if (parsel != NULL) {
        /* Rotation around local centers are allowed to propagate. */
        if ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
            (t->mode == TFM_ROTATION || t->mode == TFM_TRACKBALL)) {
          base->flag_legacy |= BA_TRANSFORM_CHILD;
        }
        else {
          base->flag &= ~BASE_SELECTED;
          base->flag_legacy |= BA_WAS_SEL;
        }
      }
      flush_trans_object_base_deps_flag(depsgraph, ob);
    }
  }
  /* Store temporary bits in base indicating that base is being modified
   * (directly or indirectly) by transforming objects.
   */
  trans_object_base_deps_flag_finish(view_layer);
}

static bool mark_children(Object *ob)
{
  if (ob->flag & (SELECT | BA_TRANSFORM_CHILD)) {
    return true;
  }

  if (ob->parent) {
    if (mark_children(ob->parent)) {
      ob->flag |= BA_TRANSFORM_CHILD;
      return true;
    }
  }

  return false;
}

static int count_proportional_objects(TransInfo *t)
{
  int total = 0;
  ViewLayer *view_layer = t->view_layer;
  View3D *v3d = t->view;
  Scene *scene = t->scene;
  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
  /* Clear all flags we need. It will be used to detect dependencies. */
  trans_object_base_deps_flag_prepare(view_layer);
  /* Rotations around local centers are allowed to propagate, so we take all objects. */
  if (!((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
        (t->mode == TFM_ROTATION || t->mode == TFM_TRACKBALL))) {
    /* Mark all parents. */
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      if (BASE_SELECTED_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base)) {
        Object *parent = base->object->parent;
        /* flag all parents */
        while (parent != NULL) {
          parent->flag |= BA_TRANSFORM_PARENT;
          parent = parent->parent;
        }
      }
    }
    /* Mark all children. */
    for (Base *base = view_layer->object_bases.first; base; base = base->next) {
      /* all base not already selected or marked that is editable */
      if ((base->object->flag & (BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT)) == 0 &&
          (base->flag & BASE_SELECTED) == 0 &&
          (BASE_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base))) {
        mark_children(base->object);
      }
    }
  }
  /* Flush changed flags to all dependencies. */
  for (Base *base = view_layer->object_bases.first; base; base = base->next) {
    Object *ob = base->object;
    /* If base is not selected, not a parent of selection or not a child of
     * selection and it is editable and selectable.
     */
    if ((ob->flag & (BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT)) == 0 &&
        (base->flag & BASE_SELECTED) == 0 &&
        (BASE_EDITABLE(v3d, base) && BASE_SELECTABLE(v3d, base))) {
      flush_trans_object_base_deps_flag(depsgraph, ob);
      total += 1;
    }
  }
  /* Store temporary bits in base indicating that base is being modified
   * (directly or indirectly) by transforming objects.
   */
  trans_object_base_deps_flag_finish(view_layer);
  return total;
}

static void clear_trans_object_base_flags(TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  Base *base;

  for (base = view_layer->object_bases.first; base; base = base->next) {
    if (base->flag_legacy & BA_WAS_SEL) {
      ED_object_base_select(base, BA_SELECT);
    }

    base->flag_legacy &= ~(BA_WAS_SEL | BA_SNAP_FIX_DEPS_FIASCO | BA_TEMP_TAG |
                           BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT);
  }
}

/**
 * Auto-keyframing feature - for objects
 *
 * \param tmode: A transform mode.
 *
 * \note Context may not always be available,
 * so must check before using it as it's a luxury for a few cases.
 */
void autokeyframe_object(bContext *C, Scene *scene, ViewLayer *view_layer, Object *ob, int tmode)
{
  Main *bmain = CTX_data_main(C);
  ID *id = &ob->id;
  FCurve *fcu;

  // TODO: this should probably be done per channel instead...
  if (autokeyframe_cfra_can_key(scene, id)) {
    ReportList *reports = CTX_wm_reports(C);
    ToolSettings *ts = scene->toolsettings;
    KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
    ListBase dsources = {NULL, NULL};
    float cfra = (float)CFRA;  // xxx this will do for now
    short flag = 0;

    /* get flags used for inserting keyframes */
    flag = ANIM_get_keyframing_flags(scene, 1);

    /* add datasource override for the object */
    ANIM_relative_keyingset_add_source(&dsources, id, NULL, NULL);

    if (IS_AUTOKEY_FLAG(scene, ONLYKEYINGSET) && (active_ks)) {
      /* Only insert into active keyingset
       * NOTE: we assume here that the active Keying Set
       * does not need to have its iterator overridden.
       */
      ANIM_apply_keyingset(C, &dsources, NULL, active_ks, MODIFYKEY_MODE_INSERT, cfra);
    }
    else if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL)) {
      AnimData *adt = ob->adt;

      /* only key on available channels */
      if (adt && adt->action) {
        ListBase nla_cache = {NULL, NULL};

        for (fcu = adt->action->curves.first; fcu; fcu = fcu->next) {
          fcu->flag &= ~FCURVE_SELECTED;
          insert_keyframe(bmain,
                          reports,
                          id,
                          adt->action,
                          (fcu->grp ? fcu->grp->name : NULL),
                          fcu->rna_path,
                          fcu->array_index,
                          cfra,
                          ts->keyframe_type,
                          &nla_cache,
                          flag);
        }

        BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
      }
    }
    else if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED)) {
      bool do_loc = false, do_rot = false, do_scale = false;

      /* filter the conditions when this happens (assume that curarea->spacetype==SPACE_VIE3D) */
      if (tmode == TFM_TRANSLATION) {
        do_loc = true;
      }
      else if (ELEM(tmode, TFM_ROTATION, TFM_TRACKBALL)) {
        if (scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) {
          if (ob != OBACT(view_layer)) {
            do_loc = true;
          }
        }
        else if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CURSOR) {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_rot = true;
        }
      }
      else if (tmode == TFM_RESIZE) {
        if (scene->toolsettings->transform_pivot_point == V3D_AROUND_ACTIVE) {
          if (ob != OBACT(view_layer)) {
            do_loc = true;
          }
        }
        else if (scene->toolsettings->transform_pivot_point == V3D_AROUND_CURSOR) {
          do_loc = true;
        }

        if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
          do_scale = true;
        }
      }

      /* insert keyframes for the affected sets of channels using the builtin KeyingSets found */
      if (do_loc) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
        ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
      }
      if (do_rot) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
        ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
      }
      if (do_scale) {
        KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_SCALING_ID);
        ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
      }
    }
    /* insert keyframe in all (transform) channels */
    else {
      KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOC_ROT_SCALE_ID);
      ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
    }

    /* free temp info */
    BLI_freelistN(&dsources);
  }
}

/* Return if we need to update motion paths, only if they already exist,
 * and we will insert a keyframe at the end of transform. */
bool motionpath_need_update_object(Scene *scene, Object *ob)
{
  /* XXX: there's potential here for problems with unkeyed rotations/scale,
   *      but for now (until proper data-locality for baking operations),
   *      this should be a better fix for T24451 and T37755
   */

  if (autokeyframe_cfra_can_key(scene, &ob->id)) {
    return (ob->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

/**
 * Auto-keyframing feature - for poses/pose-channels
 *
 * \param tmode: A transform mode.
 *
 * targetless_ik: has targetless ik been done on any channels?
 *
 * \note Context may not always be available,
 * so must check before using it as it's a luxury for a few cases.
 */
void autokeyframe_pose(bContext *C, Scene *scene, Object *ob, int tmode, short targetless_ik)
{
  Main *bmain = CTX_data_main(C);
  ID *id = &ob->id;
  AnimData *adt = ob->adt;
  bAction *act = (adt) ? adt->action : NULL;
  bPose *pose = ob->pose;
  bPoseChannel *pchan;
  FCurve *fcu;

  // TODO: this should probably be done per channel instead...
  if (autokeyframe_cfra_can_key(scene, id)) {
    ReportList *reports = CTX_wm_reports(C);
    ToolSettings *ts = scene->toolsettings;
    KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
    ListBase nla_cache = {NULL, NULL};
    float cfra = (float)CFRA;
    short flag = 0;

    /* flag is initialized from UserPref keyframing settings
     * - special exception for targetless IK - INSERTKEY_MATRIX keyframes should get
     *   visual keyframes even if flag not set, as it's not that useful otherwise
     *   (for quick animation recording)
     */
    flag = ANIM_get_keyframing_flags(scene, 1);

    if (targetless_ik) {
      flag |= INSERTKEY_MATRIX;
    }

    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->bone->flag & (BONE_TRANSFORM | BONE_TRANSFORM_MIRROR)) {

        ListBase dsources = {NULL, NULL};

        /* clear any 'unkeyed' flag it may have */
        pchan->bone->flag &= ~BONE_UNKEYED;

        /* add datasource override for the camera object */
        ANIM_relative_keyingset_add_source(&dsources, id, &RNA_PoseBone, pchan);

        /* only insert into active keyingset? */
        if (IS_AUTOKEY_FLAG(scene, ONLYKEYINGSET) && (active_ks)) {
          /* run the active Keying Set on the current datasource */
          ANIM_apply_keyingset(C, &dsources, NULL, active_ks, MODIFYKEY_MODE_INSERT, cfra);
        }
        /* only insert into available channels? */
        else if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL)) {
          if (act) {
            for (fcu = act->curves.first; fcu; fcu = fcu->next) {
              /* only insert keyframes for this F-Curve if it affects the current bone */
              if (strstr(fcu->rna_path, "bones")) {
                char *pchanName = BLI_str_quoted_substrN(fcu->rna_path, "bones[");

                /* only if bone name matches too...
                 * NOTE: this will do constraints too, but those are ok to do here too?
                 */
                if (pchanName && STREQ(pchanName, pchan->name)) {
                  insert_keyframe(bmain,
                                  reports,
                                  id,
                                  act,
                                  ((fcu->grp) ? (fcu->grp->name) : (NULL)),
                                  fcu->rna_path,
                                  fcu->array_index,
                                  cfra,
                                  ts->keyframe_type,
                                  &nla_cache,
                                  flag);
                }

                if (pchanName) {
                  MEM_freeN(pchanName);
                }
              }
            }
          }
        }
        /* only insert keyframe if needed? */
        else if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED)) {
          bool do_loc = false, do_rot = false, do_scale = false;

          /* Filter the conditions when this happens
           * (assume that 'curarea->spacetype == SPACE_VIEW3D'). */
          if (tmode == TFM_TRANSLATION) {
            if (targetless_ik) {
              do_rot = true;
            }
            else {
              do_loc = true;
            }
          }
          else if (ELEM(tmode, TFM_ROTATION, TFM_TRACKBALL)) {
            if (ELEM(scene->toolsettings->transform_pivot_point,
                     V3D_AROUND_CURSOR,
                     V3D_AROUND_ACTIVE)) {
              do_loc = true;
            }

            if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
              do_rot = true;
            }
          }
          else if (tmode == TFM_RESIZE) {
            if (ELEM(scene->toolsettings->transform_pivot_point,
                     V3D_AROUND_CURSOR,
                     V3D_AROUND_ACTIVE)) {
              do_loc = true;
            }

            if ((scene->toolsettings->transform_flag & SCE_XFORM_AXIS_ALIGN) == 0) {
              do_scale = true;
            }
          }

          if (do_loc) {
            KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOCATION_ID);
            ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
          }
          if (do_rot) {
            KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_ROTATION_ID);
            ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
          }
          if (do_scale) {
            KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_SCALING_ID);
            ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
          }
        }
        /* insert keyframe in all (transform) channels */
        else {
          KeyingSet *ks = ANIM_builtin_keyingset_get_named(NULL, ANIM_KS_LOC_ROT_SCALE_ID);
          ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
        }

        /* free temp info */
        BLI_freelistN(&dsources);
      }
    }

    BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
  }
  else {
    /* tag channels that should have unkeyed data */
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->bone->flag & BONE_TRANSFORM) {
        /* tag this channel */
        pchan->bone->flag |= BONE_UNKEYED;
      }
    }
  }
}

/* Return if we need to update motion paths, only if they already exist,
 * and we will insert a keyframe at the end of transform. */
bool motionpath_need_update_pose(Scene *scene, Object *ob)
{
  if (autokeyframe_cfra_can_key(scene, &ob->id)) {
    return (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS) != 0;
  }

  return false;
}

static void special_aftertrans_update__movieclip(bContext *C, TransInfo *t)
{
  SpaceClip *sc = t->sa->spacedata.first;
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  /* Update coordinates of modified plane tracks. */
  for (MovieTrackingPlaneTrack *plane_track = plane_tracks_base->first; plane_track;
       plane_track = plane_track->next) {
    bool do_update = false;
    if (plane_track->flag & PLANE_TRACK_HIDDEN) {
      continue;
    }
    do_update |= PLANE_TRACK_VIEW_SELECTED(plane_track) != 0;
    if (do_update == false) {
      if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
        int i;
        for (i = 0; i < plane_track->point_tracksnr; i++) {
          MovieTrackingTrack *track = plane_track->point_tracks[i];
          if (TRACK_VIEW_SELECTED(sc, track)) {
            do_update = true;
            break;
          }
        }
      }
    }
    if (do_update) {
      BKE_tracking_track_plane_from_existing_motion(plane_track, framenr);
    }
  }
  if (t->scene->nodetree != NULL) {
    /* Tracks can be used for stabilization nodes,
     * flush update for such nodes.
     */
    nodeUpdateID(t->scene->nodetree, &clip->id);
    WM_event_add_notifier(C, NC_SCENE | ND_NODES, NULL);
  }
}

static void special_aftertrans_update__mask(bContext *C, TransInfo *t)
{
  Mask *mask = NULL;

  if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = t->sa->spacedata.first;
    mask = ED_space_clip_get_mask(sc);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = t->sa->spacedata.first;
    mask = ED_space_image_get_mask(sima);
  }
  else {
    BLI_assert(0);
  }

  if (t->scene->nodetree) {
    /* tracks can be used for stabilization nodes,
     * flush update for such nodes */
    // if (nodeUpdateID(t->scene->nodetree, &mask->id))
    {
      WM_event_add_notifier(C, NC_MASK | ND_DATA, &mask->id);
    }
  }

  /* TODO - dont key all masks... */
  if (IS_AUTOKEY_ON(t->scene)) {
    Scene *scene = t->scene;

    ED_mask_layer_shape_auto_key_select(mask, CFRA);
  }
}

static void special_aftertrans_update__node(bContext *C, TransInfo *t)
{
  Main *bmain = CTX_data_main(C);
  const bool canceled = (t->state == TRANS_CANCEL);

  if (canceled && t->remove_on_cancel) {
    /* remove selected nodes on cancel */
    SpaceNode *snode = (SpaceNode *)t->sa->spacedata.first;
    bNodeTree *ntree = snode->edittree;
    if (ntree) {
      bNode *node, *node_next;
      for (node = ntree->nodes.first; node; node = node_next) {
        node_next = node->next;
        if (node->flag & NODE_SELECT) {
          nodeRemoveNode(bmain, ntree, node, true);
        }
      }
    }
  }
}

static void special_aftertrans_update__mesh(bContext *UNUSED(C), TransInfo *t)
{
  /* so automerge supports mirror */
  if ((t->scene->toolsettings->automerge) && ((t->flag & T_EDIT) && t->obedit_type == OB_MESH)) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {

      BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
      BMesh *bm = em->bm;
      char hflag;
      bool has_face_sel = (bm->totfacesel != 0);

      if (tc->mirror.axis_flag) {
        TransData *td;
        int i;

        /* Rather then adjusting the selection (which the user would notice)
         * tag all mirrored verts, then auto-merge those. */
        BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

        for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
          if (td->extra) {
            BM_elem_flag_enable((BMVert *)td->extra, BM_ELEM_TAG);
          }
        }

        hflag = BM_ELEM_SELECT | BM_ELEM_TAG;
      }
      else {
        hflag = BM_ELEM_SELECT;
      }

      EDBM_automerge(t->scene, tc->obedit, true, hflag);

      /* Special case, this is needed or faces won't re-select.
       * Flush selected edges to faces. */
      if (has_face_sel && (em->selectmode == SCE_SELECT_FACE)) {
        EDBM_selectmode_flush_ex(em, SCE_SELECT_EDGE);
      }
    }
  }
}

/* inserting keys, pointcache, redraw events... */
/**
 * \note Sequencer freeing has its own function now because of a conflict
 * with transform's order of freeing (campbell).
 * Order changed, the sequencer stuff should go back in here
 */
void special_aftertrans_update(bContext *C, TransInfo *t)
{
  Main *bmain = CTX_data_main(t->context);
  BLI_assert(bmain == CTX_data_main(C));

  Object *ob;
  //  short redrawipo=0, resetslowpar=1;
  const bool canceled = (t->state == TRANS_CANCEL);
  const bool duplicate = (t->mode == TFM_TIME_DUPLICATE);

  /* early out when nothing happened */
  if (t->data_len_all == 0 || t->mode == TFM_DUMMY) {
    return;
  }

  if (t->spacetype == SPACE_VIEW3D) {
    if (t->flag & T_EDIT) {
      /* Special Exception:
       * We don't normally access 't->custom.mode' here, but its needed in this case. */

      if (canceled == 0) {
        /* we need to delete the temporary faces before automerging */
        if (t->mode == TFM_EDGE_SLIDE) {
          /* handle multires re-projection, done
           * on transform completion since it's
           * really slow -joeedh */
          projectEdgeSlideData(t, true);

          FOREACH_TRANS_DATA_CONTAINER (t, tc) {
            EdgeSlideData *sld = tc->custom.mode.data;

            if (sld == NULL) {
              continue;
            }

            /* Free temporary faces to avoid auto-merging and deleting
             * during cleanup - psy-fi. */
            freeEdgeSlideTempFaces(sld);
          }
        }
        else if (t->mode == TFM_VERT_SLIDE) {
          /* as above */
          projectVertSlideData(t, true);
          FOREACH_TRANS_DATA_CONTAINER (t, tc) {
            VertSlideData *sld = tc->custom.mode.data;
            freeVertSlideTempFaces(sld);
          }
        }

        if (t->obedit_type == OB_MESH) {
          special_aftertrans_update__mesh(C, t);
        }
      }
      else {
        if (t->mode == TFM_EDGE_SLIDE) {
          EdgeSlideParams *slp = t->custom.mode.data;
          slp->perc = 0.0;
          projectEdgeSlideData(t, false);
        }
        else if (t->mode == TFM_VERT_SLIDE) {
          EdgeSlideParams *slp = t->custom.mode.data;
          slp->perc = 0.0;
          projectVertSlideData(t, false);
        }
      }
    }
  }

  if (t->options & CTX_GPENCIL_STROKES) {
    /* pass */
  }
  else if (t->spacetype == SPACE_SEQ) {
    /* freeSeqData in transform_conversions.c does this
     * keep here so the else at the end wont run... */

    SpaceSeq *sseq = (SpaceSeq *)t->sa->spacedata.first;

    /* marker transform, not especially nice but we may want to move markers
     * at the same time as keyframes in the dope sheet. */
    if ((sseq->flag & SEQ_MARKER_TRANS) && (canceled == 0)) {
      /* cant use TFM_TIME_EXTEND
       * for some reason EXTEND is changed into TRANSLATE, so use frame_side instead */

      if (t->mode == TFM_SEQ_SLIDE) {
        if (t->frame_side == 'B') {
          ED_markers_post_apply_transform(
              &t->scene->markers, t->scene, TFM_TIME_TRANSLATE, t->values[0], t->frame_side);
        }
      }
      else if (ELEM(t->frame_side, 'L', 'R')) {
        ED_markers_post_apply_transform(
            &t->scene->markers, t->scene, TFM_TIME_EXTEND, t->values[0], t->frame_side);
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      special_aftertrans_update__mask(C, t);
    }
  }
  else if (t->spacetype == SPACE_NODE) {
    SpaceNode *snode = (SpaceNode *)t->sa->spacedata.first;
    special_aftertrans_update__node(C, t);
    if (canceled == 0) {
      ED_node_post_apply_transform(C, snode->edittree);

      ED_node_link_insert(bmain, t->sa);
    }

    /* clear link line */
    ED_node_link_intersect_test(t->sa, 0);
  }
  else if (t->spacetype == SPACE_CLIP) {
    if (t->options & CTX_MOVIECLIP) {
      special_aftertrans_update__movieclip(C, t);
    }
    else if (t->options & CTX_MASK) {
      special_aftertrans_update__mask(C, t);
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
    bAnimContext ac;

    /* initialize relevant anim-context 'context' data */
    if (ANIM_animdata_get_context(C, &ac) == 0) {
      return;
    }

    ob = ac.obact;

    if (ELEM(ac.datatype, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY, ANIMCONT_TIMELINE)) {
      ListBase anim_data = {NULL, NULL};
      bAnimListElem *ale;
      short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/);

      /* get channels to work on */
      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

      /* these should all be F-Curves */
      for (ale = anim_data.first; ale; ale = ale->next) {
        AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
        FCurve *fcu = (FCurve *)ale->key_data;

        /* 3 cases here for curve cleanups:
         * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done
         * 2) canceled == 0        -> user confirmed the transform,
         *                            so duplicates should be removed
         * 3) canceled + duplicate -> user canceled the transform,
         *                            but we made duplicates, so get rid of these
         */
        if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
          if (adt) {
            ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 0);
            posttrans_fcurve_clean(fcu, false); /* only use handles in graph editor */
            ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
          }
          else {
            posttrans_fcurve_clean(fcu, false); /* only use handles in graph editor */
          }
        }
      }

      /* free temp memory */
      ANIM_animdata_freelist(&anim_data);
    }
    else if (ac.datatype == ANIMCONT_ACTION) {  // TODO: just integrate into the above...
      /* Depending on the lock status, draw necessary views */
      // fixme... some of this stuff is not good
      if (ob) {
        if (ob->pose || BKE_key_from_object(ob)) {
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
        }
        else {
          DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
        }
      }

      /* 3 cases here for curve cleanups:
       * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done
       * 2) canceled == 0        -> user confirmed the transform,
       *                            so duplicates should be removed.
       * 3) canceled + duplicate -> user canceled the transform,
       *                            but we made duplicates, so get rid of these.
       */
      if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
        posttrans_action_clean(&ac, (bAction *)ac.data);
      }
    }
    else if (ac.datatype == ANIMCONT_GPENCIL) {
      /* remove duplicate frames and also make sure points are in order! */
      /* 3 cases here for curve cleanups:
       * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done
       * 2) canceled == 0        -> user confirmed the transform,
       *                            so duplicates should be removed
       * 3) canceled + duplicate -> user canceled the transform,
       *                            but we made duplicates, so get rid of these
       */
      if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
        bGPdata *gpd;

        // XXX: BAD! this get gpencil datablocks directly from main db...
        // but that's how this currently works :/
        for (gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
          if (ID_REAL_USERS(gpd)) {
            posttrans_gpd_clean(gpd);
          }
        }
      }
    }
    else if (ac.datatype == ANIMCONT_MASK) {
      /* remove duplicate frames and also make sure points are in order! */
      /* 3 cases here for curve cleanups:
       * 1) NOTRANSKEYCULL on:
       *    Cleanup of duplicates shouldn't be done.
       * 2) canceled == 0:
       *    User confirmed the transform, so duplicates should be removed.
       * 3) Canceled + duplicate:
       *    User canceled the transform, but we made duplicates, so get rid of these.
       */
      if ((saction->flag & SACTION_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
        Mask *mask;

        // XXX: BAD! this get gpencil datablocks directly from main db...
        // but that's how this currently works :/
        for (mask = bmain->masks.first; mask; mask = mask->id.next) {
          if (ID_REAL_USERS(mask)) {
            posttrans_mask_clean(mask);
          }
        }
      }
    }

    /* marker transform, not especially nice but we may want to move markers
     * at the same time as keyframes in the dope sheet.
     */
    if ((saction->flag & SACTION_MARKERS_MOVE) && (canceled == 0)) {
      if (t->mode == TFM_TIME_TRANSLATE) {
#if 0
        if (ELEM(t->frame_side, 'L', 'R')) { /* TFM_TIME_EXTEND */
          /* same as below */
          ED_markers_post_apply_transform(
              ED_context_get_markers(C), t->scene, t->mode, t->values[0], t->frame_side);
        }
        else /* TFM_TIME_TRANSLATE */
#endif
        {
          ED_markers_post_apply_transform(
              ED_context_get_markers(C), t->scene, t->mode, t->values[0], t->frame_side);
        }
      }
      else if (t->mode == TFM_TIME_SCALE) {
        ED_markers_post_apply_transform(
            ED_context_get_markers(C), t->scene, t->mode, t->values[0], t->frame_side);
      }
    }

    /* make sure all F-Curves are set correctly */
    if (!ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
      ANIM_editkeyframes_refresh(&ac);
    }

    /* clear flag that was set for time-slide drawing */
    saction->flag &= ~SACTION_MOVING;
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
    bAnimContext ac;
    const bool use_handle = (sipo->flag & SIPO_NOHANDLES) == 0;

    /* initialize relevant anim-context 'context' data */
    if (ANIM_animdata_get_context(C, &ac) == 0) {
      return;
    }

    if (ac.datatype) {
      ListBase anim_data = {NULL, NULL};
      bAnimListElem *ale;
      short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE);

      /* get channels to work on */
      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

      for (ale = anim_data.first; ale; ale = ale->next) {
        AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
        FCurve *fcu = (FCurve *)ale->key_data;

        /* 3 cases here for curve cleanups:
         * 1) NOTRANSKEYCULL on    -> cleanup of duplicates shouldn't be done
         * 2) canceled == 0        -> user confirmed the transform,
         *                            so duplicates should be removed
         * 3) canceled + duplicate -> user canceled the transform,
         *                            but we made duplicates, so get rid of these
         */
        if ((sipo->flag & SIPO_NOTRANSKEYCULL) == 0 && ((canceled == 0) || (duplicate))) {
          if (adt) {
            ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 0);
            posttrans_fcurve_clean(fcu, use_handle);
            ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
          }
          else {
            posttrans_fcurve_clean(fcu, use_handle);
          }
        }
      }

      /* free temp memory */
      ANIM_animdata_freelist(&anim_data);
    }

    /* Make sure all F-Curves are set correctly, but not if transform was
     * canceled, since then curves were already restored to initial state.
     * Note: if the refresh is really needed after cancel then some way
     *       has to be added to not update handle types (see bug 22289).
     */
    if (!canceled) {
      ANIM_editkeyframes_refresh(&ac);
    }
  }
  else if (t->spacetype == SPACE_NLA) {
    bAnimContext ac;

    /* initialize relevant anim-context 'context' data */
    if (ANIM_animdata_get_context(C, &ac) == 0) {
      return;
    }

    if (ac.datatype) {
      ListBase anim_data = {NULL, NULL};
      bAnimListElem *ale;
      short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT);

      /* get channels to work on */
      ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

      for (ale = anim_data.first; ale; ale = ale->next) {
        NlaTrack *nlt = (NlaTrack *)ale->data;

        /* make sure strips are in order again */
        BKE_nlatrack_sort_strips(nlt);

        /* remove the temp metas */
        BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
      }

      /* free temp memory */
      ANIM_animdata_freelist(&anim_data);

      /* perform after-transfrom validation */
      ED_nla_postop_refresh(&ac);
    }
  }
  else if (t->flag & T_EDIT) {
    if (t->obedit_type == OB_MESH) {
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
        /* table needs to be created for each edit command, since vertices can move etc */
        ED_mesh_mirror_spatial_table(tc->obedit, em, NULL, NULL, 'e');
        /* TODO(campbell): xform: We need support for many mirror objects at once! */
        break;
      }
    }
  }
  else if (t->flag & T_POSE && (t->mode == TFM_BONESIZE)) {
    /* Handle the exception where for TFM_BONESIZE in edit mode we pretend to be
     * in pose mode (to use bone orientation matrix),
     * in that case we don't do operations like autokeyframing. */
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      ob = tc->poseobj;
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }
  else if (t->flag & T_POSE) {
    GSet *motionpath_updates = BLI_gset_ptr_new("motionpath updates");

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {

      bPoseChannel *pchan;
      short targetless_ik = 0;

      ob = tc->poseobj;

      if ((t->flag & T_AUTOIK) && (t->options & CTX_AUTOCONFIRM)) {
        /* when running transform non-interactively (operator exec),
         * we need to update the pose otherwise no updates get called during
         * transform and the auto-ik is not applied. see [#26164] */
        struct Object *pose_ob = tc->poseobj;
        BKE_pose_where_is(t->depsgraph, t->scene, pose_ob);
      }

      /* set BONE_TRANSFORM flags for autokey, gizmo draw might have changed them */
      if (!canceled && (t->mode != TFM_DUMMY)) {
        count_set_pose_transflags(ob, t->mode, t->around, NULL);
      }

      /* if target-less IK grabbing, we calculate the pchan transforms and clear flag */
      if (!canceled && t->mode == TFM_TRANSLATION) {
        targetless_ik = apply_targetless_ik(ob);
      }
      else {
        /* not forget to clear the auto flag */
        for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
          bKinematicConstraint *data = has_targetless_ik(pchan);
          if (data) {
            data->flag &= ~CONSTRAINT_IK_AUTO;
          }
        }
      }

      if (t->mode == TFM_TRANSLATION) {
        pose_grab_with_ik_clear(bmain, ob);
      }

      /* automatic inserting of keys and unkeyed tagging -
       * only if transform wasn't canceled (or TFM_DUMMY) */
      if (!canceled && (t->mode != TFM_DUMMY)) {
        autokeyframe_pose(C, t->scene, ob, t->mode, targetless_ik);
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
      else {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }

      if (t->mode != TFM_DUMMY && motionpath_need_update_pose(t->scene, ob)) {
        BLI_gset_insert(motionpath_updates, ob);
      }
    }

    /* Update motion paths once for all transformed bones in an object. */
    GSetIterator gs_iter;
    GSET_ITER (gs_iter, motionpath_updates) {
      bool current_frame_only = canceled;
      ob = BLI_gsetIterator_getKey(&gs_iter);
      ED_pose_recalculate_paths(C, t->scene, ob, current_frame_only);
    }
    BLI_gset_free(motionpath_updates, NULL);
  }
  else if (t->options & CTX_PAINT_CURVE) {
    /* pass */
  }
  else if ((t->view_layer->basact) && (ob = t->view_layer->basact->object) &&
           (ob->mode & OB_MODE_PARTICLE_EDIT) && PE_get_current(t->scene, ob)) {
    /* do nothing */
  }
  else if (t->flag & T_CURSOR) {
    /* do nothing */
  }
  else { /* Objects */
    BLI_assert(t->flag & (T_OBJECT | T_TEXTURE));

    TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
    bool motionpath_update = false;

    for (int i = 0; i < tc->data_len; i++) {
      TransData *td = tc->data + i;
      ListBase pidlist;
      PTCacheID *pid;
      ob = td->ob;

      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      /* flag object caches as outdated */
      BKE_ptcache_ids_from_object(&pidlist, ob, t->scene, MAX_DUPLI_RECUR);
      for (pid = pidlist.first; pid; pid = pid->next) {
        if (pid->type != PTCACHE_TYPE_PARTICLES) {
          /* particles don't need reset on geometry change */
          pid->cache->flag |= PTCACHE_OUTDATED;
        }
      }
      BLI_freelistN(&pidlist);

      /* pointcache refresh */
      if (BKE_ptcache_object_reset(t->scene, ob, PTCACHE_RESET_OUTDATED)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }

      /* Needed for proper updating of "quick cached" dynamics. */
      /* Creates troubles for moving animated objects without */
      /* autokey though, probably needed is an anim sys override? */
      /* Please remove if some other solution is found. -jahka */
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

      /* Set autokey if necessary */
      if (!canceled) {
        autokeyframe_object(C, t->scene, t->view_layer, ob, t->mode);
      }

      motionpath_update |= motionpath_need_update_object(t->scene, ob);

      /* restore rigid body transform */
      if (ob->rigidbody_object && canceled) {
        float ctime = BKE_scene_frame_get(t->scene);
        if (BKE_rigidbody_check_sim_running(t->scene->rigidbody_world, ctime)) {
          BKE_rigidbody_aftertrans_update(ob,
                                          td->ext->oloc,
                                          td->ext->orot,
                                          td->ext->oquat,
                                          td->ext->orotAxis,
                                          td->ext->orotAngle);
        }
      }
    }

    if (motionpath_update) {
      /* Update motion paths once for all transformed objects. */
      bool current_frame_only = canceled;
      ED_objects_recalculate_paths(C, t->scene, current_frame_only);
    }
  }

  clear_trans_object_base_flags(t);
}

int special_transform_moving(TransInfo *t)
{
  if (t->spacetype == SPACE_SEQ) {
    return G_TRANSFORM_SEQ;
  }
  else if (t->spacetype == SPACE_GRAPH) {
    return G_TRANSFORM_FCURVES;
  }
  else if ((t->flag & T_EDIT) || (t->flag & T_POSE)) {
    return G_TRANSFORM_EDIT;
  }
  else if (t->flag & (T_OBJECT | T_TEXTURE)) {
    return G_TRANSFORM_OBJ;
  }

  return 0;
}

static void createTransObject(bContext *C, TransInfo *t)
{
  TransData *td = NULL;
  TransDataExtension *tx;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

  set_trans_object_base_flags(t);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* count */
  tc->data_len = CTX_DATA_COUNT(C, selected_bases);

  if (!tc->data_len) {
    /* clear here, main transform function escapes too */
    clear_trans_object_base_flags(t);
    return;
  }

  if (is_prop_edit) {
    tc->data_len += count_proportional_objects(t);
  }

  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransOb");
  tx = tc->data_ext = MEM_callocN(tc->data_len * sizeof(TransDataExtension), "TransObExtension");

  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    Object *ob = base->object;

    td->flag = TD_SELECTED;
    td->protectflag = ob->protectflag;
    td->ext = tx;
    td->ext->rotOrder = ob->rotmode;

    if (base->flag & BA_TRANSFORM_CHILD) {
      td->flag |= TD_NOCENTER;
      td->flag |= TD_NO_LOC;
    }

    /* select linked objects, but skip them later */
    if (ID_IS_LINKED(ob)) {
      td->flag |= TD_SKIP;
    }

    ObjectToTransData(t, td, ob);
    td->val = NULL;
    td++;
    tx++;
  }
  CTX_DATA_END;

  if (is_prop_edit) {
    ViewLayer *view_layer = t->view_layer;
    View3D *v3d = t->view;
    Base *base;

    for (base = view_layer->object_bases.first; base; base = base->next) {
      Object *ob = base->object;

      /* if base is not selected, not a parent of selection
       * or not a child of selection and it is editable and selectable */
      if ((ob->flag & (BA_TRANSFORM_CHILD | BA_TRANSFORM_PARENT)) == 0 &&
          (base->flag & BASE_SELECTED) == 0 && BASE_EDITABLE(v3d, base) &&
          BASE_SELECTABLE(v3d, base)) {
        td->protectflag = ob->protectflag;
        td->ext = tx;
        td->ext->rotOrder = ob->rotmode;

        ObjectToTransData(t, td, ob);
        td->val = NULL;
        td++;
        tx++;
      }
    }
  }
}

/* transcribe given node into TransData2D for Transforming */
static void NodeToTransData(TransData *td, TransData2D *td2d, bNode *node, const float dpi_fac)
{
  float locx, locy;

  /* account for parents (nested nodes) */
  if (node->parent) {
    nodeToView(node->parent, node->locx, node->locy, &locx, &locy);
  }
  else {
    locx = node->locx;
    locy = node->locy;
  }

  /* use top-left corner as the transform origin for nodes */
  /* weirdo - but the node system is a mix of free 2d elements and dpi sensitive UI */
#ifdef USE_NODE_CENTER
  td2d->loc[0] = (locx * dpi_fac) + (BLI_rctf_size_x(&node->totr) * +0.5f);
  td2d->loc[1] = (locy * dpi_fac) + (BLI_rctf_size_y(&node->totr) * -0.5f);
#else
  td2d->loc[0] = locx * dpi_fac;
  td2d->loc[1] = locy * dpi_fac;
#endif
  td2d->loc[2] = 0.0f;
  td2d->loc2d = td2d->loc; /* current location */

  td->flag = 0;

  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);
  /* use node center instead of origin (top-left corner) */
  td->center[0] = td2d->loc[0];
  td->center[1] = td2d->loc[1];
  td->center[2] = 0.0f;

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  td->extra = node;
}

static bool is_node_parent_select(bNode *node)
{
  while ((node = node->parent)) {
    if (node->flag & NODE_TRANSFORM) {
      return true;
    }
  }
  return false;
}

static void createTransNodeData(bContext *UNUSED(C), TransInfo *t)
{
  const float dpi_fac = UI_DPI_FAC;
  TransData *td;
  TransData2D *td2d;
  SpaceNode *snode = t->sa->spacedata.first;
  bNode *node;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!snode->edittree) {
    return;
  }

  /* nodes dont support PET and probably never will */
  t->flag &= ~T_PROP_EDIT_ALL;

  /* set transform flags on nodes */
  for (node = snode->edittree->nodes.first; node; node = node->next) {
    if (node->flag & NODE_SELECT && is_node_parent_select(node) == false) {
      node->flag |= NODE_TRANSFORM;
      tc->data_len++;
    }
    else {
      node->flag &= ~NODE_TRANSFORM;
    }
  }

  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransNode TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransNode TransData2D");

  for (node = snode->edittree->nodes.first; node; node = node->next) {
    if (node->flag & NODE_TRANSFORM) {
      NodeToTransData(td++, td2d++, node, dpi_fac);
    }
  }
}

/* *** CLIP EDITOR *** */

/* * motion tracking * */

enum transDataTracking_Mode {
  transDataTracking_ModeTracks = 0,
  transDataTracking_ModeCurves = 1,
  transDataTracking_ModePlaneTracks = 2,
};

typedef struct TransDataTracking {
  int mode, flag;

  /* tracks transformation from main window */
  int area;
  const float *relative, *loc;
  float soffset[2], srelative[2];
  float offset[2];

  float (*smarkers)[2];
  int markersnr;
  MovieTrackingMarker *markers;

  /* marker transformation from curves editor */
  float *prev_pos, scale;
  short coord;

  MovieTrackingTrack *track;
  MovieTrackingPlaneTrack *plane_track;
} TransDataTracking;

static void markerToTransDataInit(TransData *td,
                                  TransData2D *td2d,
                                  TransDataTracking *tdt,
                                  MovieTrackingTrack *track,
                                  MovieTrackingMarker *marker,
                                  int area,
                                  float loc[2],
                                  float rel[2],
                                  const float off[2],
                                  const float aspect[2])
{
  int anchor = area == TRACK_AREA_POINT && off;

  tdt->mode = transDataTracking_ModeTracks;

  if (anchor) {
    td2d->loc[0] = rel[0] * aspect[0]; /* hold original location */
    td2d->loc[1] = rel[1] * aspect[1];

    tdt->loc = loc;
    td2d->loc2d = loc; /* current location */
  }
  else {
    td2d->loc[0] = loc[0] * aspect[0]; /* hold original location */
    td2d->loc[1] = loc[1] * aspect[1];

    td2d->loc2d = loc; /* current location */
  }
  td2d->loc[2] = 0.0f;

  tdt->relative = rel;
  tdt->area = area;

  tdt->markersnr = track->markersnr;
  tdt->markers = track->markers;
  tdt->track = track;

  if (rel) {
    if (!anchor) {
      td2d->loc[0] += rel[0] * aspect[0];
      td2d->loc[1] += rel[1] * aspect[1];
    }

    copy_v2_v2(tdt->srelative, rel);
  }

  if (off) {
    copy_v2_v2(tdt->soffset, off);
  }

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);

  // copy_v3_v3(td->center, td->loc);
  td->flag |= TD_INDIVIDUAL_SCALE;
  td->center[0] = marker->pos[0] * aspect[0];
  td->center[1] = marker->pos[1] * aspect[1];

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

static void trackToTransData(const int framenr,
                             TransData *td,
                             TransData2D *td2d,
                             TransDataTracking *tdt,
                             MovieTrackingTrack *track,
                             const float aspect[2])
{
  MovieTrackingMarker *marker = BKE_tracking_marker_ensure(track, framenr);

  tdt->flag = marker->flag;
  marker->flag &= ~(MARKER_DISABLED | MARKER_TRACKED);

  markerToTransDataInit(td++,
                        td2d++,
                        tdt++,
                        track,
                        marker,
                        TRACK_AREA_POINT,
                        track->offset,
                        marker->pos,
                        track->offset,
                        aspect);

  if (track->flag & SELECT) {
    markerToTransDataInit(
        td++, td2d++, tdt++, track, marker, TRACK_AREA_POINT, marker->pos, NULL, NULL, aspect);
  }

  if (track->pat_flag & SELECT) {
    int a;

    for (a = 0; a < 4; a++) {
      markerToTransDataInit(td++,
                            td2d++,
                            tdt++,
                            track,
                            marker,
                            TRACK_AREA_PAT,
                            marker->pattern_corners[a],
                            marker->pos,
                            NULL,
                            aspect);
    }
  }

  if (track->search_flag & SELECT) {
    markerToTransDataInit(td++,
                          td2d++,
                          tdt++,
                          track,
                          marker,
                          TRACK_AREA_SEARCH,
                          marker->search_min,
                          marker->pos,
                          NULL,
                          aspect);

    markerToTransDataInit(td++,
                          td2d++,
                          tdt++,
                          track,
                          marker,
                          TRACK_AREA_SEARCH,
                          marker->search_max,
                          marker->pos,
                          NULL,
                          aspect);
  }
}

static void planeMarkerToTransDataInit(TransData *td,
                                       TransData2D *td2d,
                                       TransDataTracking *tdt,
                                       MovieTrackingPlaneTrack *plane_track,
                                       float corner[2],
                                       const float aspect[2])
{
  tdt->mode = transDataTracking_ModePlaneTracks;
  tdt->plane_track = plane_track;

  td2d->loc[0] = corner[0] * aspect[0]; /* hold original location */
  td2d->loc[1] = corner[1] * aspect[1];

  td2d->loc2d = corner; /* current location */
  td2d->loc[2] = 0.0f;

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->center, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

static void planeTrackToTransData(const int framenr,
                                  TransData *td,
                                  TransData2D *td2d,
                                  TransDataTracking *tdt,
                                  MovieTrackingPlaneTrack *plane_track,
                                  const float aspect[2])
{
  MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_ensure(plane_track, framenr);
  int i;

  tdt->flag = plane_marker->flag;
  plane_marker->flag &= ~PLANE_MARKER_TRACKED;

  for (i = 0; i < 4; i++) {
    planeMarkerToTransDataInit(td++, td2d++, tdt++, plane_track, plane_marker->corners[i], aspect);
  }
}

static void transDataTrackingFree(TransInfo *UNUSED(t),
                                  TransDataContainer *UNUSED(tc),
                                  TransCustomData *custom_data)
{
  if (custom_data->data) {
    TransDataTracking *tdt = custom_data->data;
    if (tdt->smarkers) {
      MEM_freeN(tdt->smarkers);
    }

    MEM_freeN(tdt);
    custom_data->data = NULL;
  }
}

static void createTransTrackingTracksData(bContext *C, TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
  MovieTrackingTrack *track;
  MovieTrackingPlaneTrack *plane_track;
  TransDataTracking *tdt;
  int framenr = ED_space_clip_get_clip_frame_number(sc);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* count */
  tc->data_len = 0;

  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      tc->data_len++; /* offset */

      if (track->flag & SELECT) {
        tc->data_len++;
      }

      if (track->pat_flag & SELECT) {
        tc->data_len += 4;
      }

      if (track->search_flag & SELECT) {
        tc->data_len += 2;
      }
    }

    track = track->next;
  }

  for (plane_track = plane_tracks_base->first; plane_track; plane_track = plane_track->next) {
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      tc->data_len += 4;
    }
  }

  if (tc->data_len == 0) {
    return;
  }

  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransTracking TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D),
                                   "TransTracking TransData2D");
  tdt = tc->custom.type.data = MEM_callocN(tc->data_len * sizeof(TransDataTracking),
                                           "TransTracking TransDataTracking");

  tc->custom.type.free_cb = transDataTrackingFree;

  /* create actual data */
  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      trackToTransData(framenr, td, td2d, tdt, track, t->aspect);

      /* offset */
      td++;
      td2d++;
      tdt++;

      if (track->flag & SELECT) {
        td++;
        td2d++;
        tdt++;
      }

      if (track->pat_flag & SELECT) {
        td += 4;
        td2d += 4;
        tdt += 4;
      }

      if (track->search_flag & SELECT) {
        td += 2;
        td2d += 2;
        tdt += 2;
      }
    }

    track = track->next;
  }

  for (plane_track = plane_tracks_base->first; plane_track; plane_track = plane_track->next) {
    if (PLANE_TRACK_VIEW_SELECTED(plane_track)) {
      planeTrackToTransData(framenr, td, td2d, tdt, plane_track, t->aspect);
      td += 4;
      td2d += 4;
      tdt += 4;
    }
  }
}

static void markerToTransCurveDataInit(TransData *td,
                                       TransData2D *td2d,
                                       TransDataTracking *tdt,
                                       MovieTrackingTrack *track,
                                       MovieTrackingMarker *marker,
                                       MovieTrackingMarker *prev_marker,
                                       short coord,
                                       float size)
{
  float frames_delta = (marker->framenr - prev_marker->framenr);

  tdt->flag = marker->flag;
  marker->flag &= ~MARKER_TRACKED;

  tdt->mode = transDataTracking_ModeCurves;
  tdt->coord = coord;
  tdt->scale = 1.0f / size * frames_delta;
  tdt->prev_pos = prev_marker->pos;
  tdt->track = track;

  /* calculate values depending on marker's speed */
  td2d->loc[0] = marker->framenr;
  td2d->loc[1] = (marker->pos[coord] - prev_marker->pos[coord]) * size / frames_delta;
  td2d->loc[2] = 0.0f;

  td2d->loc2d = marker->pos; /* current location */

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

static void createTransTrackingCurvesData(bContext *C, TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *tracksbase = BKE_tracking_get_active_tracks(&clip->tracking);
  MovieTrackingTrack *track;
  MovieTrackingMarker *marker, *prev_marker;
  TransDataTracking *tdt;
  int i, width, height;

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* count */
  tc->data_len = 0;

  if ((sc->flag & SC_SHOW_GRAPH_TRACKS_MOTION) == 0) {
    return;
  }

  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      for (i = 1; i < track->markersnr; i++) {
        marker = &track->markers[i];
        prev_marker = &track->markers[i - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & MARKER_GRAPH_SEL_X) {
          tc->data_len += 1;
        }

        if (marker->flag & MARKER_GRAPH_SEL_Y) {
          tc->data_len += 1;
        }
      }
    }

    track = track->next;
  }

  if (tc->data_len == 0) {
    return;
  }

  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransTracking TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D),
                                   "TransTracking TransData2D");
  tc->custom.type.data = tdt = MEM_callocN(tc->data_len * sizeof(TransDataTracking),
                                           "TransTracking TransDataTracking");
  tc->custom.type.free_cb = transDataTrackingFree;

  /* create actual data */
  track = tracksbase->first;
  while (track) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      for (i = 1; i < track->markersnr; i++) {
        marker = &track->markers[i];
        prev_marker = &track->markers[i - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & MARKER_GRAPH_SEL_X) {
          markerToTransCurveDataInit(
              td, td2d, tdt, track, marker, &track->markers[i - 1], 0, width);
          td += 1;
          td2d += 1;
          tdt += 1;
        }

        if (marker->flag & MARKER_GRAPH_SEL_Y) {
          markerToTransCurveDataInit(
              td, td2d, tdt, track, marker, &track->markers[i - 1], 1, height);

          td += 1;
          td2d += 1;
          tdt += 1;
        }
      }
    }

    track = track->next;
  }
}

static void createTransTrackingData(bContext *C, TransInfo *t)
{
  ARegion *ar = CTX_wm_region(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int width, height;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!clip) {
    return;
  }

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }

  if (ar->regiontype == RGN_TYPE_PREVIEW) {
    /* transformation was called from graph editor */
    createTransTrackingCurvesData(C, t);
  }
  else {
    createTransTrackingTracksData(C, t);
  }
}

static void cancelTransTracking(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  SpaceClip *sc = t->sa->spacedata.first;
  int i, framenr = ED_space_clip_get_clip_frame_number(sc);
  TransDataTracking *tdt_array = tc->custom.type.data;

  i = 0;
  while (i < tc->data_len) {
    TransDataTracking *tdt = &tdt_array[i];

    if (tdt->mode == transDataTracking_ModeTracks) {
      MovieTrackingTrack *track = tdt->track;
      MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

      marker->flag = tdt->flag;

      if (track->flag & SELECT) {
        i++;
      }

      if (track->pat_flag & SELECT) {
        i += 4;
      }

      if (track->search_flag & SELECT) {
        i += 2;
      }
    }
    else if (tdt->mode == transDataTracking_ModeCurves) {
      MovieTrackingTrack *track = tdt->track;
      MovieTrackingMarker *marker, *prev_marker;
      int a;

      for (a = 1; a < track->markersnr; a++) {
        marker = &track->markers[a];
        prev_marker = &track->markers[a - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & (MARKER_GRAPH_SEL_X | MARKER_GRAPH_SEL_Y)) {
          marker->flag = tdt->flag;
        }
      }
    }
    else if (tdt->mode == transDataTracking_ModePlaneTracks) {
      MovieTrackingPlaneTrack *plane_track = tdt->plane_track;
      MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

      plane_marker->flag = tdt->flag;
      i += 3;
    }

    i++;
  }
}

void flushTransTracking(TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  TransDataTracking *tdt;
  int a;

  if (t->state == TRANS_CANCEL) {
    cancelTransTracking(t);
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data, td2d = tc->data_2d, tdt = tc->custom.type.data; a < tc->data_len;
       a++, td2d++, td++, tdt++) {
    if (tdt->mode == transDataTracking_ModeTracks) {
      float loc2d[2];

      if (t->mode == TFM_ROTATION && tdt->area == TRACK_AREA_SEARCH) {
        continue;
      }

      loc2d[0] = td2d->loc[0] / t->aspect[0];
      loc2d[1] = td2d->loc[1] / t->aspect[1];

      if (t->flag & T_ALT_TRANSFORM) {
        if (t->mode == TFM_RESIZE) {
          if (tdt->area != TRACK_AREA_PAT) {
            continue;
          }
        }
        else if (t->mode == TFM_TRANSLATION) {
          if (tdt->area == TRACK_AREA_POINT && tdt->relative) {
            float d[2], d2[2];

            if (!tdt->smarkers) {
              tdt->smarkers = MEM_callocN(sizeof(*tdt->smarkers) * tdt->markersnr,
                                          "flushTransTracking markers");
              for (a = 0; a < tdt->markersnr; a++) {
                copy_v2_v2(tdt->smarkers[a], tdt->markers[a].pos);
              }
            }

            sub_v2_v2v2(d, loc2d, tdt->soffset);
            sub_v2_v2(d, tdt->srelative);

            sub_v2_v2v2(d2, loc2d, tdt->srelative);

            for (a = 0; a < tdt->markersnr; a++) {
              add_v2_v2v2(tdt->markers[a].pos, tdt->smarkers[a], d2);
            }

            negate_v2_v2(td2d->loc2d, d);
          }
        }
      }

      if (tdt->area != TRACK_AREA_POINT || tdt->relative == NULL) {
        td2d->loc2d[0] = loc2d[0];
        td2d->loc2d[1] = loc2d[1];

        if (tdt->relative) {
          sub_v2_v2(td2d->loc2d, tdt->relative);
        }
      }
    }
    else if (tdt->mode == transDataTracking_ModeCurves) {
      td2d->loc2d[tdt->coord] = tdt->prev_pos[tdt->coord] + td2d->loc[1] * tdt->scale;
    }
    else if (tdt->mode == transDataTracking_ModePlaneTracks) {
      td2d->loc2d[0] = td2d->loc[0] / t->aspect[0];
      td2d->loc2d[1] = td2d->loc[1] / t->aspect[1];
    }
  }
}

/* * masking * */

typedef struct TransDataMasking {
  bool is_handle;

  float handle[2], orig_handle[2];
  float vec[3][3];
  MaskSplinePoint *point;
  float parent_matrix[3][3];
  float parent_inverse_matrix[3][3];
  char orig_handle_type;

  eMaskWhichHandle which_handle;
} TransDataMasking;

static void MaskHandleToTransData(MaskSplinePoint *point,
                                  eMaskWhichHandle which_handle,
                                  TransData *td,
                                  TransData2D *td2d,
                                  TransDataMasking *tdm,
                                  const float asp[2],
                                  /*const*/ float parent_matrix[3][3],
                                  /*const*/ float parent_inverse_matrix[3][3])
{
  BezTriple *bezt = &point->bezt;
  const bool is_sel_any = MASKPOINT_ISSEL_ANY(point);

  tdm->point = point;
  copy_m3_m3(tdm->vec, bezt->vec);

  tdm->is_handle = true;
  copy_m3_m3(tdm->parent_matrix, parent_matrix);
  copy_m3_m3(tdm->parent_inverse_matrix, parent_inverse_matrix);

  BKE_mask_point_handle(point, which_handle, tdm->handle);
  tdm->which_handle = which_handle;

  copy_v2_v2(tdm->orig_handle, tdm->handle);

  mul_v2_m3v2(td2d->loc, parent_matrix, tdm->handle);
  td2d->loc[0] *= asp[0];
  td2d->loc[1] *= asp[1];
  td2d->loc[2] = 0.0f;

  td2d->loc2d = tdm->handle;

  td->flag = 0;
  td->loc = td2d->loc;
  mul_v2_m3v2(td->center, parent_matrix, bezt->vec[1]);
  td->center[0] *= asp[0];
  td->center[1] *= asp[1];
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  if (is_sel_any) {
    td->flag |= TD_SELECTED;
  }

  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  if (which_handle == MASK_WHICH_HANDLE_LEFT) {
    tdm->orig_handle_type = bezt->h1;
  }
  else if (which_handle == MASK_WHICH_HANDLE_RIGHT) {
    tdm->orig_handle_type = bezt->h2;
  }
}

static void MaskPointToTransData(Scene *scene,
                                 MaskSplinePoint *point,
                                 TransData *td,
                                 TransData2D *td2d,
                                 TransDataMasking *tdm,
                                 const bool is_prop_edit,
                                 const float asp[2])
{
  BezTriple *bezt = &point->bezt;
  const bool is_sel_point = MASKPOINT_ISSEL_KNOT(point);
  const bool is_sel_any = MASKPOINT_ISSEL_ANY(point);
  float parent_matrix[3][3], parent_inverse_matrix[3][3];

  BKE_mask_point_parent_matrix_get(point, CFRA, parent_matrix);
  invert_m3_m3(parent_inverse_matrix, parent_matrix);

  if (is_prop_edit || is_sel_point) {
    int i;

    tdm->point = point;
    copy_m3_m3(tdm->vec, bezt->vec);

    for (i = 0; i < 3; i++) {
      copy_m3_m3(tdm->parent_matrix, parent_matrix);
      copy_m3_m3(tdm->parent_inverse_matrix, parent_inverse_matrix);

      /* CV coords are scaled by aspects. this is needed for rotations and
       * proportional editing to be consistent with the stretched CV coords
       * that are displayed. this also means that for display and numinput,
       * and when the CV coords are flushed, these are converted each time */
      mul_v2_m3v2(td2d->loc, parent_matrix, bezt->vec[i]);
      td2d->loc[0] *= asp[0];
      td2d->loc[1] *= asp[1];
      td2d->loc[2] = 0.0f;

      td2d->loc2d = bezt->vec[i];

      td->flag = 0;
      td->loc = td2d->loc;
      mul_v2_m3v2(td->center, parent_matrix, bezt->vec[1]);
      td->center[0] *= asp[0];
      td->center[1] *= asp[1];
      copy_v3_v3(td->iloc, td->loc);

      memset(td->axismtx, 0, sizeof(td->axismtx));
      td->axismtx[2][2] = 1.0f;

      td->ext = NULL;

      if (i == 1) {
        /* scaling weights */
        td->val = &bezt->weight;
        td->ival = *td->val;
      }
      else {
        td->val = NULL;
      }

      if (is_sel_any) {
        td->flag |= TD_SELECTED;
      }
      td->dist = 0.0;

      unit_m3(td->mtx);
      unit_m3(td->smtx);

      if (i == 0) {
        tdm->orig_handle_type = bezt->h1;
      }
      else if (i == 2) {
        tdm->orig_handle_type = bezt->h2;
      }

      td++;
      td2d++;
      tdm++;
    }
  }
  else {
    if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
      MaskHandleToTransData(point,
                            MASK_WHICH_HANDLE_STICK,
                            td,
                            td2d,
                            tdm,
                            asp,
                            parent_matrix,
                            parent_inverse_matrix);

      td++;
      td2d++;
      tdm++;
    }
    else {
      if (bezt->f1 & SELECT) {
        MaskHandleToTransData(point,
                              MASK_WHICH_HANDLE_LEFT,
                              td,
                              td2d,
                              tdm,
                              asp,
                              parent_matrix,
                              parent_inverse_matrix);

        if (bezt->h1 == HD_VECT) {
          bezt->h1 = HD_FREE;
        }
        else if (bezt->h1 == HD_AUTO) {
          bezt->h1 = HD_ALIGN_DOUBLESIDE;
          bezt->h2 = HD_ALIGN_DOUBLESIDE;
        }

        td++;
        td2d++;
        tdm++;
      }
      if (bezt->f3 & SELECT) {
        MaskHandleToTransData(point,
                              MASK_WHICH_HANDLE_RIGHT,
                              td,
                              td2d,
                              tdm,
                              asp,
                              parent_matrix,
                              parent_inverse_matrix);

        if (bezt->h2 == HD_VECT) {
          bezt->h2 = HD_FREE;
        }
        else if (bezt->h2 == HD_AUTO) {
          bezt->h1 = HD_ALIGN_DOUBLESIDE;
          bezt->h2 = HD_ALIGN_DOUBLESIDE;
        }

        td++;
        td2d++;
        tdm++;
      }
    }
  }
}

static void createTransMaskingData(bContext *C, TransInfo *t)
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  MaskLayer *masklay;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataMasking *tdm = NULL;
  int count = 0, countsel = 0;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT);
  float asp[2];

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!mask) {
    return;
  }

  if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = t->sa->spacedata.first;
    MovieClip *clip = ED_space_clip_get_clip(sc);
    if (!clip) {
      return;
    }
  }

  /* count */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      int i;

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          if (MASKPOINT_ISSEL_KNOT(point)) {
            countsel += 3;
          }
          else {
            if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
              countsel += 1;
            }
            else {
              BezTriple *bezt = &point->bezt;
              if (bezt->f1 & SELECT) {
                countsel++;
              }
              if (bezt->f3 & SELECT) {
                countsel++;
              }
            }
          }
        }

        if (is_prop_edit) {
          count += 3;
        }
      }
    }
  }

  /* note: in prop mode we need at least 1 selected */
  if (countsel == 0) {
    return;
  }

  ED_mask_get_aspect(t->sa, t->ar, &asp[0], &asp[1]);

  tc->data_len = (is_prop_edit) ? count : countsel;
  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Mask Editing)");
  /* for each 2d uv coord a 3d vector is allocated, so that they can be
   * treated just as if they were 3d verts */
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D),
                                   "TransObData2D(Mask Editing)");
  tc->custom.type.data = tdm = MEM_callocN(tc->data_len * sizeof(TransDataMasking),
                                           "TransDataMasking(Mask Editing)");
  tc->custom.type.use_free = true;

  /* create data */
  for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
    MaskSpline *spline;

    if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
      continue;
    }

    for (spline = masklay->splines.first; spline; spline = spline->next) {
      int i;

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (is_prop_edit || MASKPOINT_ISSEL_ANY(point)) {
          MaskPointToTransData(scene, point, td, td2d, tdm, is_prop_edit, asp);

          if (is_prop_edit || MASKPOINT_ISSEL_KNOT(point)) {
            td += 3;
            td2d += 3;
            tdm += 3;
          }
          else {
            if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
              td++;
              td2d++;
              tdm++;
            }
            else {
              BezTriple *bezt = &point->bezt;
              if (bezt->f1 & SELECT) {
                td++;
                td2d++;
                tdm++;
              }
              if (bezt->f3 & SELECT) {
                td++;
                td2d++;
                tdm++;
              }
            }
          }
        }
      }
    }
  }
}

void flushTransMasking(TransInfo *t)
{
  TransData2D *td;
  TransDataMasking *tdm;
  int a;
  float asp[2], inv[2];

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  ED_mask_get_aspect(t->sa, t->ar, &asp[0], &asp[1]);
  inv[0] = 1.0f / asp[0];
  inv[1] = 1.0f / asp[1];

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data_2d, tdm = tc->custom.type.data; a < tc->data_len; a++, td++, tdm++) {
    td->loc2d[0] = td->loc[0] * inv[0];
    td->loc2d[1] = td->loc[1] * inv[1];
    mul_m3_v2(tdm->parent_inverse_matrix, td->loc2d);

    if (tdm->is_handle) {
      BKE_mask_point_set_handle(tdm->point,
                                tdm->which_handle,
                                td->loc2d,
                                (t->flag & T_ALT_TRANSFORM) != 0,
                                tdm->orig_handle,
                                tdm->vec);
    }

    if (t->state == TRANS_CANCEL) {
      if (tdm->which_handle == MASK_WHICH_HANDLE_LEFT) {
        tdm->point->bezt.h1 = tdm->orig_handle_type;
      }
      else if (tdm->which_handle == MASK_WHICH_HANDLE_RIGHT) {
        tdm->point->bezt.h2 = tdm->orig_handle_type;
      }
    }
  }
}

typedef struct TransDataPaintCurve {
  PaintCurvePoint *pcp; /* initial curve point */
  char id;
} TransDataPaintCurve;

#define PC_IS_ANY_SEL(pc) (((pc)->bez.f1 | (pc)->bez.f2 | (pc)->bez.f3) & SELECT)

static void PaintCurveConvertHandle(
    PaintCurvePoint *pcp, int id, TransData2D *td2d, TransDataPaintCurve *tdpc, TransData *td)
{
  BezTriple *bezt = &pcp->bez;
  copy_v2_v2(td2d->loc, bezt->vec[id]);
  td2d->loc[2] = 0.0f;
  td2d->loc2d = bezt->vec[id];

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, bezt->vec[1]);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;
  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  tdpc->id = id;
  tdpc->pcp = pcp;
}

static void PaintCurvePointToTransData(PaintCurvePoint *pcp,
                                       TransData *td,
                                       TransData2D *td2d,
                                       TransDataPaintCurve *tdpc)
{
  BezTriple *bezt = &pcp->bez;

  if (pcp->bez.f2 == SELECT) {
    int i;
    for (i = 0; i < 3; i++) {
      copy_v2_v2(td2d->loc, bezt->vec[i]);
      td2d->loc[2] = 0.0f;
      td2d->loc2d = bezt->vec[i];

      td->flag = 0;
      td->loc = td2d->loc;
      copy_v3_v3(td->center, bezt->vec[1]);
      copy_v3_v3(td->iloc, td->loc);

      memset(td->axismtx, 0, sizeof(td->axismtx));
      td->axismtx[2][2] = 1.0f;

      td->ext = NULL;
      td->val = NULL;
      td->flag |= TD_SELECTED;
      td->dist = 0.0;

      unit_m3(td->mtx);
      unit_m3(td->smtx);

      tdpc->id = i;
      tdpc->pcp = pcp;

      td++;
      td2d++;
      tdpc++;
    }
  }
  else {
    if (bezt->f3 & SELECT) {
      PaintCurveConvertHandle(pcp, 2, td2d, tdpc, td);
      td2d++;
      tdpc++;
      td++;
    }

    if (bezt->f1 & SELECT) {
      PaintCurveConvertHandle(pcp, 0, td2d, tdpc, td);
    }
  }
}

static void createTransPaintCurveVerts(bContext *C, TransInfo *t)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  PaintCurve *pc;
  PaintCurvePoint *pcp;
  Brush *br;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataPaintCurve *tdpc = NULL;
  int i;
  int total = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!paint || !paint->brush || !paint->brush->paint_curve) {
    return;
  }

  br = paint->brush;
  pc = br->paint_curve;

  for (pcp = pc->points, i = 0; i < pc->tot_points; i++, pcp++) {
    if (PC_IS_ANY_SEL(pcp)) {
      if (pcp->bez.f2 & SELECT) {
        total += 3;
        continue;
      }
      else {
        if (pcp->bez.f1 & SELECT) {
          total++;
        }
        if (pcp->bez.f3 & SELECT) {
          total++;
        }
      }
    }
  }

  if (!total) {
    return;
  }

  tc->data_len = total;
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransData2D");
  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData");
  tc->custom.type.data = tdpc = MEM_callocN(tc->data_len * sizeof(TransDataPaintCurve),
                                            "TransDataPaintCurve");
  tc->custom.type.use_free = true;

  for (pcp = pc->points, i = 0; i < pc->tot_points; i++, pcp++) {
    if (PC_IS_ANY_SEL(pcp)) {
      PaintCurvePointToTransData(pcp, td, td2d, tdpc);

      if (pcp->bez.f2 & SELECT) {
        td += 3;
        td2d += 3;
        tdpc += 3;
      }
      else {
        if (pcp->bez.f1 & SELECT) {
          td++;
          td2d++;
          tdpc++;
        }
        if (pcp->bez.f3 & SELECT) {
          td++;
          td2d++;
          tdpc++;
        }
      }
    }
  }
}

void flushTransPaintCurve(TransInfo *t)
{
  int i;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  TransData2D *td2d = tc->data_2d;
  TransDataPaintCurve *tdpc = tc->custom.type.data;

  for (i = 0; i < tc->data_len; i++, tdpc++, td2d++) {
    PaintCurvePoint *pcp = tdpc->pcp;
    copy_v2_v2(pcp->bez.vec[tdpc->id], td2d->loc);
  }
}

static void createTransGPencil_center_get(bGPDstroke *gps, float r_center[3])
{
  bGPDspoint *pt;
  int i;

  zero_v3(r_center);
  int tot_sel = 0;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      add_v3_v3(r_center, &pt->x);
      tot_sel++;
    }
  }

  if (tot_sel > 0) {
    mul_v3_fl(r_center, 1.0f / tot_sel);
  }
}

static void createTransGPencil(bContext *C, TransInfo *t)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bool use_multiframe_falloff = (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;

  Object *obact = CTX_data_active_object(C);
  bGPDlayer *gpl;
  TransData *td = NULL;
  float mtx[3][3], smtx[3][3];

  const Scene *scene = CTX_data_scene(C);
  const int cfra_scene = CFRA;

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_edit_connected = (t->flag & T_PROP_CONNECTED) != 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* == Grease Pencil Strokes to Transform Data ==
   * Grease Pencil stroke points can be a mixture of 2D (screen-space),
   * or 3D coordinates. However, they're always saved as 3D points.
   * For now, we just do these without creating TransData2D for the 2D
   * strokes. This may cause issues in future though.
   */
  tc->data_len = 0;

  if (gpd == NULL) {
    return;
  }

  /* initialize falloff curve */
  if (is_multiedit) {
    curvemapping_initialize(ts->gp_sculpt.cur_falloff);
  }

  /* First Pass: Count the number of data-points required for the strokes,
   * (and additional info about the configuration - e.g. 2D/3D?).
   */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf;
      bGPDstroke *gps;
      bGPDframe *init_gpf = gpl->actframe;
      if (is_multiedit) {
        init_gpf = gpl->frames.first;
      }

      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          for (gps = gpf->strokes.first; gps; gps = gps->next) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
              continue;
            }

            if (is_prop_edit) {
              /* Proportional Editing... */
              if (is_prop_edit_connected) {
                /* connected only - so only if selected */
                if (gps->flag & GP_STROKE_SELECT) {
                  tc->data_len += gps->totpoints;
                }
              }
              else {
                /* everything goes - connection status doesn't matter */
                tc->data_len += gps->totpoints;
              }
            }
            else {
              /* only selected stroke points are considered */
              if (gps->flag & GP_STROKE_SELECT) {
                bGPDspoint *pt;
                int i;

                // TODO: 2D vs 3D?
                for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                  if (pt->flag & GP_SPOINT_SELECT) {
                    tc->data_len++;
                  }
                }
              }
            }
          }
        }
        /* if not multiedit out of loop */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }

  /* Stop trying if nothing selected */
  if (tc->data_len == 0) {
    return;
  }

  /* Allocate memory for data */
  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(GPencil)");
  td = tc->data;

  unit_m3(smtx);
  unit_m3(mtx);

  /* Second Pass: Build transdata array */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      const int cfra = (gpl->flag & GP_LAYER_FRAMELOCK) ? gpl->actframe->framenum : cfra_scene;
      bGPDframe *gpf = gpl->actframe;
      bGPDstroke *gps;
      float diff_mat[4][4];
      float inverse_diff_mat[4][4];

      bGPDframe *init_gpf = gpl->actframe;
      if (is_multiedit) {
        init_gpf = gpl->frames.first;
      }
      /* init multiframe falloff options */
      int f_init = 0;
      int f_end = 0;

      if (use_multiframe_falloff) {
        BKE_gpencil_get_range_selected(gpl, &f_init, &f_end);
      }

      /* calculate difference matrix */
      ED_gpencil_parent_location(depsgraph, obact, gpd, gpl, diff_mat);
      /* undo matrix */
      invert_m4_m4(inverse_diff_mat, diff_mat);

      /* Make a new frame to work on if the layer's frame
       * and the current scene frame don't match up.
       *
       * - This is useful when animating as it saves that "uh-oh" moment when you realize you've
       *   spent too much time editing the wrong frame...
       */
      // XXX: should this be allowed when framelock is enabled?
      if ((gpf->framenum != cfra) && (!is_multiedit)) {
        gpf = BKE_gpencil_frame_addcopy(gpl, cfra);
        /* in some weird situations (framelock enabled) return NULL */
        if (gpf == NULL) {
          continue;
        }
        if (!is_multiedit) {
          init_gpf = gpf;
        }
      }

      /* Loop over strokes, adding TransData for points as needed... */
      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

          /* if multiframe and falloff, recalculate and save value */
          float falloff = 1.0f; /* by default no falloff */
          if ((is_multiedit) && (use_multiframe_falloff)) {
            /* Faloff depends on distance to active frame (relative to the overall frame range) */
            falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }

          for (gps = gpf->strokes.first; gps; gps = gps->next) {
            TransData *head = td;
            TransData *tail = td;
            bool stroke_ok;

            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
              continue;
            }
            /* What we need to include depends on proportional editing settings... */
            if (is_prop_edit) {
              if (is_prop_edit_connected) {
                /* A) "Connected" - Only those in selected strokes */
                stroke_ok = (gps->flag & GP_STROKE_SELECT) != 0;
              }
              else {
                /* B) All points, always */
                stroke_ok = true;
              }
            }
            else {
              /* C) Only selected points in selected strokes */
              stroke_ok = (gps->flag & GP_STROKE_SELECT) != 0;
            }

            /* Do stroke... */
            if (stroke_ok && gps->totpoints) {
              bGPDspoint *pt;
              int i;

              /* save falloff factor */
              gps->runtime.multi_frame_falloff = falloff;

              /* calculate stroke center */
              float center[3];
              createTransGPencil_center_get(gps, center);

              /* add all necessary points... */
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                bool point_ok;

                /* include point? */
                if (is_prop_edit) {
                  /* Always all points in strokes that get included */
                  point_ok = true;
                }
                else {
                  /* Only selected points in selected strokes */
                  point_ok = (pt->flag & GP_SPOINT_SELECT) != 0;
                }

                /* do point... */
                if (point_ok) {
                  copy_v3_v3(td->iloc, &pt->x);
                  /* only copy center in local origins.
                   * This allows get interesting effects also when move
                   * using proportional editing */
                  if ((gps->flag & GP_STROKE_SELECT) &&
                      (ts->transform_pivot_point == V3D_AROUND_LOCAL_ORIGINS)) {
                    copy_v3_v3(td->center, center);
                  }
                  else {
                    copy_v3_v3(td->center, &pt->x);
                  }

                  td->loc = &pt->x;

                  td->flag = 0;

                  if (pt->flag & GP_SPOINT_SELECT) {
                    td->flag |= TD_SELECTED;
                  }

                  /* for other transform modes (e.g. shrink-fatten), need to additional data
                   * but never for scale or mirror
                   */
                  if ((t->mode != TFM_RESIZE) && (t->mode != TFM_MIRROR)) {
                    if (t->mode != TFM_GPENCIL_OPACITY) {
                      td->val = &pt->pressure;
                      td->ival = pt->pressure;
                    }
                    else {
                      td->val = &pt->strength;
                      td->ival = pt->strength;
                    }
                  }

                  /* screenspace needs special matrices... */
                  if ((gps->flag & (GP_STROKE_3DSPACE | GP_STROKE_2DSPACE | GP_STROKE_2DIMAGE)) ==
                      0) {
                    /* screenspace */
                    td->protectflag = OB_LOCK_LOCZ | OB_LOCK_ROTZ | OB_LOCK_SCALEZ;
                  }
                  else {
                    /* configure 2D dataspace points so that they don't play up... */
                    if (gps->flag & (GP_STROKE_2DSPACE | GP_STROKE_2DIMAGE)) {
                      td->protectflag = OB_LOCK_LOCZ | OB_LOCK_ROTZ | OB_LOCK_SCALEZ;
                    }
                  }
                  /* apply parent transformations */
                  copy_m3_m4(td->smtx, inverse_diff_mat); /* final position */
                  copy_m3_m4(td->mtx, diff_mat);          /* display position */
                  copy_m3_m4(td->axismtx, diff_mat);      /* axis orientation */

                  /* Triangulation must be calculated again,
                   * so save the stroke for recalc function */
                  td->extra = gps;

                  /* save pointer to object */
                  td->ob = obact;

                  td++;
                  tail++;
                }
              }

              /* March over these points, and calculate the proportional editing distances */
              if (is_prop_edit && (head != tail)) {
                /* XXX: for now, we are similar enough that this works... */
                calc_distanceCurveVerts(head, tail - 1);
              }
            }
          }
        }
        /* if not multiedit out of loop */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
}

static int countAndCleanTransDataContainer(TransInfo *t)
{
  BLI_assert(ELEM(t->data_len_all, 0, -1));
  t->data_len_all = 0;
  uint data_container_len_orig = t->data_container_len;
  for (TransDataContainer *th_end = t->data_container - 1,
                          *tc = t->data_container + (t->data_container_len - 1);
       tc != th_end;
       tc--) {
    if (tc->data_len == 0) {
      uint index = tc - t->data_container;
      if (index + 1 != t->data_container_len) {
        SWAP(TransDataContainer,
             t->data_container[index],
             t->data_container[t->data_container_len - 1]);
      }
      t->data_container_len -= 1;
    }
    else {
      t->data_len_all += tc->data_len;
    }
  }
  if (data_container_len_orig != t->data_container_len) {
    t->data_container = MEM_reallocN(t->data_container,
                                     sizeof(*t->data_container) * t->data_container_len);
  }
  return t->data_len_all;
}

void createTransData(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  ViewLayer *view_layer = t->view_layer;
  Object *ob = OBACT(view_layer);

  bool has_transform_context = true;
  t->data_len_all = -1;

  /* if tests must match recalcData for correct updates */
  if (t->options & CTX_CURSOR) {
    t->flag |= T_CURSOR;
    t->obedit_type = -1;

    if (t->spacetype == SPACE_IMAGE) {
      createTransCursor_image(t);
    }
    else {
      createTransCursor_view3d(t);
    }
    countAndCleanTransDataContainer(t);
  }
  else if (t->options & CTX_TEXTURE) {
    t->flag |= T_TEXTURE;
    t->obedit_type = -1;

    createTransTexspace(t);
    countAndCleanTransDataContainer(t);
  }
  else if (t->options & CTX_EDGE) {
    /* Multi object editing. */
    initTransDataContainers_FromObjectData(t, ob, NULL, 0);
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      tc->data_ext = NULL;
    }
    t->flag |= T_EDIT;

    createTransEdge(t);
    countAndCleanTransDataContainer(t);

    if (t->data_len_all && t->flag & T_PROP_EDIT) {
      sort_trans_data_selected_first(t);
      set_prop_dist(t, 1);
      sort_trans_data_dist(t);
    }
  }
  else if (t->options & CTX_GPENCIL_STROKES) {
    t->options |= CTX_GPENCIL_STROKES;
    t->flag |= T_POINTS | T_EDIT;

    initTransDataContainers_FromObjectData(t, ob, NULL, 0);
    createTransGPencil(C, t);
    countAndCleanTransDataContainer(t);

    if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
      sort_trans_data_selected_first(t);
      set_prop_dist(t, 1);
      sort_trans_data_dist(t);
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    t->flag |= T_POINTS | T_2D_EDIT;
    if (t->options & CTX_MASK) {

      /* copied from below */
      createTransMaskingData(C, t);
      countAndCleanTransDataContainer(t);

      if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
        sort_trans_data_selected_first(t);
        set_prop_dist(t, true);
        sort_trans_data_dist(t);
      }
    }
    else if (t->options & CTX_PAINT_CURVE) {
      if (!ELEM(t->mode, TFM_SHEAR, TFM_SHRINKFATTEN)) {
        createTransPaintCurveVerts(C, t);
        countAndCleanTransDataContainer(t);
      }
      else {
        has_transform_context = false;
      }
    }
    else if (t->obedit_type == OB_MESH) {

      initTransDataContainers_FromObjectData(t, ob, NULL, 0);
      createTransUVs(C, t);
      countAndCleanTransDataContainer(t);

      t->flag |= T_EDIT;

      if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
        sort_trans_data_selected_first(t);
        set_prop_dist(t, 1);
        sort_trans_data_dist(t);
      }
    }
    else {
      has_transform_context = false;
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    t->flag |= T_POINTS | T_2D_EDIT;
    t->obedit_type = -1;

    createTransActionData(C, t);
    countAndCleanTransDataContainer(t);

    if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
      sort_trans_data_selected_first(t);
      /* don't do that, distance has been set in createTransActionData already */
      // set_prop_dist(t, false);
      sort_trans_data_dist(t);
    }
  }
  else if (t->spacetype == SPACE_NLA) {
    t->flag |= T_POINTS | T_2D_EDIT;
    t->obedit_type = -1;

    createTransNlaData(C, t);
    countAndCleanTransDataContainer(t);
  }
  else if (t->spacetype == SPACE_SEQ) {
    t->flag |= T_POINTS | T_2D_EDIT;
    t->obedit_type = -1;

    t->num.flag |= NUM_NO_FRACTION; /* sequencer has no use for floating point trasnform */
    createTransSeqData(C, t);
    countAndCleanTransDataContainer(t);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    t->flag |= T_POINTS | T_2D_EDIT;
    t->obedit_type = -1;

    createTransGraphEditData(C, t);
    countAndCleanTransDataContainer(t);

    if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
      /* makes selected become first in array */
      sort_trans_data_selected_first(t);

      /* don't do that, distance has been set in createTransGraphEditData already */
      set_prop_dist(t, false);

      sort_trans_data_dist(t);
    }
  }
  else if (t->spacetype == SPACE_NODE) {
    t->flag |= T_POINTS | T_2D_EDIT;
    t->obedit_type = -1;

    createTransNodeData(C, t);
    countAndCleanTransDataContainer(t);

    if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
      sort_trans_data_selected_first(t);
      set_prop_dist(t, 1);
      sort_trans_data_dist(t);
    }
  }
  else if (t->spacetype == SPACE_CLIP) {
    t->flag |= T_POINTS | T_2D_EDIT;
    t->obedit_type = -1;

    if (t->options & CTX_MOVIECLIP) {
      createTransTrackingData(C, t);
      countAndCleanTransDataContainer(t);
    }
    else if (t->options & CTX_MASK) {
      /* copied from above */
      createTransMaskingData(C, t);
      countAndCleanTransDataContainer(t);

      if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
        sort_trans_data_selected_first(t);
        set_prop_dist(t, true);
        sort_trans_data_dist(t);
      }
    }
    else {
      has_transform_context = false;
    }
  }
  else if (t->obedit_type != -1) {
    /* Multi object editing. */
    initTransDataContainers_FromObjectData(t, ob, NULL, 0);

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      tc->data_ext = NULL;
    }
    if (t->obedit_type == OB_MESH) {
      createTransEditVerts(t);
    }
    else if (ELEM(t->obedit_type, OB_CURVE, OB_SURF)) {
      createTransCurveVerts(t);
    }
    else if (t->obedit_type == OB_LATTICE) {
      createTransLatticeVerts(t);
    }
    else if (t->obedit_type == OB_MBALL) {
      createTransMBallVerts(t);
    }
    else if (t->obedit_type == OB_ARMATURE) {
      t->flag &= ~T_PROP_EDIT;
      createTransArmatureVerts(t);
    }
    else {
      printf("edit type not implemented!\n");
    }

    countAndCleanTransDataContainer(t);

    t->flag |= T_EDIT | T_POINTS;

    if (t->data_len_all) {
      if (t->flag & T_PROP_EDIT) {
        if (ELEM(t->obedit_type, OB_CURVE, OB_MESH)) {
          sort_trans_data_selected_first(t);
          if ((t->obedit_type == OB_MESH) && (t->flag & T_PROP_CONNECTED)) {
            /* already calculated by editmesh_set_connectivity_distance */
          }
          else {
            set_prop_dist(t, 0);
          }
          sort_trans_data_dist(t);
        }
        else {
          sort_trans_data_selected_first(t);
          set_prop_dist(t, 1);
          sort_trans_data_dist(t);
        }
      }
      else {
        if (ELEM(t->obedit_type, OB_CURVE)) {
          /* Needed because bezier handles can be partially selected
           * and are still added into transform data. */
          sort_trans_data_selected_first(t);
        }
      }
    }

    /* exception... hackish, we want bonesize to use bone orientation matrix (ton) */
    if (t->mode == TFM_BONESIZE) {
      t->flag &= ~(T_EDIT | T_POINTS);
      t->flag |= T_POSE;
      t->obedit_type = -1;

      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        tc->poseobj = tc->obedit;
        tc->obedit = NULL;
      }
    }
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    /* XXX this is currently limited to active armature only... */

    /* XXX active-layer checking isn't done
     * as that should probably be checked through context instead. */

    /* Multi object editing. */
    initTransDataContainers_FromObjectData(t, ob, NULL, 0);
    createTransPose(t);
    countAndCleanTransDataContainer(t);
  }
  else if (ob && (ob->mode & OB_MODE_WEIGHT_PAINT) && !(t->options & CTX_PAINT_CURVE)) {
    /* important that ob_armature can be set even when its not selected [#23412]
     * lines below just check is also visible */
    has_transform_context = false;
    Object *ob_armature = modifiers_isDeformedByArmature(ob);
    if (ob_armature && ob_armature->mode & OB_MODE_POSE) {
      Base *base_arm = BKE_view_layer_base_find(t->view_layer, ob_armature);
      if (base_arm) {
        View3D *v3d = t->view;
        if (BASE_VISIBLE(v3d, base_arm)) {
          Object *objects[1];
          objects[0] = ob_armature;
          uint objects_len = 1;
          initTransDataContainers_FromObjectData(t, ob_armature, objects, objects_len);
          createTransPose(t);
          countAndCleanTransDataContainer(t);
          has_transform_context = true;
        }
      }
    }
  }
  else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT) && PE_start_edit(PE_get_current(scene, ob))) {
    createTransParticleVerts(C, t);
    countAndCleanTransDataContainer(t);
    t->flag |= T_POINTS;

    if (t->data_len_all && t->flag & T_PROP_EDIT) {
      sort_trans_data_selected_first(t);
      set_prop_dist(t, 1);
      sort_trans_data_dist(t);
    }
  }
  else if (ob && (ob->mode & OB_MODE_ALL_PAINT)) {
    if ((t->options & CTX_PAINT_CURVE) && !ELEM(t->mode, TFM_SHEAR, TFM_SHRINKFATTEN)) {
      t->flag |= T_POINTS | T_2D_EDIT;
      createTransPaintCurveVerts(C, t);
      countAndCleanTransDataContainer(t);
    }
    else {
      has_transform_context = false;
    }
  }
  else if ((ob) &&
           (ELEM(
               ob->mode, OB_MODE_PAINT_GPENCIL, OB_MODE_SCULPT_GPENCIL, OB_MODE_WEIGHT_GPENCIL))) {
    /* In grease pencil all transformations must be canceled if not Object or Edit. */
    has_transform_context = false;
  }
  else {
    /* Needed for correct Object.obmat after duplication, see: T62135. */
    BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

    createTransObject(C, t);
    countAndCleanTransDataContainer(t);
    t->flag |= T_OBJECT;

    if (t->data_len_all && t->flag & T_PROP_EDIT) {
      // selected objects are already first, no need to presort
      set_prop_dist(t, 1);
      sort_trans_data_dist(t);
    }

    /* Check if we're transforming the camera from the camera */
    if ((t->spacetype == SPACE_VIEW3D) && (t->ar->regiontype == RGN_TYPE_WINDOW)) {
      View3D *v3d = t->view;
      RegionView3D *rv3d = t->ar->regiondata;
      if ((rv3d->persp == RV3D_CAMOB) && v3d->camera) {
        /* we could have a flag to easily check an object is being transformed */
        if (v3d->camera->id.tag & LIB_TAG_DOIT) {
          t->flag |= T_CAMERA;
        }
      }
    }
  }

  /* Check that 'countAndCleanTransDataContainer' ran. */
  if (has_transform_context) {
    BLI_assert(t->data_len_all != -1);
  }
  else {
    BLI_assert(t->data_len_all == -1);
    t->data_len_all = 0;
  }

  BLI_assert((!(t->flag & T_EDIT)) == (!(t->obedit_type != -1)));
}
