/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"

#ifdef WITH_FREESTYLE
#  include "DNA_meshdata_types.h"
#endif

#include "BLI_linklist.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_mesh_types.hh"
#include "BKE_report.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_uvedit.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#include "DEG_depsgraph.hh"

#include "mesh_intern.hh" /* own include */

using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Path Select Struct & Properties
 * \{ */

enum {
  EDGE_MODE_SELECT = 0,
  EDGE_MODE_TAG_SEAM = 1,
  EDGE_MODE_TAG_SHARP = 2,
  EDGE_MODE_TAG_CREASE = 3,
  EDGE_MODE_TAG_BEVEL = 4,
  EDGE_MODE_TAG_FREESTYLE = 5,
};

struct PathSelectParams {
  /** ensure the active element is the last selected item (handy for picking) */
  bool track_active;
  bool use_topology_distance;
  bool use_face_step;
  bool use_fill;
  char edge_mode;
  CheckerIntervalParams interval_params;
};

static void path_select_properties(wmOperatorType *ot)
{
  static const EnumPropertyItem edge_tag_items[] = {
      {EDGE_MODE_SELECT, "SELECT", 0, "Select", ""},
      {EDGE_MODE_TAG_SEAM, "SEAM", 0, "Tag Seam", ""},
      {EDGE_MODE_TAG_SHARP, "SHARP", 0, "Tag Sharp", ""},
      {EDGE_MODE_TAG_CREASE, "CREASE", 0, "Tag Crease", ""},
      {EDGE_MODE_TAG_BEVEL, "BEVEL", 0, "Tag Bevel", ""},
      {EDGE_MODE_TAG_FREESTYLE, "FREESTYLE", 0, "Tag Freestyle Edge Mark", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "edge_mode",
               edge_tag_items,
               EDGE_MODE_SELECT,
               "Edge Tag",
               "The edge flag to tag when selecting the shortest path");

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

static void path_select_params_from_op(wmOperator *op,
                                       ToolSettings *ts,
                                       PathSelectParams *op_params)
{
  {
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "edge_mode");
    if (RNA_property_is_set(op->ptr, prop)) {
      op_params->edge_mode = RNA_property_enum_get(op->ptr, prop);
      if (op->flag & OP_IS_INVOKE) {
        ts->edge_mode = op_params->edge_mode;
      }
    }
    else {
      op_params->edge_mode = ts->edge_mode;
      RNA_property_enum_set(op->ptr, prop, op_params->edge_mode);
    }
  }

  op_params->track_active = false;
  op_params->use_face_step = RNA_boolean_get(op->ptr, "use_face_step");
  op_params->use_fill = RNA_boolean_get(op->ptr, "use_fill");
  op_params->use_topology_distance = RNA_boolean_get(op->ptr, "use_topology_distance");
  WM_operator_properties_checker_interval_from_op(op, &op_params->interval_params);
}

static bool path_select_poll_property(const bContext *C,
                                      wmOperator * /*op*/,
                                      const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  if (STREQ(prop_id, "edge_mode")) {
    const Scene *scene = CTX_data_scene(C);
    ToolSettings *ts = scene->toolsettings;
    if ((ts->selectmode & SCE_SELECT_EDGE) == 0) {
      return false;
    }
  }
  return true;
}

struct UserData {
  BMesh *bm;
  Mesh *mesh;
  int cd_offset;
  const PathSelectParams *op_params;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vert Path
 * \{ */

/* callbacks */
static bool verttag_filter_cb(BMVert *v, void * /*user_data_v*/)
{
  return !BM_elem_flag_test(v, BM_ELEM_HIDDEN);
}
static bool verttag_test_cb(BMVert *v, void * /*user_data_v*/)
{
  return BM_elem_flag_test_bool(v, BM_ELEM_SELECT);
}
static void verttag_set_cb(BMVert *v, bool val, void *user_data_v)
{
  UserData *user_data = static_cast<UserData *>(user_data_v);
  BM_vert_select_set(user_data->bm, v, val);
}

static void mouse_mesh_shortest_path_vert(Scene * /*scene*/,
                                          Object *obedit,
                                          const PathSelectParams *op_params,
                                          BMVert *v_act,
                                          BMVert *v_dst)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

  int cd_offset = -1;
  switch (op_params->edge_mode) {
    case EDGE_MODE_SELECT:
    case EDGE_MODE_TAG_SEAM:
    case EDGE_MODE_TAG_SHARP:
#ifdef WITH_FREESTYLE
    case EDGE_MODE_TAG_FREESTYLE:
#endif
      break;
    case EDGE_MODE_TAG_CREASE:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "crease_edge");
      break;
    case EDGE_MODE_TAG_BEVEL:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
      break;
  }

  UserData user_data = {bm, static_cast<Mesh *>(obedit->data), cd_offset, op_params};
  LinkNode *path = nullptr;
  bool is_path_ordered = false;

  if (v_act && (v_act != v_dst)) {
    if (op_params->use_fill) {
      path = BM_mesh_calc_path_region_vert(
          bm, (BMElem *)v_act, (BMElem *)v_dst, verttag_filter_cb, &user_data);
    }
    else {
      is_path_ordered = true;
      BMCalcPathParams params{};
      params.use_topology_distance = op_params->use_topology_distance;
      params.use_step_face = op_params->use_face_step;
      path = BM_mesh_calc_path_vert(bm, v_act, v_dst, &params, verttag_filter_cb, &user_data);
    }

    if (path) {
      if (op_params->track_active) {
        BM_select_history_remove(bm, v_act);
      }
    }
  }

  BMVert *v_dst_last = v_dst;

  if (path) {
    /* toggle the flag */
    bool all_set = true;
    LinkNode *node;

    node = path;
    do {
      if (!verttag_test_cb((BMVert *)node->link, &user_data)) {
        all_set = false;
        break;
      }
    } while ((node = node->next));

    int depth = -1;
    node = path;
    do {
      if ((is_path_ordered == false) ||
          WM_operator_properties_checker_interval_test(&op_params->interval_params, depth))
      {
        verttag_set_cb((BMVert *)node->link, !all_set, &user_data);
        if (is_path_ordered) {
          v_dst_last = static_cast<BMVert *>(node->link);
        }
      }
    } while ((void)depth++, (node = node->next));

    BLI_linklist_free(path, nullptr);
  }
  else {
    const bool is_act = !verttag_test_cb(v_dst, &user_data);
    verttag_set_cb(v_dst, is_act, &user_data); /* switch the face option */
  }

  EDBM_selectmode_flush(em);
  EDBM_uvselect_clear(em);

  if (op_params->track_active) {
    /* even if this is selected it may not be in the selection list */
    if (BM_elem_flag_test(v_dst_last, BM_ELEM_SELECT) == 0) {
      BM_select_history_remove(bm, v_dst_last);
    }
    else {
      BM_select_history_store(bm, v_dst_last);
    }
  }

  EDBMUpdate_Params params{};
  params.calc_looptris = false;
  params.calc_normals = false;
  params.is_destructive = false;
  EDBM_update(static_cast<Mesh *>(obedit->data), &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Path
 * \{ */

/* callbacks */
static bool edgetag_filter_cb(BMEdge *e, void * /*user_data_v*/)
{
  return !BM_elem_flag_test(e, BM_ELEM_HIDDEN);
}
static bool edgetag_test_cb(BMEdge *e, void *user_data_v)
{
  UserData *user_data = static_cast<UserData *>(user_data_v);
  const char edge_mode = user_data->op_params->edge_mode;

  switch (edge_mode) {
    case EDGE_MODE_SELECT:
      return BM_elem_flag_test(e, BM_ELEM_SELECT) ? true : false;
    case EDGE_MODE_TAG_SEAM:
      return BM_elem_flag_test(e, BM_ELEM_SEAM) ? true : false;
    case EDGE_MODE_TAG_SHARP:
      return BM_elem_flag_test(e, BM_ELEM_SMOOTH) ? false : true;
    case EDGE_MODE_TAG_CREASE:
    case EDGE_MODE_TAG_BEVEL:
      return BM_ELEM_CD_GET_FLOAT(e, user_data->cd_offset);
#ifdef WITH_FREESTYLE
    case EDGE_MODE_TAG_FREESTYLE: {
      return BM_ELEM_CD_GET_BOOL(e, user_data->cd_offset);
    }
#endif
  }
  return false;
}
static void edgetag_set_cb(BMEdge *e, bool val, void *user_data_v)
{
  UserData *user_data = static_cast<UserData *>(user_data_v);
  const char edge_mode = user_data->op_params->edge_mode;
  BMesh *bm = user_data->bm;

  switch (edge_mode) {
    case EDGE_MODE_SELECT:
      BM_edge_select_set(bm, e, val);
      break;
    case EDGE_MODE_TAG_SEAM:
      BM_elem_flag_set(e, BM_ELEM_SEAM, val);
      break;
    case EDGE_MODE_TAG_SHARP:
      BM_elem_flag_set(e, BM_ELEM_SMOOTH, !val);
      break;
    case EDGE_MODE_TAG_CREASE:
    case EDGE_MODE_TAG_BEVEL:
      BM_ELEM_CD_SET_FLOAT(e, user_data->cd_offset, (val) ? 1.0f : 0.0f);
      break;
#ifdef WITH_FREESTYLE
    case EDGE_MODE_TAG_FREESTYLE: {
      BM_ELEM_CD_SET_BOOL(e, user_data->cd_offset, val);
      break;
    }
#endif
  }
}

static void edgetag_ensure_cd_flag(Mesh *mesh, const char edge_mode)
{
  BMesh *bm = mesh->runtime->edit_mesh->bm;

  switch (edge_mode) {
    case EDGE_MODE_TAG_CREASE:
      if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "crease_edge")) {
        BM_data_layer_add_named(bm, &bm->edata, CD_PROP_FLOAT, "crease_edge");
      }
      break;
    case EDGE_MODE_TAG_BEVEL:
      if (!CustomData_has_layer_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge")) {
        BM_data_layer_add_named(bm, &bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
      }
      break;
#ifdef WITH_FREESTYLE
    case EDGE_MODE_TAG_FREESTYLE:
      if (!CustomData_has_layer_named(&bm->edata, CD_PROP_BOOL, "freestyle_edge")) {
        BM_data_layer_add_named(bm, &bm->edata, CD_PROP_BOOL, "freestyle_edge");
      }
      break;
#endif
    default:
      break;
  }
}

/* Mesh shortest path select, uses previously-selected edge. */

/* since you want to create paths with multiple selects, it doesn't have extend option */
static void mouse_mesh_shortest_path_edge(
    Scene *scene, Object *obedit, const PathSelectParams *op_params, BMEdge *e_act, BMEdge *e_dst)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

  int cd_offset = -1;
  switch (op_params->edge_mode) {
    case EDGE_MODE_SELECT:
    case EDGE_MODE_TAG_SEAM:
    case EDGE_MODE_TAG_SHARP:
#ifdef WITH_FREESTYLE
    case EDGE_MODE_TAG_FREESTYLE:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_BOOL, "freestyle_edge");
#endif
      break;
    case EDGE_MODE_TAG_CREASE:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "crease_edge");
      break;
    case EDGE_MODE_TAG_BEVEL:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
      break;
  }

  UserData user_data = {bm, static_cast<Mesh *>(obedit->data), cd_offset, op_params};
  LinkNode *path = nullptr;
  bool is_path_ordered = false;

  edgetag_ensure_cd_flag(static_cast<Mesh *>(obedit->data), op_params->edge_mode);

  if (e_act && (e_act != e_dst)) {
    if (op_params->use_fill) {
      path = BM_mesh_calc_path_region_edge(
          bm, (BMElem *)e_act, (BMElem *)e_dst, edgetag_filter_cb, &user_data);
    }
    else {
      is_path_ordered = true;
      BMCalcPathParams params{};
      params.use_topology_distance = op_params->use_topology_distance;
      params.use_step_face = op_params->use_face_step;
      path = BM_mesh_calc_path_edge(bm, e_act, e_dst, &params, edgetag_filter_cb, &user_data);
    }

    if (path) {
      if (op_params->track_active) {
        BM_select_history_remove(bm, e_act);
      }
    }
  }

  BMEdge *e_dst_last = e_dst;

  if (path) {
    /* toggle the flag */
    bool all_set = true;
    LinkNode *node;

    node = path;
    do {
      if (!edgetag_test_cb((BMEdge *)node->link, &user_data)) {
        all_set = false;
        break;
      }
    } while ((node = node->next));

    int depth = -1;
    node = path;
    do {
      if ((is_path_ordered == false) ||
          WM_operator_properties_checker_interval_test(&op_params->interval_params, depth))
      {
        edgetag_set_cb((BMEdge *)node->link, !all_set, &user_data);
        if (is_path_ordered) {
          e_dst_last = static_cast<BMEdge *>(node->link);
        }
      }
    } while ((void)depth++, (node = node->next));

    BLI_linklist_free(path, nullptr);
  }
  else {
    const bool is_act = !edgetag_test_cb(e_dst, &user_data);
    edgetag_ensure_cd_flag(static_cast<Mesh *>(obedit->data), op_params->edge_mode);
    edgetag_set_cb(e_dst, is_act, &user_data); /* switch the edge option */
  }

  if (op_params->edge_mode != EDGE_MODE_SELECT) {
    if (op_params->track_active) {
      /* simple rules - last edge is _always_ active and selected */
      if (e_act) {
        BM_edge_select_set(bm, e_act, false);
      }
      BM_edge_select_set(bm, e_dst_last, true);
      BM_select_history_store(bm, e_dst_last);
    }
  }

  EDBM_selectmode_flush(em);
  EDBM_uvselect_clear(em);

  if (op_params->track_active) {
    /* even if this is selected it may not be in the selection list */
    if (op_params->edge_mode == EDGE_MODE_SELECT) {
      if (edgetag_test_cb(e_dst_last, &user_data) == 0) {
        BM_select_history_remove(bm, e_dst_last);
      }
      else {
        BM_select_history_store(bm, e_dst_last);
      }
    }
  }

  EDBMUpdate_Params params{};
  params.calc_looptris = false;
  params.calc_normals = false;
  params.is_destructive = false;
  EDBM_update(static_cast<Mesh *>(obedit->data), &params);

  if (op_params->edge_mode == EDGE_MODE_TAG_SEAM) {
    ED_uvedit_live_unwrap(scene, {obedit});
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Path
 * \{ */

/* callbacks */
static bool facetag_filter_cb(BMFace *f, void * /*user_data_v*/)
{
  return !BM_elem_flag_test(f, BM_ELEM_HIDDEN);
}
// static bool facetag_test_cb(Scene * /*scene*/, BMesh * /*bm*/, BMFace *f)
static bool facetag_test_cb(BMFace *f, void * /*user_data_v*/)
{
  return BM_elem_flag_test_bool(f, BM_ELEM_SELECT);
}
// static void facetag_set_cb(BMesh *bm, Scene * /*scene*/, BMFace *f, const bool val)
static void facetag_set_cb(BMFace *f, bool val, void *user_data_v)
{
  UserData *user_data = static_cast<UserData *>(user_data_v);
  BM_face_select_set(user_data->bm, f, val);
}

static void mouse_mesh_shortest_path_face(Scene * /*scene*/,
                                          Object *obedit,
                                          const PathSelectParams *op_params,
                                          BMFace *f_act,
                                          BMFace *f_dst)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

  int cd_offset = -1;
  switch (op_params->edge_mode) {
    case EDGE_MODE_SELECT:
    case EDGE_MODE_TAG_SEAM:
    case EDGE_MODE_TAG_SHARP:
#ifdef WITH_FREESTYLE
    case EDGE_MODE_TAG_FREESTYLE:
#endif
      break;
    case EDGE_MODE_TAG_CREASE:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "crease_edge");
      break;
    case EDGE_MODE_TAG_BEVEL:
      cd_offset = CustomData_get_offset_named(&bm->edata, CD_PROP_FLOAT, "bevel_weight_edge");
      break;
  }

  UserData user_data = {bm, static_cast<Mesh *>(obedit->data), cd_offset, op_params};
  LinkNode *path = nullptr;
  bool is_path_ordered = false;

  if (f_act) {
    if (op_params->use_fill) {
      path = BM_mesh_calc_path_region_face(
          bm, (BMElem *)f_act, (BMElem *)f_dst, facetag_filter_cb, &user_data);
    }
    else {
      is_path_ordered = true;
      BMCalcPathParams params{};
      params.use_topology_distance = op_params->use_topology_distance;
      params.use_step_face = op_params->use_face_step;
      path = BM_mesh_calc_path_face(bm, f_act, f_dst, &params, facetag_filter_cb, &user_data);
    }

    if (f_act != f_dst) {
      if (path) {
        if (op_params->track_active) {
          BM_select_history_remove(bm, f_act);
        }
      }
    }
  }

  BMFace *f_dst_last = f_dst;

  if (path) {
    /* toggle the flag */
    bool all_set = true;
    LinkNode *node;

    node = path;
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
          WM_operator_properties_checker_interval_test(&op_params->interval_params, depth))
      {
        facetag_set_cb((BMFace *)node->link, !all_set, &user_data);
        if (is_path_ordered) {
          f_dst_last = static_cast<BMFace *>(node->link);
        }
      }
    } while ((void)depth++, (node = node->next));

    BLI_linklist_free(path, nullptr);
  }
  else {
    const bool is_act = !facetag_test_cb(f_dst, &user_data);
    facetag_set_cb(f_dst, is_act, &user_data); /* switch the face option */
  }

  EDBM_selectmode_flush(em);
  EDBM_uvselect_clear(em);

  if (op_params->track_active) {
    /* even if this is selected it may not be in the selection list */
    if (facetag_test_cb(f_dst_last, &user_data) == 0) {
      BM_select_history_remove(bm, f_dst_last);
    }
    else {
      BM_select_history_store(bm, f_dst_last);
    }
    BM_mesh_active_face_set(bm, f_dst_last);

    blender::ed::object::material_active_index_set(obedit, f_dst_last->mat_nr);
    em->mat_nr = f_dst_last->mat_nr;
  }

  EDBMUpdate_Params params{};
  params.calc_looptris = false;
  params.calc_normals = false;
  params.is_destructive = false;
  EDBM_update(static_cast<Mesh *>(obedit->data), &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Operator for vert/edge/face tag
 * \{ */

static bool edbm_shortest_path_pick_ex(Scene *scene,
                                       Object *obedit,
                                       const PathSelectParams *op_params,
                                       BMElem *ele_src,
                                       BMElem *ele_dst)
{
  bool ok = false;

  if (ELEM(nullptr, ele_src, ele_dst) || (ele_src->head.htype != ele_dst->head.htype)) {
    /* pass */
  }
  else if (ele_src->head.htype == BM_VERT) {
    mouse_mesh_shortest_path_vert(scene, obedit, op_params, (BMVert *)ele_src, (BMVert *)ele_dst);
    ok = true;
  }
  else if (ele_src->head.htype == BM_EDGE) {
    mouse_mesh_shortest_path_edge(scene, obedit, op_params, (BMEdge *)ele_src, (BMEdge *)ele_dst);
    ok = true;
  }
  else if (ele_src->head.htype == BM_FACE) {
    mouse_mesh_shortest_path_face(scene, obedit, op_params, (BMFace *)ele_src, (BMFace *)ele_dst);
    ok = true;
  }

  if (ok) {
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }

  return ok;
}

static wmOperatorStatus edbm_shortest_path_pick_exec(bContext *C, wmOperator *op);

static BMElem *edbm_elem_find_nearest(ViewContext *vc, const char htype)
{
  BMEditMesh *em = vc->em;
  float dist = ED_view3d_select_dist_px();

  if ((em->selectmode & SCE_SELECT_VERTEX) && (htype == BM_VERT)) {
    return (BMElem *)EDBM_vert_find_nearest(vc, &dist);
  }
  if ((em->selectmode & SCE_SELECT_EDGE) && (htype == BM_EDGE)) {
    return (BMElem *)EDBM_edge_find_nearest(vc, &dist);
  }
  if ((em->selectmode & SCE_SELECT_FACE) && (htype == BM_FACE)) {
    return (BMElem *)EDBM_face_find_nearest(vc, &dist);
  }

  return nullptr;
}

static BMElem *edbm_elem_active_elem_or_face_get(BMesh *bm)
{
  BMElem *ele = BM_mesh_active_elem_get(bm);

  if ((ele == nullptr) && bm->act_face && BM_elem_flag_test(bm->act_face, BM_ELEM_SELECT)) {
    ele = (BMElem *)bm->act_face;
  }

  return ele;
}

static wmOperatorStatus edbm_shortest_path_pick_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  if (RNA_struct_property_is_set(op->ptr, "index")) {
    return edbm_shortest_path_pick_exec(C, op);
  }

  BMVert *eve = nullptr;
  BMEdge *eed = nullptr;
  BMFace *efa = nullptr;

  bool track_active = true;

  ViewContext vc = em_setup_viewcontext(C);
  copy_v2_v2_int(vc.mval, event->mval);
  BKE_view_layer_synced_ensure(vc.scene, vc.view_layer);
  Base *basact = BKE_view_layer_active_base_get(vc.view_layer);
  BMEditMesh *em = vc.em;

  view3d_operator_needs_gpu(C);

  {
    int base_index = -1;
    Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode(
        vc.scene, vc.view_layer, vc.v3d);
    if (EDBM_unified_findnearest(&vc, bases, &base_index, &eve, &eed, &efa)) {
      basact = bases[base_index];
      ED_view3d_viewcontext_init_object(&vc, basact->object);
      em = vc.em;
    }
  }

  /* If nothing is selected, let's select the picked vertex/edge/face. */
  if ((vc.em->bm->totvertsel == 0) && (eve || eed || efa)) {
    /* TODO(dfelinto): right now we try to find the closest element twice.
     * The ideal is to refactor EDBM_select_pick so it doesn't
     * have to pick the nearest vert/edge/face again. */
    const SelectPick_Params params = {
        /*sel_op*/ SEL_OP_ADD,
    };
    EDBM_select_pick(C, event->mval, params);
    return OPERATOR_FINISHED;
  }

  PathSelectParams op_params;
  path_select_params_from_op(op, vc.scene->toolsettings, &op_params);

  BMElem *ele_src, *ele_dst;
  if (!(ele_src = edbm_elem_active_elem_or_face_get(em->bm)) ||
      !(ele_dst = edbm_elem_find_nearest(&vc, ele_src->head.htype)))
  {
    /* special case, toggle edge tags even when we don't have a path */
    if (((em->selectmode & SCE_SELECT_EDGE) && (op_params.edge_mode != EDGE_MODE_SELECT)) &&
        /* check if we only have a destination edge */
        ((ele_src == nullptr) && (ele_dst = edbm_elem_find_nearest(&vc, BM_EDGE))))
    {
      ele_src = ele_dst;
      track_active = false;
    }
    else {
      return OPERATOR_PASS_THROUGH;
    }
  }

  op_params.track_active = track_active;

  if (!edbm_shortest_path_pick_ex(vc.scene, vc.obedit, &op_params, ele_src, ele_dst)) {
    return OPERATOR_PASS_THROUGH;
  }

  BKE_view_layer_synced_ensure(vc.scene, vc.view_layer);
  if (BKE_view_layer_active_base_get(vc.view_layer) != basact) {
    blender::ed::object::base_activate(C, basact);
  }

  /* to support redo */
  BM_mesh_elem_index_ensure(em->bm, ele_dst->head.htype);
  int index = EDBM_elem_to_index_any(em, ele_dst);

  RNA_int_set(op->ptr, "index", index);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus edbm_shortest_path_pick_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;

  const int index = RNA_int_get(op->ptr, "index");
  if (index < 0 || index >= (bm->totvert + bm->totedge + bm->totface)) {
    return OPERATOR_CANCELLED;
  }

  BMElem *ele_src, *ele_dst;
  if (!(ele_src = edbm_elem_active_elem_or_face_get(em->bm)) ||
      !(ele_dst = EDBM_elem_from_index_any(em, index)))
  {
    return OPERATOR_CANCELLED;
  }

  PathSelectParams op_params;
  path_select_params_from_op(op, scene->toolsettings, &op_params);
  op_params.track_active = true;

  if (!edbm_shortest_path_pick_ex(scene, obedit, &op_params, ele_src, ele_dst)) {
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_shortest_path_pick(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Pick Shortest Path";
  ot->idname = "MESH_OT_shortest_path_pick";
  ot->description = "Select shortest path between two selections";

  /* API callbacks. */
  ot->invoke = edbm_shortest_path_pick_invoke;
  ot->exec = edbm_shortest_path_pick_exec;
  ot->poll = ED_operator_editmesh_region_view3d;
  ot->poll_property = path_select_poll_property;

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

static wmOperatorStatus edbm_shortest_path_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  bool found_valid_elements = false;

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    BMIter iter;
    BMEditSelection *ese_src, *ese_dst;
    BMElem *ele_src = nullptr, *ele_dst = nullptr, *ele;

    if ((em->bm->totvertsel == 0) && (em->bm->totedgesel == 0) && (em->bm->totfacesel == 0)) {
      continue;
    }

    /* first try to find vertices in edit selection */
    ese_src = static_cast<BMEditSelection *>(bm->selected.last);
    if (ese_src && (ese_dst = ese_src->prev) && (ese_src->htype == ese_dst->htype)) {
      ele_src = ese_src->ele;
      ele_dst = ese_dst->ele;
    }
    else {
      /* if selection history isn't available, find two selected elements */
      ele_src = ele_dst = nullptr;
      if ((em->selectmode & SCE_SELECT_VERTEX) && (bm->totvertsel >= 2)) {
        BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
          if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
            if (ele_src == nullptr) {
              ele_src = ele;
            }
            else if (ele_dst == nullptr) {
              ele_dst = ele;
            }
            else {
              break;
            }
          }
        }
      }

      if ((ele_dst == nullptr) && (em->selectmode & SCE_SELECT_EDGE) && (bm->totedgesel >= 2)) {
        ele_src = nullptr;
        BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
          if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
            if (ele_src == nullptr) {
              ele_src = ele;
            }
            else if (ele_dst == nullptr) {
              ele_dst = ele;
            }
            else {
              break;
            }
          }
        }
      }

      if ((ele_dst == nullptr) && (em->selectmode & SCE_SELECT_FACE) && (bm->totfacesel >= 2)) {
        ele_src = nullptr;
        BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
          if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
            if (ele_src == nullptr) {
              ele_src = ele;
            }
            else if (ele_dst == nullptr) {
              ele_dst = ele;
            }
            else {
              break;
            }
          }
        }
      }
    }

    if (ele_src && ele_dst) {
      PathSelectParams op_params;
      path_select_params_from_op(op, scene->toolsettings, &op_params);

      edbm_shortest_path_pick_ex(scene, obedit, &op_params, ele_src, ele_dst);

      found_valid_elements = true;
    }
  }

  if (!found_valid_elements) {
    BKE_report(
        op->reports, RPT_WARNING, "Path selection requires two matching elements to be selected");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_shortest_path_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Shortest Path";
  ot->idname = "MESH_OT_shortest_path_select";
  ot->description = "Selected shortest path between two vertices/edges/faces";

  /* API callbacks. */
  ot->exec = edbm_shortest_path_select_exec;
  ot->poll = ED_operator_editmesh;
  ot->poll_property = path_select_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  path_select_properties(ot);
}

/** \} */
