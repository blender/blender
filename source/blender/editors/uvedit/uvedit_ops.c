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
 * \ingroup eduv
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_image_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_lasso_2d.h"
#include "BLI_blenlib.h"
#include "BLI_array.h"
#include "BLI_kdtree.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_uvedit.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "uvedit_intern.h"

static bool uv_select_is_any_selected(Scene *scene, Image *ima, Object *obedit);
static bool uv_select_is_any_selected_multi(Scene *scene,
                                            Image *ima,
                                            Object **objects,
                                            const uint objects_len);
static void uv_select_all_perform(Scene *scene, Image *ima, Object *obedit, int action);
static void uv_select_all_perform_multi(
    Scene *scene, Image *ima, Object **objects, const uint objects_len, int action);
static void uv_select_flush_from_tag_face(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select);
static void uv_select_flush_from_tag_loop(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select);
static void uv_select_tag_update_for_object(Depsgraph *depsgraph,
                                            const ToolSettings *ts,
                                            Object *obedit);

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

static bool ED_operator_uvedit_can_uv_sculpt(struct bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ToolSettings *toolsettings = CTX_data_tool_settings(C);
  Object *obedit = CTX_data_edit_object(C);

  return ED_space_image_show_uvedit(sima, obedit) && !(toolsettings->use_uv_sculpt);
}

static int UNUSED_FUNCTION(ED_operator_uvmap_mesh)(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob && ob->type == OB_MESH) {
    Mesh *me = ob->data;

    if (CustomData_get_layer(&me->fdata, CD_MTFACE) != NULL) {
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
                                bNode **r_node,
                                bNodeTree **r_ntree)
{
  Material *ma = give_current_material(ob, mat_nr);
  bNodeTree *ntree = (ma && ma->use_nodes) ? ma->nodetree : NULL;
  bNode *node = (ntree) ? nodeGetActiveTexture(ntree) : NULL;

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
        *r_iuser = NULL;
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
    *r_ima = NULL;
  }
  if (r_iuser) {
    *r_iuser = NULL;
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
  Material *ma = give_current_material(ob, mat_nr);
  bNode *node = (ma && ma->use_nodes) ? nodeGetActiveTexture(ma->nodetree) : NULL;

  if (node && is_image_texture_node(node)) {
    node->id = &ima->id;
    ED_node_tag_update_nodetree(bmain, ma->nodetree, node);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Conversion
 * \{ */

static void uvedit_pixel_to_float(SpaceImage *sima, float *dist, float pixeldist)
{
  int width, height;

  if (sima) {
    ED_space_image_get_size(sima, &width, &height);
  }
  else {
    width = IMG_SIZE_FALLBACK;
    height = IMG_SIZE_FALLBACK;
  }

  dist[0] = pixeldist / width;
  dist[1] = pixeldist / height;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Visibility and Selection Utilities
 * \{ */

static void uvedit_vertex_select_tagged(BMEditMesh *em,
                                        Scene *scene,
                                        bool select,
                                        int cd_loop_uv_offset)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
        uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
      }
    }
  }
}

bool uvedit_face_visible_nolocal_ex(const ToolSettings *ts, BMFace *efa)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0);
  }
  else {
    return (BM_elem_flag_test(efa, BM_ELEM_HIDDEN) == 0 && BM_elem_flag_test(efa, BM_ELEM_SELECT));
  }
}
bool uvedit_face_visible_nolocal(Scene *scene, BMFace *efa)
{
  return uvedit_face_visible_nolocal_ex(scene->toolsettings, efa);
}

bool uvedit_face_visible_test_ex(const ToolSettings *ts, Object *obedit, Image *ima, BMFace *efa)
{
  if (ts->uv_flag & UV_SHOW_SAME_IMAGE) {
    Image *face_image;
    ED_object_get_active_image(obedit, efa->mat_nr + 1, &face_image, NULL, NULL, NULL);
    return (face_image == ima) ? uvedit_face_visible_nolocal_ex(ts, efa) : false;
  }
  else {
    return uvedit_face_visible_nolocal_ex(ts, efa);
  }
}
bool uvedit_face_visible_test(Scene *scene, Object *obedit, Image *ima, BMFace *efa)
{
  return uvedit_face_visible_test_ex(scene->toolsettings, obedit, ima, efa);
}

bool uvedit_face_select_test_ex(const ToolSettings *ts, BMFace *efa, const int cd_loop_uv_offset)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (BM_elem_flag_test(efa, BM_ELEM_SELECT));
  }
  else {
    BMLoop *l;
    MLoopUV *luv;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      if (!(luv->flag & MLOOPUV_VERTSEL)) {
        return false;
      }
    }

    return true;
  }
}
bool uvedit_face_select_test(Scene *scene, BMFace *efa, const int cd_loop_uv_offset)
{
  return uvedit_face_select_test_ex(scene->toolsettings, efa, cd_loop_uv_offset);
}

bool uvedit_face_select_set(struct Scene *scene,
                            struct BMEditMesh *em,
                            struct BMFace *efa,
                            const bool select,
                            const bool do_history,
                            const int cd_loop_uv_offset)
{
  if (select) {
    return uvedit_face_select_enable(scene, em, efa, do_history, cd_loop_uv_offset);
  }
  else {
    return uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
  }
}

bool uvedit_face_select_enable(
    Scene *scene, BMEditMesh *em, BMFace *efa, const bool do_history, const int cd_loop_uv_offset)
{
  ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BM_face_select_set(em->bm, efa, true);
    if (do_history) {
      BM_select_history_store(em->bm, (BMElem *)efa);
    }
  }
  else {
    BMLoop *l;
    MLoopUV *luv;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->flag |= MLOOPUV_VERTSEL;
    }

    return true;
  }

  return false;
}

bool uvedit_face_select_disable(Scene *scene,
                                BMEditMesh *em,
                                BMFace *efa,
                                const int cd_loop_uv_offset)
{
  ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BM_face_select_set(em->bm, efa, false);
  }
  else {
    BMLoop *l;
    MLoopUV *luv;
    BMIter liter;

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->flag &= ~MLOOPUV_VERTSEL;
    }

    return true;
  }

  return false;
}

bool uvedit_edge_select_test_ex(const ToolSettings *ts, BMLoop *l, const int cd_loop_uv_offset)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      return BM_elem_flag_test(l->f, BM_ELEM_SELECT);
    }
    else if (ts->selectmode == SCE_SELECT_EDGE) {
      return BM_elem_flag_test(l->e, BM_ELEM_SELECT);
    }
    else {
      return BM_elem_flag_test(l->v, BM_ELEM_SELECT) &&
             BM_elem_flag_test(l->next->v, BM_ELEM_SELECT);
    }
  }
  else {
    MLoopUV *luv1, *luv2;

    luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

    return (luv1->flag & MLOOPUV_VERTSEL) && (luv2->flag & MLOOPUV_VERTSEL);
  }
}
bool uvedit_edge_select_test(Scene *scene, BMLoop *l, const int cd_loop_uv_offset)
{
  return uvedit_edge_select_test_ex(scene->toolsettings, l, cd_loop_uv_offset);
}

void uvedit_edge_select_set(BMEditMesh *em,
                            Scene *scene,
                            BMLoop *l,
                            const bool select,
                            const bool do_history,
                            const int cd_loop_uv_offset)

{
  if (select) {
    uvedit_edge_select_enable(em, scene, l, do_history, cd_loop_uv_offset);
  }
  else {
    uvedit_edge_select_disable(em, scene, l, cd_loop_uv_offset);
  }
}

void uvedit_edge_select_enable(
    BMEditMesh *em, Scene *scene, BMLoop *l, const bool do_history, const int cd_loop_uv_offset)

{
  ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, true);
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      BM_edge_select_set(em->bm, l->e, true);
    }
    else {
      BM_vert_select_set(em->bm, l->e->v1, true);
      BM_vert_select_set(em->bm, l->e->v2, true);
    }

    if (do_history) {
      BM_select_history_store(em->bm, (BMElem *)l->e);
    }
  }
  else {
    MLoopUV *luv1, *luv2;

    luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

    luv1->flag |= MLOOPUV_VERTSEL;
    luv2->flag |= MLOOPUV_VERTSEL;
  }
}

void uvedit_edge_select_disable(BMEditMesh *em,
                                Scene *scene,
                                BMLoop *l,
                                const int cd_loop_uv_offset)

{
  ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, false);
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      BM_edge_select_set(em->bm, l->e, false);
    }
    else {
      BM_vert_select_set(em->bm, l->e->v1, false);
      BM_vert_select_set(em->bm, l->e->v2, false);
    }
  }
  else {
    MLoopUV *luv1, *luv2;

    luv1 = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv2 = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

    luv1->flag &= ~MLOOPUV_VERTSEL;
    luv2->flag &= ~MLOOPUV_VERTSEL;
  }
}

bool uvedit_uv_select_test_ex(const ToolSettings *ts, BMLoop *l, const int cd_loop_uv_offset)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      return BM_elem_flag_test_bool(l->f, BM_ELEM_SELECT);
    }
    else {
      return BM_elem_flag_test_bool(l->v, BM_ELEM_SELECT);
    }
  }
  else {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    return (luv->flag & MLOOPUV_VERTSEL) != 0;
  }
}
bool uvedit_uv_select_test(Scene *scene, BMLoop *l, const int cd_loop_uv_offset)
{
  return uvedit_uv_select_test_ex(scene->toolsettings, l, cd_loop_uv_offset);
}

void uvedit_uv_select_set(BMEditMesh *em,
                          Scene *scene,
                          BMLoop *l,
                          const bool select,
                          const bool do_history,
                          const int cd_loop_uv_offset)
{
  if (select) {
    uvedit_uv_select_enable(em, scene, l, do_history, cd_loop_uv_offset);
  }
  else {
    uvedit_uv_select_disable(em, scene, l, cd_loop_uv_offset);
  }
}

void uvedit_uv_select_enable(
    BMEditMesh *em, Scene *scene, BMLoop *l, const bool do_history, const int cd_loop_uv_offset)
{
  ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, true);
    }
    else {
      BM_vert_select_set(em->bm, l->v, true);
    }

    if (do_history) {
      BM_select_history_remove(em->bm, (BMElem *)l->v);
    }
  }
  else {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv->flag |= MLOOPUV_VERTSEL;
  }
}

void uvedit_uv_select_disable(BMEditMesh *em, Scene *scene, BMLoop *l, const int cd_loop_uv_offset)
{
  ToolSettings *ts = scene->toolsettings;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode & SCE_SELECT_FACE) {
      BM_face_select_set(em->bm, l->f, false);
    }
    else {
      BM_vert_select_set(em->bm, l->v, false);
    }
  }
  else {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    luv->flag &= ~MLOOPUV_VERTSEL;
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

void uv_poly_center(BMFace *f, float r_cent[2], const int cd_loop_uv_offset)
{
  BMLoop *l;
  MLoopUV *luv;
  BMIter liter;

  zero_v2(r_cent);

  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
    add_v2_v2(r_cent, luv->uv);
  }

  mul_v2_fl(r_cent, 1.0f / (float)f->len);
}

void uv_poly_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy, int len)
{
  int i;
  for (i = 0; i < len; i++) {
    uv[i][0] = uv_orig[i][0] * aspx;
    uv[i][1] = uv_orig[i][1] * aspy;
  }
}

bool ED_uvedit_minmax_multi(Scene *scene,
                            Image *ima,
                            Object **objects_edit,
                            uint objects_len,
                            float r_min[2],
                            float r_max[2])
{
  bool changed = false;
  INIT_MINMAX2(r_min, r_max);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects_edit[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    MLoopUV *luv;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          minmax_v2v2_v2(r_min, r_max, luv->uv);
          changed = true;
        }
      }
    }
  }
  return changed;
}

