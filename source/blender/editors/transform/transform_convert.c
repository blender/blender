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
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_kdtree.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_nla.h"
#include "BKE_scene.h"

#include "ED_keyframes_edit.h"
#include "ED_keyframing.h"
#include "ED_particle.h"
#include "ED_screen.h"
#include "ED_screen_types.h"

#include "UI_view2d.h"

#include "WM_types.h"

#include "DEG_depsgraph_build.h"

#include "transform.h"
#include "transform_snap.h"

/* Own include. */
#include "transform_convert.h"

bool transform_mode_use_local_origins(const TransInfo *t)
{
  return ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL);
}

/**
 * Transforming around ourselves is no use, fallback to individual origins,
 * useful for curve/armatures.
 */
void transform_around_single_fallback_ex(TransInfo *t, int data_len_all)
{
  if ((ELEM(t->around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN, V3D_AROUND_ACTIVE)) &&
      transform_mode_use_local_origins(t)) {
    if (data_len_all == 1) {
      t->around = V3D_AROUND_LOCAL_ORIGINS;
    }
  }
}

void transform_around_single_fallback(TransInfo *t)
{
  transform_around_single_fallback_ex(t, t->data_len_all);
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
  if (td_a->dist > td_b->dist) {
    return 1;
  }
  return 0;
}

