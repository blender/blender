/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_function_ref.hh"
#include "BLI_kdtree.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_modifier.hh"
#include "BKE_nla.hh"
#include "BKE_scene.hh"

#include "ED_particle.hh"
#include "ED_screen.hh"
#include "ED_screen_types.hh"
#include "ED_sequencer.hh"

#include "ANIM_keyframing.hh"
#include "ANIM_nla.hh"

#include "UI_view2d.hh"

#include "WM_types.hh"

#include "DEG_depsgraph_build.hh"

#include "transform.hh"

/* Own include. */
#include "transform_convert.hh"

namespace blender::ed::transform {

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

/**
 * Construct `tc->sorted_index_map` so that its indices visit `tc->{data,data_ext,data_2d}` in
 * sorted order, given the compare function.
 */
static void make_sorted_index_map(TransDataContainer *tc, FunctionRef<bool(int, int)> compare)
{
  BLI_assert(tc->sorted_index_map == nullptr);
  tc->sorted_index_map = MEM_malloc_arrayN<int>(tc->data_len, __func__);

  const MutableSpan sorted_index_span(tc->sorted_index_map, tc->data_len);
  array_utils::fill_index_range(sorted_index_span);
  std::sort(sorted_index_span.begin(), sorted_index_span.end(), compare);
}

/**
 * Construct an index map to visit `tc->data`, `tc->data_ext`, and `tc->data_2d` in order of
 * selection state (selected first). Unselected items are visited by either their `dist` or `rdist`
 * property, depending on a flag in `t`.
 */
static void sort_trans_data_dist_container(const TransInfo *t, TransDataContainer *tc)
{
  const bool use_dist = (t->flag & T_PROP_CONNECTED);
  const auto compare = [&](const int a, const int b) {
    /* If both selected, then they are equivalent. To keep memory access sequential (and thus more
     * predictable for pre-caching) when iterating the arrays, keep them sorted by array index. */
    const bool is_selected_a = tc->data[a].flag & TD_SELECTED;
    const bool is_selected_b = tc->data[b].flag & TD_SELECTED;
    if (is_selected_a && is_selected_b) {
      return a < b;
    }

    /* Selected comes before unselected. */
    if (is_selected_a) {
      return true;
    }
    if (is_selected_b) {
      return false;
    }

    /* If both are unselected, only then the distance matters. */
    if (use_dist) {
      return tc->data[a].dist < tc->data[b].dist;
    }
    return tc->data[a].rdist < tc->data[b].rdist;
  };

  /* The "sort by distance" is often preceded by "calculate distance", which is
   * often preceded by "sort selected first". */
  MEM_SAFE_FREE(tc->sorted_index_map);

  make_sorted_index_map(tc, compare);
}
void sort_trans_data_dist(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sort_trans_data_dist_container(t, tc);
  }
}

/**
 * Construct an index map to visit `tc->data`, `tc->data_ext`, and `tc->data_2d` in order of
 * selection state (selected first).
 */
static void sort_trans_data_selected_first_container(TransDataContainer *tc)
{
  BLI_assert_msg(tc->sorted_index_map == nullptr,
                 "Expected sorting by selection state to only happen once");

  const auto compare = [&](const int a, const int b) {
    /* If the selection state is the same, they are equivalent. To keep memory
     * access sequential (and thus more predictable for pre-caching) when
     * iterating the arrays, keep them sorted by array index. */
    const bool is_selected_a = tc->data[a].flag & TD_SELECTED;
    const bool is_selected_b = tc->data[b].flag & TD_SELECTED;
    if (is_selected_a == is_selected_b) {
      return a < b;
    }

    /* If A is selected, a comes before b, so return true.
     * If B is selected, a comes after b, so return false. */
    return is_selected_a;
  };
  make_sorted_index_map(tc, compare);
}
static void sort_trans_data_selected_first(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sort_trans_data_selected_first_container(tc);
  }
}

static float3 prop_dist_loc_get(const TransDataContainer *tc,
                                const TransData *td,
                                const bool use_island,
                                const float proj_vec[3])
{
  float3 vec;

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

  return vec;
}