bool ED_uvedit_minmax(Scene *scene, Image *ima, Object *obedit, float r_min[2], float r_max[2])
{
  return ED_uvedit_minmax_multi(scene, ima, &obedit, 1, r_min, r_max);
}

/* Be careful when using this, it bypasses all synchronization options */
void ED_uvedit_select_all(BMesh *bm)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv->flag |= MLOOPUV_VERTSEL;
    }
  }
}

static bool ED_uvedit_median_multi(
    Scene *scene, Image *ima, Object **objects_edit, uint objects_len, float co[2])
{
  unsigned int sel = 0;
  zero_v2(co);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects_edit[ob_index];

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    MLoopUV *luv;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          add_v2_v2(co, luv->uv);
          sel++;
        }
      }
    }
  }

  mul_v2_fl(co, 1.0f / (float)sel);

  return (sel != 0);
}

static bool UNUSED_FUNCTION(ED_uvedit_median)(Scene *scene,
                                              Image *ima,
                                              Object *obedit,
                                              float co[2])
{
  return ED_uvedit_median_multi(scene, ima, &obedit, 1, co);
}

bool ED_uvedit_center_multi(
    Scene *scene, Image *ima, Object **objects_edit, uint objects_len, float cent[2], char mode)
{
  bool changed = false;

  if (mode == V3D_AROUND_CENTER_BOUNDS) { /* bounding box */
    float min[2], max[2];
    if (ED_uvedit_minmax_multi(scene, ima, objects_edit, objects_len, min, max)) {
      mid_v2_v2v2(cent, min, max);
      changed = true;
    }
  }
  else {
    if (ED_uvedit_median_multi(scene, ima, objects_edit, objects_len, cent)) {
      changed = true;
    }
  }

  return changed;
}

bool ED_uvedit_center(Scene *scene, Image *ima, Object *obedit, float cent[2], char mode)
{
  return ED_uvedit_center_multi(scene, ima, &obedit, 1, cent, mode);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Nearest Elements
 * \{ */

bool uv_find_nearest_edge(
    Scene *scene, Image *ima, Object *obedit, const float co[2], UvNearestHit *hit)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv, *luv_next;
  int i;
  bool found = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
      continue;
    }
    BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);

      const float dist_test_sq = dist_squared_to_line_segment_v2(co, luv->uv, luv_next->uv);

      if (dist_test_sq < hit->dist_sq) {
        hit->efa = efa;

        hit->l = l;
        hit->luv = luv;
        hit->luv_next = luv_next;
        hit->lindex = i;

        hit->dist_sq = dist_test_sq;
        found = true;
      }
    }
  }
  return found;
}

bool uv_find_nearest_edge_multi(Scene *scene,
                                Image *ima,
                                Object **objects,
                                const uint objects_len,
                                const float co[2],
                                UvNearestHit *hit_final)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_edge(scene, ima, obedit, co, hit_final)) {
      hit_final->ob = obedit;
      found = true;
    }
  }
  return found;
}

bool uv_find_nearest_face(
    Scene *scene, Image *ima, Object *obedit, const float co[2], UvNearestHit *hit_final)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  bool found = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  /* this will fill in hit.vert1 and hit.vert2 */
  float dist_sq_init = hit_final->dist_sq;
  UvNearestHit hit = *hit_final;
  if (uv_find_nearest_edge(scene, ima, obedit, co, &hit)) {
    hit.dist_sq = dist_sq_init;
    hit.l = NULL;
    hit.luv = hit.luv_next = NULL;

    BMIter iter;
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      float cent[2];
      uv_poly_center(efa, cent, cd_loop_uv_offset);

      const float dist_test_sq = len_squared_v2v2(co, cent);

      if (dist_test_sq < hit.dist_sq) {
        hit.efa = efa;
        hit.dist_sq = dist_test_sq;
        found = true;
      }
    }
  }
  if (found) {
    *hit_final = hit;
  }
  return found;
}

bool uv_find_nearest_face_multi(Scene *scene,
                                Image *ima,
                                Object **objects,
                                const uint objects_len,
                                const float co[2],
                                UvNearestHit *hit_final)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_face(scene, ima, obedit, co, hit_final)) {
      hit_final->ob = obedit;
      found = true;
    }
  }
  return found;
}

static bool uv_nearest_between(const BMLoop *l, const float co[2], const int cd_loop_uv_offset)
{
  const float *uv_prev = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l->prev, cd_loop_uv_offset))->uv;
  const float *uv_curr = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset))->uv;
  const float *uv_next = ((MLoopUV *)BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset))->uv;

  return ((line_point_side_v2(uv_prev, uv_curr, co) > 0.0f) &&
          (line_point_side_v2(uv_next, uv_curr, co) <= 0.0f));
}

bool uv_find_nearest_vert(Scene *scene,
                          Image *ima,
                          Object *obedit,
                          float const co[2],
                          const float penalty_dist,
                          UvNearestHit *hit_final)
{
  bool found = false;

  /* this will fill in hit.vert1 and hit.vert2 */
  float dist_sq_init = hit_final->dist_sq;
  UvNearestHit hit = *hit_final;
  if (uv_find_nearest_edge(scene, ima, obedit, co, &hit)) {
    hit.dist_sq = dist_sq_init;

    hit.l = NULL;
    hit.luv = hit.luv_next = NULL;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMIter iter;

    BM_mesh_elem_index_ensure(em->bm, BM_VERT);

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BMIter liter;
      BMLoop *l;
      int i;
      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
        float dist_test_sq;
        MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (penalty_dist != 0.0f && uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          dist_test_sq = len_v2v2(co, luv->uv) + penalty_dist;
          dist_test_sq = SQUARE(dist_test_sq);
        }
        else {
          dist_test_sq = len_squared_v2v2(co, luv->uv);
        }

        if (dist_test_sq <= hit.dist_sq) {
          if (dist_test_sq == hit.dist_sq) {
            if (!uv_nearest_between(l, co, cd_loop_uv_offset)) {
              continue;
            }
          }

          hit.dist_sq = dist_test_sq;

          hit.l = l;
          hit.luv = luv;
          hit.luv_next = BM_ELEM_CD_GET_VOID_P(l->next, cd_loop_uv_offset);
          hit.efa = efa;
          hit.lindex = i;
          found = true;
        }
      }
    }
  }

  if (found) {
    *hit_final = hit;
  }

  return found;
}

bool uv_find_nearest_vert_multi(Scene *scene,
                                Image *ima,
                                Object **objects,
                                const uint objects_len,
                                float const co[2],
                                const float penalty_dist,
                                UvNearestHit *hit_final)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_find_nearest_vert(scene, ima, obedit, co, penalty_dist, hit_final)) {
      hit_final->ob = obedit;
      found = true;
    }
  }
  return found;
}

