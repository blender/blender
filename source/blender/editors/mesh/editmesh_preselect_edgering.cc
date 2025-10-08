/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_math_vector.h"
#include "BLI_stack.h"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "ED_mesh.hh"

#include "UI_resources.hh"

#include "bmesh.hh"

using blender::float3;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Mesh Edge Ring Pre-Select
 * Public API:
 *
 * #EDBM_preselect_edgering_create
 * #EDBM_preselect_edgering_destroy
 * #EDBM_preselect_edgering_clear
 * #EDBM_preselect_edgering_draw
 * #EDBM_preselect_edgering_update_from_edge
 *
 * \{ */

static void edgering_vcos_get(BMVert *v[2][2],
                              float r_cos[2][2][3],
                              const Span<float3> vert_positions)
{
  if (!vert_positions.is_empty()) {
    int j, k;
    for (j = 0; j < 2; j++) {
      for (k = 0; k < 2; k++) {
        copy_v3_v3(r_cos[j][k], vert_positions[BM_elem_index_get(v[j][k])]);
      }
    }
  }
  else {
    int j, k;
    for (j = 0; j < 2; j++) {
      for (k = 0; k < 2; k++) {
        copy_v3_v3(r_cos[j][k], v[j][k]->co);
      }
    }
  }
}

static void edgering_vcos_get_pair(BMVert *v[2],
                                   float r_cos[2][3],
                                   const Span<float3> vert_positions)
{
  if (!vert_positions.is_empty()) {
    int j;
    for (j = 0; j < 2; j++) {
      copy_v3_v3(r_cos[j], vert_positions[BM_elem_index_get(v[j])]);
    }
  }
  else {
    int j;
    for (j = 0; j < 2; j++) {
      copy_v3_v3(r_cos[j], v[j]->co);
    }
  }
}

/**
 * Given two opposite edges in a face, finds the ordering of their vertices so
 * that cut preview lines won't cross each other.
 */
static void edgering_find_order(BMEdge *eed_last, BMEdge *eed, BMVert *eve_last, BMVert *v[2][2])
{
  BMLoop *l = eed->l;

  /* find correct order for v[1] */
  if (!(BM_edge_in_face(eed, l->f) && BM_edge_in_face(eed_last, l->f))) {
    BMIter liter;
    BM_ITER_ELEM (l, &liter, l, BM_LOOPS_OF_LOOP) {
      if (BM_edge_in_face(eed, l->f) && BM_edge_in_face(eed_last, l->f)) {
        break;
      }
    }
  }

  /* this should never happen */
  if (!l) {
    v[0][0] = eed->v1;
    v[0][1] = eed->v2;
    v[1][0] = eed_last->v1;
    v[1][1] = eed_last->v2;
    return;
  }

  BMLoop *l_other = BM_loop_other_edge_loop(l, eed->v1);
  const bool rev = (l_other == l->prev);
  while (!ELEM(l_other->v, eed_last->v1, eed_last->v2)) {
    l_other = rev ? l_other->prev : l_other->next;
  }

  if (l_other->v == eve_last) {
    v[0][0] = eed->v1;
    v[0][1] = eed->v2;
  }
  else {
    v[0][0] = eed->v2;
    v[0][1] = eed->v1;
  }
}

struct EditMesh_PreSelEdgeRing {
  float (*edges)[2][3];
  int edges_len;

  float (*verts)[3];
  int verts_len;
};

EditMesh_PreSelEdgeRing *EDBM_preselect_edgering_create()
{
  EditMesh_PreSelEdgeRing *psel = static_cast<EditMesh_PreSelEdgeRing *>(
      MEM_callocN(sizeof(*psel), __func__));
  return psel;
}

void EDBM_preselect_edgering_destroy(EditMesh_PreSelEdgeRing *psel)
{
  EDBM_preselect_edgering_clear(psel);
  MEM_freeN(psel);
}

void EDBM_preselect_edgering_clear(EditMesh_PreSelEdgeRing *psel)
{
  MEM_SAFE_FREE(psel->edges);
  psel->edges_len = 0;

  MEM_SAFE_FREE(psel->verts);
  psel->verts_len = 0;
}

void EDBM_preselect_edgering_draw(EditMesh_PreSelEdgeRing *psel, const float matrix[4][4])
{
  if ((psel->edges_len == 0) && (psel->verts_len == 0)) {
    return;
  }

  GPU_depth_test(GPU_DEPTH_NONE);
  GPU_blend(GPU_BLEND_ALPHA);

  GPU_matrix_push();
  GPU_matrix_mul(matrix);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32_32);

  if (psel->edges_len > 0) {
    float viewport[4];
    GPU_viewport_size_get_f(viewport);

    immBindBuiltinProgram(GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR);
    immUniform2fv("viewportSize", &viewport[2]);
    immUniformThemeColor3(TH_GIZMO_PRIMARY);
    immUniform1f("lineWidth", U.pixelsize);
    immBegin(GPU_PRIM_LINES, psel->edges_len * 2);

    for (int i = 0; i < psel->edges_len; i++) {
      immVertex3fv(pos, psel->edges[i][0]);
      immVertex3fv(pos, psel->edges[i][1]);
    }

    immEnd();
    immUnbindProgram();
  }

  if (psel->verts_len > 0) {
    GPU_program_point_size(true);
    immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
    immUniformThemeColor3(TH_GIZMO_PRIMARY);

    /* Same size as an edit mode vertex */
    immUniform1f("size",
                 2.0 * U.pixelsize *
                     max_ff(1.0f, UI_GetThemeValuef(TH_VERTEX_SIZE) * float(M_SQRT2) / 2.0f));

    immBegin(GPU_PRIM_POINTS, psel->verts_len);

    for (int i = 0; i < psel->verts_len; i++) {
      immVertex3fv(pos, psel->verts[i]);
    }

    immEnd();
    immUnbindProgram();
    GPU_program_point_size(false);
  }

  GPU_matrix_pop();

  /* Reset default */
  GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  GPU_blend(GPU_BLEND_NONE);
}

