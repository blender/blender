/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
#include "BKE_image.h"
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

void transform_around_single_fallback_ex(TransInfo *t, int data_len_all)
{
  if (data_len_all != 1) {
    return;
  }
  if (!ELEM(t->around, V3D_AROUND_CENTER_BOUNDS, V3D_AROUND_CENTER_MEDIAN, V3D_AROUND_ACTIVE)) {
    return;
  }
  if (!transform_mode_use_local_origins(t)) {
    return;
  }
  if (t->flag & T_OVERRIDE_CENTER) {
    return;
  }

  t->around = V3D_AROUND_LOCAL_ORIGINS;
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

  /* Create and fill KD-tree of selected's positions - in global or proj_vec space. */
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
    if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
      continue;
    }
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
/** \name Transform Utilities
 * \{ */

bool constraints_list_needinv(TransInfo *t, ListBase *list)
{
  bConstraint *con;

  /* loop through constraints, checking if there's one of the mentioned
   * constraints needing special crazy-space corrections
   */
  if (list) {
    for (con = list->first; con; con = con->next) {
      /* only consider constraint if it is enabled, and has influence on result */
      if ((con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) == 0 && (con->enforce != 0.0f)) {
        /* (affirmative) returns for specific constraints here... */
        /* constraints that require this regardless. */
        if (ELEM(con->type,
                 CONSTRAINT_TYPE_FOLLOWPATH,
                 CONSTRAINT_TYPE_CLAMPTO,
                 CONSTRAINT_TYPE_ARMATURE,
                 CONSTRAINT_TYPE_OBJECTSOLVER,
                 CONSTRAINT_TYPE_FOLLOWTRACK))
        {
          return true;
        }

        /* constraints that require this only under special conditions */
        if (con->type == CONSTRAINT_TYPE_CHILDOF) {
          /* ChildOf constraint only works when using all location components, see #42256. */
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

          if (ELEM(data->mix_mode, TRANSLIKE_MIX_BEFORE, TRANSLIKE_MIX_BEFORE_FULL) &&
              ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION))
          {
            return true;
          }
          if (ELEM(data->mix_mode, TRANSLIKE_MIX_BEFORE_SPLIT) && ELEM(t->mode, TFM_ROTATION)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_ACTION) {
          /* The Action constraint only does this in the Before mode. */
          bActionConstraint *data = (bActionConstraint *)con->data;

          if (ELEM(data->mix_mode, ACTCON_MIX_BEFORE, ACTCON_MIX_BEFORE_FULL) &&
              ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION))
          {
            return true;
          }
          if (ELEM(data->mix_mode, ACTCON_MIX_BEFORE_SPLIT) && ELEM(t->mode, TFM_ROTATION)) {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_TRANSFORM) {
          /* Transform constraint needs it for rotation at least (r.57309),
           * but doing so when translating may also mess things up, see: #36203. */
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

void special_aftertrans_update(bContext *C, TransInfo *t)
{
  /* NOTE: Sequencer freeing has its own function now because of a conflict
   * with transform's order of freeing (campbell).
   * Order changed, the sequencer stuff should go back in here. */

  /* early out when nothing happened */
  if (t->data_len_all == 0 || t->mode == TFM_DUMMY) {
    return;
  }

  if (!t->data_type || !t->data_type->special_aftertrans_update) {
    return;
  }

  BLI_assert(CTX_data_main(t->context) == CTX_data_main(C));
  t->data_type->special_aftertrans_update(C, t);
}

int special_transform_moving(TransInfo *t)
{
  if (t->options & CTX_CURSOR) {
    return G_TRANSFORM_CURSOR;
  }
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
       tc--)
  {
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
  /* NOTE: Proportional editing is not usable in pose mode yet #32444. */
  if (!ELEM(t->data_type,
            &TransConvertType_Action,
            &TransConvertType_Curve,
            &TransConvertType_Curves,
            &TransConvertType_Graph,
            &TransConvertType_GPencil,
            &TransConvertType_Lattice,
            &TransConvertType_Mask,
            &TransConvertType_MBall,
            &TransConvertType_Mesh,
            &TransConvertType_MeshEdge,
            &TransConvertType_MeshSkin,
            &TransConvertType_MeshUV,
            &TransConvertType_MeshVertCData,
            &TransConvertType_Node,
            &TransConvertType_Object,
            &TransConvertType_Particle))
  {
    /* Disable proportional editing */
    t->options |= CTX_NO_PET;
    t->flag &= ~T_PROP_EDIT_ALL;
    return;
  }

  if (t->data_len_all && (t->flag & T_PROP_EDIT)) {
    if (t->data_type == &TransConvertType_Object) {
      /* Selected objects are already first, no need to presort. */
    }
    else {
      sort_trans_data_selected_first(t);
    }

    if (ELEM(t->data_type, &TransConvertType_Action, &TransConvertType_Graph)) {
      /* Distance has already been set. */
    }
    else if (ELEM(t->data_type,
                  &TransConvertType_Mesh,
                  &TransConvertType_MeshSkin,
                  &TransConvertType_MeshVertCData))
    {
      if (t->flag & T_PROP_CONNECTED) {
        /* Already calculated by transform_convert_mesh_connectivity_distance. */
      }
      else {
        set_prop_dist(t, false);
      }
    }
    else if (t->data_type == &TransConvertType_MeshUV && t->flag & T_PROP_CONNECTED) {
      /* Already calculated by uv_set_connectivity_distance. */
    }
    else if (ELEM(t->data_type, &TransConvertType_Curve, &TransConvertType_Curves)) {
      BLI_assert(t->obedit_type == OB_CURVES_LEGACY || t->obedit_type == OB_CURVES);
      set_prop_dist(t, false);
    }
    else {
      set_prop_dist(t, true);
    }

    sort_trans_data_dist(t);
  }
  else if (ELEM(t->obedit_type, OB_CURVES_LEGACY)) {
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
  if (!ELEM(t->data_type,
            &TransConvertType_Pose,
            &TransConvertType_EditArmature,
            &TransConvertType_Curve,
            &TransConvertType_Curves,
            &TransConvertType_GPencil,
            &TransConvertType_Lattice,
            &TransConvertType_MBall,
            &TransConvertType_Mesh,
            &TransConvertType_MeshEdge,
            &TransConvertType_MeshSkin,
            &TransConvertType_MeshUV,
            &TransConvertType_MeshVertCData))
  {
    /* Does not support Multi object editing. */
    return;
  }

  const eObjectMode object_mode = obact ? obact->mode : OB_MODE_OBJECT;
  const short object_type = obact ? obact->type : -1;

  if ((object_mode & OB_MODE_EDIT) || (t->data_type == &TransConvertType_GPencil) ||
      ((object_mode & OB_MODE_POSE) && (object_type == OB_ARMATURE)))
  {
    if (t->data_container) {
      MEM_freeN(t->data_container);
    }

    bool free_objects = false;
    if (objects == NULL) {
      struct ObjectsInModeParams params = {0};
      params.object_mode = object_mode;
      /* Pose transform operates on `ob->pose` so don't skip duplicate object-data. */
      params.no_dup_data = (object_mode & OB_MODE_POSE) == 0;
      objects = BKE_view_layer_array_from_objects_in_mode_params(
          t->scene,
          t->view_layer,
          (t->spacetype == SPACE_VIEW3D) ? t->view : NULL,
          &objects_len,
          &params);
      free_objects = true;
    }

    t->data_container = MEM_callocN(sizeof(*t->data_container) * objects_len, __func__);
    t->data_container_len = objects_len;

    for (int i = 0; i < objects_len; i++) {
      TransDataContainer *tc = &t->data_container[i];
      if (!(t->flag & T_NO_MIRROR) && (objects[i]->type == OB_MESH)) {
        tc->use_mirror_axis_x = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_X) != 0;
        tc->use_mirror_axis_y = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_Y) != 0;
        tc->use_mirror_axis_z = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_Z) != 0;
      }

      if (object_mode & OB_MODE_EDIT) {
        tc->obedit = objects[i];
        /* Check needed for UVs */
        if ((t->flag & T_2D_EDIT) == 0) {
          tc->use_local_mat = true;
        }
      }
      else if (object_mode & OB_MODE_POSE) {
        tc->poseobj = objects[i];
        tc->use_local_mat = true;
      }
      else if (t->data_type == &TransConvertType_GPencil) {
        tc->use_local_mat = true;
      }

      if (tc->use_local_mat) {
        BLI_assert((t->flag & T_2D_EDIT) == 0);
        copy_m4_m4(tc->mat, objects[i]->object_to_world);
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

static TransConvertTypeInfo *convert_type_get(const TransInfo *t, Object **r_obj_armature)
{
  ViewLayer *view_layer = t->view_layer;
  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* if tests must match recalcData for correct updates */
  if (t->options & CTX_CURSOR) {
    if (t->spacetype == SPACE_IMAGE) {
      return &TransConvertType_CursorImage;
    }

    if (t->spacetype == SPACE_SEQ) {
      return &TransConvertType_CursorSequencer;
    }

    return &TransConvertType_Cursor3D;
  }
  if (!(t->options & CTX_PAINT_CURVE) && (t->spacetype == SPACE_VIEW3D) && ob &&
      (ob->mode == OB_MODE_SCULPT) && ob->sculpt)
  {
    return &TransConvertType_Sculpt;
  }
  if (t->options & CTX_TEXTURE_SPACE) {
    return &TransConvertType_ObjectTexSpace;
  }
  if (t->options & CTX_EDGE_DATA) {
    return &TransConvertType_MeshEdge;
  }
  if (t->options & CTX_GPENCIL_STROKES) {
    return &TransConvertType_GPencil;
  }
  if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      return &TransConvertType_Mask;
    }
    if (t->options & CTX_PAINT_CURVE) {
      if (!ELEM(t->mode, TFM_SHEAR, TFM_SHRINKFATTEN)) {
        return &TransConvertType_PaintCurve;
      }
    }
    else if (t->obedit_type == OB_MESH) {
      return &TransConvertType_MeshUV;
    }
    return NULL;
  }
  if (t->spacetype == SPACE_ACTION) {
    return &TransConvertType_Action;
  }
  if (t->spacetype == SPACE_NLA) {
    return &TransConvertType_NLA;
  }
  if (t->spacetype == SPACE_SEQ) {
    if (t->options & CTX_SEQUENCER_IMAGE) {
      return &TransConvertType_SequencerImage;
    }
    return &TransConvertType_Sequencer;
  }
  if (t->spacetype == SPACE_GRAPH) {
    return &TransConvertType_Graph;
  }
  if (t->spacetype == SPACE_NODE) {
    return &TransConvertType_Node;
  }
  if (t->spacetype == SPACE_CLIP) {
    if (t->options & CTX_MOVIECLIP) {
      if (t->region->regiontype == RGN_TYPE_PREVIEW) {
        return &TransConvertType_TrackingCurves;
      }
      return &TransConvertType_Tracking;
    }
    if (t->options & CTX_MASK) {
      return &TransConvertType_Mask;
    }
    return NULL;
  }
  if (t->obedit_type != -1) {
    if (t->obedit_type == OB_MESH) {
      if (t->mode == TFM_SKIN_RESIZE) {
        return &TransConvertType_MeshSkin;
      }
      if (ELEM(t->mode, TFM_BWEIGHT, TFM_VERT_CREASE)) {
        return &TransConvertType_MeshVertCData;
      }
      return &TransConvertType_Mesh;
    }
    if (ELEM(t->obedit_type, OB_CURVES_LEGACY, OB_SURF)) {
      return &TransConvertType_Curve;
    }
    if (t->obedit_type == OB_LATTICE) {
      return &TransConvertType_Lattice;
    }
    if (t->obedit_type == OB_MBALL) {
      return &TransConvertType_MBall;
    }
    if (t->obedit_type == OB_ARMATURE) {
      return &TransConvertType_EditArmature;
    }
    if (t->obedit_type == OB_CURVES) {
      return &TransConvertType_Curves;
    }
    return NULL;
  }
  if (ob && (ob->mode & OB_MODE_POSE)) {
    return &TransConvertType_Pose;
  }
  if (ob && (ob->mode & OB_MODE_ALL_WEIGHT_PAINT) && !(t->options & CTX_PAINT_CURVE)) {
    Object *ob_armature = transform_object_deform_pose_armature_get(t, ob);
    if (ob_armature) {
      *r_obj_armature = ob_armature;
      return &TransConvertType_Pose;
    }
    return NULL;
  }
  if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT) &&
      PE_start_edit(PE_get_current(t->depsgraph, t->scene, ob)))
  {
    return &TransConvertType_Particle;
  }
  if (ob && ((ob->mode & OB_MODE_ALL_PAINT) || (ob->mode & OB_MODE_SCULPT_CURVES))) {
    if ((t->options & CTX_PAINT_CURVE) && !ELEM(t->mode, TFM_SHEAR, TFM_SHRINKFATTEN)) {
      return &TransConvertType_PaintCurve;
    }
    return NULL;
  }
  if (ob && (ob->mode & OB_MODE_ALL_PAINT_GPENCIL)) {
    /* In grease pencil all transformations must be canceled if not Object or Edit. */
    return NULL;
  }
  return &TransConvertType_Object;
}

void createTransData(bContext *C, TransInfo *t)
{
  t->data_len_all = -1;

  Object *ob_armature = NULL;
  t->data_type = convert_type_get(t, &ob_armature);
  if (t->data_type == NULL) {
    printf("edit type not implemented!\n");
    BLI_assert(t->data_len_all == -1);
    t->data_len_all = 0;
    return;
  }

  t->flag |= t->data_type->flags;

  if (ob_armature) {
    init_TransDataContainers(t, ob_armature, &ob_armature, 1);
  }
  else {
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Object *ob = BKE_view_layer_active_object_get(t->view_layer);
    init_TransDataContainers(t, ob, NULL, 0);
  }

  if (t->data_type == &TransConvertType_Object) {
    t->options |= CTX_OBJECT;

    /* Needed for correct Object.obmat after duplication, see: #62135. */
    BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

    if ((t->settings->transform_flag & SCE_XFORM_DATA_ORIGIN) != 0) {
      t->options |= CTX_OBMODE_XFORM_OBDATA;
    }
    if ((t->settings->transform_flag & SCE_XFORM_SKIP_CHILDREN) != 0) {
      t->options |= CTX_OBMODE_XFORM_SKIP_CHILDREN;
    }
    TransConvertType_Object.createTransData(C, t);
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
  }
  else {
    if (t->data_type == &TransConvertType_Pose) {
      t->options |= CTX_POSE_BONE;
    }
    else if (t->data_type == &TransConvertType_Sequencer) {
      /* Sequencer has no use for floating point transform. */
      t->num.flag |= NUM_NO_FRACTION;
    }
    else if (t->data_type == &TransConvertType_SequencerImage) {
      t->obedit_type = -1;
    }
    t->data_type->createTransData(C, t);
  }

  countAndCleanTransDataContainer(t);

  init_proportional_edit(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Data Recalc/Flush
 * \{ */

void transform_convert_clip_mirror_modifier_apply(TransDataContainer *tc)
{
  Object *ob = tc->obedit;
  ModifierData *md = ob->modifiers.first;

  for (; md; md = md->next) {
    if ((md->type == eModifierType_Mirror) && (md->mode & eModifierMode_Realtime)) {
      MirrorModifierData *mmd = (MirrorModifierData *)md;

      if ((mmd->flag & MOD_MIR_CLIPPING) == 0) {
        continue;
      }

      if ((mmd->flag & (MOD_MIR_AXIS_X | MOD_MIR_AXIS_Y | MOD_MIR_AXIS_Z)) == 0) {
        continue;
      }

      float mtx[4][4], imtx[4][4];

      if (mmd->mirror_ob) {
        float obinv[4][4];

        invert_m4_m4(obinv, mmd->mirror_ob->object_to_world);
        mul_m4_m4m4(mtx, obinv, ob->object_to_world);
        invert_m4_m4(imtx, mtx);
      }

      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
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

        bool is_clipping = false;
        if (mmd->flag & MOD_MIR_AXIS_X) {
          if (fabsf(iloc[0]) <= mmd->tolerance || loc[0] * iloc[0] < 0.0f) {
            loc[0] = 0.0f;
            is_clipping = true;
          }
        }

        if (mmd->flag & MOD_MIR_AXIS_Y) {
          if (fabsf(iloc[1]) <= mmd->tolerance || loc[1] * iloc[1] < 0.0f) {
            loc[1] = 0.0f;
            is_clipping = true;
          }
        }
        if (mmd->flag & MOD_MIR_AXIS_Z) {
          if (fabsf(iloc[2]) <= mmd->tolerance || loc[2] * iloc[2] < 0.0f) {
            loc[2] = 0.0f;
            is_clipping = true;
          }
        }

        if (is_clipping) {
          if (mmd->mirror_ob) {
            mul_m4_v3(imtx, loc);
          }
          copy_v3_v3(td->loc, loc);
        }
      }
    }
  }
}

void animrecord_check_state(TransInfo *t, struct ID *id)
{
  Scene *scene = t->scene;
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
      (scene->toolsettings->autokey_flag & ANIMRECORD_FLAG_WITHNLA))
  {
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
           * change the effect  [#54766]
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

void transform_convert_flush_handle2D(TransData *td, TransData2D *td2d, const float y_fac)
{
  float delta_x = td->loc[0] - td->iloc[0];
  float delta_y = (td->loc[1] - td->iloc[1]) * y_fac;

  /* If the handles are to be moved too
   * (as side-effect of keyframes moving, to keep the general effect)
   * offset them by the same amount so that the general angles are maintained
   * (i.e. won't change while handles are free-to-roam and keyframes are snap-locked).
   */
  if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
    td2d->h1[0] = td2d->ih1[0] + delta_x;
    td2d->h1[1] = td2d->ih1[1] + delta_y;
  }
  if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
    td2d->h2[0] = td2d->ih2[0] + delta_x;
    td2d->h2[1] = td2d->ih2[1] + delta_y;
  }
}

void recalcData(TransInfo *t)
{
  if (!t->data_type || !t->data_type->recalcData) {
    return;
  }
  t->data_type->recalcData(t);
}

/** \} */