bool ED_uvedit_nearest_uv(
    Scene *scene, Object *obedit, Image *ima, const float co[2], float *dist_sq, float r_uv[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMIter iter;
  BMFace *efa;
  const float *uv_best = NULL;
  float dist_best = *dist_sq;
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
      continue;
    }
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
    do {
      const float *uv = ((const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset))->uv;
      const float dist_test = len_squared_v2v2(co, uv);
      if (dist_best > dist_test) {
        dist_best = dist_test;
        uv_best = uv;
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  if (uv_best != NULL) {
    copy_v2_v2(r_uv, uv_best);
    *dist_sq = dist_best;
    return true;
  }
  else {
    return false;
  }
}

bool ED_uvedit_nearest_uv_multi(Scene *scene,
                                Image *ima,
                                Object **objects,
                                const uint objects_len,
                                const float co[2],
                                float *dist_sq,
                                float r_uv[2])
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (ED_uvedit_nearest_uv(scene, obedit, ima, co, dist_sq, r_uv)) {
      found = true;
    }
  }
  return found;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Select
 * \{ */

static void uv_select_edgeloop_vertex_loop_flag(UvMapVert *first)
{
  UvMapVert *iterv;
  int count = 0;

  for (iterv = first; iterv; iterv = iterv->next) {
    if (iterv->separate && iterv != first) {
      break;
    }

    count++;
  }

  if (count < 5) {
    first->flag = 1;
  }
}

static UvMapVert *uv_select_edgeloop_vertex_map_get(UvVertMap *vmap, BMFace *efa, BMLoop *l)
{
  UvMapVert *iterv, *first;
  first = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

  for (iterv = first; iterv; iterv = iterv->next) {
    if (iterv->separate) {
      first = iterv;
    }
    if (iterv->poly_index == BM_elem_index_get(efa)) {
      return first;
    }
  }

  return NULL;
}

static bool uv_select_edgeloop_edge_tag_faces(BMEditMesh *em,
                                              UvMapVert *first1,
                                              UvMapVert *first2,
                                              int *totface)
{
  UvMapVert *iterv1, *iterv2;
  BMFace *efa;
  int tot = 0;

  /* count number of faces this edge has */
  for (iterv1 = first1; iterv1; iterv1 = iterv1->next) {
    if (iterv1->separate && iterv1 != first1) {
      break;
    }

    for (iterv2 = first2; iterv2; iterv2 = iterv2->next) {
      if (iterv2->separate && iterv2 != first2) {
        break;
      }

      if (iterv1->poly_index == iterv2->poly_index) {
        /* if face already tagged, don't do this edge */
        efa = BM_face_at_index(em->bm, iterv1->poly_index);
        if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
          return false;
        }

        tot++;
        break;
      }
    }
  }

  if (*totface == 0) { /* start edge */
    *totface = tot;
  }
  else if (tot != *totface) { /* check for same number of faces as start edge */
    return false;
  }

  /* tag the faces */
  for (iterv1 = first1; iterv1; iterv1 = iterv1->next) {
    if (iterv1->separate && iterv1 != first1) {
      break;
    }

    for (iterv2 = first2; iterv2; iterv2 = iterv2->next) {
      if (iterv2->separate && iterv2 != first2) {
        break;
      }

      if (iterv1->poly_index == iterv2->poly_index) {
        efa = BM_face_at_index(em->bm, iterv1->poly_index);
        BM_elem_flag_enable(efa, BM_ELEM_TAG);
        break;
      }
    }
  }

  return true;
}

static int uv_select_edgeloop(Scene *scene,
                              Image *ima,
                              Object *obedit,
                              UvNearestHit *hit,
                              const float limit[2],
                              const bool extend)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMIter iter, liter;
  BMLoop *l;
  UvVertMap *vmap;
  UvMapVert *iterv_curr;
  UvMapVert *iterv_next;
  int starttotf;
  bool looking, select;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  /* setup */
  BM_mesh_elem_table_ensure(em->bm, BM_FACE);
  vmap = BM_uv_vert_map_create(em->bm, limit, false, false);

  BM_mesh_elem_index_ensure(em->bm, BM_VERT | BM_FACE);

  if (!extend) {
    uv_select_all_perform(scene, ima, obedit, SEL_DESELECT);
  }

  BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

  /* set flags for first face and verts */
  iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l);
  iterv_next = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l->next);
  uv_select_edgeloop_vertex_loop_flag(iterv_curr);
  uv_select_edgeloop_vertex_loop_flag(iterv_next);

  starttotf = 0;
  uv_select_edgeloop_edge_tag_faces(em, iterv_curr, iterv_next, &starttotf);

  /* sorry, first edge isn't even ok */
  looking = !(iterv_curr->flag == 0 && iterv_next->flag == 0);

  /* iterate */
  while (looking) {
    looking = false;

    /* find correct valence edges which are not tagged yet, but connect to tagged one */

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_TAG) &&
          uvedit_face_visible_test(scene, obedit, ima, efa)) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          /* check face not hidden and not tagged */
          if (!(iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, efa, l))) {
            continue;
          }
          if (!(iterv_next = uv_select_edgeloop_vertex_map_get(vmap, efa, l->next))) {
            continue;
          }

          /* check if vertex is tagged and has right valence */
          if (iterv_curr->flag || iterv_next->flag) {
            if (uv_select_edgeloop_edge_tag_faces(em, iterv_curr, iterv_next, &starttotf)) {
              looking = true;
              BM_elem_flag_enable(efa, BM_ELEM_TAG);

              uv_select_edgeloop_vertex_loop_flag(iterv_curr);
              uv_select_edgeloop_vertex_loop_flag(iterv_next);
              break;
            }
          }
        }
      }
    }
  }

  /* do the actual select/deselect */
  iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l);
  iterv_next = uv_select_edgeloop_vertex_map_get(vmap, hit->efa, hit->l->next);
  iterv_curr->flag = 1;
  iterv_next->flag = 1;

  if (extend) {
    select = !(uvedit_uv_select_test(scene, hit->l, cd_loop_uv_offset));
  }
  else {
    select = true;
  }

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      iterv_curr = uv_select_edgeloop_vertex_map_get(vmap, efa, l);

      if (iterv_curr->flag) {
        uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
      }
    }
  }

  /* cleanup */
  BM_uv_vert_map_free(vmap);

  return (select) ? 1 : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked
 * \{ */

static void uv_select_linked_multi(Scene *scene,
                                   Image *ima,
                                   Object **objects,
                                   const uint objects_len,
                                   const float limit[2],
                                   UvNearestHit *hit_final,
                                   bool extend,
                                   bool deselect,
                                   bool toggle,
                                   bool select_faces)
{
  /* loop over objects, or just use hit_final->ob */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    if (hit_final && ob_index != 0) {
      break;
    }
    Object *obedit = hit_final ? hit_final->ob : objects[ob_index];

    BMFace *efa;
    BMLoop *l;
    BMIter iter, liter;
    MLoopUV *luv;
    UvVertMap *vmap;
    UvMapVert *vlist, *iterv, *startv;
    int i, stacksize = 0, *stack;
    unsigned int a;
    char *flag;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE); /* we can use this too */

    /* Note, we had 'use winding' so we don't consider overlapping islands as connected, see T44320
     * this made *every* projection split the island into front/back islands.
     * Keep 'use_winding' to false, see: T50970.
     *
     * Better solve this by having a delimit option for select-linked operator,
     * keeping island-select working as is. */
    vmap = BM_uv_vert_map_create(em->bm, limit, !select_faces, false);

    if (vmap == NULL) {
      return;
    }

    stack = MEM_mallocN(sizeof(*stack) * (em->bm->totface + 1), "UvLinkStack");
    flag = MEM_callocN(sizeof(*flag) * em->bm->totface, "UvLinkFlag");

    if (hit_final == NULL) {
      /* Use existing selection */
      BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
        if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
          if (select_faces) {
            if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
              stack[stacksize] = a;
              stacksize++;
              flag[a] = 1;
            }
          }
          else {
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

              if (luv->flag & MLOOPUV_VERTSEL) {
                stack[stacksize] = a;
                stacksize++;
                flag[a] = 1;

                break;
              }
            }
          }
        }
      }
    }
    else {
      BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
        if (efa == hit_final->efa) {
          stack[stacksize] = a;
          stacksize++;
          flag[a] = 1;
          break;
        }
      }
    }

    while (stacksize > 0) {

      stacksize--;
      a = stack[stacksize];

      efa = BM_face_at_index(em->bm, a);

      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {

        /* make_uv_vert_map_EM sets verts tmp.l to the indices */
        vlist = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

        startv = vlist;

        for (iterv = vlist; iterv; iterv = iterv->next) {
          if (iterv->separate) {
            startv = iterv;
          }
          if (iterv->poly_index == a) {
            break;
          }
        }

        for (iterv = startv; iterv; iterv = iterv->next) {
          if ((startv != iterv) && (iterv->separate)) {
            break;
          }
          else if (!flag[iterv->poly_index]) {
            flag[iterv->poly_index] = 1;
            stack[stacksize] = iterv->poly_index;
            stacksize++;
          }
        }
      }
    }

    /* Toggling - if any of the linked vertices is selected (and visible), we deselect. */
    if ((toggle == true) && (extend == false) && (deselect == false)) {
      BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
        bool found_selected = false;
        if (!flag[a]) {
          continue;
        }

        if (select_faces) {
          if (BM_elem_flag_test(efa, BM_ELEM_SELECT) && !BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
            found_selected = true;
          }
        }
        else {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

            if (luv->flag & MLOOPUV_VERTSEL) {
              found_selected = true;
            }
          }

          if (found_selected) {
            deselect = true;
            break;
          }
        }
      }
    }

#define SET_SELECTION(value) \
  if (select_faces) { \
    BM_face_select_set(em->bm, efa, value); \
  } \
  else { \
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) { \
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset); \
      luv->flag = (value) ? (luv->flag | MLOOPUV_VERTSEL) : (luv->flag & ~MLOOPUV_VERTSEL); \
    } \
  } \
  (void)0

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, a) {
      if (!flag[a]) {
        if (!extend && !deselect && !toggle) {
          SET_SELECTION(false);
        }
        continue;
      }

      if (!deselect) {
        SET_SELECTION(true);
      }
      else {
        SET_SELECTION(false);
      }
    }

#undef SET_SELECTION

    MEM_freeN(stack);
    MEM_freeN(flag);
    BM_uv_vert_map_free(vmap);
  }
}

/* WATCH IT: this returns first selected UV,
 * not ideal in many cases since there could be multiple */
static float *uv_sel_co_from_eve(
    Scene *scene, Object *obedit, Image *ima, BMEditMesh *em, BMVert *eve)
{
  BMIter liter;
  BMLoop *l;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
    if (!uvedit_face_visible_test(scene, obedit, ima, l->f)) {
      continue;
    }

    if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      return luv->uv;
    }
  }

  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operator
 * \{ */

static int uv_select_more_less(bContext *C, const bool select)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Image *ima = CTX_data_edit_image(C);
  SpaceImage *sima = CTX_wm_space_image(C);

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  ToolSettings *ts = scene->toolsettings;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (ts->uv_flag & UV_SYNC_SELECTION) {
      if (select) {
        EDBM_select_more(em, true);
      }
      else {
        EDBM_select_less(em, true);
      }

      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
      continue;
    }

    if (ts->uv_selectmode == UV_SELECT_FACE) {

      /* clear tags */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);

      /* mark loops to be selected */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (uvedit_face_visible_test(scene, obedit, ima, efa)) {

#define IS_SEL 1
#define IS_UNSEL 2

          int sel_state = 0;

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            if (luv->flag & MLOOPUV_VERTSEL) {
              sel_state |= IS_SEL;
            }
            else {
              sel_state |= IS_UNSEL;
            }

            /* if we have a mixed selection, tag to grow it */
            if (sel_state == (IS_SEL | IS_UNSEL)) {
              BM_elem_flag_enable(efa, BM_ELEM_TAG);
              changed = true;
              break;
            }
          }

#undef IS_SEL
#undef IS_UNSEL
        }
      }
    }
    else {

      /* clear tags */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          BM_elem_flag_disable(l, BM_ELEM_TAG);
        }
      }

      /* mark loops to be selected */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {

            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

            if (((luv->flag & MLOOPUV_VERTSEL) != 0) == select) {
              BM_elem_flag_enable(l->next, BM_ELEM_TAG);
              BM_elem_flag_enable(l->prev, BM_ELEM_TAG);
              changed = true;
            }
          }
        }
      }
    }

    if (changed) {
      /* Select tagged loops. */
      uv_select_flush_from_tag_loop(sima, scene, obedit, select);
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
  return uv_select_more_less(C, true);
}

static void UV_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->description = "Select more UV vertices connected to initial selection";
  ot->idname = "UV_OT_select_more";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_more_exec;
  ot->poll = ED_operator_uvedit_space_image;
}

static int uv_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
  return uv_select_more_less(C, false);
}

static void UV_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->description = "Deselect UV vertices at the boundary of each selection region";
  ot->idname = "UV_OT_select_less";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_less_exec;
  ot->poll = ED_operator_uvedit_space_image;
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

