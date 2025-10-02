/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "DNA_object_types.h"

#include "ED_mesh.hh"
#include "ED_view3d.hh"

#include "bmesh.hh"

using blender::float3;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Mesh Element Pre-Select
 * Public API:
 *
 * #EDBM_preselect_elem_create
 * #EDBM_preselect_elem_destroy
 * #EDBM_preselect_elem_clear
 * #EDBM_preselect_elem_draw
 * #EDBM_preselect_elem_update_from_single
 *
 * \{ */

static void vcos_get(BMVert *v, float r_co[3], const Span<float3> vert_positions)
{
  if (!vert_positions.is_empty()) {
    copy_v3_v3(r_co, vert_positions[BM_elem_index_get(v)]);
  }
  else {
    copy_v3_v3(r_co, v->co);
  }
}

static void vcos_get_pair(BMVert *v[2], float r_cos[2][3], const Span<float3> vert_positions)
{
  if (!vert_positions.is_empty()) {
    for (int j = 0; j < 2; j++) {
      copy_v3_v3(r_cos[j], vert_positions[BM_elem_index_get(v[j])]);
    }
  }
  else {
    for (int j = 0; j < 2; j++) {
      copy_v3_v3(r_cos[j], v[j]->co);
    }
  }
}

struct EditMesh_PreSelElem {
  float (*edges)[2][3];
  int edges_len;

  float (*verts)[3];
  int verts_len;

  float (*preview_tris)[3][3];
  int preview_tris_len;
  float (*preview_lines)[2][3];
  int preview_lines_len;

  eEditMesh_PreSelPreviewAction preview_action;
};

void EDBM_preselect_action_set(EditMesh_PreSelElem *psel, eEditMesh_PreSelPreviewAction action)
{
  psel->preview_action = action;
}

eEditMesh_PreSelPreviewAction EDBM_preselect_action_get(EditMesh_PreSelElem *psel)
{
  return psel->preview_action;
}

EditMesh_PreSelElem *EDBM_preselect_elem_create()
{
  EditMesh_PreSelElem *psel = static_cast<EditMesh_PreSelElem *>(
      MEM_callocN(sizeof(*psel), __func__));
  psel->preview_action = PRESELECT_ACTION_TRANSFORM;
  return psel;
}

void EDBM_preselect_elem_destroy(EditMesh_PreSelElem *psel)
{
  EDBM_preselect_elem_clear(psel);
  EDBM_preselect_preview_clear(psel);
  MEM_freeN(psel);
}

void EDBM_preselect_preview_clear(EditMesh_PreSelElem *psel)
{
  MEM_SAFE_FREE(psel->preview_tris);
  psel->preview_tris_len = 0;

  MEM_SAFE_FREE(psel->preview_lines);
  psel->preview_lines_len = 0;
}

void EDBM_preselect_elem_clear(EditMesh_PreSelElem *psel)
{
  MEM_SAFE_FREE(psel->edges);
  psel->edges_len = 0;

  MEM_SAFE_FREE(psel->verts);
  psel->verts_len = 0;
}

enum class PreselectColor { Polygons, LinesOrPoints, Delete };
static void edbm_preselect_imm_color(PreselectColor preselect_color)
{
  switch (preselect_color) {
    case PreselectColor::Polygons:
      immUniformColor4ub(141, 171, 186, 100);
      break;
    case PreselectColor::LinesOrPoints:
      immUniformColor4ub(3, 161, 252, 200);
      break;
    case PreselectColor::Delete:
      immUniformColor4ub(252, 49, 10, 200);
      break;
  }
}

void EDBM_preselect_elem_draw(EditMesh_PreSelElem *psel, const float matrix[4][4])
{
  if ((psel->edges_len == 0) && (psel->verts_len == 0)) {
    return;
  }

  GPU_depth_test(GPU_DEPTH_NONE);

  GPU_matrix_push();
  GPU_matrix_mul(matrix);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  edbm_preselect_imm_color(PreselectColor::Polygons);
  if (psel->preview_action != PRESELECT_ACTION_TRANSFORM) {
    if (psel->preview_tris_len > 0) {
      immBegin(GPU_PRIM_TRIS, psel->preview_tris_len * 3);

      for (int i = 0; i < psel->preview_tris_len; i++) {
        immVertex3fv(pos, psel->preview_tris[i][0]);
        immVertex3fv(pos, psel->preview_tris[i][1]);
        immVertex3fv(pos, psel->preview_tris[i][2]);
      }
      immEnd();
    }

    if (psel->preview_lines_len > 0) {
      edbm_preselect_imm_color(PreselectColor::LinesOrPoints);
      GPU_line_width(2.0f);
      immBegin(GPU_PRIM_LINES, psel->preview_lines_len * 2);
      for (int i = 0; i < psel->preview_lines_len; i++) {
        immVertex3fv(pos, psel->preview_lines[i][0]);
        immVertex3fv(pos, psel->preview_lines[i][1]);
      }
      immEnd();
    }
  }

  edbm_preselect_imm_color(psel->preview_action == PRESELECT_ACTION_DELETE ?
                               PreselectColor::Delete :
                               PreselectColor::LinesOrPoints);

  if (psel->edges_len > 0) {
    GPU_line_width(3.0f);
    immBegin(GPU_PRIM_LINES, psel->edges_len * 2);

    for (int i = 0; i < psel->edges_len; i++) {
      immVertex3fv(pos, psel->edges[i][0]);
      immVertex3fv(pos, psel->edges[i][1]);
    }

    immEnd();
  }

  if (psel->verts_len > 0) {
    immUnbindProgram();
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_COLOR);
    edbm_preselect_imm_color(psel->preview_action == PRESELECT_ACTION_DELETE ?
                                 PreselectColor::Delete :
                                 PreselectColor::LinesOrPoints);
    GPU_point_size(4.0f);

    immBegin(GPU_PRIM_POINTS, psel->verts_len);

    for (int i = 0; i < psel->verts_len; i++) {
      immVertex3fv(pos, psel->verts[i]);
    }

    immEnd();
  }

  immUnbindProgram();

  GPU_matrix_pop();

  /* Reset default */
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
}