/**
 * Distance calculated from not-selected vertex to nearest selected vertex.
 * If the #transdata_check_local_islands() check succeeds, this will also change
 * the TransData center and axismtx of unselected points to the center and axismtx of the closest
 * point found (for proportional editing around individual origins).
 */
static void set_prop_dist(TransInfo *t, const bool with_dist)
{
  float _proj_vec[3];
  const float *proj_vec = nullptr;

  /* Support for face-islands. */
  const bool use_island = transdata_check_local_islands(t, t->around);

  if (t->flag & T_PROP_PROJECTED) {
    if (t->spacetype == SPACE_VIEW3D && t->region && t->region->regiontype == RGN_TYPE_WINDOW) {
      RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
      normalize_v3_v3(_proj_vec, rv3d->viewinv[2]);
      proj_vec = _proj_vec;
    }
  }

  /* Count number of selected. */
  int td_table_len = 0;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    tc->foreach_index_selected([&](const int /*i*/) { td_table_len++; });
  }

  /* Pointers to selected's #TransData.
   * Used to find #TransData from the index returned by #BLI_kdtree_find_nearest. */
  TransData **td_table = static_cast<TransData **>(
      MEM_mallocN(sizeof(*td_table) * td_table_len, __func__));

  /* Create and fill KD-tree of selected's positions - in global or proj_vec space. */
  KDTree_3d *td_tree = BLI_kdtree_3d_new(td_table_len);

  int td_table_index = 0;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    tc->foreach_index_selected([&](const int i) {
      TransData *td = &tc->data[i];
      /* Initialize, it was malloced. */
      td->rdist = 0.0f;

      const float3 vec = prop_dist_loc_get(tc, td, use_island, proj_vec);

      BLI_kdtree_3d_insert(td_tree, td_table_index, vec);
      td_table[td_table_index++] = td;
    });
  }
  BLI_assert(td_table_index == td_table_len);

  BLI_kdtree_3d_balance(td_tree);

  /* For each non-selected vertex, find distance to the nearest selected vertex. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    tc->foreach_index([&](const int i) {
      TransData *td = &tc->data[i];
      if (td->flag & TD_SELECTED) {
        return true;
      }

      const float3 vec = prop_dist_loc_get(tc, td, use_island, proj_vec);

      KDTreeNearest_3d nearest;
      const int td_index = BLI_kdtree_3d_find_nearest(td_tree, vec, &nearest);

      td->rdist = -1.0f;
      if (td_index != -1) {
        td->rdist = nearest.dist;
        if (use_island) {
          /* Use center and axismtx of closest point found. */
          copy_v3_v3(td->center, td_table[td_index]->center);
          copy_m3_m3(td->axismtx, td_table[td_index]->axismtx);
        }
      }

      if (with_dist) {
        td->dist = td->rdist;
      }
      return true;
    });
  }

  BLI_kdtree_3d_free(td_tree);
  MEM_freeN(td_table);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pose Mode (Auto-IK)
 * \{ */

/** Adjust pose-channel's auto-ik chainlen. */
static bool pchan_autoik_adjust(bPoseChannel *pchan, short chainlen)
{
  bool changed = false;

  /* Don't bother to search if no valid constraints. */
  if ((pchan->constflag & (PCHAN_HAS_IK | PCHAN_HAS_NO_TARGET)) == 0) {
    return changed;
  }

  /* Check if pchan has ik-constraint. */
  LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
    if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
      continue;
    }
    if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->enforce != 0.0f)) {
      bKinematicConstraint *data = static_cast<bKinematicConstraint *>(con->data);

      /* Only accept if a temporary one (for auto-IK). */
      if (data->flag & CONSTRAINT_IK_TEMP) {
        /* `chainlen` is new `chainlen`, but is limited by maximum `chainlen`. */
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

  /* `mode` determines what change to apply to `chainlen`. */
  if (mode == 1) {
    /* `mode==1` is from WHEELMOUSEDOWN: increases len. */
    (*chainlen)++;
  }
  else if (mode == -1) {
    /* `mode==-1` is from WHEELMOUSEUP: decreases len. */
    if (*chainlen > 0) {
      (*chainlen)--;
    }
    else {
      /* IK length did not change, skip updates. */
      return;
    }
  }

  /* Apply to all pose-channels. */
  bool changed = false;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    /* Sanity checks (don't assume `t->poseobj` is set, or that it is an armature). */
    if (ELEM(nullptr, tc->poseobj, tc->poseobj->pose)) {
      continue;
    }

    LISTBASE_FOREACH (bPoseChannel *, pchan, &tc->poseobj->pose->chanbase) {
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

    TransData *next_td = nullptr;

    if (td + 1 <= tail) {
      next_td = td + 1;
    }
    else if (cyclic) {
      next_td = head;
    }

    if (next_td != nullptr) {
      sub_v3_v3v3(vec, next_td->center, td->center);
      mul_m3_v3(head->mtx, vec);
      dist = len_v3(vec) + td->dist;

      if (dist < next_td->dist) {
        next_td->dist = dist;
        BLI_LINKSTACK_PUSH(queue, next_td);
      }
    }

    next_td = nullptr;

    if (td - 1 >= head) {
      next_td = td - 1;
    }
    else if (cyclic) {
      next_td = tail;
    }

    if (next_td != nullptr) {
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

TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, BezTriple *bezt)
{
  TransDataCurveHandleFlags *hdata;
  td->flag |= TD_BEZTRIPLE;
  hdata = td->hdata = MEM_mallocN<TransDataCurveHandleFlags>("CuHandle Data");
  hdata->ih1 = bezt->h1;
  hdata->h1 = &bezt->h1;
  hdata->ih2 = bezt->h2; /* In case the second is not selected. */
  hdata->h2 = &bezt->h2;
  return hdata;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Coordinates
 * \{ */

void clipUVData(TransInfo *t)
{
  /* NOTE(@ideasman42): Often used to clip UV's after proportional editing:
   * In this case the radius of the proportional region can end outside the clipping area,
   * while not ideal an elegant solution here would likely be computationally expensive
   * as it would need to calculate the transform value that would meet the UV bounds.
   * While it would be technically correct to handle this properly,
   * there isn't a strong use case for it. */

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
  char dir;
  float center[2];
  if (t->flag & T_MODAL) {
    UI_view2d_region_to_view(
        (View2D *)t->view, t->mouse.imval[0], t->mouse.imval[1], &center[0], &center[1]);
    dir = (center[0] > cframe) ? 'R' : 'L';
    {
      /* XXX: This saves the direction in the "mirror" property to be used for redo! */
      if (dir == 'R') {
        t->flag |= T_NO_MIRROR;
      }
    }
  }
  else {
    dir = (t->flag & T_NO_MIRROR) ? 'R' : 'L';
  }

  return dir;
}

bool FrameOnMouseSide(char side, float frame, float cframe)
{
  /* Both sides, so it doesn't matter. */
  if (side == 'B') {
    return true;
  }

  /* Only on the named side. */
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
  /* Loop through constraints, checking if there's one of the mentioned
   * constraints needing special crazy-space corrections. */
  if (list) {
    LISTBASE_FOREACH (bConstraint *, con, list) {
      /* Only consider constraint if it is enabled, and has influence on result. */
      if ((con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) == 0 && (con->enforce != 0.0f)) {
        /* Affirmative: returns for specific constraints here. */
        /* Constraints that require this regardless. */
        if (ELEM(con->type,
                 CONSTRAINT_TYPE_FOLLOWPATH,
                 CONSTRAINT_TYPE_CLAMPTO,
                 CONSTRAINT_TYPE_ARMATURE,
                 CONSTRAINT_TYPE_OBJECTSOLVER,
                 CONSTRAINT_TYPE_FOLLOWTRACK))
        {
          return true;
        }

        /* Constraints that require this only under special conditions. */
        if (con->type == CONSTRAINT_TYPE_CHILDOF) {
          /* ChildOf constraint only works when using all location components, see #42256. */
          bChildOfConstraint *data = (bChildOfConstraint *)con->data;

          if ((data->flag & CHILDOF_LOCX) && (data->flag & CHILDOF_LOCY) &&
              (data->flag & CHILDOF_LOCZ))
          {
            return true;
          }
        }
        else if (con->type == CONSTRAINT_TYPE_ROTLIKE) {
          /* CopyRot constraint only does this when rotating, and offset is on. */
          bRotateLikeConstraint *data = (bRotateLikeConstraint *)con->data;

          if (ELEM(data->mix_mode, ROTLIKE_MIX_OFFSET, ROTLIKE_MIX_BEFORE) &&
              ELEM(t->mode, TFM_ROTATION))
          {
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

  /* No appropriate candidates found. */
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

  /* Early out when nothing happened. */
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
        std::swap(t->data_container[index], t->data_container[t->data_container_len - 1]);
      }
      t->data_container_len -= 1;
    }
    else {
      t->data_len_all += tc->data_len;
    }
  }
  if (data_container_len_orig != t->data_container_len) {
    t->data_container = static_cast<TransDataContainer *>(
        MEM_reallocN(t->data_container, sizeof(*t->data_container) * t->data_container_len));
  }
  return t->data_len_all;
}

static void init_proportional_edit(TransInfo *t)
{
  /* NOTE: Proportional editing is not usable in pose mode yet #32444. */
  /* NOTE: This `ELEM` uses more than 16 elements and so has been split. */
  if (!(ELEM(t->data_type,
             &TransConvertType_Action,
             &TransConvertType_Curve,
             &curves::TransConvertType_Curves,
             &TransConvertType_Graph,
             &greasepencil::TransConvertType_GreasePencil,
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
             &pointcloud::TransConvertType_PointCloud) ||
        ELEM(t->data_type, &TransConvertType_Particle)))
  {
    /* Disable proportional editing. */
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
        /* Already calculated by #transform_convert_mesh_connectivity_distance. */
      }
      else {
        set_prop_dist(t, false);
      }
    }
    else if (t->data_type == &TransConvertType_MeshUV && t->flag & T_PROP_CONNECTED) {
      /* Already calculated by #uv_set_connectivity_distance. */
    }
    else if (t->data_type == &TransConvertType_Curve) {
      BLI_assert(t->obedit_type == OB_CURVES_LEGACY);
      if (t->flag & T_PROP_CONNECTED) {
        /* Already calculated by #calc_distanceCurveVerts. */
      }
      else {
        set_prop_dist(t, false);
      }
    }
    else if (ELEM(t->data_type,
                  &curves::TransConvertType_Curves,
                  &greasepencil::TransConvertType_GreasePencil))
    {
      BLI_assert(t->obedit_type == OB_CURVES || t->obedit_type == OB_GREASE_PENCIL);
      if (t->flag & T_PROP_CONNECTED) {
        /* Already calculated by #calculate_curve_point_distances_for_proportional_editing. */
      }
      else {
        set_prop_dist(t, false);
      }
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
static void init_TransDataContainers(TransInfo *t, Object *obact, Span<Object *> objects)
{
  if (!ELEM(t->data_type,
            &TransConvertType_Pose,
            &TransConvertType_EditArmature,
            &TransConvertType_Curve,
            &curves::TransConvertType_Curves,
            &greasepencil::TransConvertType_GreasePencil,
            &pointcloud::TransConvertType_PointCloud,
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

  const eObjectMode object_mode = eObjectMode(obact ? obact->mode : OB_MODE_OBJECT);
  const short object_type = obact ? obact->type : -1;

  if ((object_mode & OB_MODE_EDIT) ||
      (t->data_type == &greasepencil::TransConvertType_GreasePencil) ||
      ((object_mode & OB_MODE_POSE) && (object_type == OB_ARMATURE)))
  {
    if (t->data_container) {
      MEM_freeN(t->data_container);
    }

    Vector<Object *> local_objects;
    if (objects.is_empty()) {
      ObjectsInModeParams params = {0};
      params.object_mode = object_mode;
      /* Pose transform operates on `ob->pose` so don't skip duplicate object-data. */
      params.no_dup_data = (object_mode & OB_MODE_POSE) == 0;
      local_objects = BKE_view_layer_array_from_objects_in_mode_params(
          t->scene,
          t->view_layer,
          static_cast<const View3D *>((t->spacetype == SPACE_VIEW3D) ? t->view : nullptr),
          &params);
      objects = local_objects;
    }

    t->data_container = MEM_calloc_arrayN<TransDataContainer>(objects.size(), __func__);
    t->data_container_len = objects.size();

    for (int i = 0; i < objects.size(); i++) {
      TransDataContainer *tc = &t->data_container[i];
      if (!(t->flag & T_NO_MIRROR) && (objects[i]->type == OB_MESH)) {
        tc->use_mirror_axis_x = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_X) != 0;
        tc->use_mirror_axis_y = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_Y) != 0;
        tc->use_mirror_axis_z = (((Mesh *)objects[i]->data)->symmetry & ME_SYMMETRY_Z) != 0;
      }

      if (object_mode & OB_MODE_EDIT) {
        tc->obedit = objects[i];
        /* Check needed for UVs. */
        if ((t->flag & T_2D_EDIT) == 0) {
          tc->use_local_mat = true;
        }
      }
      else if (object_mode & OB_MODE_POSE) {
        tc->poseobj = objects[i];
        tc->use_local_mat = true;
      }
      else if (t->data_type == &greasepencil::TransConvertType_GreasePencil) {
        tc->use_local_mat = true;
      }

      if (tc->use_local_mat) {
        BLI_assert((t->flag & T_2D_EDIT) == 0);
        copy_m4_m4(tc->mat, objects[i]->object_to_world().ptr());
        copy_m3_m4(tc->mat3, tc->mat);
        /* For non-invertible scale matrices, #invert_m4_m4_fallback()
         * can still provide a valid pivot. */
        invert_m4_m4_fallback(tc->imat, tc->mat);
        invert_m3_m3(tc->imat3, tc->mat3);
        normalize_m3_m3(tc->mat3_unit, tc->mat3);
      }
      /* Otherwise leave as zero. */
    }
  }
}

static TransConvertTypeInfo *convert_type_get(const TransInfo *t, Object **r_obj_armature)
{
  ViewLayer *view_layer = t->view_layer;
  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);

  /* If tests must match recalc_data for correct updates. */
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
    if (t->obedit_type == OB_GREASE_PENCIL) {
      return &greasepencil::TransConvertType_GreasePencil;
    }
    return nullptr;
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
    return nullptr;
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
    if (vse::sequencer_retiming_mode_is_active(t->context)) {
      return &TransConvertType_SequencerRetiming;
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
    return nullptr;
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
      return &curves::TransConvertType_Curves;
    }
    if (t->obedit_type == OB_POINTCLOUD) {
      return &pointcloud::TransConvertType_PointCloud;
    }
    return nullptr;
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
    return nullptr;
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
    return nullptr;
  }
  if (ob && (ob->mode & OB_MODE_ALL_PAINT_GPENCIL)) {
    /* In grease pencil all transformations must be canceled if not Object or Edit. */
    return nullptr;
  }
  return &TransConvertType_Object;
}

void create_trans_data(bContext *C, TransInfo *t)
{
  t->data_len_all = -1;

  Object *ob_armature = nullptr;
  t->data_type = convert_type_get(t, &ob_armature);
  if (t->data_type == nullptr) {
    printf("edit type not implemented!\n");
    BLI_assert(t->data_len_all == -1);
    t->data_len_all = 0;
    return;
  }

  t->flag |= eTFlag(t->data_type->flags);

  if (ob_armature) {
    init_TransDataContainers(t, ob_armature, {ob_armature});
  }
  else {
    BKE_view_layer_synced_ensure(t->scene, t->view_layer);
    Object *ob = BKE_view_layer_active_object_get(t->view_layer);
    init_TransDataContainers(t, ob, {});
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
    TransConvertType_Object.create_trans_data(C, t);
    /* Check if we're transforming the camera from the camera. */
    if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
      View3D *v3d = static_cast<View3D *>(t->view);
      RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);
      if ((rv3d->persp == RV3D_CAMOB) && v3d->camera) {
        /* We could have a flag to easily check an object is being transformed. */
        if (v3d->camera->id.tag & ID_TAG_DOIT) {
          t->options |= CTX_CAMERA;
        }
      }
      else if (v3d->ob_center && v3d->ob_center->id.tag & ID_TAG_DOIT) {
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
    t->data_type->create_trans_data(C, t);
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
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);

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

        invert_m4_m4(obinv, mmd->mirror_ob->object_to_world().ptr());
        mul_m4_m4m4(mtx, obinv, ob->object_to_world().ptr());
        invert_m4_m4(imtx, mtx);
      }

      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
        float loc[3], iloc[3];

        if (td->loc == nullptr) {
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

void animrecord_check_state(TransInfo *t, ID *id)
{
  Scene *scene = t->scene;
  wmTimer *animtimer = t->animtimer;
  ScreenAnimData *sad = static_cast<ScreenAnimData *>((animtimer) ? animtimer->customdata :
                                                                    nullptr);

  /* Sanity checks. */
  if (ELEM(nullptr, scene, id, sad)) {
    return;
  }

  /* Check if we need a new strip if:
   * - If `animtimer` is running.
   * - We're not only keying for available channels.
   * - The option to add new actions for each round is not enabled.
   */
  if (animrig::is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE) == 0 &&
      (scene->toolsettings->keying_flag & AUTOKEY_FLAG_LAYERED_RECORD))
  {
    /* If playback has just looped around,
     * we need to add a new NLA track+strip to allow a clean pass to occur. */
    if ((sad) && (sad->flag & ANIMPLAY_FLAG_JUMPED)) {
      AnimData *adt = BKE_animdata_from_id(id);
      const bool is_first = (adt) && (adt->nla_tracks.first == nullptr);

      /* Perform push-down manually with some differences
       * NOTE: #BKE_nla_action_pushdown() sync warning. */
      if ((adt->action) && !(adt->flag & ADT_NLA_EDIT_ON)) {
        /* Only push down if action is more than 1-2 frames long. */
        const float2 frame_range = adt->action->wrap().get_frame_range_of_keys(true);
        if (frame_range[1] > frame_range[0] + 2.0f) {
          /* TODO: call #BKE_nla_action_pushdown() instead? */

          /* Add a new NLA strip to the track, which references the active action + slot. */
          NlaStrip *strip = BKE_nlastack_add_strip({*id, *adt}, ID_IS_OVERRIDE_LIBRARY(id));
          BLI_assert(strip);
          animrig::nla::assign_action_slot_handle(*strip, adt->slot_handle, *id);

          /* Clear reference to action now that we've pushed it onto the stack. */
          const bool unassign_ok = animrig::unassign_action(*id);
          BLI_assert_msg(
              unassign_ok,
              "Expecting un-assigning an action to always work when pushing down an NLA strip");
          UNUSED_VARS_NDEBUG(unassign_ok);

          /* Adjust blending + extend so that they will behave correctly. */
          strip->extendmode = NLASTRIP_EXTEND_NOTHING;
          strip->flag &= ~(NLASTRIP_FLAG_AUTO_BLENDS | NLASTRIP_FLAG_SELECT |
                           NLASTRIP_FLAG_ACTIVE);

          /* Copy current "action blending" settings from adt to the strip,
           * as it was keyframed with these settings, so omitting them will
           * change the effect, see: #54766. */
          if (is_first == false) {
            strip->blendmode = adt->act_blendmode;
            strip->influence = adt->act_influence;

            if (adt->act_influence < 1.0f) {
              /* Enable "user-controlled" influence (which will insert a default keyframe)
               * so that the influence doesn't get lost on the new update.
               *
               * NOTE: An alternative way would have been to instead hack the influence
               * to not get always get reset to full strength if NLASTRIP_FLAG_USR_INFLUENCE
               * is disabled but auto-blending isn't being used. However, that approach
               * is a bit hacky/hard to discover, and may cause backwards compatibility issues,
               * so it's better to just do it this way. */
              strip->flag |= NLASTRIP_FLAG_USR_INFLUENCE;
              BKE_nlastrip_validate_fcurves(strip);
            }
          }

          /* Also, adjust the AnimData's action extend mode to be on
           * 'nothing' so that previous result still play. */
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

void recalc_data(TransInfo *t)
{
  if (!t->data_type || !t->data_type->recalc_data) {
    return;
  }
  t->data_type->recalc_data(t);
}

/** \} */

}  // namespace blender::ed::transform