static void uv_weld_align(bContext *C, eUVWeldAlign tool)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = CTX_data_edit_image(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;
  float cent[2], min[2], max[2];

  INIT_MINMAX2(min, max);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  if (tool == UV_ALIGN_AUTO) {
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *obedit = objects[ob_index];
      BMEditMesh *em = BKE_editmesh_from_object(obedit);

      if (synced_selection && (em->bm->totvertsel == 0)) {
        continue;
      }

      const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

      BMIter iter, liter;
      BMFace *efa;
      BMLoop *l;

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            minmax_v2v2_v2(min, max, luv->uv);
          }
        }
      }
    }
    tool = (max[0] - min[0] >= max[1] - min[1]) ? UV_ALIGN_Y : UV_ALIGN_X;
  }

  ED_uvedit_center_multi(scene, ima, objects, objects_len, cent, 0);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (ELEM(tool, UV_ALIGN_X, UV_WELD)) {
      BMIter iter, liter;
      BMFace *efa;
      BMLoop *l;

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            luv->uv[0] = cent[0];
            changed = true;
          }
        }
      }
    }

    if (ELEM(tool, UV_ALIGN_Y, UV_WELD)) {
      BMIter iter, liter;
      BMFace *efa;
      BMLoop *l;

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            luv->uv[1] = cent[1];
            changed = true;
          }
        }
      }
    }

    if (ELEM(tool, UV_STRAIGHTEN, UV_STRAIGHTEN_X, UV_STRAIGHTEN_Y)) {
      BMEdge *eed;
      BMLoop *l;
      BMVert *eve;
      BMVert *eve_start;
      BMIter iter, liter, eiter;

      /* clear tag */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      /* tag verts with a selected UV */
      BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
        BM_ITER_ELEM (l, &liter, eve, BM_LOOPS_OF_VERT) {
          if (!uvedit_face_visible_test(scene, obedit, ima, l->f)) {
            continue;
          }

          if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
            BM_elem_flag_enable(eve, BM_ELEM_TAG);
            break;
          }
        }
      }

      /* flush vertex tags to edges */
      BM_ITER_MESH (eed, &iter, em->bm, BM_EDGES_OF_MESH) {
        BM_elem_flag_set(
            eed,
            BM_ELEM_TAG,
            (BM_elem_flag_test(eed->v1, BM_ELEM_TAG) && BM_elem_flag_test(eed->v2, BM_ELEM_TAG)));
      }

      /* find a vertex with only one tagged edge */
      eve_start = NULL;
      BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
        int tot_eed_tag = 0;
        BM_ITER_ELEM (eed, &eiter, eve, BM_EDGES_OF_VERT) {
          if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
            tot_eed_tag++;
          }
        }

        if (tot_eed_tag == 1) {
          eve_start = eve;
          break;
        }
      }

      if (eve_start) {
        BMVert **eve_line = NULL;
        BMVert *eve_next = NULL;
        BLI_array_declare(eve_line);
        int i;

        eve = eve_start;

        /* walk over edges, building an array of verts in a line */
        while (eve) {
          BLI_array_append(eve_line, eve);
          /* don't touch again */
          BM_elem_flag_disable(eve, BM_ELEM_TAG);

          eve_next = NULL;

          /* find next eve */
          BM_ITER_ELEM (eed, &eiter, eve, BM_EDGES_OF_VERT) {
            if (BM_elem_flag_test(eed, BM_ELEM_TAG)) {
              BMVert *eve_other = BM_edge_other_vert(eed, eve);
              if (BM_elem_flag_test(eve_other, BM_ELEM_TAG)) {
                /* this is a tagged vert we didn't walk over yet, step onto it */
                eve_next = eve_other;
                break;
              }
            }
          }

          eve = eve_next;
        }

        /* now we have all verts, make into a line */
        if (BLI_array_len(eve_line) > 2) {

          /* we know the returns from these must be valid */
          const float *uv_start = uv_sel_co_from_eve(scene, obedit, ima, em, eve_line[0]);
          const float *uv_end = uv_sel_co_from_eve(
              scene, obedit, ima, em, eve_line[BLI_array_len(eve_line) - 1]);
          /* For UV_STRAIGHTEN_X & UV_STRAIGHTEN_Y modes */
          float a = 0.0f;
          eUVWeldAlign tool_local = tool;

          if (tool_local == UV_STRAIGHTEN_X) {
            if (uv_start[1] == uv_end[1]) {
              tool_local = UV_STRAIGHTEN;
            }
            else {
              a = (uv_end[0] - uv_start[0]) / (uv_end[1] - uv_start[1]);
            }
          }
          else if (tool_local == UV_STRAIGHTEN_Y) {
            if (uv_start[0] == uv_end[0]) {
              tool_local = UV_STRAIGHTEN;
            }
            else {
              a = (uv_end[1] - uv_start[1]) / (uv_end[0] - uv_start[0]);
            }
          }

          /* go over all verts except for endpoints */
          for (i = 0; i < BLI_array_len(eve_line); i++) {
            BM_ITER_ELEM (l, &liter, eve_line[i], BM_LOOPS_OF_VERT) {
              if (!uvedit_face_visible_test(scene, obedit, ima, l->f)) {
                continue;
              }

              if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
                MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
                /* Projection of point (x, y) over line (x1, y1, x2, y2) along X axis:
                 * new_y = (y2 - y1) / (x2 - x1) * (x - x1) + y1
                 * Maybe this should be a BLI func? Or is it already existing?
                 * Could use interp_v2_v2v2, but not sure it's worth it here...*/
                if (tool_local == UV_STRAIGHTEN_X) {
                  luv->uv[0] = a * (luv->uv[1] - uv_start[1]) + uv_start[0];
                }
                else if (tool_local == UV_STRAIGHTEN_Y) {
                  luv->uv[1] = a * (luv->uv[0] - uv_start[0]) + uv_start[1];
                }
                else {
                  closest_to_line_segment_v2(luv->uv, luv->uv, uv_start, uv_end);
                }
                changed = true;
              }
            }
          }
        }
        else {
          /* error - not a line, needs 3+ points  */
        }

        if (eve_line) {
          MEM_freeN(eve_line);
        }
      }
      else {
        /* error - cant find an endpoint */
      }
    }

    if (changed) {
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(obedit->data, 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }

  MEM_freeN(objects);
}

static int uv_align_exec(bContext *C, wmOperator *op)
{
  uv_weld_align(C, RNA_enum_get(op->ptr, "axis"));

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
      {0, NULL, 0, NULL, NULL},
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
  Image *ima = CTX_data_edit_image(C);
  ToolSettings *ts = scene->toolsettings;

  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  bool *changed = MEM_callocN(sizeof(bool) * objects_len, "uv_remove_doubles_selected.changed");

  /* Maximum index of an objects[i]'s MLoopUVs in MLoopUV_arr.
   * It helps find which MLoopUV in *MLoopUV_arr belongs to which object. */
  uint *ob_mloopuv_max_idx = MEM_callocN(sizeof(uint) * objects_len,
                                         "uv_remove_doubles_selected.ob_mloopuv_max_idx");

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

  int *duplicates = NULL;
  BLI_array_declare(duplicates);

  MLoopUV **mloopuv_arr = NULL;
  BLI_array_declare(mloopuv_arr);

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

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          BLI_kdtree_2d_insert(tree, mloopuv_count, luv->uv);
          BLI_array_append(duplicates, -1);
          BLI_array_append(mloopuv_arr, luv);
          mloopuv_count++;
        }
      }
    }

    ob_mloopuv_max_idx[ob_index] = mloopuv_count - 1;
  }

  BLI_kdtree_2d_balance(tree);
  int found_duplicates = BLI_kdtree_2d_calc_duplicates_fast(tree, threshold, false, duplicates);

  if (found_duplicates > 0) {
    /* Calculate average uv for duplicates. */
    int *uv_duplicate_count = MEM_callocN(sizeof(int) * mloopuv_count,
                                          "uv_remove_doubles_selected.uv_duplicate_count");
    for (int i = 0; i < mloopuv_count; i++) {
      if (duplicates[i] == -1) { /* If doesn't reference another */
        uv_duplicate_count[i]++; /* self */
        continue;
      }

      if (duplicates[i] != i) {
        /* If not self then accumulate uv for averaging.
         * Self uv is already present in accumulator */
        add_v2_v2(mloopuv_arr[duplicates[i]]->uv, mloopuv_arr[i]->uv);
      }
      uv_duplicate_count[duplicates[i]]++;
    }

    for (int i = 0; i < mloopuv_count; i++) {
      if (uv_duplicate_count[i] < 2) {
        continue;
      }

      mul_v2_fl(mloopuv_arr[i]->uv, 1.0f / (float)uv_duplicate_count[i]);
    }
    MEM_freeN(uv_duplicate_count);

    /* Update duplicated uvs. */
    uint ob_index = 0;
    for (int i = 0; i < mloopuv_count; i++) {
      /* Make sure we know which object owns the MLoopUV at this index.
       * Remember that in some cases the object will have no loop uv,
       * thus we need the while loop, and not simply an if check. */
      while (ob_mloopuv_max_idx[ob_index] < i) {
        ob_index++;
      }

      if (duplicates[i] == -1) {
        continue;
      }

      copy_v2_v2(mloopuv_arr[i]->uv, mloopuv_arr[duplicates[i]]->uv);
      changed[ob_index] = true;
    }

    for (ob_index = 0; ob_index < objects_len; ob_index++) {
      if (changed[ob_index]) {
        Object *obedit = objects[ob_index];
        uvedit_live_unwrap_update(sima, scene, obedit);
        DEG_id_tag_update(obedit->data, 0);
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      }
    }
  }

  BLI_kdtree_2d_free(tree);
  BLI_array_free(mloopuv_arr);
  BLI_array_free(duplicates);
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
  Image *ima = CTX_data_edit_image(C);
  ToolSettings *ts = scene->toolsettings;

  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  /* Calculate max possible number of kdtree nodes. */
  int uv_maxlen = 0;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    uv_maxlen += em->bm->totloop;
  }

  KDTree_2d *tree = BLI_kdtree_2d_new(uv_maxlen);

  MLoopUV **mloopuv_arr = NULL;
  BLI_array_declare(mloopuv_arr);

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

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (!uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          BLI_kdtree_2d_insert(tree, mloopuv_count, luv->uv);
          BLI_array_append(mloopuv_arr, luv);
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

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          KDTreeNearest_2d nearest;
          const int i = BLI_kdtree_2d_find_nearest(tree, luv->uv, &nearest);

          if (i != -1 && nearest.dist < threshold) {
            copy_v2_v2(luv->uv, mloopuv_arr[i]->uv);
            changed = true;
          }
        }
      }
    }

    if (changed) {
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(obedit->data, 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }

  BLI_kdtree_2d_free(tree);
  BLI_array_free(mloopuv_arr);
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static int uv_remove_doubles_exec(bContext *C, wmOperator *op)
{
  if (RNA_boolean_get(op->ptr, "use_unselected")) {
    return uv_remove_doubles_to_unselected(C, op);
  }
  else {
    return uv_remove_doubles_to_selected(C, op);
  }
}

static void UV_OT_remove_doubles(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Doubles UV";
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

static int uv_weld_exec(bContext *C, wmOperator *UNUSED(op))
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
/** \name (De)Select All Operator
 * \{ */

static bool uv_select_is_any_selected(Scene *scene, Image *ima, Object *obedit)
{
  ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    return (em->bm->totvertsel || em->bm->totedgesel || em->bm->totfacesel);
  }
  else {
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        if (luv->flag & MLOOPUV_VERTSEL) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool uv_select_is_any_selected_multi(Scene *scene,
                                            Image *ima,
                                            Object **objects,
                                            const uint objects_len)
{
  bool found = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    if (uv_select_is_any_selected(scene, ima, obedit)) {
      found = true;
      break;
    }
  }
  return found;
}

static void uv_select_all_perform(Scene *scene, Image *ima, Object *obedit, int action)
{
  ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if (action == SEL_TOGGLE) {
    action = uv_select_is_any_selected(scene, ima, obedit) ? SEL_DESELECT : SEL_SELECT;
  }

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    switch (action) {
      case SEL_TOGGLE:
        EDBM_select_toggle_all(em);
        break;
      case SEL_SELECT:
        EDBM_flag_enable_all(em, BM_ELEM_SELECT);
        break;
      case SEL_DESELECT:
        EDBM_flag_disable_all(em, BM_ELEM_SELECT);
        break;
      case SEL_INVERT:
        EDBM_select_swap(em);
        EDBM_selectmode_flush(em);
        break;
    }
  }
  else {
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        switch (action) {
          case SEL_SELECT:
            luv->flag |= MLOOPUV_VERTSEL;
            break;
          case SEL_DESELECT:
            luv->flag &= ~MLOOPUV_VERTSEL;
            break;
          case SEL_INVERT:
            luv->flag ^= MLOOPUV_VERTSEL;
            break;
        }
      }
    }
  }
}

static void uv_select_all_perform_multi(
    Scene *scene, Image *ima, Object **objects, const uint objects_len, int action)
{
  if (action == SEL_TOGGLE) {
    action = uv_select_is_any_selected_multi(scene, ima, objects, objects_len) ? SEL_DESELECT :
                                                                                 SEL_SELECT;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    uv_select_all_perform(scene, ima, obedit, action);
  }
}