static void view3d_preselect_mesh_elem_update_from_vert(EditMesh_PreSelElem *psel,
                                                        BMesh * /*bm*/,
                                                        BMVert *eve,
                                                        const Span<float3> vert_positions)
{
  float (*verts)[3] = static_cast<float (*)[3]>(MEM_mallocN(sizeof(*psel->verts), __func__));
  vcos_get(eve, verts[0], vert_positions);
  psel->verts = verts;
  psel->verts_len = 1;
}

static void view3d_preselect_mesh_elem_update_from_edge(EditMesh_PreSelElem *psel,
                                                        BMesh * /*bm*/,
                                                        BMEdge *eed,
                                                        const Span<float3> vert_positions)
{
  float (*edges)[2][3] = static_cast<float (*)[2][3]>(MEM_mallocN(sizeof(*psel->edges), __func__));
  vcos_get_pair(&eed->v1, edges[0], vert_positions);
  psel->edges = edges;
  psel->edges_len = 1;
}

static void view3d_preselect_update_preview_triangle_from_vert(
    EditMesh_PreSelElem *psel, ViewContext *vc, BMesh * /*bm*/, BMVert *eed, const int mval[2])
{
  BMVert *v_act = eed;
  BMEdge *e_pair[2] = {nullptr};
  float center[3];

  if (v_act->e != nullptr) {
    for (uint allow_wire = 0; allow_wire < 2 && (e_pair[1] == nullptr); allow_wire++) {
      int i = 0;
      BMEdge *e_iter = v_act->e;
      do {
        if ((BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN) == false) &&
            (allow_wire ? BM_edge_is_wire(e_iter) : BM_edge_is_boundary(e_iter)))
        {
          if (i == 2) {
            e_pair[0] = e_pair[1] = nullptr;
            break;
          }
          e_pair[i++] = e_iter;
        }
      } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v_act)) != v_act->e);
    }
  }

  if (e_pair[1] != nullptr) {
    mul_v3_m4v3(center, vc->obedit->object_to_world().ptr(), v_act->co);
    ED_view3d_win_to_3d_int(vc->v3d, vc->region, center, mval, center);
    mul_m4_v3(vc->obedit->world_to_object().ptr(), center);

    psel->preview_tris = static_cast<float (*)[3][3]>(
        MEM_mallocN(sizeof(*psel->preview_tris) * 2, __func__));
    psel->preview_lines = static_cast<float (*)[2][3]>(
        MEM_mallocN(sizeof(*psel->preview_lines) * 4, __func__));

    copy_v3_v3(psel->preview_tris[0][0], e_pair[0]->v1->co);
    copy_v3_v3(psel->preview_tris[0][1], e_pair[0]->v2->co);
    copy_v3_v3(psel->preview_tris[0][2], center);

    copy_v3_v3(psel->preview_tris[1][0], e_pair[1]->v1->co);
    copy_v3_v3(psel->preview_tris[1][1], e_pair[1]->v2->co);
    copy_v3_v3(psel->preview_tris[1][2], center);

    copy_v3_v3(psel->preview_lines[0][0], e_pair[0]->v1->co);
    copy_v3_v3(psel->preview_lines[0][1], e_pair[0]->v2->co);

    copy_v3_v3(psel->preview_lines[1][0], e_pair[1]->v1->co);
    copy_v3_v3(psel->preview_lines[1][1], e_pair[1]->v2->co);

    copy_v3_v3(psel->preview_lines[2][0], center);
    if (e_pair[0]->v1 == v_act) {
      copy_v3_v3(psel->preview_lines[2][1], e_pair[0]->v2->co);
    }
    else {
      copy_v3_v3(psel->preview_lines[2][1], e_pair[0]->v1->co);
    }

    copy_v3_v3(psel->preview_lines[3][0], center);
    if (e_pair[1]->v1 == v_act) {
      copy_v3_v3(psel->preview_lines[3][1], e_pair[1]->v2->co);
    }
    else {
      copy_v3_v3(psel->preview_lines[3][1], e_pair[1]->v1->co);
    }
    psel->preview_tris_len = 2;
    psel->preview_lines_len = 4;
  }
}

