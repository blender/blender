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
 *
 * \param use_nearest: When false use the post distant pair of loops,
 * use when filling a region as we want both verts from each edge to be included in the region.
 */
static void bm_loop_calc_vert_pair_from_edge_pair(const bool use_nearest,
                                                  const int cd_loop_uv_offset,
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
    if (use_nearest) {
      if (tests[i].len_sq < tests[i_best].len_sq) {
        i_best = i;
      }
    }
    else {
      if (tests[i].len_sq > tests[i_best].len_sq) {
        i_best = i;
      }
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
  bool use_fill;
  struct CheckerIntervalParams interval_params;
};

struct UserData_UV {
  Scene *scene;
  BMEditMesh *em;
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
  RNA_def_boolean(ot->srna,
                  "use_fill",
                  false,
                  "Fill Region",
                  "Select all paths between the source/destination elements");

  WM_operator_properties_checker_interval(ot, true);
}

static void path_select_params_from_op(wmOperator *op, struct PathSelectParams *op_params)
{
  op_params->track_active = false;
  op_params->use_face_step = RNA_boolean_get(op->ptr, "use_face_step");
  op_params->use_fill = RNA_boolean_get(op->ptr, "use_fill");
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
  const Scene *scene = user_data->scene;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, l->v, BM_LOOPS_OF_VERT) {
    if (looptag_filter_cb(l_iter, user_data)) {
      const MLoopUV *luv_iter = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
      if (equals_v2v2(luv->uv, luv_iter->uv)) {
        if (!uvedit_uv_select_test(scene, l_iter, cd_loop_uv_offset)) {
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
  const Scene *scene = user_data->scene;
  BMEditMesh *em = user_data->em;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, l->v, BM_LOOPS_OF_VERT) {
    if (looptag_filter_cb(l_iter, user_data)) {
      MLoopUV *luv_iter = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
      if (equals_v2v2(luv->uv, luv_iter->uv)) {
        uvedit_uv_select_set(scene, em, l_iter, val, false, cd_loop_uv_offset);
      }
    }
  }
}

static int mouse_mesh_uv_shortest_path_vert(Scene *scene,
                                            Object *obedit,
                                            const struct PathSelectParams *op_params,
                                            BMLoop *l_src,
                                            BMLoop *l_dst,
                                            const float aspect_y,
                                            const int cd_loop_uv_offset)
{
  const char uv_selectmode = ED_uvedit_select_mode_get(scene);
  const bool use_fake_edge_select = (uv_selectmode & UV_SELECT_EDGE);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  int flush = 0;

  /* Variables to use when `use_fake_edge_select` is set. */
  struct {
    BMLoop *l_dst_activate;
    BMLoop *l_dst_add_to_path;
  } fake_edge_select = {NULL};

  if (use_fake_edge_select) {
    fake_edge_select.l_dst_activate = l_dst;

    /* Use most distant when doing region selection.
     * without this we get dangling edges outside the region. */
    bool use_neaerst = (op_params->use_fill == false);
    BMElem *ele_src = (BMElem *)l_src;
    BMElem *ele_dst = (BMElem *)l_dst;
    BMElem *ele_dst_final = NULL;
    bm_loop_calc_vert_pair_from_edge_pair(
        use_neaerst, cd_loop_uv_offset, aspect_y, &ele_src, &ele_dst, &ele_dst_final);

    if (op_params->use_fill == false) {
      /* Always activate the item under the cursor. */
      fake_edge_select.l_dst_add_to_path = (BMLoop *)ele_dst_final;
    }

    l_src = (BMLoop *)ele_src;
    l_dst = (BMLoop *)ele_dst;
  }

  struct UserData_UV user_data = {
      .scene = scene,
      .em = em,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };

  const struct BMCalcPathUVParams params = {
      .use_topology_distance = op_params->use_topology_distance,
      .use_step_face = op_params->use_face_step,
      .aspect_y = aspect_y,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };

  LinkNode *path = NULL;
  bool is_path_ordered = false;

  if (l_src != l_dst) {
    if (op_params->use_fill) {
      path = BM_mesh_calc_path_uv_region_vert(bm,
                                              (BMElem *)l_src,
                                              (BMElem *)l_dst,
                                              params.cd_loop_uv_offset,
                                              looptag_filter_cb,
                                              &user_data);
    }
    else {
      is_path_ordered = true;
      path = BM_mesh_calc_path_uv_vert(bm, l_src, l_dst, &params, looptag_filter_cb, &user_data);
    }
  }

  BMLoop *l_dst_last = l_dst;

  if (path) {
    if (use_fake_edge_select) {
      if ((fake_edge_select.l_dst_add_to_path != NULL) &&
          (BLI_linklist_index(path, fake_edge_select.l_dst_add_to_path) == -1)) {
        /* Append, this isn't optimal compared to #BLI_linklist_append, it's a one-off lookup. */
        LinkNode *path_last = BLI_linklist_find_last(path);
        BLI_linklist_insert_after(&path_last, fake_edge_select.l_dst_add_to_path);
        BLI_assert(BLI_linklist_find_last(path)->link == fake_edge_select.l_dst_add_to_path);
      }
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
    flush = all_set ? -1 : 1;
  }
  else {
    const bool is_act = !looptag_test_cb(l_dst, &user_data);
    looptag_set_cb(l_dst, is_act, &user_data); /* switch the face option */
  }

  if (op_params->track_active) {
    /* Fake edge selection. */
    if (use_fake_edge_select) {
      BMLoop *l_dst_activate = fake_edge_select.l_dst_activate;
      /* TODO(campbell): Search for an active loop attached to 'l_dst'.
       * when `BLI_linklist_index(path, l_dst_activate) == -1`
       * In practice this rarely happens though. */
      ED_uvedit_active_edge_loop_set(bm, l_dst_activate);
    }
    else {
      ED_uvedit_active_vert_loop_set(bm, l_dst_last);
    }
  }
  return flush;
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
  const Scene *scene = user_data->scene;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  BMIter iter;
  BMLoop *l_iter;
  BM_ITER_ELEM (l_iter, &iter, f, BM_LOOPS_OF_FACE) {
    if (!uvedit_uv_select_test(scene, l_iter, cd_loop_uv_offset)) {
      return false;
    }
  }
  return true;
}
static void facetag_set_cb(BMFace *f, bool val, void *user_data_v)
{
  struct UserData_UV *user_data = user_data_v;
  const Scene *scene = user_data->scene;
  BMEditMesh *em = user_data->em;
  const uint cd_loop_uv_offset = user_data->cd_loop_uv_offset;
  uvedit_face_select_set(scene, em, f, val, false, cd_loop_uv_offset);
}

static int mouse_mesh_uv_shortest_path_face(Scene *scene,
                                            Object *obedit,
                                            const struct PathSelectParams *op_params,
                                            BMFace *f_src,
                                            BMFace *f_dst,
                                            const float aspect_y,
                                            const int cd_loop_uv_offset)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  int flush = 0;

  struct UserData_UV user_data = {
      .scene = scene,
      .em = em,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };

  const struct BMCalcPathUVParams params = {
      .use_topology_distance = op_params->use_topology_distance,
      .use_step_face = op_params->use_face_step,
      .aspect_y = aspect_y,
      .cd_loop_uv_offset = cd_loop_uv_offset,
  };

  LinkNode *path = NULL;
  bool is_path_ordered = false;

  if (f_src != f_dst) {
    if (op_params->use_fill) {
      path = BM_mesh_calc_path_uv_region_face(bm,
                                              (BMElem *)f_src,
                                              (BMElem *)f_dst,
                                              params.cd_loop_uv_offset,
                                              facetag_filter_cb,
                                              &user_data);
    }
    else {
      is_path_ordered = true;
      path = BM_mesh_calc_path_uv_face(bm, f_src, f_dst, &params, facetag_filter_cb, &user_data);
    }
  }

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
    flush = all_set ? -1 : 1;
  }
  else {
    const bool is_act = !facetag_test_cb(f_dst, &user_data);
    facetag_set_cb(f_dst, is_act, &user_data); /* switch the face option */
  }

  if (op_params->track_active) {
    /* Unlike other types, we can track active without it being selected. */
    BM_mesh_active_face_set(bm, f_dst_last);
  }
  return flush;
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
  const ToolSettings *ts = scene->toolsettings;
  const char uv_selectmode = ED_uvedit_select_mode_get(scene);
  bool ok = false;
  int flush = 0;

  if (ELEM(NULL, ele_src, ele_dst) || (ele_src->head.htype != ele_dst->head.htype)) {
    /* pass */
  }
  else if (ele_src->head.htype == BM_FACE) {
    flush = mouse_mesh_uv_shortest_path_face(scene,
                                             obedit,
                                             op_params,
                                             (BMFace *)ele_src,
                                             (BMFace *)ele_dst,
                                             aspect_y,
                                             cd_loop_uv_offset);
    ok = true;
  }
  else if (ele_src->head.htype == BM_LOOP) {
    flush = mouse_mesh_uv_shortest_path_vert(scene,
                                             obedit,
                                             op_params,
                                             (BMLoop *)ele_src,
                                             (BMLoop *)ele_dst,
                                             aspect_y,
                                             cd_loop_uv_offset);
    ok = true;
  }

  if (ok) {
    if (flush != 0) {
      const bool select = (flush == 1);
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      if (ts->uv_flag & UV_SYNC_SELECTION) {
        if (uv_selectmode & UV_SELECT_EDGE) {
          /* Special case as we don't use true edge selection,
           * flush the selection from the vertices. */
          BM_mesh_select_mode_flush_ex(em->bm, SCE_SELECT_VERTEX);
        }
      }
      ED_uvedit_select_sync_flush(scene->toolsettings, em, select);
    }

    if (ts->uv_flag & UV_SYNC_SELECTION) {
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    }
    else {
      Object *obedit_eval = DEG_get_evaluated_object(depsgraph, obedit);
      BKE_mesh_batch_cache_dirty_tag(obedit_eval->data, BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT);
    }
    /* Only for region redraw. */
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }

  return ok;
}

static int uv_shortest_path_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  const char uv_selectmode = ED_uvedit_select_mode_get(scene);

  /* We could support this, it needs further testing. */
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

  if (uv_selectmode == UV_SELECT_FACE) {
    UvNearestHit hit = UV_NEAREST_HIT_INIT;
    if (!uv_find_nearest_face(scene, obedit, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMFace *f_src = BM_mesh_active_face_get(bm, false, false);
    /* Check selection? */

    ele_src = (BMElem *)f_src;
    ele_dst = (BMElem *)hit.efa;
  }

  else if (uv_selectmode & UV_SELECT_EDGE) {
    UvNearestHit hit = UV_NEAREST_HIT_INIT;
    if (!uv_find_nearest_edge(scene, obedit, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMLoop *l_src = NULL;
    if (ts->uv_flag & UV_SYNC_SELECTION) {
      BMEdge *e_src = BM_mesh_active_edge_get(bm);
      if (e_src != NULL) {
        l_src = uv_find_nearest_loop_from_edge(scene, obedit, e_src, co);
      }
    }
    else {
      l_src = ED_uvedit_active_edge_loop_get(bm);
      if (l_src != NULL) {
        if ((!uvedit_uv_select_test(scene, l_src, cd_loop_uv_offset)) &&
            (!uvedit_uv_select_test(scene, l_src->next, cd_loop_uv_offset))) {
          l_src = NULL;
        }
        ele_src = (BMElem *)l_src;
      }
    }
    ele_src = (BMElem *)l_src;
    ele_dst = (BMElem *)hit.l;
  }
  else {
    UvNearestHit hit = UV_NEAREST_HIT_INIT;
    if (!uv_find_nearest_vert(scene, obedit, co, 0.0f, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMLoop *l_src = NULL;
    if (ts->uv_flag & UV_SYNC_SELECTION) {
      BMVert *v_src = BM_mesh_active_vert_get(bm);
      if (v_src != NULL) {
        l_src = uv_find_nearest_loop_from_vert(scene, obedit, v_src, co);
      }
    }
    else {
      l_src = ED_uvedit_active_vert_loop_get(bm);
      if (l_src != NULL) {
        if (!uvedit_uv_select_test(scene, l_src, cd_loop_uv_offset)) {
          l_src = NULL;
        }
      }
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
  if (uv_selectmode & UV_SELECT_FACE) {
    BM_mesh_elem_index_ensure(bm, BM_FACE);
    index = BM_elem_index_get(ele_dst);
  }
  else if (uv_selectmode & UV_SELECT_EDGE) {
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
  const char uv_selectmode = ED_uvedit_select_mode_get(scene);
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

  if (uv_selectmode & UV_SELECT_FACE) {
    if (index < 0 || index >= bm->totface) {
      return OPERATOR_CANCELLED;
    }
    if (!(ele_src = (BMElem *)BM_mesh_active_face_get(bm, false, false)) ||
        !(ele_dst = (BMElem *)BM_face_at_index_find_or_table(bm, index))) {
      return OPERATOR_CANCELLED;
    }
  }
  else if (uv_selectmode & UV_SELECT_EDGE) {
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

/* -------------------------------------------------------------------- */
/** \name Select Path Between Existing Selection
 * \{ */

static int uv_shortest_path_select_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  const char uv_selectmode = ED_uvedit_select_mode_get(scene);
  bool found_valid_elements = false;

  float aspect_y;
  {
    Object *obedit = CTX_data_edit_object(C);
    float aspx, aspy;
    ED_uvedit_get_aspect(obedit, &aspx, &aspy);
    aspect_y = aspx / aspy;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
    BMElem *ele_src = NULL, *ele_dst = NULL;

    /* Find 2x elements. */
    {
      BMElem **ele_array = NULL;
      int ele_array_len = 0;
      if (uv_selectmode & UV_SELECT_FACE) {
        ele_array = (BMElem **)ED_uvedit_selected_faces(scene, bm, 3, &ele_array_len);
      }
      else if (uv_selectmode & UV_SELECT_EDGE) {
        ele_array = (BMElem **)ED_uvedit_selected_edges(scene, bm, 3, &ele_array_len);
      }
      else {
        ele_array = (BMElem **)ED_uvedit_selected_verts(scene, bm, 3, &ele_array_len);
      }

      if (ele_array_len == 2) {
        ele_src = ele_array[0];
        ele_dst = ele_array[1];
      }
      MEM_freeN(ele_array);
    }

    if (ele_src && ele_dst) {
      struct PathSelectParams op_params;
      path_select_params_from_op(op, &op_params);

      uv_shortest_path_pick_ex(
          scene, depsgraph, obedit, &op_params, ele_src, ele_dst, aspect_y, cd_loop_uv_offset);

      found_valid_elements = true;
    }
  }
  MEM_freeN(objects);

  if (!found_valid_elements) {
    BKE_report(
        op->reports, RPT_WARNING, "Path selection requires two matching elements to be selected");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void UV_OT_shortest_path_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Shortest Path";
  ot->idname = "UV_OT_shortest_path_select";
  ot->description = "Selected shortest path between two vertices/edges/faces";

  /* api callbacks */
  ot->exec = uv_shortest_path_select_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  path_select_properties(ot);
}

/** \} */