static int uv_select_all_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  Image *ima = CTX_data_edit_image(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  int action = RNA_enum_get(op->ptr, "action");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  uv_select_all_perform_multi(scene, ima, objects, objects_len, action);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    uv_select_tag_update_for_object(depsgraph, ts, obedit);
  }

  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static void UV_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all UV vertices";
  ot->idname = "UV_OT_select_all";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_all_exec;
  ot->poll = ED_operator_uvedit;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mouse Select Operator
 * \{ */

static bool uv_sticky_select(
    float *limit, int hitv[], int v, float *hituv[], float *uv, int sticky, int hitlen)
{
  int i;

  /* this function test if some vertex needs to selected
   * in addition to the existing ones due to sticky select */
  if (sticky == SI_STICKY_DISABLE) {
    return false;
  }

  for (i = 0; i < hitlen; i++) {
    if (hitv[i] == v) {
      if (sticky == SI_STICKY_LOC) {
        if (fabsf(hituv[i][0] - uv[0]) < limit[0] && fabsf(hituv[i][1] - uv[1]) < limit[1]) {
          return true;
        }
      }
      else if (sticky == SI_STICKY_VERTEX) {
        return true;
      }
    }
  }

  return false;
}

static int uv_mouse_select_multi(
    bContext *C, Object **objects, uint objects_len, const float co[2], bool extend, bool loop)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  Image *ima = CTX_data_edit_image(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  UvNearestHit hit = UV_NEAREST_HIT_INIT;
  int i, selectmode, sticky, sync, *hitv = NULL;
  bool select = true;
  /* 0 == don't flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
  int flush = 0;
  int hitlen = 0;
  float limit[2], **hituv = NULL;

  /* notice 'limit' is the same no matter the zoom level, since this is like
   * remove doubles and could annoying if it joined points when zoomed out.
   * 'penalty' is in screen pixel space otherwise zooming in on a uv-vert and
   * shift-selecting can consider an adjacent point close enough to add to
   * the selection rather than de-selecting the closest. */

  float penalty_dist;
  {
    float penalty[2];
    uvedit_pixel_to_float(sima, limit, 0.05f);
    uvedit_pixel_to_float(sima, penalty, 5.0f / (sima ? sima->zoom : 1.0f));
    penalty_dist = len_v2(penalty);
  }

  /* retrieve operation mode */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    sync = 1;

    if (ts->selectmode & SCE_SELECT_FACE) {
      selectmode = UV_SELECT_FACE;
    }
    else if (ts->selectmode & SCE_SELECT_EDGE) {
      selectmode = UV_SELECT_EDGE;
    }
    else {
      selectmode = UV_SELECT_VERTEX;
    }

    sticky = SI_STICKY_DISABLE;
  }
  else {
    sync = 0;
    selectmode = ts->uv_selectmode;
    sticky = (sima) ? sima->sticky : 1;
  }

  /* find nearest element */
  if (loop) {
    /* find edge */
    if (!uv_find_nearest_edge_multi(scene, ima, objects, objects_len, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    hitlen = 0;
  }
  else if (selectmode == UV_SELECT_VERTEX) {
    /* find vertex */
    if (!uv_find_nearest_vert_multi(scene, ima, objects, objects_len, co, penalty_dist, &hit)) {
      return OPERATOR_CANCELLED;
    }

    /* mark 1 vertex as being hit */
    hitv = BLI_array_alloca(hitv, hit.efa->len);
    hituv = BLI_array_alloca(hituv, hit.efa->len);
    copy_vn_i(hitv, hit.efa->len, 0xFFFFFFFF);

    hitv[hit.lindex] = BM_elem_index_get(hit.l->v);
    hituv[hit.lindex] = hit.luv->uv;

    hitlen = hit.efa->len;
  }
  else if (selectmode == UV_SELECT_EDGE) {
    /* find edge */
    if (!uv_find_nearest_edge_multi(scene, ima, objects, objects_len, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    /* mark 2 edge vertices as being hit */
    hitv = BLI_array_alloca(hitv, hit.efa->len);
    hituv = BLI_array_alloca(hituv, hit.efa->len);
    copy_vn_i(hitv, hit.efa->len, 0xFFFFFFFF);

    hitv[hit.lindex] = BM_elem_index_get(hit.l->v);
    hitv[(hit.lindex + 1) % hit.efa->len] = BM_elem_index_get(hit.l->next->v);
    hituv[hit.lindex] = hit.luv->uv;
    hituv[(hit.lindex + 1) % hit.efa->len] = hit.luv_next->uv;

    hitlen = hit.efa->len;
  }
  else if (selectmode == UV_SELECT_FACE) {
    /* find face */
    if (!uv_find_nearest_face_multi(scene, ima, objects, objects_len, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    BMEditMesh *em = BKE_editmesh_from_object(hit.ob);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    /* make active */
    BM_mesh_active_face_set(em->bm, hit.efa);

    /* mark all face vertices as being hit */

    hitv = BLI_array_alloca(hitv, hit.efa->len);
    hituv = BLI_array_alloca(hituv, hit.efa->len);
    BM_ITER_ELEM_INDEX (l, &liter, hit.efa, BM_LOOPS_OF_FACE, i) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      hituv[i] = luv->uv;
      hitv[i] = BM_elem_index_get(l->v);
    }

    hitlen = hit.efa->len;
  }
  else if (selectmode == UV_SELECT_ISLAND) {
    if (!uv_find_nearest_edge_multi(scene, ima, objects, objects_len, co, &hit)) {
      return OPERATOR_CANCELLED;
    }

    hitlen = 0;
  }
  else {
    hitlen = 0;
    return OPERATOR_CANCELLED;
  }

  Object *obedit = hit.ob;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  /* do selection */
  if (loop) {
    if (!extend) {
      /* TODO(MULTI_EDIT): We only need to de-select non-active */
      uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);
    }
    flush = uv_select_edgeloop(scene, ima, obedit, &hit, limit, extend);
  }
  else if (selectmode == UV_SELECT_ISLAND) {
    if (!extend) {
      /* TODO(MULTI_EDIT): We only need to de-select non-active */
      uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);
    }
    /* Current behavior of 'extend'
     * is actually toggling, so pass extend flag as 'toggle' here */
    uv_select_linked_multi(
        scene, ima, objects, objects_len, limit, &hit, false, false, extend, false);
  }
  else if (extend) {
    if (selectmode == UV_SELECT_VERTEX) {
      /* (de)select uv vertex */
      select = !uvedit_uv_select_test(scene, hit.l, cd_loop_uv_offset);
      uvedit_uv_select_set(em, scene, hit.l, select, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_EDGE) {
      /* (de)select edge */
      select = !(uvedit_edge_select_test(scene, hit.l, cd_loop_uv_offset));
      uvedit_edge_select_set(em, scene, hit.l, select, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_FACE) {
      /* (de)select face */
      select = !(uvedit_face_select_test(scene, hit.efa, cd_loop_uv_offset));
      uvedit_face_select_set(scene, em, hit.efa, select, true, cd_loop_uv_offset);
      flush = -1;
    }

    /* de-selecting an edge may deselect a face too - validate */
    if (sync) {
      if (select == false) {
        BM_select_history_validate(em->bm);
      }
    }

    /* (de)select sticky uv nodes */
    if (sticky != SI_STICKY_DISABLE) {

      BM_mesh_elem_index_ensure(em->bm, BM_VERT);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (uv_sticky_select(
                  limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen)) {
            uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
          }
        }
      }

      flush = select ? 1 : -1;
    }
  }
  else {
    /* deselect all */
    uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);

    if (selectmode == UV_SELECT_VERTEX) {
      /* select vertex */
      uvedit_uv_select_enable(em, scene, hit.l, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_EDGE) {
      /* select edge */
      uvedit_edge_select_enable(em, scene, hit.l, true, cd_loop_uv_offset);
      flush = 1;
    }
    else if (selectmode == UV_SELECT_FACE) {
      /* select face */
      uvedit_face_select_enable(scene, em, hit.efa, true, cd_loop_uv_offset);
    }

    /* select sticky uvs */
    if (sticky != SI_STICKY_DISABLE) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (sticky == SI_STICKY_DISABLE) {
            continue;
          }
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

          if (uv_sticky_select(
                  limit, hitv, BM_elem_index_get(l->v), hituv, luv->uv, sticky, hitlen)) {
            uvedit_uv_select_enable(em, scene, l, false, cd_loop_uv_offset);
          }

          flush = 1;
        }
      }
    }
  }

  if (sync) {
    /* flush for mesh selection */

    /* before bmesh */
#if 0
    if (ts->selectmode != SCE_SELECT_FACE) {
      if (flush == 1)
        EDBM_select_flush(em);
      else if (flush == -1)
        EDBM_deselect_flush(em);
    }
#else
    if (flush != 0) {
      if (loop) {
        /* push vertex -> edge selection */
        if (select) {
          EDBM_select_flush(em);
        }
        else {
          EDBM_deselect_flush(em);
        }
      }
      else {
        EDBM_selectmode_flush(em);
      }
    }
#endif
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obiter = objects[ob_index];
    uv_select_tag_update_for_object(depsgraph, ts, obiter);
  }

  return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
}
static int uv_mouse_select(bContext *C, const float co[2], bool extend, bool loop)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);
  int ret = uv_mouse_select_multi(C, objects, objects_len, co, extend, loop);
  MEM_freeN(objects);
  return ret;
}

static int uv_select_exec(bContext *C, wmOperator *op)
{
  float co[2];
  bool extend, loop;

  RNA_float_get_array(op->ptr, "location", co);
  extend = RNA_boolean_get(op->ptr, "extend");
  loop = false;

  return uv_mouse_select(C, co, extend, loop);
}

static int uv_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  return uv_select_exec(C, op);
}

static void UV_OT_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select";
  ot->description = "Select UV vertices";
  ot->idname = "UV_OT_select";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_exec;
  ot->invoke = uv_select_invoke;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* properties */
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Select Operator
 * \{ */

static int uv_select_loop_exec(bContext *C, wmOperator *op)
{
  float co[2];
  bool extend, loop;

  RNA_float_get_array(op->ptr, "location", co);
  extend = RNA_boolean_get(op->ptr, "extend");
  loop = true;

  return uv_mouse_select(C, co, extend, loop);
}

static int uv_select_loop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  float co[2];

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
  RNA_float_set_array(op->ptr, "location", co);

  return uv_select_loop_exec(C, op);
}