static void view3d_preselect_update_preview_triangle_from_face(EditMesh_PreSelElem *psel,
                                                               ViewContext * /*vc*/,
                                                               BMesh * /*bm*/,
                                                               BMFace *efa,
                                                               const int /*mval*/[2])
{
  float (*preview_lines)[2][3] = static_cast<float (*)[2][3]>(
      MEM_mallocN(sizeof(*psel->edges) * efa->len, __func__));
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
  int i = 0;
  do {
    vcos_get_pair(&l_iter->e->v1, preview_lines[i++], {});
  } while ((l_iter = l_iter->next) != l_first);
  psel->preview_lines = preview_lines;
  psel->preview_lines_len = efa->len;
}

static void view3d_preselect_update_preview_triangle_from_edge(
    EditMesh_PreSelElem *psel, ViewContext *vc, BMesh * /*bm*/, BMEdge *eed, const int mval[2])
{
  float center[3];
  psel->preview_tris = static_cast<float (*)[3][3]>(
      MEM_mallocN(sizeof(*psel->preview_tris), __func__));
  psel->preview_lines = static_cast<float (*)[2][3]>(
      MEM_mallocN(sizeof(*psel->preview_lines) * 3, __func__));
  mid_v3_v3v3(center, eed->v1->co, eed->v2->co);
  mul_m4_v3(vc->obedit->object_to_world().ptr(), center);
  ED_view3d_win_to_3d_int(vc->v3d, vc->region, center, mval, center);
  mul_m4_v3(vc->obedit->world_to_object().ptr(), center);

  copy_v3_v3(psel->preview_tris[0][0], eed->v1->co);
  copy_v3_v3(psel->preview_tris[0][1], eed->v2->co);
  copy_v3_v3(psel->preview_tris[0][2], center);

  copy_v3_v3(psel->preview_lines[0][0], eed->v1->co);
  copy_v3_v3(psel->preview_lines[0][1], eed->v2->co);

  copy_v3_v3(psel->preview_lines[1][0], eed->v2->co);
  copy_v3_v3(psel->preview_lines[1][1], center);

  copy_v3_v3(psel->preview_lines[2][0], center);
  copy_v3_v3(psel->preview_lines[2][1], eed->v1->co);
  psel->preview_tris_len = 1;
  psel->preview_lines_len = 3;
}

static void view3d_preselect_mesh_elem_update_from_face(EditMesh_PreSelElem *psel,
                                                        BMesh * /*bm*/,
                                                        BMFace *efa,
                                                        const Span<float3> vert_positions)
{
  float (*edges)[2][3] = static_cast<float (*)[2][3]>(
      MEM_mallocN(sizeof(*psel->edges) * efa->len, __func__));
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
  int i = 0;
  do {
    vcos_get_pair(&l_iter->e->v1, edges[i++], vert_positions);
  } while ((l_iter = l_iter->next) != l_first);
  psel->edges = edges;
  psel->edges_len = efa->len;
}

void EDBM_preselect_elem_update_from_single(EditMesh_PreSelElem *psel,
                                            BMesh *bm,
                                            BMElem *ele,
                                            const Span<float3> vert_positions)
{
  EDBM_preselect_elem_clear(psel);

  if (!vert_positions.is_empty()) {
    BM_mesh_elem_index_ensure(bm, BM_VERT);
  }

  switch (ele->head.htype) {
    case BM_VERT:
      view3d_preselect_mesh_elem_update_from_vert(psel, bm, (BMVert *)ele, vert_positions);
      break;
    case BM_EDGE:
      view3d_preselect_mesh_elem_update_from_edge(psel, bm, (BMEdge *)ele, vert_positions);
      break;
    case BM_FACE:
      view3d_preselect_mesh_elem_update_from_face(psel, bm, (BMFace *)ele, vert_positions);
      break;
    default:
      BLI_assert(0);
  }
}

void EDBM_preselect_elem_update_preview(
    EditMesh_PreSelElem *psel, ViewContext *vc, BMesh *bm, BMElem *ele, const int mval[2])
{
  EDBM_preselect_preview_clear(psel);

  switch (ele->head.htype) {
    case BM_VERT:
      if (EDBM_preselect_action_get(psel) == PRESELECT_ACTION_CREATE) {
        view3d_preselect_update_preview_triangle_from_vert(psel, vc, bm, (BMVert *)ele, mval);
      }
      break;
    case BM_EDGE:
      view3d_preselect_update_preview_triangle_from_edge(psel, vc, bm, (BMEdge *)ele, mval);
      break;
    case BM_FACE:
      view3d_preselect_update_preview_triangle_from_face(psel, vc, bm, (BMFace *)ele, mval);
      break;
    default:
      BLI_assert(0);
  }
}

/** \} */