static int trans_data_compare_rdist(const void *a, const void *b)
{
  const TransData *td_a = (const TransData *)a;
  const TransData *td_b = (const TransData *)b;

  if (td_a->rdist < td_b->rdist) {
    return -1;
  }
  if (td_a->rdist > td_b->rdist) {
    return 1;
  }
  return 0;
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
      qsort(start, (size_t)tc->data_len - i, sizeof(TransData), trans_data_compare_dist);
    }
    else {
      qsort(start, (size_t)tc->data_len - i, sizeof(TransData), trans_data_compare_rdist);
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
  sel = &tc->data[tc->data_len - 1];
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Surface
 * \{ */

void calc_distanceCurveVerts(TransData *head, TransData *tail, bool cyclic)
{
  TransData *td;
  BLI_LINKSTACK_DECLARE(queue, TransData *);
  BLI_LINKSTACK_INIT(queue);
  for (td = head; td <= tail; td++) {
    if (td->flag & TD_SELECTED) {
      td->dist = 0.0f;
      BLI_LINKSTACK_PUSH(queue, td);
    }
    else {
      td->dist = FLT_MAX;
    }
  }

  while ((td = BLI_LINKSTACK_POP(queue))) {
    float dist;
    float vec[3];

    TransData *next_td = NULL;

    if (td + 1 <= tail) {
      next_td = td + 1;
    }
    else if (cyclic) {
      next_td = head;
    }

    if (next_td != NULL && !(next_td->flag & TD_NOTCONNECTED)) {
      sub_v3_v3v3(vec, next_td->center, td->center);
      mul_m3_v3(head->mtx, vec);
      dist = len_v3(vec) + td->dist;

      if (dist < next_td->dist) {
        next_td->dist = dist;
        BLI_LINKSTACK_PUSH(queue, next_td);
      }
    }

    next_td = NULL;

    if (td - 1 >= head) {
      next_td = td - 1;
    }
    else if (cyclic) {
      next_td = tail;
    }

    if (next_td != NULL && !(next_td->flag & TD_NOTCONNECTED)) {
      sub_v3_v3v3(vec, next_td->center, td->center);
      mul_m3_v3(head->mtx, vec);
      dist = len_v3(vec) + td->dist;

      if (dist < next_td->dist) {
        next_td->dist = dist;
        BLI_LINKSTACK_PUSH(queue, next_td);
      }
    }
  }
  BLI_LINKSTACK_FREE(queue);
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
  return (frame <= cframe);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation Editor
 * \{ */

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
void posttrans_fcurve_clean(FCurve *fcu, const int sel_flag, const bool use_handle)
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
        if (rk->frame < bezt->vec[1][0]) {
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

  /* Compute the average values for each retained keyframe */
  LISTBASE_FOREACH (tRetainedKeyframe *, rk, &retained_keys) {
    rk->val = rk->val / (float)rk->tot_count;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Utilities
 * \{ */

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
   * constraints needing special crazy-space corrections
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
           * but doing so when translating may also mess things up, see: T36203. */
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
/** \name Transform (After-Transform Update)
 * \{ */

/* inserting keys, pointcache, redraw events... */
/**
 * \note Sequencer freeing has its own function now because of a conflict
 * with transform's order of freeing (campbell).
 * Order changed, the sequencer stuff should go back in here
 */
void special_aftertrans_update(bContext *C, TransInfo *t)
{
  /* early out when nothing happened */
  if (t->data_len_all == 0 || t->mode == TFM_DUMMY) {
    return;
  }

  BLI_assert(CTX_data_main(t->context) == CTX_data_main(C));
  switch (t->data_type) {
    case TC_ACTION_DATA:
      special_aftertrans_update__actedit(C, t);
      break;
    case TC_POSE:
      special_aftertrans_update__pose(C, t);
      break;
    case TC_GRAPH_EDIT_DATA:
      special_aftertrans_update__graph(C, t);
      break;
    case TC_MASKING_DATA:
      special_aftertrans_update__mask(C, t);
      break;
    case TC_MESH_VERTS:
    case TC_MESH_EDGES:
      special_aftertrans_update__mesh(C, t);
      break;
    case TC_NLA_DATA:
      special_aftertrans_update__nla(C, t);
      break;
    case TC_NODE_DATA:
      special_aftertrans_update__node(C, t);
      break;
    case TC_OBJECT:
      special_aftertrans_update__object(C, t);
      break;
    case TC_SCULPT:
      special_aftertrans_update__sculpt(C, t);
      break;
    case TC_SEQ_DATA:
      special_aftertrans_update__sequencer(C, t);
      break;
    case TC_TRACKING_DATA:
      special_aftertrans_update__movieclip(C, t);
      break;
    case TC_ARMATURE_VERTS:
    case TC_CURSOR_IMAGE:
    case TC_CURSOR_VIEW3D:
    case TC_CURVE_VERTS:
    case TC_GPENCIL:
    case TC_LATTICE_VERTS:
    case TC_MBALL_VERTS:
    case TC_MESH_UV:
    case TC_MESH_SKIN:
    case TC_OBJECT_TEXSPACE:
    case TC_PAINT_CURVE_VERTS:
    case TC_PARTICLE_VERTS:
    case TC_NONE:
    default:
      break;
  }
}

int special_transform_moving(TransInfo *t)
{
  if (t->spacetype == SPACE_SEQ) {
    return G_TRANSFORM_SEQ;
  }
  if (t->spacetype == SPACE_GRAPH) {
    return G_TRANSFORM_FCURVES;
  }
  if ((t->flag & T_EDIT) || (t->options & CTX_POSE_BONE)) {
    return G_TRANSFORM_EDIT;
  }
  if (t->options & (CTX_OBJECT | CTX_TEXTURE_SPACE)) {
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
  int data_container_len_orig = t->data_container_len;
  for (TransDataContainer *th_end = t->data_container - 1,
                          *tc = &t->data_container[t->data_container_len - 1];
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

static void init_proportional_edit(TransInfo *t)
{
  eTConvertType convert_type = t->data_type;
  switch (convert_type) {
    case TC_ACTION_DATA:
    case TC_CURVE_VERTS:
    case TC_GRAPH_EDIT_DATA:
    case TC_GPENCIL:
    case TC_LATTICE_VERTS:
    case TC_MASKING_DATA:
    case TC_MBALL_VERTS:
    case TC_MESH_VERTS:
    case TC_MESH_EDGES:
    case TC_MESH_SKIN:
    case TC_MESH_UV:
    case TC_NODE_DATA:
    case TC_OBJECT:
    case TC_PARTICLE_VERTS:
      break;
    case TC_POSE: /* Disable PET, its not usable in pose mode yet T32444. */
    case TC_ARMATURE_VERTS:
    case TC_CURSOR_IMAGE:
    case TC_CURSOR_VIEW3D:
    case TC_NLA_DATA:
    case TC_OBJECT_TEXSPACE:
    case TC_PAINT_CURVE_VERTS:
    case TC_SCULPT:
    case TC_SEQ_DATA:
    case TC_TRACKING_DATA:
    case TC_NONE:
    default:
      t->options |= CTX_NO_PET;
      t->flag &= ~T_PROP_EDIT_ALL;
      return;
  }

  if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
    if (convert_type == TC_OBJECT) {
      /* Selected objects are already first, no need to presort. */
    }
    else {
      sort_trans_data_selected_first(t);
    }

    if (ELEM(convert_type, TC_ACTION_DATA, TC_GRAPH_EDIT_DATA)) {
      /* Distance has already been set. */
    }
    else if (ELEM(convert_type, TC_MESH_VERTS, TC_MESH_SKIN)) {
      if (t->flag & T_PROP_CONNECTED) {
        /* Already calculated by transform_convert_mesh_connectivity_distance. */
      }
      else {
        set_prop_dist(t, false);
      }
    }
    else if (convert_type == TC_MESH_UV && t->flag & T_PROP_CONNECTED) {
      /* Already calculated by uv_set_connectivity_distance. */
    }
    else if (convert_type == TC_CURVE_VERTS && t->obedit_type == OB_CURVE) {
      set_prop_dist(t, false);
    }
    else {
      set_prop_dist(t, true);
    }

    sort_trans_data_dist(t);
  }
  else if (ELEM(t->obedit_type, OB_CURVE)) {
    /* Needed because bezier handles can be partially selected
     * and are still added into transform data. */
    sort_trans_data_selected_first(t);
  }
}

/* For multi object editing. */
static void init_TransDataContainers(TransInfo *t,
                                     Object *obact,
                                     Object **objects,
                                     uint objects_len)
{
  switch (t->data_type) {
    case TC_POSE:
    case TC_ARMATURE_VERTS:
    case TC_CURVE_VERTS:
    case TC_GPENCIL:
    case TC_LATTICE_VERTS:
    case TC_MBALL_VERTS:
    case TC_MESH_VERTS:
    case TC_MESH_EDGES:
    case TC_MESH_SKIN:
    case TC_MESH_UV:
      break;
    case TC_ACTION_DATA:
    case TC_GRAPH_EDIT_DATA:
    case TC_CURSOR_IMAGE:
    case TC_CURSOR_VIEW3D:
    case TC_MASKING_DATA:
    case TC_NLA_DATA:
    case TC_NODE_DATA:
    case TC_OBJECT:
    case TC_OBJECT_TEXSPACE:
    case TC_PAINT_CURVE_VERTS:
    case TC_PARTICLE_VERTS:
    case TC_SCULPT:
    case TC_SEQ_DATA:
    case TC_TRACKING_DATA:
    case TC_NONE:
    default:
      /* Does not support Multi object editing. */
      return;
  }

  const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;
  const short object_type = obact ? obact->type : -1;

  if ((object_mode & OB_MODE_EDIT) || (t->data_type == TC_GPENCIL) ||
      ((object_mode & OB_MODE_POSE) && (object_type == OB_ARMATURE))) {
    if (t->data_container) {
      MEM_freeN(t->data_container);
    }

    bool free_objects = false;
    if (objects == NULL) {
      objects = BKE_view_layer_array_from_objects_in_mode(
          t->view_layer,
          (t->spacetype == SPACE_VIEW3D) ? t->view : NULL,
          &objects_len,
          {
              .object_mode = object_mode,
              .no_dup_data = true,
          });
      free_objects = true;
    }

    t->data_container = MEM_callocN(sizeof(*t->data_container) * objects_len, __func__);
    t->data_container_len = objects_len;

    for (int i = 0; i < objects_len; i++) {
      TransDataContainer *tc = &t->data_container[i];
      if (((t->flag & T_NO_MIRROR) == 0) && ((t->options & CTX_NO_MIRROR) == 0) &&
          (objects[i]->type == OB_MESH)) {
        tc->use_mirror_axis_x = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_X) != 0;
        tc->use_mirror_axis_y = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_Y) != 0;
        tc->use_mirror_axis_z = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_Z) != 0;
      }

      if (object_mode & OB_MODE_EDIT) {
        tc->obedit = objects[i];
        /* Check needed for UV's */
        if ((t->flag & T_2D_EDIT) == 0) {
          tc->use_local_mat = true;
        }
      }
      else if (object_mode & OB_MODE_POSE) {
        tc->poseobj = objects[i];
        tc->use_local_mat = true;
      }
      else if (t->data_type == TC_GPENCIL) {
        tc->use_local_mat = true;
      }

      if (tc->use_local_mat) {
        BLI_assert((t->flag & T_2D_EDIT) == 0);
        copy_m4_m4(tc->mat, objects[i]->obmat);
        copy_m3_m4(tc->mat3, tc->mat);
        /* for non-invertible scale matrices, invert_m4_m4_fallback()
         * can still provide a valid pivot */
        invert_m4_m4_fallback(tc->imat, tc->mat);
        invert_m3_m3(tc->imat3, tc->mat3);
        normalize_m3_m3(tc->mat3_unit, tc->mat3);
      }
      /* Otherwise leave as zero. */
    }

    if (free_objects) {
      MEM_freeN(objects);
    }
  }
}

static eTFlag flags_from_data_type(eTConvertType data_type)
{
  switch (data_type) {
    case TC_ACTION_DATA:
    case TC_GRAPH_EDIT_DATA:
    case TC_MASKING_DATA:
    case TC_NLA_DATA:
    case TC_NODE_DATA:
    case TC_PAINT_CURVE_VERTS:
    case TC_SEQ_DATA:
    case TC_TRACKING_DATA:
      return T_POINTS | T_2D_EDIT;
    case TC_ARMATURE_VERTS:
    case TC_CURVE_VERTS:
    case TC_GPENCIL:
    case TC_LATTICE_VERTS:
    case TC_MBALL_VERTS:
    case TC_MESH_VERTS:
    case TC_MESH_SKIN:
      return T_EDIT | T_POINTS;
    case TC_MESH_EDGES:
      return T_EDIT;
    case TC_MESH_UV:
      return T_EDIT | T_POINTS | T_2D_EDIT;
    case TC_PARTICLE_VERTS:
      return T_POINTS;
    case TC_POSE:
    case TC_CURSOR_IMAGE:
    case TC_CURSOR_VIEW3D:
    case TC_OBJECT:
    case TC_OBJECT_TEXSPACE:
    case TC_SCULPT:
    case TC_NONE:
    default:
      break;
  }
  return 0;
}

static eTConvertType convert_type_get(const TransInfo *t, Object **r_obj_armature)
{
  ViewLayer *view_layer = t->view_layer;
  Object *ob = OBACT(view_layer);
  eTConvertType convert_type = TC_NONE;

  /* if tests must match recalcData for correct updates */
  if (t->options & CTX_CURSOR) {
    if (t->spacetype == SPACE_IMAGE) {
      convert_type = TC_CURSOR_IMAGE;
    }
    else {
      convert_type = TC_CURSOR_VIEW3D;
    }
  }
  else if (!(t->options & CTX_PAINT_CURVE) && (t->spacetype == SPACE_VIEW3D) && ob &&
           (ob->mode == OB_MODE_SCULPT) && ob->sculpt) {
    convert_type = TC_SCULPT;
  }
  else if (t->options & CTX_TEXTURE_SPACE) {
    convert_type = TC_OBJECT_TEXSPACE;
  }
  else if (t->options & CTX_EDGE_DATA) {
    convert_type = TC_MESH_EDGES;
  }
  else if (t->options & CTX_GPENCIL_STROKES) {
    convert_type = TC_GPENCIL;
  }
  else if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      convert_type = TC_MASKING_DATA;
    }
    else if (t->options & CTX_PAINT_CURVE) {
      if (!ELEM(t->mode, TFM_SHEAR, TFM_SHRINKFATTEN)) {
        convert_type = TC_PAINT_CURVE_VERTS;
      }
    }
    else if (t->obedit_type == OB_MESH) {
      convert_type = TC_MESH_UV;
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    convert_type = TC_ACTION_DATA;
  }
  else if (t->spacetype == SPACE_NLA) {
    convert_type = TC_NLA_DATA;
  }
  else if (t->spacetype == SPACE_SEQ) {
    convert_type = TC_SEQ_DATA;
  }
  else if (t->spacetype == SPACE_GRAPH) {
    convert_type = TC_GRAPH_EDIT_DATA;
  }
  else if (t->spacetype == SPACE_NODE) {
    convert_type = TC_NODE_DATA;
  }
  else if (t->spacetype == SPACE_CLIP) {
    if (t->options & CTX_MOVIECLIP) {
      convert_type = TC_TRACKING_DATA;
    }
    else if (t->options & CTX_MASK) {
      convert_type = TC_MASKING_DATA;
    }
  }
  else if (t->obedit_type != -1) {
    if (t->obedit_type == OB_MESH) {
      if (t->mode == TFM_SKIN_RESIZE) {
        convert_type = TC_MESH_SKIN;
      }
      else {
        convert_type = TC_MESH_VERTS;
      }
    }
    else if (ELEM(t->obedit_type, OB_CURVE, OB_SURF)) {
      convert_type = TC_CURVE_VERTS;
    }
    else if (t->obedit_type == OB_LATTICE) {
      convert_type = TC_LATTICE_VERTS;
    }
    else if (t->obedit_type == OB_MBALL) {
      convert_type = TC_MBALL_VERTS;
    }
    else if (t->obedit_type == OB_ARMATURE) {
      convert_type = TC_ARMATURE_VERTS;
    }
  }
  else if (ob && (ob->mode & OB_MODE_POSE)) {
    convert_type = TC_POSE;
  }
  else if (ob && (ob->mode & OB_MODE_ALL_WEIGHT_PAINT) && !(t->options & CTX_PAINT_CURVE)) {
    Object *ob_armature = transform_object_deform_pose_armature_get(t, ob);
    if (ob_armature) {
      *r_obj_armature = ob_armature;
      convert_type = TC_POSE;
    }
  }
  else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT) &&
           PE_start_edit(PE_get_current(t->depsgraph, t->scene, ob))) {
    convert_type = TC_PARTICLE_VERTS;
  }
  else if (ob && (ob->mode & OB_MODE_ALL_PAINT)) {
    if ((t->options & CTX_PAINT_CURVE) && !ELEM(t->mode, TFM_SHEAR, TFM_SHRINKFATTEN)) {
      convert_type = TC_PAINT_CURVE_VERTS;
    }
  }
  else if ((ob) && (ELEM(ob->mode,
                         OB_MODE_PAINT_GPENCIL,
                         OB_MODE_SCULPT_GPENCIL,
                         OB_MODE_WEIGHT_GPENCIL,
                         OB_MODE_VERTEX_GPENCIL))) {
    /* In grease pencil all transformations must be canceled if not Object or Edit. */
  }
  else {
    convert_type = TC_OBJECT;
  }

  return convert_type;
}

void createTransData(bContext *C, TransInfo *t)
{
  t->data_len_all = -1;

  Object *ob_armature = NULL;
  t->data_type = convert_type_get(t, &ob_armature);
  t->flag |= flags_from_data_type(t->data_type);

  if (ob_armature) {
    init_TransDataContainers(t, ob_armature, &ob_armature, 1);
  }
  else {
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);
    init_TransDataContainers(t, ob, NULL, 0);
  }

  switch (t->data_type) {
    case TC_ACTION_DATA:
      t->obedit_type = -1;
      createTransActionData(C, t);
      break;
    case TC_POSE:
      t->options |= CTX_POSE_BONE;

      /* XXX active-layer checking isn't done
       * as that should probably be checked through context instead. */
      createTransPose(t);
      break;
    case TC_ARMATURE_VERTS:
      createTransArmatureVerts(t);
      break;
    case TC_CURSOR_IMAGE:
      createTransCursor_image(t);
      break;
    case TC_CURSOR_VIEW3D:
      createTransCursor_view3d(t);
      break;
    case TC_CURVE_VERTS:
      createTransCurveVerts(t);
      break;
    case TC_GRAPH_EDIT_DATA:
      t->obedit_type = -1;
      createTransGraphEditData(C, t);
      break;
    case TC_GPENCIL:
      createTransGPencil(C, t);
      break;
    case TC_LATTICE_VERTS:
      createTransLatticeVerts(t);
      break;
    case TC_MASKING_DATA:
      if (t->spacetype == SPACE_CLIP) {
        t->obedit_type = -1;
      }
      createTransMaskingData(C, t);
      break;
    case TC_MBALL_VERTS:
      createTransMBallVerts(t);
      break;
    case TC_MESH_VERTS:
      createTransEditVerts(t);
      break;
    case TC_MESH_EDGES:
      createTransEdge(t);
      break;
    case TC_MESH_SKIN:
      createTransMeshSkin(t);
      break;
    case TC_MESH_UV:
      createTransUVs(C, t);
      break;
    case TC_NLA_DATA:
      t->obedit_type = -1;
      createTransNlaData(C, t);
      break;
    case TC_NODE_DATA:
      t->obedit_type = -1;
      createTransNodeData(t);
      break;
    case TC_OBJECT:
      t->options |= CTX_OBJECT;

      /* Needed for correct Object.obmat after duplication, see: T62135. */
      BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

      if ((t->settings->transform_flag & SCE_XFORM_DATA_ORIGIN) != 0) {
        t->options |= CTX_OBMODE_XFORM_OBDATA;
      }
      if ((t->settings->transform_flag & SCE_XFORM_SKIP_CHILDREN) != 0) {
        t->options |= CTX_OBMODE_XFORM_SKIP_CHILDREN;
      }
      createTransObject(C, t);
      /* Check if we're transforming the camera from the camera */
      if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
        View3D *v3d = t->view;
        RegionView3D *rv3d = t->region->regiondata;
        if ((rv3d->persp == RV3D_CAMOB) && v3d->camera) {
          /* we could have a flag to easily check an object is being transformed */
          if (v3d->camera->id.tag & LIB_TAG_DOIT) {
            t->options |= CTX_CAMERA;
          }
        }
        else if (v3d->ob_center && v3d->ob_center->id.tag & LIB_TAG_DOIT) {
          t->options |= CTX_CAMERA;
        }
      }
      break;
    case TC_OBJECT_TEXSPACE:
      createTransTexspace(t);
      break;
    case TC_PAINT_CURVE_VERTS:
      createTransPaintCurveVerts(C, t);
      break;
    case TC_PARTICLE_VERTS:
      createTransParticleVerts(t);
      break;
    case TC_SCULPT:
      createTransSculpt(C, t);
      break;
    case TC_SEQ_DATA:
      t->obedit_type = -1;
      t->num.flag |= NUM_NO_FRACTION; /* sequencer has no use for floating point transform. */
      createTransSeqData(t);
      break;
    case TC_TRACKING_DATA:
      t->obedit_type = -1;
      createTransTrackingData(C, t);
      break;
    case TC_NONE:
    default:
      printf("edit type not implemented!\n");
      BLI_assert(t->data_len_all == -1);
      t->data_len_all = 0;
      return;
  }

  countAndCleanTransDataContainer(t);

  init_proportional_edit(t);

  BLI_assert((!(t->flag & T_EDIT)) == (!(t->obedit_type != -1)));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Data Recalc/Flush
 * \{ */

void clipMirrorModifier(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    Object *ob = tc->obedit;
    ModifierData *md = ob->modifiers.first;
    float tolerance[3] = {0.0f, 0.0f, 0.0f};
    int axis = 0;

    for (; md; md = md->next) {
      if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
        MirrorModifierData *mmd = (MirrorModifierData *)md;

        if (mmd->flag & MOD_MIR_CLIPPING) {
          axis = 0;
          if (mmd->flag & MOD_MIR_AXIS_X) {
            axis |= 1;
            tolerance[0] = mmd->tolerance;
          }
          if (mmd->flag & MOD_MIR_AXIS_Y) {
            axis |= 2;
            tolerance[1] = mmd->tolerance;
          }
          if (mmd->flag & MOD_MIR_AXIS_Z) {
            axis |= 4;
            tolerance[2] = mmd->tolerance;
          }
          if (axis) {
            float mtx[4][4], imtx[4][4];
            int i;

            if (mmd->mirror_ob) {
              float obinv[4][4];

              invert_m4_m4(obinv, mmd->mirror_ob->obmat);
              mul_m4_m4m4(mtx, obinv, ob->obmat);
              invert_m4_m4(imtx, mtx);
            }

            TransData *td = tc->data;
            for (i = 0; i < tc->data_len; i++, td++) {
              int clip;
              float loc[3], iloc[3];

              if (td->loc == NULL) {
                break;
              }

              if (td->flag & TD_SKIP) {
                continue;
              }

              copy_v3_v3(loc, td->loc);
              copy_v3_v3(iloc, td->iloc);

              if (mmd->mirror_ob) {
                mul_m4_v3(mtx, loc);
                mul_m4_v3(mtx, iloc);
              }

              clip = 0;
              if (axis & 1) {
                if (fabsf(iloc[0]) <= tolerance[0] || loc[0] * iloc[0] < 0.0f) {
                  loc[0] = 0.0f;
                  clip = 1;
                }
              }

              if (axis & 2) {
                if (fabsf(iloc[1]) <= tolerance[1] || loc[1] * iloc[1] < 0.0f) {
                  loc[1] = 0.0f;
                  clip = 1;
                }
              }
              if (axis & 4) {
                if (fabsf(iloc[2]) <= tolerance[2] || loc[2] * iloc[2] < 0.0f) {
                  loc[2] = 0.0f;
                  clip = 1;
                }
              }
              if (clip) {
                if (mmd->mirror_ob) {
                  mul_m4_v3(imtx, loc);
                }
                copy_v3_v3(td->loc, loc);
              }
            }
          }
        }
      }
    }
  }
}

/* for the realtime animation recording feature, handle overlapping data */
void animrecord_check_state(TransInfo *t, struct Object *ob)
{
  Scene *scene = t->scene;
  ID *id = &ob->id;
  wmTimer *animtimer = t->animtimer;
  ScreenAnimData *sad = (animtimer) ? animtimer->customdata : NULL;

  /* sanity checks */
  if (ELEM(NULL, scene, id, sad)) {
    return;
  }

  /* check if we need a new strip if:
   * - if animtimer is running
   * - we're not only keying for available channels
   * - the option to add new actions for each round is not enabled
   */
  if (IS_AUTOKEY_FLAG(scene, INSERTAVAIL) == 0 &&
      (scene->toolsettings->autokey_flag & ANIMRECORD_FLAG_WITHNLA)) {
    /* if playback has just looped around,
     * we need to add a new NLA track+strip to allow a clean pass to occur */
    if ((sad) && (sad->flag & ANIMPLAY_FLAG_JUMPED)) {
      AnimData *adt = BKE_animdata_from_id(id);
      const bool is_first = (adt) && (adt->nla_tracks.first == NULL);

      /* perform push-down manually with some differences
       * NOTE: BKE_nla_action_pushdown() sync warning...
       */
      if ((adt->action) && !(adt->flag & ADT_NLA_EDIT_ON)) {
        float astart, aend;

        /* only push down if action is more than 1-2 frames long */
        calc_action_range(adt->action, &astart, &aend, 1);
        if (aend > astart + 2.0f) {
          NlaStrip *strip = BKE_nlastack_add_strip(adt, adt->action, ID_IS_OVERRIDE_LIBRARY(id));

          /* clear reference to action now that we've pushed it onto the stack */
          id_us_min(&adt->action->id);
          adt->action = NULL;

          /* adjust blending + extend so that they will behave correctly */
          strip->extendmode = NLASTRIP_EXTEND_NOTHING;
          strip->flag &= ~(NLASTRIP_FLAG_AUTO_BLENDS | NLASTRIP_FLAG_SELECT |
                           NLASTRIP_FLAG_ACTIVE);

          /* copy current "action blending" settings from adt to the strip,
           * as it was keyframed with these settings, so omitting them will
           * change the effect  [T54766]
           */
          if (is_first == false) {
            strip->blendmode = adt->act_blendmode;
            strip->influence = adt->act_influence;

            if (adt->act_influence < 1.0f) {
              /* enable "user-controlled" influence (which will insert a default keyframe)
               * so that the influence doesn't get lost on the new update
               *
               * NOTE: An alternative way would have been to instead hack the influence
               * to not get always get reset to full strength if NLASTRIP_FLAG_USR_INFLUENCE
               * is disabled but auto-blending isn't being used. However, that approach
               * is a bit hacky/hard to discover, and may cause backwards compatibility issues,
               * so it's better to just do it this way.
               */
              strip->flag |= NLASTRIP_FLAG_USR_INFLUENCE;
              BKE_nlastrip_validate_fcurves(strip);
            }
          }

          /* also, adjust the AnimData's action extend mode to be on
           * 'nothing' so that previous result still play
           */
          adt->act_extendmode = NLASTRIP_EXTEND_NOTHING;
        }
      }
    }
  }
}

static void recalcData_cursor(TransInfo *t)
{
  DEG_id_tag_update(&t->scene->id, ID_RECALC_COPY_ON_WRITE);
}

static void recalcData_obedit(TransInfo *t)
{
  if (t->state != TRANS_CANCEL) {
    applyProject(t);
  }
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len) {
      DEG_id_tag_update(tc->obedit->data, 0); /* sets recalc flags */
    }
  }
}

