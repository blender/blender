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
 */

/** \file
 * \ingroup eduv
 *
 * \note The logic in this file closely follows editmesh_path.c
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_linklist.h"
#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_uvedit.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "intern/bmesh_marking.h"
#include "uvedit_intern.h"

#include "bmesh_tools.h"

/* TODO(campbell): region filling, matching mesh selection. */
// #define USE_FILL

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

/**
 * Support edge-path using vert-path calculation code.
 *
 * Cheat! Pick 2 closest loops and do vertex path,
 * in practices only obscure/contrived cases will make give noticeably worse behavior.
 *
 * While the code below is a bit awkward, it's significantly less overhead than
 * adding full edge selection which is nearly the same as vertex path in the case of UV's.
 */
static void bm_loop_calc_vert_pair_from_edge_pair(const int cd_loop_uv_offset,
                                                  const float aspect_y,
                                                  BMElem **ele_src_p,
                                                  BMElem **ele_dst_p,
                                                  BMElem **r_ele_dst_final)
{
  BMLoop *l_src = (BMLoop *)*ele_src_p;
  BMLoop *l_dst = (BMLoop *)*ele_dst_p;

  const MLoopUV *luv_src_v1 = BM_ELEM_CD_GET_VOID_P(l_src, cd_loop_uv_offset);
  const MLoopUV *luv_src_v2 = BM_ELEM_CD_GET_VOID_P(l_src->next, cd_loop_uv_offset);
  const MLoopUV *luv_dst_v1 = BM_ELEM_CD_GET_VOID_P(l_dst, cd_loop_uv_offset);
  const MLoopUV *luv_dst_v2 = BM_ELEM_CD_GET_VOID_P(l_dst->next, cd_loop_uv_offset);

  const float uv_src_v1[2] = {luv_src_v1->uv[0], luv_src_v1->uv[1] / aspect_y};
  const float uv_src_v2[2] = {luv_src_v2->uv[0], luv_src_v2->uv[1] / aspect_y};
  const float uv_dst_v1[2] = {luv_dst_v1->uv[0], luv_dst_v1->uv[1] / aspect_y};
  const float uv_dst_v2[2] = {luv_dst_v2->uv[0], luv_dst_v2->uv[1] / aspect_y};

  struct {
    int src_index;
    int dst_index;
    float len_sq;
  } tests[4] = {
      {0, 0, len_squared_v2v2(uv_src_v1, uv_dst_v1)},
      {0, 1, len_squared_v2v2(uv_src_v1, uv_dst_v2)},
      {1, 0, len_squared_v2v2(uv_src_v2, uv_dst_v1)},
      {1, 1, len_squared_v2v2(uv_src_v2, uv_dst_v2)},
  };
  int i_best = 0;
  for (int i = 1; i < ARRAY_SIZE(tests); i++) {
    if (tests[i].len_sq < tests[i_best].len_sq) {
      i_best = i;
    }
  }

  *ele_src_p = (BMElem *)(tests[i_best].src_index ? l_src->next : l_src);
  *ele_dst_p = (BMElem *)(tests[i_best].dst_index ? l_dst->next : l_dst);

  /* Ensure the edge is selected, not just the vertices up until we hit it. */
  *r_ele_dst_final = (BMElem *)(tests[i_best].dst_index ? l_dst : l_dst->next);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path Select Struct & Properties
 * \{ */

struct PathSelectParams {
  /** ensure the active element is the last selected item (handy for picking) */
  bool track_active;
  bool use_topology_distance;
  bool use_face_step;
#ifdef USE_FILL
  bool use_fill;
#endif
  struct CheckerIntervalParams interval_params;
};

struct UserData_UV {
  Scene *scene;
  uint cd_loop_uv_offset;
};

static void path_select_properties(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna,
                  "use_face_step",
                  false,
                  "Face Stepping",
                  "Traverse connected faces (includes diagonals and edge-rings)");
  RNA_def_boolean(ot->srna,
                  "use_topology_distance",
                  false,
                  "Topology Distance",
                  "Find the minimum number of steps, ignoring spatial distance");
#ifdef USE_FILL
  RNA_def_boolean(ot->srna,
                  "use_fill",
                  false,
                  "Fill Region",
                  "Select all paths between the source/destination elements");
#endif

  WM_operator_properties_checker_interval(ot, true);
}

static void path_select_params_from_op(wmOperator *op, struct PathSelectParams *op_params)
{
  op_params->track_active = false;
  op_params->use_face_step = RNA_boolean_get(op->ptr, "use_face_step");
#ifdef USE_FILL
  op_params->use_fill = RNA_boolean_get(op->ptr, "use_fill");
#endif
  op_params->use_topology_distance = RNA_boolean_get(op->ptr, "use_topology_distance");
  WM_operator_properties_checker_interval_from_op(op, &op_params->interval_params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Vert Path
 * \{ */

/* callbacks */
static bool looptag_filter_cb(BMLoop *l, void *user_data_v)
{
  struct UserData_UV *user_data = user_data_v;
  return uvedit_face_visible_test(user_data->scene, l->f);
}
static bool looptag_test_cb(BMLoop *l, void *user_data_v)
{
  /* All connected loops are selected or we return false. */
  struct UserData_UV *user_data = user_data_v;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, l->v, BM_LOOPS_OF_VERT) {
    if (looptag_filter_cb(l_iter, user_data)) {
      const MLoopUV *luv_iter = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
      if (equals_v2v2(luv->uv, luv_iter->uv)) {
        if ((luv_iter->flag & MLOOPUV_VERTSEL) == 0) {
          return false;
        }
      }
    }
  }
  return true;
}
static void looptag_set_cb(BMLoop *l, bool val, void *user_data_v)
{
  struct UserData_UV *user_data = user_data_v;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, l->v, BM_LOOPS_OF_VERT) {
    if (looptag_filter_cb(l_iter, user_data)) {
      MLoopUV *luv_iter = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
      if (equals_v2v2(luv->uv, luv_iter->uv)) {
        SET_FLAG_FROM_TEST(luv_iter->flag, val, MLOOPUV_VERTSEL);
      }
    }
  }
}

static void mouse_mesh_uv_shortest_path_vert(Scene *scene,
                                             Object *obedit,
                                             const struct PathSelectParams *op_params,
                                             BMLoop *l_src,
                                             BMLoop *l_dst,
                                             BMLoop *l_dst_add_to_path,
                                             const float aspect_y,
                                             const int cd_loop_uv_offset)
{
  const ToolSettings *ts = scene->toolsettings;
  const bool use_fake_edge_select = (ts->uv_selectmode & UV_SELECT_EDGE);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

  struct UserData_UV user_data = {
      .scene = scene,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };

  const struct BMCalcPathUVParams params = {
      .use_topology_distance = op_params->use_topology_distance,
      .use_step_face = op_params->use_face_step,
      .aspect_y = aspect_y,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };
  LinkNode *path = BM_mesh_calc_path_uv_vert(
      bm, l_src, l_dst, &params, looptag_filter_cb, &user_data);
  /* TODO: false when we support region selection. */
  bool is_path_ordered = true;

  BMLoop *l_dst_last = l_dst;

  if (path) {
    if ((l_dst_add_to_path != NULL) && (BLI_linklist_index(path, l_dst_add_to_path) == -1)) {
      /* Append, this isn't optimal compared to #BLI_linklist_append, it's a one-off lookup. */
      LinkNode *path_last = BLI_linklist_find_last(path);
      BLI_linklist_insert_after(&path_last, l_dst_add_to_path);
      BLI_assert(BLI_linklist_find_last(path)->link == l_dst_add_to_path);
    }

    /* toggle the flag */
    bool all_set = true;
    LinkNode *node = path;
    do {
      if (!looptag_test_cb((BMLoop *)node->link, &user_data)) {
        all_set = false;
        break;
      }
    } while ((node = node->next));

    int depth = -1;
    node = path;
    do {
      if ((is_path_ordered == false) ||
          WM_operator_properties_checker_interval_test(&op_params->interval_params, depth)) {
        looptag_set_cb((BMLoop *)node->link, !all_set, &user_data);
        if (is_path_ordered) {
          l_dst_last = node->link;
        }
      }
    } while ((void)depth++, (node = node->next));

    BLI_linklist_free(path, NULL);
  }
  else {
    const bool is_act = !looptag_test_cb(l_dst, &user_data);
    looptag_set_cb(l_dst, is_act, &user_data); /* switch the face option */
  }

  if (op_params->track_active) {
    /* Fake edge selection. */
    if (use_fake_edge_select) {
      ED_uvedit_active_edge_loop_set(bm, l_dst_last);
    }
    else {
      ED_uvedit_active_vert_loop_set(bm, l_dst_last);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Face Path
 * \{ */

/* callbacks */
static bool facetag_filter_cb(BMFace *f, void *user_data_v)
{
  struct UserData_UV *user_data = user_data_v;
  return uvedit_face_visible_test(user_data->scene, f);
}
static bool facetag_test_cb(BMFace *f, void *user_data_v)
{
  /* All connected loops are selected or we return false. */
  struct UserData_UV *user_data = user_data_v;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, f, BM_LOOPS_OF_FACE) {
    const MLoopUV *luv_iter = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    if ((luv_iter->flag & MLOOPUV_VERTSEL) == 0) {
      return false;
    }
  }
  return true;
}
static void facetag_set_cb(BMFace *f, bool val, void *user_data_v)
{
  struct UserData_UV *user_data = user_data_v;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, f, BM_LOOPS_OF_FACE) {
    MLoopUV *luv_iter = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    SET_FLAG_FROM_TEST(luv_iter->flag, val, MLOOPUV_VERTSEL);
  }
}

static void mouse_mesh_uv_shortest_path_face(Scene *scene,
                                             Object *obedit,
                                             const struct PathSelectParams *op_params,
                                             BMFace *f_src,
                                             BMFace *f_dst,
                                             const float aspect_y,
                                             const int cd_loop_uv_offset)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

  struct UserData_UV user_data = {
      .scene = scene,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };

  const struct BMCalcPathUVParams params = {
      .use_topology_distance = op_params->use_topology_distance,
      .use_step_face = op_params->use_face_step,
      .aspect_y = aspect_y,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };
  LinkNode *path = BM_mesh_calc_path_uv_face(
      bm, f_src, f_dst, &params, facetag_filter_cb, &user_data);
  /* TODO: false when we support region selection. */
  bool is_path_ordered = true;

  BMFace *f_dst_last = f_dst;

  if (path) {
    /* toggle the flag */
    bool all_set = true;
    LinkNode *node = path;
    do {
      if (!facetag_test_cb((BMFace *)node->link, &user_data)) {
        all_set = false;
        break;
      }
    } while ((node = node->next));

    int depth = -1;
    node = path;
    do {
      if ((is_path_ordered == false) ||
          WM_operator_properties_checker_interval_test(&op_params->interval_params, depth)) {
        facetag_set_cb((BMFace *)node->link, !all_set, &user_data);
        if (is_path_ordered) {
          f_dst_last = node->link;
        }
      }
    } while ((void)depth++, (node = node->next));

    BLI_linklist_free(path, NULL);
  }
  else {
    const bool is_act = !facetag_test_cb(f_dst, &user_data);
    facetag_set_cb(f_dst, is_act, &user_data); /* switch the face option */
  }

  if (op_params->track_active) {
    /* Unlike other types, we can track active without it being selected. */
    BM_mesh_active_face_set(bm, f_dst_last);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Operator for vert/edge/face tag
 * \{ */

static int uv_shortest_path_pick_exec(bContext *C, wmOperator *op);

static bool uv_shortest_path_pick_ex(Scene *scene,
                                     Depsgraph *depsgraph,
                                     Object *obedit,
                                     const struct PathSelectParams *op_params,
                                     BMElem *ele_src,
                                     BMElem *ele_dst,
                                     const float aspect_y,
                                     const int cd_loop_uv_offset)
{
  bool ok = false;

  if (ELEM(NULL, ele_src, ele_dst) || (ele_src->head.htype != ele_dst->head.htype)) {
    /* pass */
  }
  else if (ele_src->head.htype == BM_FACE) {
    mouse_mesh_uv_shortest_path_face(scene,
                                     obedit,
                                     op_params,
                                     (BMFace *)ele_src,
                                     (BMFace *)ele_dst,
                                     aspect_y,
                                     cd_loop_uv_offset);
    ok = true;
  }
  else if (ele_src->head.htype == BM_LOOP) {
    const ToolSettings *ts = scene->toolsettings;
    BMElem *ele_dst_final = NULL;
    if (ts->uv_selectmode & UV_SELECT_EDGE) {
      bm_loop_calc_vert_pair_from_edge_pair(
          cd_loop_uv_offset, aspect_y, &ele_src, &ele_dst, &ele_dst_final);
    }
    mouse_mesh_uv_shortest_path_vert(scene,
                                     obedit,
                                     op_params,
                                     (BMLoop *)ele_src,
                                     (BMLoop *)ele_dst,
                                     (BMLoop *)ele_dst_final,
                                     aspect_y,
                                     cd_loop_uv_offset);
    ok = true;
  }

  if (ok) {
    Object *obedit_eval = DEG_get_evaluated_object(depsgraph, obedit);
    BKE_mesh_batch_cache_dirty_tag(obedit_eval->data, BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT);
    /* Only for region redraw. */
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }

  return ok;
}

static int uv_shortest_path_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;

  /* We could support this, it needs further testing. */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BKE_report(op->reports, RPT_ERROR, "Sync selection doesn't support path select");
    return OPERATOR_CANCELLED;
  }

  if (RNA_struct_property_is_set(op->ptr, "index")) {
    return uv_shortest_path_pick_exec(C, op);
  }

  struct PathSelectParams op_params;
  path_select_params_from_op(op, &op_params);

  /* Set false if we support edge tagging. */
  op_params.track_active = true;

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  float co[2];

  const ARegion *region = CTX_wm_region(C);

  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

  float aspect_y;
  {
    float aspx, aspy;
    ED_uvedit_get_aspect(obedit, &aspx, &aspy);
    aspect_y = aspx / aspy;
  }

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);

  BMElem *ele_src = NULL, *ele_dst = NULL;

  if (ts->uv_selectmode & UV_SELECT_FACE) {
    UvNearestHit hit = UV_NEAREST_HIT_INIT;
    if (!uv_find_nearest_face(scene, obedit, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMFace *f_src = BM_mesh_active_face_get(bm, false, false);
    /* Check selection? */

    ele_src = (BMElem *)f_src;
    ele_dst = (BMElem *)hit.efa;
  }
  else if (ts->uv_selectmode & UV_SELECT_EDGE) {
    UvNearestHit hit = UV_NEAREST_HIT_INIT;
    if (!uv_find_nearest_edge(scene, obedit, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMLoop *l_src = ED_uvedit_active_edge_loop_get(bm);
    const MLoopUV *luv_src_v1 = BM_ELEM_CD_GET_VOID_P(l_src, cd_loop_uv_offset);
    const MLoopUV *luv_src_v2 = BM_ELEM_CD_GET_VOID_P(l_src->next, cd_loop_uv_offset);
    if ((luv_src_v1->flag & MLOOPUV_VERTSEL) == 0 && (luv_src_v2->flag & MLOOPUV_VERTSEL) == 0) {
      l_src = NULL;
    }

    ele_src = (BMElem *)l_src;
    ele_dst = (BMElem *)hit.l;
  }
  else {
    UvNearestHit hit = UV_NEAREST_HIT_INIT;
    if (!uv_find_nearest_vert(scene, obedit, co, 0.0f, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMLoop *l_src = ED_uvedit_active_vert_loop_get(bm);
    const MLoopUV *luv_src = BM_ELEM_CD_GET_VOID_P(l_src, cd_loop_uv_offset);
    if ((luv_src->flag & MLOOPUV_VERTSEL) == 0) {
      l_src = NULL;
    }

    ele_src = (BMElem *)l_src;
    ele_dst = (BMElem *)hit.l;
  }

  if (ele_src == NULL || ele_dst == NULL) {
    return OPERATOR_CANCELLED;
  }

  uv_shortest_path_pick_ex(
      scene, depsgraph, obedit, &op_params, ele_src, ele_dst, aspect_y, cd_loop_uv_offset);

  /* To support redo. */
  int index;
  if (ts->uv_selectmode & UV_SELECT_FACE) {
    BM_mesh_elem_index_ensure(bm, BM_FACE);
    index = BM_elem_index_get(ele_dst);
  }
  else if (ts->uv_selectmode & UV_SELECT_EDGE) {
    BM_mesh_elem_index_ensure(bm, BM_LOOP);
    index = BM_elem_index_get(ele_dst);
  }
  else {
    BM_mesh_elem_index_ensure(bm, BM_LOOP);
    index = BM_elem_index_get(ele_dst);
  }
  RNA_int_set(op->ptr, "index", index);

  return OPERATOR_FINISHED;
}

static int uv_shortest_path_pick_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

  float aspect_y;
  {
    float aspx, aspy;
    ED_uvedit_get_aspect(obedit, &aspx, &aspy);
    aspect_y = aspx / aspy;
  }

  const int index = RNA_int_get(op->ptr, "index");

  BMElem *ele_src, *ele_dst;

  if (ts->uv_selectmode & UV_SELECT_FACE) {
    if (index < 0 || index >= bm->totface) {
      return OPERATOR_CANCELLED;
    }
    if (!(ele_src = (BMElem *)BM_mesh_active_face_get(bm, false, false)) ||
        !(ele_dst = (BMElem *)BM_face_at_index_find_or_table(bm, index))) {
      return OPERATOR_CANCELLED;
    }
  }
  else if (ts->uv_selectmode & UV_SELECT_EDGE) {
    if (index < 0 || index >= bm->totloop) {
      return OPERATOR_CANCELLED;
    }
    if (!(ele_src = (BMElem *)ED_uvedit_active_edge_loop_get(bm)) ||
        !(ele_dst = (BMElem *)BM_loop_at_index_find(bm, index))) {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    if (index < 0 || index >= bm->totloop) {
      return OPERATOR_CANCELLED;
    }
    if (!(ele_src = (BMElem *)ED_uvedit_active_vert_loop_get(bm)) ||
        !(ele_dst = (BMElem *)BM_loop_at_index_find(bm, index))) {
      return OPERATOR_CANCELLED;
    }
  }

  struct PathSelectParams op_params;
  path_select_params_from_op(op, &op_params);
  op_params.track_active = true;

  if (!uv_shortest_path_pick_ex(
          scene, depsgraph, obedit, &op_params, ele_src, ele_dst, aspect_y, cd_loop_uv_offset)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void UV_OT_shortest_path_pick(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Pick Shortest Path";
  ot->idname = "UV_OT_shortest_path_pick";
  ot->description = "Select shortest path between two selections";

  /* api callbacks */
  ot->invoke = uv_shortest_path_pick_invoke;
  ot->exec = uv_shortest_path_pick_exec;
  ot->poll = ED_operator_uvedit;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  path_select_properties(ot);

  /* use for redo */
  prop = RNA_def_int(ot->srna, "index", -1, -1, INT_MAX, "", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */
