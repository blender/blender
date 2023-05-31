/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup eduv
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_kdtree.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh_mapping.h"
#include "BKE_node.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

/* -------------------------------------------------------------------- */
/** \name State Testing
 * \{ */

bool ED_uvedit_test(Object *obedit)
{
  BMEditMesh *em;
  int ret;

  if (!obedit) {
    return 0;
  }

  if (obedit->type != OB_MESH) {
    return 0;
  }

  em = BKE_editmesh_from_object(obedit);
  ret = EDBM_uv_check(em);

  return ret;
}

static int UNUSED_FUNCTION(ED_operator_uvmap_mesh)(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob && ob->type == OB_MESH) {
    Mesh *me = static_cast<Mesh *>(ob->data);

    if (CustomData_get_layer(&me->ldata, CD_PROP_FLOAT2) != nullptr) {
      return 1;
    }
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Active Image
 * \{ */

static bool is_image_texture_node(bNode *node)
{
  return ELEM(node->type, SH_NODE_TEX_IMAGE, SH_NODE_TEX_ENVIRONMENT);
}

bool ED_object_get_active_image(Object *ob,
                                int mat_nr,
                                Image **r_ima,
                                ImageUser **r_iuser,
                                const bNode **r_node,
                                const bNodeTree **r_ntree)
{
  Material *ma = DEG_is_evaluated_object(ob) ? BKE_object_material_get_eval(ob, mat_nr) :
                                               BKE_object_material_get(ob, mat_nr);
  bNodeTree *ntree = (ma && ma->use_nodes) ? ma->nodetree : nullptr;
  bNode *node = (ntree) ? nodeGetActiveTexture(ntree) : nullptr;

  if (node && is_image_texture_node(node)) {
    if (r_ima) {
      *r_ima = (Image *)node->id;
    }
    if (r_iuser) {
      if (node->type == SH_NODE_TEX_IMAGE) {
        *r_iuser = &((NodeTexImage *)node->storage)->iuser;
      }
      else if (node->type == SH_NODE_TEX_ENVIRONMENT) {
        *r_iuser = &((NodeTexEnvironment *)node->storage)->iuser;
      }
      else {
        *r_iuser = nullptr;
      }
    }
    if (r_node) {
      *r_node = node;
    }
    if (r_ntree) {
      *r_ntree = ntree;
    }
    return true;
  }

  if (r_ima) {
    *r_ima = nullptr;
  }
  if (r_iuser) {
    *r_iuser = nullptr;
  }
  if (r_node) {
    *r_node = node;
  }
  if (r_ntree) {
    *r_ntree = ntree;
  }

  return false;
}

void ED_object_assign_active_image(Main *bmain, Object *ob, int mat_nr, Image *ima)
{
  Material *ma = BKE_object_material_get(ob, mat_nr);
  bNode *node = (ma && ma->use_nodes) ? nodeGetActiveTexture(ma->nodetree) : nullptr;

  if (node && is_image_texture_node(node)) {
    node->id = &ima->id;
    ED_node_tree_propagate_change(nullptr, bmain, ma->nodetree);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Live Unwrap Utilities
 * \{ */

void uvedit_live_unwrap_update(SpaceImage *sima, Scene *scene, Object *obedit)
{
  if (sima && (sima->flag & SI_LIVE_UNWRAP)) {
    ED_uvedit_live_unwrap_begin(scene, obedit);
    ED_uvedit_live_unwrap_re_solve();
    ED_uvedit_live_unwrap_end(0);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometric Utilities
 * \{ */

bool ED_uvedit_minmax_multi(
    const Scene *scene, Object **objects_edit, uint objects_len, float r_min[2], float r_max[2])
{
  bool changed = false;
  INIT_MINMAX2(r_min, r_max);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects_edit[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    float *luv;

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, offsets)) {
          luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          minmax_v2v2_v2(r_min, r_max, luv);
          changed = true;
        }
      }
    }
  }
  return changed;
}

bool ED_uvedit_minmax(const Scene *scene, Object *obedit, float r_min[2], float r_max[2])
{
  return ED_uvedit_minmax_multi(scene, &obedit, 1, r_min, r_max);
}

void ED_uvedit_select_all(BMesh *bm)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, true);
      BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, true);
    }
  }
}

static bool ED_uvedit_median_multi(const Scene *scene,
                                   Object **objects_edit,
                                   uint objects_len,
                                   float co[2])
{
  uint sel = 0;
  zero_v2(co);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects_edit[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    float *luv;

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        if (uvedit_uv_select_test(scene, l, offsets)) {
          add_v2_v2(co, luv);
          sel++;
        }
      }
    }
  }

  mul_v2_fl(co, 1.0f / float(sel));

  return (sel != 0);
}

bool ED_uvedit_center_multi(
    const Scene *scene, Object **objects_edit, uint objects_len, float cent[2], char mode)
{
  bool changed = false;

  if (mode == V3D_AROUND_CENTER_BOUNDS) { /* bounding box */
    float min[2], max[2];
    if (ED_uvedit_minmax_multi(scene, objects_edit, objects_len, min, max)) {
      mid_v2_v2v2(cent, min, max);
      changed = true;
    }
  }
  else {
    if (ED_uvedit_median_multi(scene, objects_edit, objects_len, cent)) {
      changed = true;
    }
  }

  return changed;
}

bool ED_uvedit_center_from_pivot_ex(SpaceImage *sima,
                                    Scene *scene,
                                    ViewLayer *view_layer,
                                    float r_center[2],
                                    char mode,
                                    bool *r_has_select)
{
  bool changed = false;
  switch (mode) {
    case V3D_AROUND_CURSOR: {
      copy_v2_v2(r_center, sima->cursor);
      changed = true;
      if (r_has_select != nullptr) {
        uint objects_len = 0;
        Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
            scene, view_layer, ((View3D *)nullptr), &objects_len);
        *r_has_select = uvedit_select_is_any_selected_multi(scene, objects, objects_len);
        MEM_freeN(objects);
      }
      break;
    }
    default: {
      uint objects_len = 0;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
          scene, view_layer, ((View3D *)nullptr), &objects_len);
      changed = ED_uvedit_center_multi(scene, objects, objects_len, r_center, mode);
      MEM_freeN(objects);
      if (r_has_select != nullptr) {
        *r_has_select = changed;
      }
      break;
    }
  }
  return changed;
}

bool ED_uvedit_center_from_pivot(
    SpaceImage *sima, Scene *scene, ViewLayer *view_layer, float r_center[2], char mode)
{
  return ED_uvedit_center_from_pivot_ex(sima, scene, view_layer, r_center, mode, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld Align Operator
 * \{ */

typedef enum eUVWeldAlign {
  UV_STRAIGHTEN,
  UV_STRAIGHTEN_X,
  UV_STRAIGHTEN_Y,
  UV_ALIGN_AUTO,
  UV_ALIGN_X,
  UV_ALIGN_Y,
  UV_WELD,
} eUVWeldAlign;

static bool uvedit_uv_align_weld(Scene *scene,
                                 BMesh *bm,
                                 const eUVWeldAlign tool,
                                 const float cent[2])
{
  bool changed = false;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

  BMIter iter;
  BMFace *efa;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    BMIter liter;
    BMLoop *l;
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (!uvedit_uv_select_test(scene, l, offsets)) {
        continue;
      }
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      if (ELEM(tool, UV_ALIGN_X, UV_WELD)) {
        if (luv[0] != cent[0]) {
          luv[0] = cent[0];
          changed = true;
        }
      }
      if (ELEM(tool, UV_ALIGN_Y, UV_WELD)) {
        if (luv[1] != cent[1]) {
          luv[1] = cent[1];
          changed = true;
        }
      }
    }
  }
  return changed;
}

/** Bitwise-or together, then choose loop with highest value. */
typedef enum eUVEndPointPrecedence {
  UVEP_INVALID = 0,
  UVEP_SELECTED = (1 << 0),
  UVEP_PINNED = (1 << 1), /* i.e. Pinned verts are preferred to selected. */
} eUVEndPointPrecedence;
ENUM_OPERATORS(eUVEndPointPrecedence, UVEP_PINNED);

static eUVEndPointPrecedence uvedit_line_update_get_precedence(const bool pinned)
{
  eUVEndPointPrecedence precedence = UVEP_SELECTED;
  if (pinned) {
    precedence |= UVEP_PINNED;
  }
  return precedence;
}

/**
 * Helper to find two endpoints (`a` and `b`) which have higher precedence, and are far apart.
 * Note that is only a heuristic and won't always find the best two endpoints.
 */
static bool uvedit_line_update_endpoint(const float *luv,
                                        const bool pinned,
                                        float uv_a[2],
                                        eUVEndPointPrecedence *prec_a,
                                        float uv_b[2],
                                        eUVEndPointPrecedence *prec_b)
{
  eUVEndPointPrecedence flags = uvedit_line_update_get_precedence(pinned);

  float len_sq_a = len_squared_v2v2(uv_a, luv);
  float len_sq_b = len_squared_v2v2(uv_b, luv);

  /* Caching the value of `len_sq_ab` is unlikely to be faster than recalculating.
   * Profile before optimizing. */
  float len_sq_ab = len_squared_v2v2(uv_a, uv_b);

  if ((*prec_a < flags && 0.0f < len_sq_b) || (*prec_a == flags && len_sq_ab < len_sq_b)) {
    *prec_a = flags;
    copy_v2_v2(uv_a, luv);
    return true;
  }

  if ((*prec_b < flags && 0.0f < len_sq_a) || (*prec_b == flags && len_sq_ab < len_sq_a)) {
    *prec_b = flags;
    copy_v2_v2(uv_b, luv);
    return true;
  }

  return false;
}

/**
 * Find two end extreme points to specify a line, then straighten `len` elements
 * by moving UVs on the X-axis, Y-axis, or the closest point on the line segment.
 */
static bool uvedit_uv_straighten_elements(const UvElement *element,
                                          const int len,
                                          const BMUVOffsets offsets,
                                          const eUVWeldAlign tool)
{
  float uv_start[2];
  float uv_end[2];
  eUVEndPointPrecedence prec_start = UVEP_INVALID;
  eUVEndPointPrecedence prec_end = UVEP_INVALID;

  /* Find start and end of line. */
  for (int i = 0; i < 10; i++) { /* Heuristic to prevent infinite loop. */
    bool update = false;
    for (int j = 0; j < len; j++) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(element[j].l, offsets.uv);
      bool pinned = BM_ELEM_CD_GET_BOOL(element[j].l, offsets.pin);
      update |= uvedit_line_update_endpoint(luv, pinned, uv_start, &prec_start, uv_end, &prec_end);
    }
    if (!update) {
      break;
    }
  }

  if (prec_start == UVEP_INVALID || prec_end == UVEP_INVALID) {
    return false; /* Unable to find two endpoints. */
  }

  float a = 0.0f; /* Similar to "slope". */
  eUVWeldAlign tool_local = tool;

  if (tool_local == UV_STRAIGHTEN_X) {
    if (uv_start[1] == uv_end[1]) {
      /* Caution, different behavior outside line segment. */
      tool_local = UV_STRAIGHTEN;
    }
    else {
      a = (uv_end[0] - uv_start[0]) / (uv_end[1] - uv_start[1]);
    }
  }
  else if (tool_local == UV_STRAIGHTEN_Y) {
    if (uv_start[0] == uv_end[0]) {
      /* Caution, different behavior outside line segment. */
      tool_local = UV_STRAIGHTEN;
    }
    else {
      a = (uv_end[1] - uv_start[1]) / (uv_end[0] - uv_start[0]);
    }
  }

  bool changed = false;
  for (int j = 0; j < len; j++) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(element[j].l, offsets.uv);
    /* Projection of point (x, y) over line (x1, y1, x2, y2) along X axis:
     * new_y = (y2 - y1) / (x2 - x1) * (x - x1) + y1
     * Maybe this should be a BLI func? Or is it already existing?
     * Could use interp_v2_v2v2, but not sure it's worth it here. */
    if (tool_local == UV_STRAIGHTEN_X) {
      luv[0] = a * (luv[1] - uv_start[1]) + uv_start[0];
    }
    else if (tool_local == UV_STRAIGHTEN_Y) {
      luv[1] = a * (luv[0] - uv_start[0]) + uv_start[1];
    }
    else {
      closest_to_line_segment_v2(luv, luv, uv_start, uv_end);
    }
    changed = true; /* TODO: Did the UV actually move? */
  }
  return changed;
}

/**
 * Group selected UVs into islands, then apply uvedit_uv_straighten_elements to each island.
 */
static bool uvedit_uv_straighten(Scene *scene, BMesh *bm, eUVWeldAlign tool)
{
  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
  if (offsets.uv == -1) {
    return false;
  }

  UvElementMap *element_map = BM_uv_element_map_create(bm, scene, true, false, true, true);
  if (element_map == nullptr) {
    return false;
  }

  bool changed = false;
  for (int i = 0; i < element_map->total_islands; i++) {
    changed |= uvedit_uv_straighten_elements(element_map->storage + element_map->island_indices[i],
                                             element_map->island_total_uvs[i],
                                             offsets,
                                             tool);
  }

  BM_uv_element_map_free(element_map);
  return changed;
}

static void uv_weld_align(bContext *C, eUVWeldAlign tool)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  const ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;
  float cent[2], min[2], max[2];

  INIT_MINMAX2(min, max);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  if (tool == UV_ALIGN_AUTO) {
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *obedit = objects[ob_index];
      BMEditMesh *em = BKE_editmesh_from_object(obedit);

      if (synced_selection && (em->bm->totvertsel == 0)) {
        continue;
      }

      const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

      BMIter iter, liter;
      BMFace *efa;
      BMLoop *l;

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_uv_select_test(scene, l, offsets)) {
            float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
            minmax_v2v2_v2(min, max, luv);
          }
        }
      }
    }
    tool = (max[0] - min[0] >= max[1] - min[1]) ? UV_ALIGN_Y : UV_ALIGN_X;
  }

  ED_uvedit_center_multi(scene, objects, objects_len, cent, 0);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    if (ELEM(tool, UV_ALIGN_AUTO, UV_ALIGN_X, UV_ALIGN_Y, UV_WELD)) {
      changed |= uvedit_uv_align_weld(scene, em->bm, tool, cent);
    }

    if (ELEM(tool, UV_STRAIGHTEN, UV_STRAIGHTEN_X, UV_STRAIGHTEN_Y)) {
      changed |= uvedit_uv_straighten(scene, em->bm, tool);
    }

    if (changed) {
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }

  MEM_freeN(objects);
}

static int uv_align_exec(bContext *C, wmOperator *op)
{
  uv_weld_align(C, eUVWeldAlign(RNA_enum_get(op->ptr, "axis")));

  return OPERATOR_FINISHED;
}

static void UV_OT_align(wmOperatorType *ot)
{
  static const EnumPropertyItem axis_items[] = {
      {UV_STRAIGHTEN,
       "ALIGN_S",
       0,
       "Straighten",
       "Align UVs along the line defined by the endpoints"},
      {UV_STRAIGHTEN_X,
       "ALIGN_T",
       0,
       "Straighten X",
       "Align UVs along the line defined by the endpoints along the X axis"},
      {UV_STRAIGHTEN_Y,
       "ALIGN_U",
       0,
       "Straighten Y",
       "Align UVs along the line defined by the endpoints along the Y axis"},
      {UV_ALIGN_AUTO,
       "ALIGN_AUTO",
       0,
       "Align Auto",
       "Automatically choose the axis on which there is most alignment already"},
      {UV_ALIGN_X, "ALIGN_X", 0, "Align X", "Align UVs on X axis"},
      {UV_ALIGN_Y, "ALIGN_Y", 0, "Align Y", "Align UVs on Y axis"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Align";
  ot->description = "Align selected UV vertices to an axis";
  ot->idname = "UV_OT_align";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_align_exec;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_enum(
      ot->srna, "axis", axis_items, UV_ALIGN_AUTO, "Axis", "Axis to align UV locations on");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Doubles Operator
 * \{ */

static int uv_remove_doubles_to_selected(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  const ToolSettings *ts = scene->toolsettings;

  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  bool *changed = static_cast<bool *>(MEM_callocN(sizeof(bool) * objects_len, __func__));

  /* Maximum index of an objects[i]'s UVs in UV_arr.
   * It helps find which UV in *mloopuv_arr belongs to which object. */
  uint *ob_mloopuv_max_idx = static_cast<uint *>(
      MEM_callocN(sizeof(uint) * objects_len, __func__));

  /* Calculate max possible number of kdtree nodes. */
  int uv_maxlen = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    uv_maxlen += em->bm->totloop;
  }

  KDTree_2d *tree = BLI_kdtree_2d_new(uv_maxlen);

  blender::Vector<int> duplicates;
  blender::Vector<float *> mloopuv_arr;

  int mloopuv_count = 0; /* Also used for *duplicates count. */

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    BMIter iter, liter;
    BMFace *efa;
    BMLoop *l;
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, offsets)) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          BLI_kdtree_2d_insert(tree, mloopuv_count, luv);
          duplicates.append(-1);
          mloopuv_arr.append(luv);
          mloopuv_count++;
        }
      }
    }

    ob_mloopuv_max_idx[ob_index] = mloopuv_count - 1;
  }

  BLI_kdtree_2d_balance(tree);
  int found_duplicates = BLI_kdtree_2d_calc_duplicates_fast(
      tree, threshold, false, duplicates.data());

  if (found_duplicates > 0) {
    /* Calculate average uv for duplicates. */
    int *uv_duplicate_count = static_cast<int *>(
        MEM_callocN(sizeof(int) * mloopuv_count, __func__));
    for (int i = 0; i < mloopuv_count; i++) {
      if (duplicates[i] == -1) { /* If doesn't reference another */
        uv_duplicate_count[i]++; /* self */
        continue;
      }

      if (duplicates[i] != i) {
        /* If not self then accumulate uv for averaging.
         * Self uv is already present in accumulator */
        add_v2_v2(mloopuv_arr[duplicates[i]], mloopuv_arr[i]);
      }
      uv_duplicate_count[duplicates[i]]++;
    }

    for (int i = 0; i < mloopuv_count; i++) {
      if (uv_duplicate_count[i] < 2) {
        continue;
      }

      mul_v2_fl(mloopuv_arr[i], 1.0f / float(uv_duplicate_count[i]));
    }
    MEM_freeN(uv_duplicate_count);

    /* Update duplicated uvs. */
    uint ob_index = 0;
    for (int i = 0; i < mloopuv_count; i++) {
      /* Make sure we know which object owns the mloopuv at this index.
       * Remember that in some cases the object will have no loop uv,
       * thus we need the while loop, and not simply an if check. */
      while (ob_mloopuv_max_idx[ob_index] < i) {
        ob_index++;
      }

      if (duplicates[i] == -1) {
        continue;
      }

      copy_v2_v2(mloopuv_arr[i], mloopuv_arr[duplicates[i]]);
      changed[ob_index] = true;
    }

    for (ob_index = 0; ob_index < objects_len; ob_index++) {
      if (changed[ob_index]) {
        Object *obedit = objects[ob_index];
        uvedit_live_unwrap_update(sima, scene, obedit);
        DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      }
    }
  }

  BLI_kdtree_2d_free(tree);
  MEM_freeN(changed);
  MEM_freeN(objects);
  MEM_freeN(ob_mloopuv_max_idx);

  return OPERATOR_FINISHED;
}

static int uv_remove_doubles_to_unselected(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  const ToolSettings *ts = scene->toolsettings;

  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  /* Calculate max possible number of kdtree nodes. */
  int uv_maxlen = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    uv_maxlen += em->bm->totloop;
  }

  KDTree_2d *tree = BLI_kdtree_2d_new(uv_maxlen);

  blender::Vector<float *> mloopuv_arr;

  int mloopuv_count = 0;

  /* Add visible non-selected uvs to tree */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    BMIter iter, liter;
    BMFace *efa;
    BMLoop *l;
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == em->bm->totvert)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (!uvedit_uv_select_test(scene, l, offsets)) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          BLI_kdtree_2d_insert(tree, mloopuv_count, luv);
          mloopuv_arr.append(luv);
          mloopuv_count++;
        }
      }
    }
  }

  BLI_kdtree_2d_balance(tree);

  /* For each selected uv, find duplicate non selected uv. */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    BMIter iter, liter;
    BMFace *efa;
    BMLoop *l;
    bool changed = false;
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, offsets)) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          KDTreeNearest_2d nearest;
          const int i = BLI_kdtree_2d_find_nearest(tree, luv, &nearest);

          if (i != -1 && nearest.dist < threshold) {
            copy_v2_v2(luv, mloopuv_arr[i]);
            changed = true;
          }
        }
      }
    }

    if (changed) {
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }

  BLI_kdtree_2d_free(tree);
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_remove_doubles_exec(bContext *C, wmOperator *op)
{
  if (RNA_boolean_get(op->ptr, "use_unselected")) {
    return uv_remove_doubles_to_unselected(C, op);
  }
  return uv_remove_doubles_to_selected(C, op);
}

static void UV_OT_remove_doubles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Merge UVs by Distance";
  ot->description =
      "Selected UV vertices that are within a radius of each other are welded together";
  ot->idname = "UV_OT_remove_doubles";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_remove_doubles_exec;
  ot->poll = ED_operator_uvedit;

  RNA_def_float(ot->srna,
                "threshold",
                0.02f,
                0.0f,
                10.0f,
                "Merge Distance",
                "Maximum distance between welded vertices",
                0.0f,
                1.0f);
  RNA_def_boolean(
      ot->srna, "use_unselected", 0, "Unselected", "Merge selected to other unselected vertices");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weld Near Operator
 * \{ */

static int uv_weld_exec(bContext *C, wmOperator * /*op*/)
{
  uv_weld_align(C, UV_WELD);

  return OPERATOR_FINISHED;
}

static void UV_OT_weld(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weld";
  ot->description = "Weld selected UV vertices together";
  ot->idname = "UV_OT_weld";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_weld_exec;
  ot->poll = ED_operator_uvedit;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Cursor Operator
 * \{ */

static void uv_snap_to_pixel(float uvco[2], float w, float h)
{
  uvco[0] = roundf(uvco[0] * w) / w;
  uvco[1] = roundf(uvco[1] * h) / h;
}

static void uv_snap_cursor_to_pixels(SpaceImage *sima)
{
  int width = 0, height = 0;

  ED_space_image_get_size(sima, &width, &height);
  uv_snap_to_pixel(sima->cursor, width, height);
}

static bool uv_snap_cursor_to_selection(Scene *scene,
                                        Object **objects_edit,
                                        uint objects_len,
                                        SpaceImage *sima)
{
  return ED_uvedit_center_multi(scene, objects_edit, objects_len, sima->cursor, sima->around);
}

static void uv_snap_cursor_to_origin(float uvco[2])
{
  uvco[0] = 0;
  uvco[1] = 0;
}

static int uv_snap_cursor_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  bool changed = false;

  switch (RNA_enum_get(op->ptr, "target")) {
    case 0:
      uv_snap_cursor_to_pixels(sima);
      changed = true;
      break;
    case 1: {
      Scene *scene = CTX_data_scene(C);
      ViewLayer *view_layer = CTX_data_view_layer(C);

      uint objects_len = 0;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
          scene, view_layer, ((View3D *)nullptr), &objects_len);
      changed = uv_snap_cursor_to_selection(scene, objects, objects_len, sima);
      MEM_freeN(objects);
      break;
    }
    case 2:
      uv_snap_cursor_to_origin(sima->cursor);
      changed = true;
      break;
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, sima);

  return OPERATOR_FINISHED;
}

static void UV_OT_snap_cursor(wmOperatorType *ot)
{
  static const EnumPropertyItem target_items[] = {
      {0, "PIXELS", 0, "Pixels", ""},
      {1, "SELECTED", 0, "Selected", ""},
      {2, "ORIGIN", 0, "Origin", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Snap Cursor";
  ot->description = "Snap cursor to target type";
  ot->idname = "UV_OT_snap_cursor";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_snap_cursor_exec;
  ot->poll = ED_operator_uvedit_space_image; /* requires space image */

  /* properties */
  RNA_def_enum(
      ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UVs to");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Selection Operator
 * \{ */

static bool uv_snap_uvs_to_cursor(Scene *scene, Object *obedit, const float cursor[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;
  bool changed = false;

  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, offsets)) {
        luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        copy_v2_v2(luv, cursor);
        changed = true;
      }
    }
  }

  return changed;
}

static bool uv_snap_uvs_offset(Scene *scene, Object *obedit, const float offset[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;
  bool changed = false;

  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, offsets)) {
        luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        add_v2_v2(luv, offset);
        changed = true;
      }
    }
  }

  return changed;
}

static bool uv_snap_uvs_to_adjacent_unselected(Scene *scene, Object *obedit)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  BMFace *f;
  BMLoop *l, *lsub;
  BMIter iter, liter, lsubiter;
  float *luv;
  bool changed = false;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  /* Index every vert that has a selected UV using it, but only once so as to
   * get unique indices and to count how much to `malloc`. */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (uvedit_face_visible_test(scene, f)) {
      BM_elem_flag_enable(f, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        BM_elem_flag_set(l, BM_ELEM_TAG, uvedit_uv_select_test(scene, l, offsets));
      }
    }
    else {
      BM_elem_flag_disable(f, BM_ELEM_TAG);
    }
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_TAG)) { /* Face: visible. */
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) { /* Loop: selected. */
          float uv[2] = {0.0f, 0.0f};
          int uv_tot = 0;

          BM_ITER_ELEM (lsub, &lsubiter, l->v, BM_LOOPS_OF_VERT) {
            if (BM_elem_flag_test(lsub->f, BM_ELEM_TAG) && /* Face: visible. */
                !BM_elem_flag_test(lsub, BM_ELEM_TAG))     /* Loop: unselected. */
            {
              luv = BM_ELEM_CD_GET_FLOAT_P(lsub, offsets.uv);
              add_v2_v2(uv, luv);
              uv_tot++;
            }
          }

          if (uv_tot) {
            luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
            mul_v2_v2fl(luv, uv, 1.0f / float(uv_tot));
            changed = true;
          }
        }
      }
    }
  }

  return changed;
}

static bool uv_snap_uvs_to_pixels(SpaceImage *sima, Scene *scene, Object *obedit)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;
  int width = 0, height = 0;
  float w, h;
  bool changed = false;

  const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

  ED_space_image_get_size(sima, &width, &height);
  w = float(width);
  h = float(height);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, offsets)) {
        luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        uv_snap_to_pixel(luv, w, h);
      }
    }

    changed = true;
  }

  return changed;
}

static int uv_snap_selection_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  const ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;
  const int target = RNA_enum_get(op->ptr, "target");
  float offset[2] = {0};

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  if (target == 2) {
    float center[2];
    if (!ED_uvedit_center_multi(scene, objects, objects_len, center, sima->around)) {
      MEM_freeN(objects);
      return OPERATOR_CANCELLED;
    }
    sub_v2_v2v2(offset, sima->cursor, center);
  }

  bool changed_multi = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    bool changed = false;
    switch (target) {
      case 0:
        changed = uv_snap_uvs_to_pixels(sima, scene, obedit);
        break;
      case 1:
        changed = uv_snap_uvs_to_cursor(scene, obedit, sima->cursor);
        break;
      case 2:
        changed = uv_snap_uvs_offset(scene, obedit, offset);
        break;
      case 3:
        changed = uv_snap_uvs_to_adjacent_unselected(scene, obedit);
        break;
    }

    if (changed) {
      changed_multi = true;
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UV_OT_snap_selected(wmOperatorType *ot)
{
  static const EnumPropertyItem target_items[] = {
      {0, "PIXELS", 0, "Pixels", ""},
      {1, "CURSOR", 0, "Cursor", ""},
      {2, "CURSOR_OFFSET", 0, "Cursor (Offset)", ""},
      {3, "ADJACENT_UNSELECTED", 0, "Adjacent Unselected", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Snap Selection";
  ot->description = "Snap selected UV vertices to target type";
  ot->idname = "UV_OT_snap_selected";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_snap_selection_exec;
  ot->poll = ED_operator_uvedit_space_image;

  /* properties */
  RNA_def_enum(
      ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UVs to");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pin UVs Operator
 * \{ */

static int uv_pin_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const ToolSettings *ts = scene->toolsettings;
  const bool clear = RNA_boolean_get(op->ptr, "clear");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;
    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_pin_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

        if (uvedit_uv_select_test(scene, l, offsets)) {
          changed = true;
          BM_ELEM_CD_SET_BOOL(l, offsets.pin, !clear);
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_COPY_ON_WRITE);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static void UV_OT_pin(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pin";
  ot->description =
      "Set/clear selected UV vertices as anchored between multiple unwrap operations";
  ot->idname = "UV_OT_pin";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_pin_exec;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_boolean(
      ot->srna, "clear", 0, "Clear", "Clear pinning for the selection instead of setting it");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

/* check if we are selected or unselected based on 'bool_test' arg,
 * needed for select swap support */
#define UV_VERT_SEL_TEST(l, bool_test) (BM_ELEM_CD_GET_BOOL(l, offsets.select_vert) == bool_test)

#define UV_EDGE_SEL_TEST(l, bool_test) (BM_ELEM_CD_GET_BOOL(l, offsets.select_edge) == bool_test)

/* is every UV vert selected or unselected depending on bool_test */
static bool bm_face_is_all_uv_sel(BMFace *f, bool select_test, const BMUVOffsets offsets)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (!UV_EDGE_SEL_TEST(l_iter, select_test)) {
      return false;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return true;
}

static int uv_hide_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;
  const bool swap = RNA_boolean_get(op->ptr, "unselected");
  const bool use_face_center = (ts->uv_selectmode == UV_SELECT_FACE);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;

    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (EDBM_mesh_hide(em, swap)) {
        Mesh *me = static_cast<Mesh *>(ob->data);
        EDBMUpdate_Params params = {0};
        params.calc_looptri = true;
        params.calc_normals = false;
        params.is_destructive = false;
        EDBM_update(me, &params);
      }
      continue;
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      int hide = 0;

      if (!uvedit_face_visible_test(scene, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

        if (UV_VERT_SEL_TEST(l, !swap) || UV_EDGE_SEL_TEST(l, !swap)) {
          hide = 1;
          break;
        }
      }

      if (hide) {
        if (use_face_center) {
          if (em->selectmode == SCE_SELECT_FACE) {
            /* Deselect BMesh face if UV face is (de)selected depending on #swap. */
            if (bm_face_is_all_uv_sel(efa, !swap, offsets)) {
              BM_face_select_set(em->bm, efa, false);
            }
            uvedit_face_select_disable(scene, em->bm, efa, offsets);
          }
          else {
            if (bm_face_is_all_uv_sel(efa, true, offsets) == !swap) {
              BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
                /* For both cases rely on edge sel tests, since all vert sel tests are invalid in
                 * case of sticky selections. */
                if (UV_EDGE_SEL_TEST(l, !swap) && (em->selectmode == SCE_SELECT_EDGE)) {
                  BM_edge_select_set(em->bm, l->e, false);
                }
                else if (UV_EDGE_SEL_TEST(l, !swap) && (em->selectmode == SCE_SELECT_VERTEX)) {
                  BM_vert_select_set(em->bm, l->v, false);
                }
              }
            }
            if (!swap) {
              uvedit_face_select_disable(scene, em->bm, efa, offsets);
            }
          }
        }
        else if (em->selectmode == SCE_SELECT_FACE) {
          /* Deselect BMesh face depending on the type of UV selectmode and the type of UV element
           * being considered. */
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            if (UV_EDGE_SEL_TEST(l, !swap) && (ts->uv_selectmode == UV_SELECT_EDGE)) {
              BM_face_select_set(em->bm, efa, false);
              break;
            }
            if (UV_VERT_SEL_TEST(l, !swap) && (ts->uv_selectmode == UV_SELECT_VERTEX)) {
              BM_face_select_set(em->bm, efa, false);
              break;
            }
            if (ts->uv_selectmode == UV_SELECT_ISLAND) {
              BM_face_select_set(em->bm, efa, false);
              break;
            }
          }
          uvedit_face_select_disable(scene, em->bm, efa, offsets);
        }
        else {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            if (UV_EDGE_SEL_TEST(l, !swap) && (ts->uv_selectmode == UV_SELECT_EDGE)) {
              if (em->selectmode == SCE_SELECT_EDGE) {
                BM_edge_select_set(em->bm, l->e, false);
              }
              else {
                BM_vert_select_set(em->bm, l->v, false);
                BM_vert_select_set(em->bm, l->next->v, false);
              }
            }
            else if (UV_VERT_SEL_TEST(l, !swap) && (ts->uv_selectmode != UV_SELECT_EDGE)) {
              if (em->selectmode == SCE_SELECT_EDGE) {
                BM_edge_select_set(em->bm, l->e, false);
              }
              else {
                BM_vert_select_set(em->bm, l->v, false);
              }
            }
          }
          if (!swap) {
            uvedit_face_select_disable(scene, em->bm, efa, offsets);
          }
        }
      }
    }

    /* Flush editmesh selections to ensure valid selection states. */
    if (em->selectmode != SCE_SELECT_FACE) {
      /* NOTE: Make sure correct flags are used. Previously this was done by passing
       * (SCE_SELECT_VERTEX | SCE_SELECT_EDGE), which doesn't work now that we support proper UV
       * edge selection. */

      BM_mesh_select_mode_flush(em->bm);
    }

    BM_select_history_validate(em->bm);

    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

#undef UV_VERT_SEL_TEST
#undef UV_EDGE_SEL_TEST

static void UV_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->description = "Hide (un)selected UV vertices";
  ot->idname = "UV_OT_hide";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_hide_exec;
  ot->poll = ED_operator_uvedit;

  /* props */
  RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reveal Operator
 * \{ */

static int uv_reveal_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);
  const ToolSettings *ts = scene->toolsettings;

  const bool use_face_center = (ts->uv_selectmode == UV_SELECT_FACE);
  const bool select = RNA_boolean_get(op->ptr, "select");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;

    const char *active_uv_name = CustomData_get_active_layer_name(&em->bm->ldata, CD_PROP_FLOAT2);
    BM_uv_map_ensure_vert_select_attr(em->bm, active_uv_name);
    BM_uv_map_ensure_edge_select_attr(em->bm, active_uv_name);
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    /* NOTE: Selecting faces is delayed so that it doesn't select verts/edges and confuse certain
     * UV selection checks.
     * This creates a temporary state which breaks certain UV selection functions that do face
     * visibility checks internally. Current implementation handles each case separately. */

    /* call the mesh function if we are in mesh sync sel */
    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (EDBM_mesh_reveal(em, select)) {
        Mesh *me = static_cast<Mesh *>(ob->data);
        EDBMUpdate_Params params = {0};
        params.calc_looptri = true;
        params.calc_normals = false;
        params.is_destructive = false;
        EDBM_update(me, &params);
      }
      continue;
    }

    /* NOTE(@sidd017): Supporting selections in all cases is quite difficult considering there are
     * at least 12 cases to look into (3 mesh select-modes + 4 uv select-modes + sticky modes).
     * For now we select all UV faces as sticky disabled to ensure proper UV selection states (vert
     * + edge flags) */
    if (use_face_center) {
      if (em->selectmode == SCE_SELECT_FACE) {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          BM_elem_flag_disable(efa, BM_ELEM_TAG);
          if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, select);
              BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, select);
            }
            /* BM_face_select_set(em->bm, efa, true); */
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
          }
        }
      }
      else {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          BM_elem_flag_disable(efa, BM_ELEM_TAG);
          if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            int totsel = 0;
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              if (em->selectmode == SCE_SELECT_VERTEX) {
                totsel += BM_elem_flag_test(l->v, BM_ELEM_SELECT);
              }
              else if (em->selectmode == SCE_SELECT_EDGE) {
                totsel += BM_elem_flag_test(l->e, BM_ELEM_SELECT);
              }
            }

            if (!totsel) {
              BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

                BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, select);
                BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, select);
              }
            }
            /* BM_face_select_set(em->bm, efa, true); */
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
          }
        }
      }
    }
    else if (em->selectmode == SCE_SELECT_FACE) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, select);
            BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, select);
          }
          /* BM_face_select_set(em->bm, efa, true); */
          BM_elem_flag_enable(efa, BM_ELEM_TAG);
        }
      }
    }
    else {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            BM_ELEM_CD_SET_BOOL(l, offsets.select_vert, select);
            BM_ELEM_CD_SET_BOOL(l, offsets.select_edge, select);
          }
          /* BM_face_select_set(em->bm, efa, true); */
          BM_elem_flag_enable(efa, BM_ELEM_TAG);
        }
      }
    }

    /* re-select tagged faces */
    BM_mesh_elem_hflag_enable_test(em->bm, BM_FACE, BM_ELEM_SELECT, true, false, BM_ELEM_TAG);

    DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static void UV_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Hidden";
  ot->description = "Reveal all hidden UV vertices";
  ot->idname = "UV_OT_reveal";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_reveal_exec;
  ot->poll = ED_operator_uvedit;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set 2D Cursor Operator
 * \{ */

static int uv_set_2d_cursor_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (!sima) {
    return OPERATOR_CANCELLED;
  }

  RNA_float_get_array(op->ptr, "location", sima->cursor);

  {
    wmMsgBus *mbus = CTX_wm_message_bus(C);
    bScreen *screen = CTX_wm_screen(C);
    WM_msg_publish_rna_prop(mbus, &screen->id, sima, SpaceImageEditor, cursor_location);
  }

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  /* Use pass-through to allow click-drag to transform the cursor. */
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

static int uv_set_2d_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  float location[2];

  if (region->regiontype == RGN_TYPE_WINDOW) {
    SpaceImage *sima = CTX_wm_space_image(C);
    if (sima && ED_space_image_show_cache_and_mval_over(sima, region, event->mval)) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
  RNA_float_set_array(op->ptr, "location", location);

  return uv_set_2d_cursor_exec(C, op);
}

static void UV_OT_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set 2D Cursor";
  ot->description = "Set 2D cursor location";
  ot->idname = "UV_OT_cursor_set";

  /* api callbacks */
  ot->exec = uv_set_2d_cursor_exec;
  ot->invoke = uv_set_2d_cursor_invoke;
  ot->poll = ED_space_image_cursor_poll;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Cursor location in normalized (0.0 to 1.0) coordinates",
                       -10.0f,
                       10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Seam from UV Islands Operator
 * \{ */

static int uv_seams_from_islands_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool mark_seams = RNA_boolean_get(op->ptr, "mark_seams");
  const bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    Mesh *me = (Mesh *)ob->data;
    BMEditMesh *em = me->edit_mesh;
    BMesh *bm = em->bm;
    BMIter iter;

    if (!EDBM_uv_check(em)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);
    bool changed = false;

    BMFace *f;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, f)) {
        continue;
      }

      BMLoop *l_iter;
      BMLoop *l_first;

      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (l_iter == l_iter->radial_next) {
          continue;
        }
        if (!uvedit_edge_select_test(scene, l_iter, offsets)) {
          continue;
        }

        bool mark = false;
        BMLoop *l_other = l_iter->radial_next;
        do {
          if (!BM_loop_uv_share_edge_check(l_iter, l_other, offsets.uv)) {
            mark = true;
            break;
          }
        } while ((l_other = l_other->radial_next) != l_iter);

        if (mark) {
          if (mark_seams) {
            BM_elem_flag_enable(l_iter->e, BM_ELEM_SEAM);
          }
          if (mark_sharp) {
            BM_elem_flag_disable(l_iter->e, BM_ELEM_SMOOTH);
          }
          changed = true;
        }
      } while ((l_iter = l_iter->next) != l_first);
    }

    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(&me->id, 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UV_OT_seams_from_islands(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Seams from Islands";
  ot->description = "Set mesh seams according to island setup in the UV editor";
  ot->idname = "UV_OT_seams_from_islands";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_seams_from_islands_exec;
  ot->poll = ED_operator_uvedit;

  RNA_def_boolean(ot->srna, "mark_seams", 1, "Mark Seams", "Mark boundary edges as seams");
  RNA_def_boolean(ot->srna, "mark_sharp", 0, "Mark Sharp", "Mark boundary edges as sharp");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mark Seam Operator
 * \{ */

static int uv_mark_seam_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const ToolSettings *ts = scene->toolsettings;

  BMFace *efa;
  BMLoop *loop;
  BMIter iter, liter;

  const bool flag_set = !RNA_boolean_get(op->ptr, "clear");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, ((View3D *)nullptr), &objects_len);

  bool changed = false;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    Mesh *me = (Mesh *)ob->data;
    BMEditMesh *em = me->edit_mesh;
    BMesh *bm = em->bm;

    if (synced_selection && (bm->totedgesel == 0)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      if (uvedit_face_visible_test(scene, efa)) {
        BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_edge_select_test(scene, loop, offsets)) {
            BM_elem_flag_set(loop->e, BM_ELEM_SEAM, flag_set);
            changed = true;
          }
        }
      }
    }

    if (changed) {
      DEG_id_tag_update(&me->id, 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
    }
  }

  if (changed) {
    ED_uvedit_live_unwrap(scene, objects, objects_len);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_mark_seam_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  uiPopupMenu *pup;
  uiLayout *layout;

  if (RNA_struct_property_is_set(op->ptr, "clear")) {
    return uv_mark_seam_exec(C, op);
  }

  pup = UI_popup_menu_begin(C, IFACE_("Edges"), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  uiItemBooleanO(layout,
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Mark Seam"),
                 ICON_NONE,
                 op->type->idname,
                 "clear",
                 false);
  uiItemBooleanO(layout,
                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Clear Seam"),
                 ICON_NONE,
                 op->type->idname,
                 "clear",
                 true);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static void UV_OT_mark_seam(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mark Seam";
  ot->description = "Mark selected UV edges as seams";
  ot->idname = "UV_OT_mark_seam";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_mark_seam_exec;
  ot->invoke = uv_mark_seam_invoke;
  ot->poll = ED_operator_uvedit;

  RNA_def_boolean(ot->srna, "clear", false, "Clear Seams", "Clear instead of marking seams");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Registration & Keymap
 * \{ */

void ED_operatortypes_uvedit(void)
{
  /* uvedit_select.c */
  WM_operatortype_append(UV_OT_select_all);
  WM_operatortype_append(UV_OT_select);
  WM_operatortype_append(UV_OT_select_loop);
  WM_operatortype_append(UV_OT_select_edge_ring);
  WM_operatortype_append(UV_OT_select_linked);
  WM_operatortype_append(UV_OT_select_linked_pick);
  WM_operatortype_append(UV_OT_select_split);
  WM_operatortype_append(UV_OT_select_pinned);
  WM_operatortype_append(UV_OT_select_box);
  WM_operatortype_append(UV_OT_select_lasso);
  WM_operatortype_append(UV_OT_select_similar);
  WM_operatortype_append(UV_OT_select_circle);
  WM_operatortype_append(UV_OT_select_more);
  WM_operatortype_append(UV_OT_select_less);
  WM_operatortype_append(UV_OT_select_overlap);
  WM_operatortype_append(UV_OT_select_mode);

  WM_operatortype_append(UV_OT_snap_cursor);
  WM_operatortype_append(UV_OT_snap_selected);

  WM_operatortype_append(UV_OT_align);

  WM_operatortype_append(UV_OT_rip);
  WM_operatortype_append(UV_OT_stitch);
  WM_operatortype_append(UV_OT_shortest_path_pick);
  WM_operatortype_append(UV_OT_shortest_path_select);

  WM_operatortype_append(UV_OT_seams_from_islands);
  WM_operatortype_append(UV_OT_mark_seam);
  WM_operatortype_append(UV_OT_weld);
  WM_operatortype_append(UV_OT_remove_doubles);
  WM_operatortype_append(UV_OT_pin);

  WM_operatortype_append(UV_OT_average_islands_scale);
  WM_operatortype_append(UV_OT_cube_project);
  WM_operatortype_append(UV_OT_cylinder_project);
  WM_operatortype_append(UV_OT_project_from_view);
  WM_operatortype_append(UV_OT_minimize_stretch);
  WM_operatortype_append(UV_OT_pack_islands);
  WM_operatortype_append(UV_OT_reset);
  WM_operatortype_append(UV_OT_sphere_project);
  WM_operatortype_append(UV_OT_unwrap);
  WM_operatortype_append(UV_OT_smart_project);

  WM_operatortype_append(UV_OT_reveal);
  WM_operatortype_append(UV_OT_hide);
  WM_operatortype_append(UV_OT_copy);
  WM_operatortype_append(UV_OT_paste);

  WM_operatortype_append(UV_OT_cursor_set);
}

void ED_operatormacros_uvedit(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("UV_OT_rip_move",
                                    "UV Rip Move",
                                    "Unstitch UVs and move the result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "UV_OT_rip");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

void ED_keymap_uvedit(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(keyconf, "UV Editor", 0, 0);
  keymap->poll = ED_operator_uvedit;
}

/** \} */