static void UV_OT_select_loop(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Loop Select";
  ot->description = "Select a loop of connected UV vertices";
  ot->idname = "UV_OT_select_loop";
  ot->flag = OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_loop_exec;
  ot->invoke = uv_select_loop_invoke;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* properties */
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int uv_select_linked_internal(bContext *C, wmOperator *op, const wmEvent *event, bool pick)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Image *ima = CTX_data_edit_image(C);
  float limit[2];
  bool extend = true;
  bool deselect = false;
  bool select_faces = (ts->uv_flag & UV_SYNC_SELECTION) && (ts->selectmode & SCE_SELECT_FACE);

  UvNearestHit hit = UV_NEAREST_HIT_INIT;

  if ((ts->uv_flag & UV_SYNC_SELECTION) && !(ts->selectmode & SCE_SELECT_FACE)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Select linked only works in face select mode when sync selection is enabled");
    return OPERATOR_CANCELLED;
  }

  if (pick) {
    extend = RNA_boolean_get(op->ptr, "extend");
    deselect = RNA_boolean_get(op->ptr, "deselect");
  }
  uvedit_pixel_to_float(sima, limit, 0.05f);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  if (pick) {
    float co[2];

    if (event) {
      /* invoke */
      ARegion *ar = CTX_wm_region(C);

      UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &co[0], &co[1]);
      RNA_float_set_array(op->ptr, "location", co);
    }
    else {
      /* exec */
      RNA_float_get_array(op->ptr, "location", co);
    }

    if (!uv_find_nearest_edge_multi(scene, ima, objects, objects_len, co, &hit)) {
      MEM_freeN(objects);
      return OPERATOR_CANCELLED;
    }
  }

  if (!extend) {
    uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);
  }

  uv_select_linked_multi(scene,
                         ima,
                         objects,
                         objects_len,
                         limit,
                         pick ? &hit : NULL,
                         extend,
                         deselect,
                         false,
                         select_faces);

  /* weak!, but works */
  Object **objects_free = objects;
  if (pick) {
    objects = &hit.ob;
    objects_len = 1;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    DEG_id_tag_update(obedit->data, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  MEM_SAFE_FREE(objects_free);

  return OPERATOR_FINISHED;
}

static int uv_select_linked_exec(bContext *C, wmOperator *op)
{
  return uv_select_linked_internal(C, op, NULL, false);
}

static void UV_OT_select_linked(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked";
  ot->description = "Select all UV vertices linked to the active UV map";
  ot->idname = "UV_OT_select_linked";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_linked_exec;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked (Cursor Pick) Operator
 * \{ */

static int uv_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return uv_select_linked_internal(C, op, event, true);
}

static int uv_select_linked_pick_exec(bContext *C, wmOperator *op)
{
  return uv_select_linked_internal(C, op, NULL, true);
}

static void UV_OT_select_linked_pick(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked Pick";
  ot->description = "Select all UV vertices linked under the mouse";
  ot->idname = "UV_OT_select_linked_pick";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->invoke = uv_select_linked_pick_invoke;
  ot->exec = uv_select_linked_pick_exec;
  ot->poll = ED_operator_uvedit; /* requires space image */

  /* properties */
  RNA_def_boolean(ot->srna,
                  "extend",
                  0,
                  "Extend",
                  "Extend selection rather than clearing the existing selection");
  RNA_def_boolean(ot->srna,
                  "deselect",
                  0,
                  "Deselect",
                  "Deselect linked UV vertices rather than selecting them");
  RNA_def_float_vector(
      ot->srna,
      "location",
      2,
      NULL,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds",
      -100.0f,
      100.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Split Operator
 * \{ */

/**
 * \note This is based on similar use case to #MESH_OT_split(), which has a similar effect
 * but in this case they are not joined to begin with (only having the behavior of being joined)
 * so its best to call this #uv_select_split() instead of just split(), but assigned to the same
 * key as #MESH_OT_split - Campbell.
 */
static int uv_select_split_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ToolSettings *ts = scene->toolsettings;
  Image *ima = CTX_data_edit_image(C);

  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    BKE_report(op->reports, RPT_ERROR, "Cannot split selection when sync selection is enabled");
    return OPERATOR_CANCELLED;
  }

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMesh *bm = BKE_editmesh_from_object(obedit)->bm;

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      bool is_sel = false;
      bool is_unsel = false;

      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      /* are we all selected? */
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        if (luv->flag & MLOOPUV_VERTSEL) {
          is_sel = true;
        }
        else {
          is_unsel = true;
        }

        /* we have mixed selection, bail out */
        if (is_sel && is_unsel) {
          break;
        }
      }

      if (is_sel && is_unsel) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          luv->flag &= ~MLOOPUV_VERTSEL;
        }

        changed = true;
      }
    }

    if (changed) {
      changed_multi = true;
      WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UV_OT_select_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Split";
  ot->description = "Select only entirely selected faces";
  ot->idname = "UV_OT_select_split";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_split_exec;
  ot->poll = ED_operator_uvedit; /* requires space image */
}

static void uv_select_sync_flush(ToolSettings *ts, BMEditMesh *em, const short select)
{
  /* bmesh API handles flushing but not on de-select */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (ts->selectmode != SCE_SELECT_FACE) {
      if (select == false) {
        EDBM_deselect_flush(em);
      }
      else {
        EDBM_select_flush(em);
      }
    }

    if (select == false) {
      BM_select_history_validate(em->bm);
    }
  }
}

static void uv_select_tag_update_for_object(Depsgraph *depsgraph,
                                            const ToolSettings *ts,
                                            Object *obedit)
{
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }
  else {
    Object *obedit_eval = DEG_get_evaluated_object(depsgraph, obedit);
    BKE_mesh_batch_cache_dirty_tag(obedit_eval->data, BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT);
    /* Only for region redraw. */
    WM_main_add_notifier(NC_GEOM | ND_SELECT, obedit->data);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select/Tag Flushing Utils
 *
 * Utility functions to flush the uv-selection from tags.
 * \{ */

/**
 * helper function for #uv_select_flush_from_tag_loop and uv_select_flush_from_tag_face
 */
static void uv_select_flush_from_tag_sticky_loc_internal(Scene *scene,
                                                         BMEditMesh *em,
                                                         UvVertMap *vmap,
                                                         const unsigned int efa_index,
                                                         BMLoop *l,
                                                         const bool select,
                                                         const int cd_loop_uv_offset)
{
  UvMapVert *start_vlist = NULL, *vlist_iter;
  BMFace *efa_vlist;

  uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);

  vlist_iter = BM_uv_vert_map_at_index(vmap, BM_elem_index_get(l->v));

  while (vlist_iter) {
    if (vlist_iter->separate) {
      start_vlist = vlist_iter;
    }

    if (efa_index == vlist_iter->poly_index) {
      break;
    }

    vlist_iter = vlist_iter->next;
  }

  vlist_iter = start_vlist;
  while (vlist_iter) {

    if (vlist_iter != start_vlist && vlist_iter->separate) {
      break;
    }

    if (efa_index != vlist_iter->poly_index) {
      BMLoop *l_other;
      efa_vlist = BM_face_at_index(em->bm, vlist_iter->poly_index);
      /* tf_vlist = BM_ELEM_CD_GET_VOID_P(efa_vlist, cd_poly_tex_offset); */ /* UNUSED */

      l_other = BM_iter_at_index(
          em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->loop_of_poly_index);

      uvedit_uv_select_set(em, scene, l_other, select, false, cd_loop_uv_offset);
    }
    vlist_iter = vlist_iter->next;
  }
}

/**
 * Flush the selection from face tags based on sticky and selection modes.
 *
 * needed because settings the selection a face is done in a number of places but it also
 * needs to respect the sticky modes for the UV verts, so dealing with the sticky modes
 * is best done in a separate function.
 *
 * \note This function is very similar to #uv_select_flush_from_tag_loop,
 * be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_face(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select)
{
  /* Selecting UV Faces with some modes requires us to change
   * the selection in other faces (depending on the sticky mode).
   *
   * This only needs to be done when the Mesh is not used for
   * selection (so for sticky modes, vertex or location based). */

  ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_VERTEX) {
    /* Tag all verts as untouched, then touch the ones that have a face center
     * in the loop and select all MLoopUV's that use a touched vert. */
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          BM_elem_flag_enable(l->v, BM_ELEM_TAG);
        }
      }
    }

    /* now select tagged verts */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
          uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
        }
      }
    }
  }
  else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
    struct UvVertMap *vmap;
    float limit[2];
    unsigned int efa_index;

    uvedit_pixel_to_float(sima, limit, 0.05);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    vmap = BM_uv_vert_map_create(em->bm, limit, false, false);
    if (vmap == NULL) {
      return;
    }

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l, select, cd_loop_uv_offset);
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        uvedit_face_select_set(scene, em, efa, select, false, cd_loop_uv_offset);
      }
    }
  }
}

/**
 * Flush the selection from loop tags based on sticky and selection modes.
 *
 * needed because settings the selection a face is done in a number of places but it also needs
 * to respect the sticky modes for the UV verts, so dealing with the sticky modes is best done
 * in a separate function.
 *
 * \note This function is very similar to #uv_select_flush_from_tag_loop,
 * be sure to update both upon changing.
 */
static void uv_select_flush_from_tag_loop(SpaceImage *sima,
                                          Scene *scene,
                                          Object *obedit,
                                          const bool select)
{
  /* Selecting UV Loops with some modes requires us to change
   * the selection in other faces (depending on the sticky mode).
   *
   * This only needs to be done when the Mesh is not used for
   * selection (so for sticky modes, vertex or location based). */

  ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_VERTEX) {
    /* Tag all verts as untouched, then touch the ones that have a face center
     * in the loop and select all MLoopUV's that use a touched vert. */
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          BM_elem_flag_enable(l->v, BM_ELEM_TAG);
        }
      }
    }

    /* now select tagged verts */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
          uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
        }
      }
    }
  }
  else if ((ts->uv_flag & UV_SYNC_SELECTION) == 0 && sima->sticky == SI_STICKY_LOC) {
    struct UvVertMap *vmap;
    float limit[2];
    unsigned int efa_index;

    uvedit_pixel_to_float(sima, limit, 0.05);

    BM_mesh_elem_table_ensure(em->bm, BM_FACE);
    vmap = BM_uv_vert_map_create(em->bm, limit, false, false);
    if (vmap == NULL) {
      return;
    }

    BM_ITER_MESH_INDEX (efa, &iter, em->bm, BM_FACES_OF_MESH, efa_index) {
      /* tf = BM_ELEM_CD_GET_VOID_P(efa, cd_poly_tex_offset); */ /* UNUSED */

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          uv_select_flush_from_tag_sticky_loc_internal(
              scene, em, vmap, efa_index, l, select, cd_loop_uv_offset);
        }
      }
    }
    BM_uv_vert_map_free(vmap);
  }
  else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) {
          uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static int uv_box_select_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Image *ima = CTX_data_edit_image(C);
  ARegion *ar = CTX_wm_region(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  rctf rectf;
  bool pinned;
  const bool use_face_center = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                                    (ts->selectmode == SCE_SELECT_FACE) :
                                    (ts->uv_selectmode == UV_SELECT_FACE));

  /* get rectangle from operator */
  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(&ar->v2d, &rectf, &rectf);

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);

  pinned = RNA_boolean_get(op->ptr, "pinned");

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);
  }

  /* don't indent to avoid diff noise! */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    /* do actual selection */
    if (use_face_center && !pinned) {
      /* handle face selection mode */
      float cent[2];

      changed = false;

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        /* assume not touched */
        BM_elem_flag_disable(efa, BM_ELEM_TAG);

        if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
          uv_poly_center(efa, cent, cd_loop_uv_offset);
          if (BLI_rctf_isect_pt_v(&rectf, cent)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
    }
    else {
      /* other selection modes */
      changed = true;
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
          continue;
        }
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

          if (!pinned || (ts->uv_flag & UV_SYNC_SELECTION)) {

            /* UV_SYNC_SELECTION - can't do pinned selection */
            if (BLI_rctf_isect_pt_v(&rectf, luv->uv)) {
              uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
              BM_elem_flag_enable(l->v, BM_ELEM_TAG);
            }
          }
          else if (pinned) {
            if ((luv->flag & MLOOPUV_PINNED) && BLI_rctf_isect_pt_v(&rectf, luv->uv)) {
              uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
              BM_elem_flag_enable(l->v, BM_ELEM_TAG);
            }
          }
        }
      }

      if (sima->sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, cd_loop_uv_offset);
      }
    }

    if (changed) {
      changed_multi = true;

      uv_select_sync_flush(ts, em, select);
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }

  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UV_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->description = "Select UV vertices using box selection";
  ot->idname = "UV_OT_select_box";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = uv_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = ED_operator_uvedit_space_image; /* requires space image */
  ot->cancel = WM_gesture_box_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "pinned", 0, "Pinned", "Border select pinned UVs only");

  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

