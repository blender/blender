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

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_modifier.h"
#include "BKE_nla.h"
#include "BKE_node.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_tracking.h"

#include "BIK_api.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_clip.h"
#include "ED_image.h"
#include "ED_keyframes_edit.h"
#include "ED_keyframing.h"
#include "ED_markers.h"
#include "ED_mask.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_object.h"
#include "ED_particle.h"

#include "UI_view2d.h"

#include "WM_api.h" /* for WM_event_add_notifier to deal with stabilization nodes */
#include "WM_types.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_mode.h"

bool transform_mode_use_local_origins(const TransInfo *t)
{
  return ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL);
}

/**
 * Transforming around ourselves is no use, fallback to individual origins,
 * useful for curve/armatures.
 */
void transform_around_single_fallback(TransInfo *t)
{
  if ((ELEM(t->around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN, V3D_AROUND_ACTIVE)) &&
      transform_mode_use_local_origins(t)) {

    bool is_data_single = false;
    if (t->data_len_all == 1) {
      is_data_single = true;
    }
    else if (t->data_len_all == 3) {
      if (t->obedit_type == OB_CURVE) {
        /* Special case check for curve, if we have a single curve bezier triple selected
         * treat */
        FOREACH_TRANS_DATA_CONTAINER (t, tc) {
          if (!tc->data_len) {
            continue;
          }
          if (tc->data_len == 3) {
            const TransData *td = tc->data;
            if ((td[0].loc == td[1].loc) && (td[1].loc == td[2].loc)) {
              is_data_single = true;
            }
          }
          break;
        }
      }
    }
    if (is_data_single) {
      t->around = V3D_AROUND_LOCAL_ORIGINS;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Proportional Editing
 * \{ */

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

/**
 * Distance calculated from not-selected vertex to nearest selected vertex.
 */
static void set_prop_dist(TransInfo *t, const bool with_dist)
{
  int a;

  float _proj_vec[3];
  const float *proj_vec = NULL;

  /* support for face-islands */
  const bool use_island = transdata_check_local_islands(t, t->around);

  if (t->flag & T_PROP_PROJECTED) {
    if (t->spacetype == SPACE_VIEW3D && t->region && t->region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = t->region->regiondata;
      normalize_v3_v3(_proj_vec, rv3d->viewinv[2]);
      proj_vec = _proj_vec;
    }
  }

  /* Count number of selected. */
  int td_table_len = 0;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (a = 0; a < tc->data_len; a++, td++) {
      if (td->flag & TD_SELECTED) {
        td_table_len++;
      }
      else {
        /* By definition transform-data has selected items in beginning. */
        break;
      }
    }
  }

  /* Pointers to selected's #TransData.
   * Used to find #TransData from the index returned by #BLI_kdtree_find_nearest. */
  TransData **td_table = MEM_mallocN(sizeof(*td_table) * td_table_len, __func__);

  /* Create and fill kd-tree of selected's positions - in global or proj_vec space. */
  KDTree_3d *td_tree = BLI_kdtree_3d_new(td_table_len);

  int td_table_index = 0;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (a = 0; a < tc->data_len; a++, td++) {
      if (td->flag & TD_SELECTED) {
        /* Initialize, it was mallocced. */
        float vec[3];
        td->rdist = 0.0f;

        if (use_island) {
          if (tc->use_local_mat) {
            mul_v3_m4v3(vec, tc->mat, td->iloc);
          }
          else {
            mul_v3_m3v3(vec, td->mtx, td->iloc);
          }
        }
        else {
          if (tc->use_local_mat) {
            mul_v3_m4v3(vec, tc->mat, td->center);
          }
          else {
            mul_v3_m3v3(vec, td->mtx, td->center);
          }
        }

        if (proj_vec) {
          float vec_p[3];
          project_v3_v3v3(vec_p, vec, proj_vec);
          sub_v3_v3(vec, vec_p);
        }

        BLI_kdtree_3d_insert(td_tree, td_table_index, vec);
        td_table[td_table_index++] = td;
      }
      else {
        /* By definition transform-data has selected items in beginning. */
        break;
      }
    }
  }
  BLI_assert(td_table_index == td_table_len);

  BLI_kdtree_3d_balance(td_tree);

  /* For each non-selected vertex, find distance to the nearest selected vertex. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (a = 0; a < tc->data_len; a++, td++) {
      if ((td->flag & TD_SELECTED) == 0) {
        float vec[3];

        if (use_island) {
          if (tc->use_local_mat) {
            mul_v3_m4v3(vec, tc->mat, td->iloc);
          }
          else {
            mul_v3_m3v3(vec, td->mtx, td->iloc);
          }
        }
        else {
          if (tc->use_local_mat) {
            mul_v3_m4v3(vec, tc->mat, td->center);
          }
          else {
            mul_v3_m3v3(vec, td->mtx, td->center);
          }
        }

        if (proj_vec) {
          float vec_p[3];
          project_v3_v3v3(vec_p, vec, proj_vec);
          sub_v3_v3(vec, vec_p);
        }

        KDTreeNearest_3d nearest;
        const int td_index = BLI_kdtree_3d_find_nearest(td_tree, vec, &nearest);

        td->rdist = -1.0f;
        if (td_index != -1) {
          td->rdist = nearest.dist;
          if (use_island) {
            copy_v3_v3(td->center, td_table[td_index]->center);
            copy_m3_m3(td->axismtx, td_table[td_index]->axismtx);
          }
        }

        if (with_dist) {
          td->dist = td->rdist;
        }
      }
    }
  }

  BLI_kdtree_3d_free(td_tree);
  MEM_freeN(td_table);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Mode
 * \{ */

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
        float mat[4][4];

        /* pose_mat(b) = pose_mat(b-1) * offs_bone * channel * constraint * IK  */
        /* we put in channel the entire result of mat = (channel * constraint * IK) */
        /* pose_mat(b) = pose_mat(b-1) * offs_bone * mat  */
        /* mat = pose_mat(b) * inv(pose_mat(b-1) * offs_bone ) */

        parchan = chanlist[segcount - 1];
        bone = parchan->bone;
        bone->flag |= BONE_TRANSFORM; /* ensures it gets an auto key inserted */

        BKE_armature_mat_pose_to_bone(parchan, parchan->pose_mat, mat);
        /* apply and decompose, doesn't work for constraints or non-uniform scale well */
        {
          float rmat3[3][3], qrmat[3][3], imat3[3][3], smat[3][3];

          copy_m3_m4(rmat3, mat);
          /* Make sure that our rotation matrix only contains rotation and not scale. */
          normalize_m3(rmat3);

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
          /* copy_v3_v3(parchan->loc, mat[3]); */
        }
      }

      apply = 1;
      data->flag &= ~CONSTRAINT_IK_AUTO;
    }
  }

  return apply;
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
      bone->flag &= ~BONE_TRANSFORM;
    }

    bone_children_clear_transflag(mode, around, &bone->childbase);
  }
}