/* called for updating while transform acts, once per redraw */
void recalcData(TransInfo *t)
{
  switch (t->data_type) {
    case TC_ACTION_DATA:
      recalcData_actedit(t);
      break;
    case TC_POSE:
      recalcData_pose(t);
      break;
    case TC_ARMATURE_VERTS:
      recalcData_edit_armature(t);
      break;
    case TC_CURVE_VERTS:
      recalcData_curve(t);
      break;
    case TC_CURSOR_IMAGE:
    case TC_CURSOR_VIEW3D:
      recalcData_cursor(t);
      break;
    case TC_GRAPH_EDIT_DATA:
      recalcData_graphedit(t);
      break;
    case TC_GPENCIL:
      recalcData_gpencil_strokes(t);
      break;
    case TC_MASKING_DATA:
      recalcData_mask_common(t);
      break;
    case TC_MESH_VERTS:
    case TC_MESH_EDGES:
      recalcData_mesh(t);
      break;
    case TC_MESH_SKIN:
      recalcData_mesh_skin(t);
      break;
    case TC_MESH_UV:
      recalcData_uv(t);
      break;
    case TC_NLA_DATA:
      recalcData_nla(t);
      break;
    case TC_NODE_DATA:
      flushTransNodes(t);
      break;
    case TC_OBJECT:
      recalcData_objects(t);
      break;
    case TC_OBJECT_TEXSPACE:
      recalcData_texspace(t);
      break;
    case TC_PAINT_CURVE_VERTS:
      flushTransPaintCurve(t);
      break;
    case TC_SCULPT:
      recalcData_sculpt(t);
      break;
    case TC_SEQ_DATA:
      recalcData_sequencer(t);
      break;
    case TC_TRACKING_DATA:
      recalcData_tracking(t);
      break;
    case TC_MBALL_VERTS:
      recalcData_obedit(t);
      break;
    case TC_LATTICE_VERTS:
      recalcData_lattice(t);
      break;
    case TC_PARTICLE_VERTS:
      recalcData_particles(t);
      break;
    case TC_NONE:
    default:
      break;
  }
}

/** \} */