static int uv_inside_circle(const float uv[2], const float offset[2], const float ellipse[2])
{
  /* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
  float x, y;
  x = (uv[0] - offset[0]) * ellipse[0];
  y = (uv[1] - offset[1]) * ellipse[1];
  return ((x * x + y * y) < 1.0f);
}

static int uv_circle_select_exec(bContext *C, wmOperator *op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = CTX_data_edit_image(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ToolSettings *ts = scene->toolsettings;
  ARegion *ar = CTX_wm_region(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  int x, y, radius, width, height;
  float zoomx, zoomy, offset[2], ellipse[2];

  const bool use_face_center = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                                    (ts->selectmode == SCE_SELECT_FACE) :
                                    (ts->uv_selectmode == UV_SELECT_FACE));

  /* get operator properties */
  x = RNA_int_get(op->ptr, "x");
  y = RNA_int_get(op->ptr, "y");
  radius = RNA_int_get(op->ptr, "radius");

  /* compute ellipse size and location, not a circle since we deal
   * with non square image. ellipse is normalized, r = 1.0. */
  ED_space_image_get_size(sima, &width, &height);
  ED_space_image_get_zoom(sima, ar, &zoomx, &zoomy);

  ellipse[0] = width * zoomx / radius;
  ellipse[1] = height * zoomy / radius;

  UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);

  bool changed_multi = false;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  const eSelectOp sel_op = ED_select_op_modal(RNA_enum_get(op->ptr, "mode"),
                                              WM_gesture_is_modal_first(op->customdata));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);
    changed_multi = true;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    /* do selection */
    if (use_face_center) {
      changed = false;
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        /* assume not touched */
        if (select != uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
          float cent[2];
          uv_poly_center(efa, cent, cd_loop_uv_offset);
          if (uv_inside_circle(cent, offset, ellipse)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
    }
    else {
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (uv_inside_circle(luv->uv, offset, ellipse)) {
            changed = true;
            uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
            BM_elem_flag_enable(l->v, BM_ELEM_TAG);
          }
        }
      }

      if (sima->sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, cd_loop_uv_offset);
      }
    }

    if (changed) {
      changed_multi = true;

      uv_select_sync_flush(ts, em, select);
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void UV_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->description = "Select UV vertices using circle selection";
  ot->idname = "UV_OT_select_circle";

  /* api callbacks */
  ot->invoke = WM_gesture_circle_invoke;
  ot->modal = WM_gesture_circle_modal;
  ot->exec = uv_circle_select_exec;
  ot->poll = ED_operator_uvedit_space_image; /* requires space image */
  ot->cancel = WM_gesture_circle_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */

static bool do_lasso_select_mesh_uv(bContext *C,
                                    const int mcords[][2],
                                    short moves,
                                    const eSelectOp sel_op)
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = CTX_data_edit_image(C);
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool use_face_center = ((ts->uv_flag & UV_SYNC_SELECTION) ?
                                    (ts->selectmode == SCE_SELECT_FACE) :
                                    (ts->uv_selectmode == UV_SELECT_FACE));
  const bool select = (sel_op != SEL_OP_SUB);

  BMIter iter, liter;

  BMFace *efa;
  BMLoop *l;
  int screen_uv[2];
  bool changed_multi = false;
  rcti rect;

  BLI_lasso_boundbox(&rect, mcords, moves);

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    uv_select_all_perform_multi(scene, ima, objects, objects_len, SEL_DESELECT);
  }

  /* don't indent to avoid diff noise! */
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];

    bool changed = false;

    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (use_face_center) { /* Face Center Sel */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        /* assume not touched */
        if (select != uvedit_face_select_test(scene, efa, cd_loop_uv_offset)) {
          float cent[2];
          uv_poly_center(efa, cent, cd_loop_uv_offset);

          if (UI_view2d_view_to_region_clip(
                  &ar->v2d, cent[0], cent[1], &screen_uv[0], &screen_uv[1]) &&
              BLI_rcti_isect_pt_v(&rect, screen_uv) &&
              BLI_lasso_is_point_inside(
                  mcords, moves, screen_uv[0], screen_uv[1], V2D_IS_CLIPPED)) {
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
            changed = true;
          }
        }
      }

      /* (de)selects all tagged faces and deals with sticky modes */
      if (changed) {
        uv_select_flush_from_tag_face(sima, scene, obedit, select);
      }
    }
    else { /* Vert Sel */
      BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT, BM_ELEM_TAG, false);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (uvedit_face_visible_test(scene, obedit, ima, efa)) {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            if ((select) != (uvedit_uv_select_test(scene, l, cd_loop_uv_offset))) {
              MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
              if (UI_view2d_view_to_region_clip(
                      &ar->v2d, luv->uv[0], luv->uv[1], &screen_uv[0], &screen_uv[1]) &&
                  BLI_rcti_isect_pt_v(&rect, screen_uv) &&
                  BLI_lasso_is_point_inside(
                      mcords, moves, screen_uv[0], screen_uv[1], V2D_IS_CLIPPED)) {
                uvedit_uv_select_set(em, scene, l, select, false, cd_loop_uv_offset);
                changed = true;
                BM_elem_flag_enable(l->v, BM_ELEM_TAG);
              }
            }
          }
        }
      }

      if (sima->sticky == SI_STICKY_VERTEX) {
        uvedit_vertex_select_tagged(em, scene, select, cd_loop_uv_offset);
      }
    }

    if (changed) {
      changed_multi = true;

      uv_select_sync_flush(ts, em, select);
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return changed_multi;
}

static int uv_lasso_select_exec(bContext *C, wmOperator *op)
{
  int mcords_tot;
  const int(*mcords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcords_tot);

  if (mcords) {
    const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
    bool changed = do_lasso_select_mesh_uv(C, mcords, mcords_tot, sel_op);
    MEM_freeN((void *)mcords);

    return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
  }

  return OPERATOR_PASS_THROUGH;
}

static void UV_OT_select_lasso(wmOperatorType *ot)
{
  ot->name = "Lasso Select UV";
  ot->description = "Select UVs using lasso selection";
  ot->idname = "UV_OT_select_lasso";

  ot->invoke = WM_gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = uv_lasso_select_exec;
  ot->poll = ED_operator_uvedit_space_image;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
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

static bool uv_snap_cursor_to_selection(
    Scene *scene, Image *ima, Object **objects_edit, uint objects_len, SpaceImage *sima)
{
  return ED_uvedit_center_multi(scene, ima, objects_edit, objects_len, sima->cursor, sima->around);
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
      Image *ima = CTX_data_edit_image(C);
      ViewLayer *view_layer = CTX_data_view_layer(C);

      uint objects_len = 0;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
          view_layer, ((View3D *)NULL), &objects_len);
      changed = uv_snap_cursor_to_selection(scene, ima, objects, objects_len, sima);
      MEM_freeN(objects);
      break;
    }
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
      {0, NULL, 0, NULL, NULL},
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

static bool uv_snap_uvs_to_cursor(Scene *scene, Image *ima, Object *obedit, const float cursor[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  bool changed = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        copy_v2_v2(luv->uv, cursor);
        changed = true;
      }
    }
  }

  return changed;
}

static bool uv_snap_uvs_offset(Scene *scene, Image *ima, Object *obedit, const float offset[2])
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  bool changed = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        add_v2_v2(luv->uv, offset);
        changed = true;
      }
    }
  }

  return changed;
}

static bool uv_snap_uvs_to_adjacent_unselected(Scene *scene, Image *ima, Object *obedit)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMesh *bm = em->bm;
  BMFace *f;
  BMLoop *l, *lsub;
  BMIter iter, liter, lsubiter;
  MLoopUV *luv;
  bool changed = false;
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

  /* index every vert that has a selected UV using it, but only once so as to
   * get unique indices and to count how much to malloc */
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (uvedit_face_visible_test(scene, obedit, ima, f)) {
      BM_elem_flag_enable(f, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        BM_elem_flag_set(l, BM_ELEM_TAG, uvedit_uv_select_test(scene, l, cd_loop_uv_offset));
      }
    }
    else {
      BM_elem_flag_disable(f, BM_ELEM_TAG);
    }
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_TAG)) { /* face: visible */
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (BM_elem_flag_test(l, BM_ELEM_TAG)) { /* loop: selected*/
          float uv[2] = {0.0f, 0.0f};
          int uv_tot = 0;

          BM_ITER_ELEM (lsub, &lsubiter, l->v, BM_LOOPS_OF_VERT) {
            if (BM_elem_flag_test(lsub->f, BM_ELEM_TAG) && /* face: visible */
                !BM_elem_flag_test(lsub, BM_ELEM_TAG))     /* loop: unselected  */
            {
              luv = BM_ELEM_CD_GET_VOID_P(lsub, cd_loop_uv_offset);
              add_v2_v2(uv, luv->uv);
              uv_tot++;
            }
          }

          if (uv_tot) {
            luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            mul_v2_v2fl(luv->uv, uv, 1.0f / (float)uv_tot);
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
  Image *ima = sima->image;
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  int width = 0, height = 0;
  float w, h;
  bool changed = false;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  ED_space_image_get_size(sima, &width, &height);
  w = (float)width;
  h = (float)height;

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        uv_snap_to_pixel(luv->uv, w, h);
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
  Image *ima = CTX_data_edit_image(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;
  const int target = RNA_enum_get(op->ptr, "target");
  float offset[2] = {0};

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  if (target == 2) {
    float center[2];
    if (!ED_uvedit_center_multi(scene, ima, objects, objects_len, center, sima->around)) {
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
        changed = uv_snap_uvs_to_cursor(scene, ima, obedit, sima->cursor);
        break;
      case 2:
        changed = uv_snap_uvs_offset(scene, ima, obedit, offset);
        break;
      case 3:
        changed = uv_snap_uvs_to_adjacent_unselected(scene, ima, obedit);
        break;
    }

    if (changed) {
      changed_multi = true;
      uvedit_live_unwrap_update(sima, scene, obedit);
      DEG_id_tag_update(obedit->data, 0);
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
      {0, NULL, 0, NULL, NULL},
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
/** \name Pin UV's Operator
 * \{ */

static int uv_pin_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Image *ima = CTX_data_edit_image(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  ToolSettings *ts = scene->toolsettings;
  const bool clear = RNA_boolean_get(op->ptr, "clear");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    bool changed = false;
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          changed = true;
          if (clear) {
            luv->flag &= ~MLOOPUV_PINNED;
          }
          else {
            luv->flag |= MLOOPUV_PINNED;
          }
        }
      }
    }

    if (changed) {
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
      DEG_id_tag_update(obedit->data, ID_RECALC_COPY_ON_WRITE);
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
/** \name Select Pinned UV's Operator
 * \{ */

static int uv_select_pinned_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Image *ima = CTX_data_edit_image(C);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);
    bool changed = false;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

        if (luv->flag & MLOOPUV_PINNED) {
          uvedit_uv_select_enable(em, scene, l, false, cd_loop_uv_offset);
          changed = true;
        }
      }
    }

    if (changed) {
      uv_select_tag_update_for_object(depsgraph, ts, obedit);
    }
  }
  MEM_freeN(objects);

  return OPERATOR_FINISHED;
}