/* Sets transform flags in the bones.
 * Returns total number of bones with `BONE_TRANSFORM`. */
int transform_convert_pose_transflags_update(Object *ob,
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
        bone->flag &= ~BONE_TRANSFORM;
      }

      bone->flag &= ~BONE_HINGE_CHILD_TRANSFORM;
      bone->flag &= ~BONE_TRANSFORM_CHILD;
    }
    else {
      bone->flag &= ~BONE_TRANSFORM;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Mode (Auto-IK)
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Surface
 * \{ */

void calc_distanceCurveVerts(TransData *head, TransData *tail)
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
TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, struct BezTriple *bezt)
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Coordinates
 * \{ */

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
      if ((td->flag & TD_SKIP) || (!td->loc)) {
        continue;
      }

      td->loc[0] = min_ff(max_ff(0.0f, td->loc[0]), t->aspect[0]);
      td->loc[1] = min_ff(max_ff(0.0f, td->loc[1]), t->aspect[1]);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Editors (General)
 * \{ */

/**
 * Used for `TFM_TIME_EXTEND`.
 */
char transform_convert_frame_side_dir_get(TransInfo *t, float cframe)
{
  char r_dir;
  float center[2];
  if (t->flag & T_MODAL) {
    UI_view2d_region_to_view(
        (View2D *)t->view, t->mouse.imval[0], t->mouse.imval[1], &center[0], &center[1]);
    r_dir = (center[0] > cframe) ? 'R' : 'L';
    {
      /* XXX: This saves the direction in the "mirror" property to be used for redo! */
      if (r_dir == 'R') {
        t->flag |= T_NO_MIRROR;
      }
    }
  }
  else {
    r_dir = (t->flag & T_NO_MIRROR) ? 'R' : 'L';
  }

  return r_dir;
}

/* This function tests if a point is on the "mouse" side of the cursor/frame-marking */
bool FrameOnMouseSide(char side, float frame, float cframe)
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Editor
 * \{ */

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

    BKE_gpencil_layer_frames_sort(gpl, &is_double);

    if (is_double) {
      for (gpf = gpl->frames.first; gpf; gpf = gpfn) {
        gpfn = gpf->next;
        if (gpfn && gpf->framenum == gpfn->framenum) {
          BKE_gpencil_layer_frame_delete(gpl, gpf);
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

  WM_main_add_notifier(NC_GPENCIL | NA_EDITED, gpd);
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

  WM_main_add_notifier(NC_MASK | NA_EDITED, mask);
}

/* Time + Average value */
typedef struct tRetainedKeyframe {
  struct tRetainedKeyframe *next, *prev;
  float frame; /* frame to cluster around */
  float val;   /* average value */

  size_t tot_count; /* number of keyframes that have been averaged */
  size_t del_count; /* number of keyframes of this sort that have been deleted so far */
} tRetainedKeyframe;

/**
 * Called during special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 *
 * \param sel_flag: The flag (bezt.f1/2/3) value to use to determine selection. Usually `SELECT`,
 *                  but may want to use a different one at times (if caller does not operate on
 *                  selection).
 */
static void posttrans_fcurve_clean(FCurve *fcu,
                                   const eBezTriple_Flag sel_flag,
                                   const bool use_handle)
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
    LISTBASE_FOREACH (tRetainedKeyframe *, rk, &retained_keys) {
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
  testhandles_fcurve(fcu, sel_flag, use_handle);

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
      posttrans_fcurve_clean(ale->key_data, SELECT, false); /* only use handles in graph editor */
      ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0);
    }
    else {
      posttrans_fcurve_clean(ale->key_data, SELECT, false); /* only use handles in graph editor */
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor
 * \{ */

/* struct for use in re-sorting BezTriples during Graph Editor transform */
typedef struct BeztMap {
  BezTriple *bezt;
  uint oldIndex;   /* index of bezt in fcu->bezt array before sorting */
  uint newIndex;   /* index of bezt in fcu->bezt array after sorting */
  short swapHs;    /* swap order of handles (-1=clear; 0=not checked, 1=swap) */
  char pipo, cipo; /* interpolation of current and next segments */
} BeztMap;

/* This function converts an FCurve's BezTriple array to a BeztMap array
 * NOTE: this allocates memory that will need to get freed later
 */
static BeztMap *bezt_to_beztmaps(BezTriple *bezts, int totvert)
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
static void sort_time_beztmaps(BeztMap *bezms, int totvert)
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
static void beztmap_to_data(TransInfo *t, FCurve *fcu, BeztMap *bezms, int totvert)
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Utilities
 * \{ */

/* This function is called by recalcData during the Transform loop to recalculate
 * the handles of curves and sort the keyframes so that the curves draw correctly.
 * It is only called if some keyframes have moved out of order.
 *
 * anim_data is the list of channels (F-Curves) retrieved already containing the
 * channels to work on. It should not be freed here as it may still need to be used.
 */
void remake_graph_transdata(TransInfo *t, ListBase *anim_data)
{
  SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
  bAnimListElem *ale;
  const bool use_handle = (sipo->flag & SIPO_NOHANDLES) == 0;

  /* sort and reassign verts */
  for (ale = anim_data->first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;

    if (fcu->bezt) {
      BeztMap *bezm;

      /* adjust transform-data pointers */
      /* note, none of these functions use 'use_handle', it could be removed */
      bezm = bezt_to_beztmaps(fcu->bezt, fcu->totvert);
      sort_time_beztmaps(bezm, fcu->totvert);
      beztmap_to_data(t, fcu, bezm, fcu->totvert);

      /* free mapping stuff */
      MEM_freeN(bezm);

      /* re-sort actual beztriples (perhaps this could be done using the beztmaps to save time?) */
      sort_time_fcurve(fcu);

      /* make sure handles are all set correctly */
      testhandles_fcurve(fcu, BEZT_FLAG_TEMP_TAG, use_handle);
    }
  }
}

/* Little helper function for ObjectToTransData used to give certain
 * constraints (ChildOf, FollowPath, and others that may be added)
 * inverse corrections for transform, so that they aren't in CrazySpace.
 * These particular constraints benefit from this, but others don't, hence
 * this semi-hack ;-)    - Aligorith
 */
bool constraints_list_needinv(TransInfo *t, ListBase *list)
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

          if (ELEM(data->mix_mode, ROTLIKE_MIX_OFFSET, ROTLIKE_MIX_BEFORE) &&
              ELEM(t->mode, TFM_ROTATION)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_TRANSLIKE) {
          /* Copy Transforms constraint only does this in the Before mode. */
          bTransLikeConstraint *data = (bTransLikeConstraint *)con->data;

          if (ELEM(data->mix_mode, TRANSLIKE_MIX_BEFORE) &&
              ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_ACTION) {
          /* The Action constraint only does this in the Before mode. */
          bActionConstraint *data = (bActionConstraint *)con->data;

          if (ELEM(data->mix_mode, ACTCON_MIX_BEFORE) &&
              ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
          /* Transform constraint needs it for rotation at least (r.57309),
           * but doing so when translating may also mess things up [#36203]
           */
          bTransformConstraint *data = (bTransformConstraint *)con->data;

          if (data->to == TRANS_ROTATION) {
            if (t->mode == TFM_ROTATION && data->mix_mode_rot == TRANS_MIXROT_BEFORE) {
              return true;
            }
          }
        }
      }
    }
  }

  /* no appropriate candidates found */
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Auto-Keyframing)
 * \{ */

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
    eInsertKeyFlags flag = 0;

    /* Get flags used for inserting keyframes. */
    flag = ANIM_get_keyframing_flags(scene, true);

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
  if (!autokeyframe_cfra_can_key(scene, id)) {
    /* tag channels that should have unkeyed data */
    for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
      if (pchan->bone->flag & BONE_TRANSFORM) {
        /* tag this channel */
        pchan->bone->flag |= BONE_UNKEYED;
      }
    }
    return;
  }

  ReportList *reports = CTX_wm_reports(C);
  ToolSettings *ts = scene->toolsettings;
  KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
  ListBase nla_cache = {NULL, NULL};
  float cfra = (float)CFRA;
  eInsertKeyFlags flag = 0;

  /* flag is initialized from UserPref keyframing settings
   * - special exception for targetless IK - INSERTKEY_MATRIX keyframes should get
   *   visual keyframes even if flag not set, as it's not that useful otherwise
   *   (for quick animation recording)
   */
  flag = ANIM_get_keyframing_flags(scene, true);

  if (targetless_ik) {
    flag |= INSERTKEY_MATRIX;
  }

  for (pchan = pose->chanbase.first; pchan; pchan = pchan->next) {
    if ((pchan->bone->flag & BONE_TRANSFORM) == 0 &&
        !((pose->flag & POSE_MIRROR_EDIT) && (pchan->bone->flag & BONE_TRANSFORM_MIRROR))) {
      continue;
    }

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
          if (strstr(fcu->rna_path, "bones") == NULL) {
            continue;
          }
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

  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (After-Transform Update)
 * \{ */

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
  SpaceClip *sc = t->area->spacedata.first;
  MovieClip *clip = ED_space_clip_get_clip(sc);
  ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  /* Update coordinates of modified plane tracks. */
  LISTBASE_FOREACH (MovieTrackingPlaneTrack *, plane_track, plane_tracks_base) {
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
    SpaceClip *sc = t->area->spacedata.first;
    mask = ED_space_clip_get_mask(sc);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = t->area->spacedata.first;
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

    if (ED_mask_layer_shape_auto_key_select(mask, CFRA)) {
      WM_event_add_notifier(C, NC_MASK | ND_DATA, &mask->id);
      DEG_id_tag_update(&mask->id, 0);
    }
  }
}

static void special_aftertrans_update__node(bContext *C, TransInfo *t)
{
  Main *bmain = CTX_data_main(C);
  const bool canceled = (t->state == TRANS_CANCEL);

  if (canceled && t->remove_on_cancel) {
    /* remove selected nodes on cancel */
    SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;
    bNodeTree *ntree = snode->edittree;
    if (ntree) {
      bNode *node, *node_next;
      for (node = ntree->nodes.first; node; node = node_next) {
        node_next = node->next;
        if (node->flag & NODE_SELECT) {
          nodeRemoveNode(bmain, ntree, node, true);
        }
      }
      ntreeUpdateTree(bmain, ntree);
    }
  }
}

static void special_aftertrans_update__mesh(bContext *UNUSED(C), TransInfo *t)
{
  bool use_automerge = (t->flag & (T_AUTOMERGE | T_AUTOSPLIT)) != 0;
  if (use_automerge && ((t->flag & T_EDIT) && t->obedit_type == OB_MESH)) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {

      BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
      BMesh *bm = em->bm;
      char hflag;
      bool has_face_sel = (bm->totfacesel != 0);

      if (tc->mirror.use_mirror_any) {
        TransDataMirror *tdm;
        int i;

        /* Rather then adjusting the selection (which the user would notice)
         * tag all mirrored verts, then auto-merge those. */
        BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

        for (i = tc->mirror.data_len, tdm = tc->mirror.data; i--; tdm++) {
          BM_elem_flag_enable((BMVert *)tdm->extra, BM_ELEM_TAG);
        }

        hflag = BM_ELEM_SELECT | BM_ELEM_TAG;
      }
      else {
        hflag = BM_ELEM_SELECT;
      }

      if (t->flag & T_AUTOSPLIT) {
        EDBM_automerge_and_split(
            tc->obedit, true, true, true, hflag, t->scene->toolsettings->doublimit);
      }
      else {
        EDBM_automerge(tc->obedit, true, hflag, t->scene->toolsettings->doublimit);
      }

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
        }
        else if (t->mode == TFM_VERT_SLIDE) {
          /* as above */
          projectVertSlideData(t, true);
        }

        if (t->obedit_type == OB_MESH) {
          special_aftertrans_update__mesh(C, t);
        }
      }
      else {
        if (t->mode == TFM_EDGE_SLIDE) {
          projectEdgeSlideData(t, false);
        }
        else if (t->mode == TFM_VERT_SLIDE) {
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

    SpaceSeq *sseq = (SpaceSeq *)t->area->spacedata.first;

    /* Marker transform, not especially nice but we may want to move markers
     * at the same time as strips in the Video Sequencer. */
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
    SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;
    special_aftertrans_update__node(C, t);
    if (canceled == 0) {
      ED_node_post_apply_transform(C, snode->edittree);

      ED_node_link_insert(bmain, t->area);
    }

    /* clear link line */
    ED_node_link_intersect_test(t->area, 0);
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
    SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;
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
            posttrans_fcurve_clean(fcu, SELECT, false); /* only use handles in graph editor */
            ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
          }
          else {
            posttrans_fcurve_clean(fcu, SELECT, false); /* only use handles in graph editor */
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
        ListBase anim_data = {NULL, NULL};
        const int filter = ANIMFILTER_DATA_VISIBLE;
        ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

        LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
          if (ale->datatype == ALE_GPFRAME) {
            ale->id->tag |= LIB_TAG_DOIT;
          }
        }
        LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
          if (ale->datatype == ALE_GPFRAME) {
            if (ale->id->tag & LIB_TAG_DOIT) {
              ale->id->tag &= ~LIB_TAG_DOIT;
              posttrans_gpd_clean((bGPdata *)ale->id);
            }
          }
        }
        ANIM_animdata_freelist(&anim_data);
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
        ListBase anim_data = {NULL, NULL};
        const int filter = ANIMFILTER_DATA_VISIBLE;
        ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

        LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
          if (ale->datatype == ALE_MASKLAY) {
            ale->id->tag |= LIB_TAG_DOIT;
          }
        }
        LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
          if (ale->datatype == ALE_MASKLAY) {
            if (ale->id->tag & LIB_TAG_DOIT) {
              ale->id->tag &= ~LIB_TAG_DOIT;
              posttrans_mask_clean((Mask *)ale->id);
            }
          }
        }
        ANIM_animdata_freelist(&anim_data);
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
    if (!ELEM(ac.datatype, ANIMCONT_GPENCIL)) {
      ANIM_editkeyframes_refresh(&ac);
    }

    /* clear flag that was set for time-slide drawing */
    saction->flag &= ~SACTION_MOVING;
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
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
            posttrans_fcurve_clean(fcu, BEZT_FLAG_TEMP_TAG, use_handle);
            ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 0);
          }
          else {
            posttrans_fcurve_clean(fcu, BEZT_FLAG_TEMP_TAG, use_handle);
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
        /* table needs to be created for each edit command, since vertices can move etc */
        ED_mesh_mirror_spatial_table_end(tc->obedit);
        /* TODO(campbell): xform: We need support for many mirror objects at once! */
        break;
      }
    }
  }
  else if (t->flag & T_POSE && (t->mode == TFM_BONESIZE)) {
    /* Handle the exception where for TFM_BONESIZE in edit mode we pretend to be
     * in pose mode (to use bone orientation matrix),
     * in that case we don't do operations like auto-keyframing. */
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
        transform_convert_pose_transflags_update(ob, t->mode, t->around, NULL);
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
      const ePosePathCalcRange range = canceled ? POSE_PATH_CALC_RANGE_CURRENT_FRAME :
                                                  POSE_PATH_CALC_RANGE_CHANGED;
      ob = BLI_gsetIterator_getKey(&gs_iter);
      ED_pose_recalculate_paths(C, t->scene, ob, range);
    }
    BLI_gset_free(motionpath_updates, NULL);
  }
  else if (t->options & CTX_PAINT_CURVE) {
    /* pass */
  }
  else if (t->options & CTX_SCULPT) {
    /* pass */
  }
  else if ((t->view_layer->basact) && (ob = t->view_layer->basact->object) &&
           (ob->mode & OB_MODE_PARTICLE_EDIT) && PE_get_current(t->depsgraph, t->scene, ob)) {
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
      const eObjectPathCalcRange range = canceled ? OBJECT_PATH_CALC_RANGE_CURRENT_FRAME :
                                                    OBJECT_PATH_CALC_RANGE_CHANGED;
      ED_objects_recalculate_paths(C, t->scene, range);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Data Create
 * \{ */

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

    if (t->spacetype == SPACE_IMAGE) {
      createTransCursor_image(t);
    }
    else {
      createTransCursor_view3d(t);
    }
    countAndCleanTransDataContainer(t);
  }
  else if ((t->options & CTX_SCULPT) && !(t->options & CTX_PAINT_CURVE)) {
    createTransSculpt(t);
    countAndCleanTransDataContainer(t);
  }
  else if (t->options & CTX_TEXTURE) {
    t->flag |= T_TEXTURE;

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
    createTransSeqData(t);
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
    Object *ob_armature = BKE_modifiers_is_deformed_by_armature(ob);
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
  else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT) &&
           PE_start_edit(PE_get_current(t->depsgraph, scene, ob))) {
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
  else if ((ob) && (ELEM(ob->mode,
                         OB_MODE_PAINT_GPENCIL,
                         OB_MODE_SCULPT_GPENCIL,
                         OB_MODE_WEIGHT_GPENCIL,
                         OB_MODE_VERTEX_GPENCIL))) {
    /* In grease pencil all transformations must be canceled if not Object or Edit. */
    has_transform_context = false;
  }
  else {
    /* Needed for correct Object.obmat after duplication, see: T62135. */
    BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

    if ((scene->toolsettings->transform_flag & SCE_XFORM_DATA_ORIGIN) != 0) {
      t->options |= CTX_OBMODE_XFORM_OBDATA;
    }
    if ((scene->toolsettings->transform_flag & SCE_XFORM_SKIP_CHILDREN) != 0) {
      t->options |= CTX_OBMODE_XFORM_SKIP_CHILDREN;
    }

    createTransObject(C, t);
    countAndCleanTransDataContainer(t);
    t->flag |= T_OBJECT;

    if (t->data_len_all && t->flag & T_PROP_EDIT) {
      // selected objects are already first, no need to presort
      set_prop_dist(t, 1);
      sort_trans_data_dist(t);
    }

    /* Check if we're transforming the camera from the camera */
    if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
      View3D *v3d = t->view;
      RegionView3D *rv3d = t->region->regiondata;
      if ((rv3d->persp == RV3D_CAMOB) && v3d->camera) {
        /* we could have a flag to easily check an object is being transformed */
        if (v3d->camera->id.tag & LIB_TAG_DOIT) {
          t->flag |= T_CAMERA;
        }
      }
      else if (v3d->ob_center && v3d->ob_center->id.tag & LIB_TAG_DOIT) {
        t->flag |= T_CAMERA;
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

/** \} */