static void view3d_preselect_mesh_edgering_update_verts_from_edge(
    EditMesh_PreSelEdgeRing *psel,
    BMesh * /*bm*/,
    BMEdge *eed_start,
    int previewlines,
    const Span<float3> vert_positions)
{
  float v_cos[2][3];
  float (*verts)[3];
  int i, tot = 0;

  verts = static_cast<float (*)[3]>(MEM_mallocN(sizeof(*psel->verts) * previewlines, __func__));

  edgering_vcos_get_pair(&eed_start->v1, v_cos, vert_positions);

  for (i = 1; i <= previewlines; i++) {
    const float fac = (i / (float(previewlines) + 1));
    interp_v3_v3v3(verts[tot], v_cos[0], v_cos[1], fac);
    tot++;
  }

  psel->verts = verts;
  psel->verts_len = previewlines;
}

static void view3d_preselect_mesh_edgering_update_edges_from_edge(
    EditMesh_PreSelEdgeRing *psel,
    BMesh *bm,
    BMEdge *eed_start,
    int previewlines,
    const Span<float3> vert_positions)
{
  BMWalker walker;
  BMEdge *eed, *eed_last;
  BMVert *v[2][2] = {{nullptr}}, *eve_last;
  float (*edges)[2][3] = nullptr;
  BLI_Stack *edge_stack;

  int i, tot = 0;

  BMW_init(&walker,
           bm,
           BMW_EDGERING,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_MASK_NOP,
           BMW_FLAG_TEST_HIDDEN,
           BMW_NIL_LAY);

  edge_stack = BLI_stack_new(sizeof(BMEdge *), __func__);

  eed_last = nullptr;
  for (eed = eed_last = static_cast<BMEdge *>(BMW_begin(&walker, eed_start)); eed;
       eed = static_cast<BMEdge *>(BMW_step(&walker)))
  {
    BLI_stack_push(edge_stack, &eed);
  }
  BMW_end(&walker);

  eed_start = *(BMEdge **)BLI_stack_peek(edge_stack);

  edges = static_cast<float (*)[2][3]>(MEM_mallocN(
      (sizeof(*edges) * (BLI_stack_count(edge_stack) + (eed_last != eed_start))) * previewlines,
      __func__));

  eve_last = nullptr;
  eed_last = nullptr;

  while (!BLI_stack_is_empty(edge_stack)) {
    BLI_stack_pop(edge_stack, &eed);

    if (eed_last) {
      if (eve_last) {
        v[1][0] = v[0][0];
        v[1][1] = v[0][1];
      }
      else {
        v[1][0] = eed_last->v1;
        v[1][1] = eed_last->v2;
        eve_last = eed_last->v1;
      }

      edgering_find_order(eed_last, eed, eve_last, v);
      eve_last = v[0][0];

      for (i = 1; i <= previewlines; i++) {
        const float fac = (i / (float(previewlines) + 1));
        float v_cos[2][2][3];

        edgering_vcos_get(v, v_cos, vert_positions);

        interp_v3_v3v3(edges[tot][0], v_cos[0][0], v_cos[0][1], fac);
        interp_v3_v3v3(edges[tot][1], v_cos[1][0], v_cos[1][1], fac);
        tot++;
      }
    }
    eed_last = eed;
  }

  if ((eed_last != eed_start) &&
#ifdef BMW_EDGERING_NGON
      BM_edge_share_face_check(eed_last, eed_start)
#else
      BM_edge_share_quad_check(eed_last, eed_start)
#endif
  )
  {
    v[1][0] = v[0][0];
    v[1][1] = v[0][1];

    edgering_find_order(eed_last, eed_start, eve_last, v);

    for (i = 1; i <= previewlines; i++) {
      const float fac = (i / (float(previewlines) + 1));
      float v_cos[2][2][3];

      if (!v[0][0] || !v[0][1] || !v[1][0] || !v[1][1]) {
        continue;
      }

      edgering_vcos_get(v, v_cos, vert_positions);

      interp_v3_v3v3(edges[tot][0], v_cos[0][0], v_cos[0][1], fac);
      interp_v3_v3v3(edges[tot][1], v_cos[1][0], v_cos[1][1], fac);
      tot++;
    }
  }

  BLI_stack_free(edge_stack);

  psel->edges = edges;
  psel->edges_len = tot;
}

void EDBM_preselect_edgering_update_from_edge(EditMesh_PreSelEdgeRing *psel,
                                              BMesh *bm,
                                              BMEdge *eed_start,
                                              int previewlines,
                                              const Span<float3> vert_positions)
{
  EDBM_preselect_edgering_clear(psel);

  if (!vert_positions.is_empty()) {
    BM_mesh_elem_index_ensure(bm, BM_VERT);
  }

  if (BM_edge_is_any_face_len_test(eed_start, 4)) {
    view3d_preselect_mesh_edgering_update_edges_from_edge(
        psel, bm, eed_start, previewlines, vert_positions);
  }
  else {
    view3d_preselect_mesh_edgering_update_verts_from_edge(
        psel, bm, eed_start, previewlines, vert_positions);
  }
}

/** \} */