static void UV_OT_select_pinned(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Selected Pinned";
  ot->description = "Select all pinned UV vertices";
  ot->idname = "UV_OT_select_pinned";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* api callbacks */
  ot->exec = uv_select_pinned_exec;
  ot->poll = ED_operator_uvedit;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Operator
 * \{ */

/* check if we are selected or unselected based on 'bool_test' arg,
 * needed for select swap support */
#define UV_SEL_TEST(luv, bool_test) \
  ((((luv)->flag & MLOOPUV_VERTSEL) == MLOOPUV_VERTSEL) == bool_test)

/* is every UV vert selected or unselected depending on bool_test */
static bool bm_face_is_all_uv_sel(BMFace *f, bool select_test, const int cd_loop_uv_offset)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l_iter, cd_loop_uv_offset);
    if (!UV_SEL_TEST(luv, select_test)) {
      return false;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return true;
}

static int uv_hide_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Object *obedit = CTX_data_edit_object(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  const bool swap = RNA_boolean_get(op->ptr, "unselected");
  Image *ima = sima ? sima->image : NULL;
  const int use_face_center = (ts->uv_selectmode == UV_SELECT_FACE);

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (EDBM_mesh_hide(em, swap)) {
      EDBM_update_generic(em, true, false);
    }
    return OPERATOR_FINISHED;
  }

  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    int hide = 0;

    if (!uvedit_face_visible_test(scene, obedit, ima, efa)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

      if (UV_SEL_TEST(luv, !swap)) {
        hide = 1;
        break;
      }
    }

    if (hide) {
      /* note, a special case for edges could be used,
       * for now edges act like verts and get flushed */
      if (use_face_center) {
        if (em->selectmode == SCE_SELECT_FACE) {
          /* check that every UV is selected */
          if (bm_face_is_all_uv_sel(efa, true, cd_loop_uv_offset) == !swap) {
            BM_face_select_set(em->bm, efa, false);
          }
          uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
        }
        else {
          if (bm_face_is_all_uv_sel(efa, true, cd_loop_uv_offset) == !swap) {
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
              if (UV_SEL_TEST(luv, !swap)) {
                BM_vert_select_set(em->bm, l->v, false);
              }
            }
          }
          if (!swap) {
            uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
          }
        }
      }
      else if (em->selectmode == SCE_SELECT_FACE) {
        /* check if a UV is de-selected */
        if (bm_face_is_all_uv_sel(efa, false, cd_loop_uv_offset) != !swap) {
          BM_face_select_set(em->bm, efa, false);
          uvedit_face_select_disable(scene, em, efa, cd_loop_uv_offset);
        }
      }
      else {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          if (UV_SEL_TEST(luv, !swap)) {
            BM_vert_select_set(em->bm, l->v, false);
            if (!swap) {
              luv->flag &= ~MLOOPUV_VERTSEL;
            }
          }
        }
      }
    }
  }

  /* flush vertex selection changes */
  if (em->selectmode != SCE_SELECT_FACE) {
    EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX | SCE_SELECT_EDGE);
  }

  BM_select_history_validate(em->bm);

  DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

  return OPERATOR_FINISHED;
}

#undef UV_SEL_TEST

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
  SpaceImage *sima = CTX_wm_space_image(C);
  Object *obedit = CTX_data_edit_object(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  const int use_face_center = (ts->uv_selectmode == UV_SELECT_FACE);
  const int stickymode = sima ? (sima->sticky != SI_STICKY_DISABLE) : 1;

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

  const bool select = RNA_boolean_get(op->ptr, "select");

  /* note on tagging, selecting faces needs to be delayed so it doesn't select the verts and
   * confuse our checks on selected verts. */

  /* call the mesh function if we are in mesh sync sel */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    if (EDBM_mesh_reveal(em, select)) {
      EDBM_update_generic(em, true, false);
    }
    return OPERATOR_FINISHED;
  }
  if (use_face_center) {
    if (em->selectmode == SCE_SELECT_FACE) {
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            SET_FLAG_FROM_TEST(luv->flag, select, MLOOPUV_VERTSEL);
          }
          /* BM_face_select_set(em->bm, efa, true); */
          BM_elem_flag_enable(efa, BM_ELEM_TAG);
        }
      }
    }
    else {
      /* enable adjacent faces to have disconnected UV selections if sticky is disabled */
      if (!stickymode) {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          BM_elem_flag_disable(efa, BM_ELEM_TAG);
          if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            int totsel = 0;
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              totsel += BM_elem_flag_test(l->v, BM_ELEM_SELECT);
            }

            if (!totsel) {
              BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
                luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
                SET_FLAG_FROM_TEST(luv->flag, select, MLOOPUV_VERTSEL);
              }
              /* BM_face_select_set(em->bm, efa, true); */
              BM_elem_flag_enable(efa, BM_ELEM_TAG);
            }
          }
        }
      }
      else {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          BM_elem_flag_disable(efa, BM_ELEM_TAG);
          if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
              if (BM_elem_flag_test(l->v, BM_ELEM_SELECT) == 0) {
                luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
                SET_FLAG_FROM_TEST(luv->flag, select, MLOOPUV_VERTSEL);
              }
            }
            /* BM_face_select_set(em->bm, efa, true); */
            BM_elem_flag_enable(efa, BM_ELEM_TAG);
          }
        }
      }
    }
  }
  else if (em->selectmode == SCE_SELECT_FACE) {
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BM_elem_flag_disable(efa, BM_ELEM_TAG);
      if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN) && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          SET_FLAG_FROM_TEST(luv->flag, select, MLOOPUV_VERTSEL);
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
          if (BM_elem_flag_test(l->v, BM_ELEM_SELECT) == 0) {
            luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
            SET_FLAG_FROM_TEST(luv->flag, select, MLOOPUV_VERTSEL);
          }
        }
        /* BM_face_select_set(em->bm, efa, true); */
        BM_elem_flag_enable(efa, BM_ELEM_TAG);
      }
    }
  }

  /* re-select tagged faces */
  BM_mesh_elem_hflag_enable_test(em->bm, BM_FACE, BM_ELEM_SELECT, true, false, BM_ELEM_TAG);

  DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);

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

static bool uv_set_2d_cursor_poll(bContext *C)
{
  return ED_operator_uvedit_space_image(C) || ED_space_image_maskedit_poll(C) ||
         ED_space_image_paint_curve(C);
}

static int uv_set_2d_cursor_exec(bContext *C, wmOperator *op)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (!sima) {
    return OPERATOR_CANCELLED;
  }

  RNA_float_get_array(op->ptr, "location", sima->cursor);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_IMAGE, NULL);

  return OPERATOR_FINISHED;
}

static int uv_set_2d_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  float location[2];

  if (ar->regiontype == RGN_TYPE_WINDOW) {
    if (event->mval[1] <= 16) {
      SpaceImage *sima = CTX_wm_space_image(C);
      if (sima && ED_space_image_show_cache(sima)) {
        return OPERATOR_PASS_THROUGH;
      }
    }
  }

  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
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
  ot->poll = uv_set_2d_cursor_poll;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       NULL,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Cursor location in normalized (0.0-1.0) coordinates",
                       -10.0f,
                       10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Seam from UV Islands Operator
 * \{ */

static int uv_seams_from_islands_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int ret = OPERATOR_CANCELLED;
  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};
  const bool mark_seams = RNA_boolean_get(op->ptr, "mark_seams");
  const bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    Mesh *me = (Mesh *)ob->data;
    BMEditMesh *em = me->edit_mesh;
    BMesh *bm = em->bm;

    UvVertMap *vmap;
    BMEdge *editedge;
    BMIter iter;

    if (!EDBM_uv_check(em)) {
      continue;
    }
    ret = OPERATOR_FINISHED;

    /* This code sets editvert->tmp.l to the index. This will be useful later on. */
    BM_mesh_elem_table_ensure(bm, BM_FACE);
    vmap = BM_uv_vert_map_create(bm, limit, false, false);

    BM_ITER_MESH (editedge, &iter, bm, BM_EDGES_OF_MESH) {
      /* flags to determine if we uv is separated from first editface match */
      char separated1 = 0, separated2;
      /* set to denote edge must be flagged as seam */
      char faces_separated = 0;
      /* flag to keep track if uv1 is disconnected from first editface match */
      char v1coincident = 1;
      /* For use with v1coincident. v1coincident will change only if we've had commonFaces */
      int commonFaces = 0;

      BMFace *efa1, *efa2;

      UvMapVert *mv1, *mvinit1, *mv2, *mvinit2, *mviter;
      /* mv2cache stores the first of the list of coincident uv's for later comparison
       * mv2sep holds the last separator and is copied to mv2cache
       * when a hit is first found */
      UvMapVert *mv2cache = NULL, *mv2sep = NULL;

      mvinit1 = vmap->vert[BM_elem_index_get(editedge->v1)];
      if (mark_seams) {
        BM_elem_flag_disable(editedge, BM_ELEM_SEAM);
      }

      for (mv1 = mvinit1; mv1 && !faces_separated; mv1 = mv1->next) {
        if (mv1->separate && commonFaces) {
          v1coincident = 0;
        }

        separated2 = 0;
        efa1 = BM_face_at_index(bm, mv1->poly_index);
        mvinit2 = vmap->vert[BM_elem_index_get(editedge->v2)];

        for (mv2 = mvinit2; mv2; mv2 = mv2->next) {
          if (mv2->separate) {
            mv2sep = mv2;
          }

          efa2 = BM_face_at_index(bm, mv2->poly_index);
          if (efa1 == efa2) {
            /* if v1 is not coincident no point in comparing */
            if (v1coincident) {
              /* have we found previously anything? */
              if (mv2cache) {
                /* flag seam unless proved to be coincident with previous hit */
                separated2 = 1;
                for (mviter = mv2cache; mviter; mviter = mviter->next) {
                  if (mviter->separate && mviter != mv2cache) {
                    break;
                  }
                  /* coincident with previous hit, do not flag seam */
                  if (mviter == mv2) {
                    separated2 = 0;
                  }
                }
              }
              /* First hit case, store the hit in the cache */
              else {
                mv2cache = mv2sep;
                commonFaces = 1;
              }
            }
            else {
              separated1 = 1;
            }

            if (separated1 || separated2) {
              faces_separated = 1;
              break;
            }
          }
        }
      }

      if (faces_separated) {
        if (mark_seams) {
          BM_elem_flag_enable(editedge, BM_ELEM_SEAM);
        }
        if (mark_sharp) {
          BM_elem_flag_disable(editedge, BM_ELEM_SMOOTH);
        }
      }
    }

    BM_uv_vert_map_free(vmap);

    DEG_id_tag_update(&me->id, 0);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, me);
  }
  MEM_freeN(objects);

  return ret;
}

static void UV_OT_seams_from_islands(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Seams From Islands";
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
  ToolSettings *ts = scene->toolsettings;

  BMFace *efa;
  BMLoop *loop;
  BMIter iter, liter;

  const bool flag_set = !RNA_boolean_get(op->ptr, "clear");
  const bool synced_selection = (ts->uv_flag & UV_SYNC_SELECTION) != 0;

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      view_layer, ((View3D *)NULL), &objects_len);

  bool changed = false;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob = objects[ob_index];
    Mesh *me = (Mesh *)ob->data;
    BMEditMesh *em = me->edit_mesh;
    BMesh *bm = em->bm;

    if (synced_selection && (bm->totedgesel == 0)) {
      continue;
    }

    const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (loop, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_edge_select_test(scene, loop, cd_loop_uv_offset)) {
          BM_elem_flag_set(loop->e, BM_ELEM_SEAM, flag_set);
          changed = true;
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

static int uv_mark_seam_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
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
  WM_operatortype_append(UV_OT_select_all);
  WM_operatortype_append(UV_OT_select);
  WM_operatortype_append(UV_OT_select_loop);
  WM_operatortype_append(UV_OT_select_linked);
  WM_operatortype_append(UV_OT_select_linked_pick);
  WM_operatortype_append(UV_OT_select_split);
  WM_operatortype_append(UV_OT_select_pinned);
  WM_operatortype_append(UV_OT_select_box);
  WM_operatortype_append(UV_OT_select_lasso);
  WM_operatortype_append(UV_OT_select_circle);
  WM_operatortype_append(UV_OT_select_more);
  WM_operatortype_append(UV_OT_select_less);

  WM_operatortype_append(UV_OT_snap_cursor);
  WM_operatortype_append(UV_OT_snap_selected);

  WM_operatortype_append(UV_OT_align);

  WM_operatortype_append(UV_OT_stitch);

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

  WM_operatortype_append(UV_OT_reveal);
  WM_operatortype_append(UV_OT_hide);

  WM_operatortype_append(UV_OT_cursor_set);
}

void ED_keymap_uvedit(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(keyconf, "UV Editor", 0, 0);
  keymap->poll = ED_operator_uvedit_can_uv_sculpt;
}

/** \} */
