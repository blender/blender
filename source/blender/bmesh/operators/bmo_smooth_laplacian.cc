/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Advanced smoothing.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_geom.h"
#include "BLI_math_vector.h"

#include "eigen_capi.h"

#include "bmesh.hh"

#include "intern/bmesh_operators_private.hh" /* own include */

// #define SMOOTH_LAPLACIAN_AREA_FACTOR 4.0f  /* UNUSED */
// #define SMOOTH_LAPLACIAN_EDGE_FACTOR 2.0f  /* UNUSED */
#define SMOOTH_LAPLACIAN_MAX_EDGE_PERCENTAGE 1.8f
#define SMOOTH_LAPLACIAN_MIN_EDGE_PERCENTAGE 0.15f

struct BLaplacianSystem {
  float *eweights;      /* Length weights per Edge. */
  float (*fweights)[3]; /* Cotangent weights per loop. */
  float *ring_areas;    /* Total area per ring. */
  float *vlengths;      /* Total sum of lengths(edges) per vertex. */
  float *vweights;      /* Total sum of weights per vertex. */
  int numEdges;         /* Number of edges. */
  int numLoops;         /* Number of loops. */
  int numVerts;         /* Number of verts. */
  bool *zerola;         /* Is zero area or length. */

  /* Pointers to data. */
  BMesh *bm;
  BMOperator *op;
  LinearSolver *context;

  /* Data. */
  float min_area;
};
using LaplacianSystem = BLaplacianSystem;

static bool vert_is_boundary(BMVert *v);
static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numLoops, int a_numVerts);
static void init_laplacian_matrix(LaplacianSystem *sys);
static void delete_laplacian_system(LaplacianSystem *sys);
static void delete_void_pointer(void *data);
static void fill_laplacian_matrix(LaplacianSystem *sys);
static void memset_laplacian_system(LaplacianSystem *sys, int val);
static void validate_solution(
    LaplacianSystem *sys, int usex, int usey, int usez, int preserve_volume);
static void volume_preservation(
    BMOperator *op, float vini, float vend, int usex, int usey, int usez);

static void delete_void_pointer(void *data)
{
  if (data) {
    MEM_freeN(data);
  }
}

static void delete_laplacian_system(LaplacianSystem *sys)
{
  delete_void_pointer(sys->eweights);
  delete_void_pointer(sys->fweights);
  delete_void_pointer(sys->ring_areas);
  delete_void_pointer(sys->vlengths);
  delete_void_pointer(sys->vweights);
  delete_void_pointer(sys->zerola);
  if (sys->context) {
    EIG_linear_solver_delete(sys->context);
  }
  sys->bm = nullptr;
  sys->op = nullptr;
  MEM_freeN(sys);
}

static void memset_laplacian_system(LaplacianSystem *sys, int val)
{
  memset(sys->eweights, val, sizeof(float) * sys->numEdges);
  memset(sys->fweights, val, sizeof(float[3]) * sys->numLoops);
  memset(sys->ring_areas, val, sizeof(float) * sys->numVerts);
  memset(sys->vlengths, val, sizeof(float) * sys->numVerts);
  memset(sys->vweights, val, sizeof(float) * sys->numVerts);
  memset(sys->zerola, val, sizeof(bool) * sys->numVerts);
}

static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numLoops, int a_numVerts)
{
  LaplacianSystem *sys;
  sys = MEM_callocN<LaplacianSystem>("ModLaplSmoothSystem");
  sys->numEdges = a_numEdges;
  sys->numLoops = a_numLoops;
  sys->numVerts = a_numVerts;

  sys->eweights = MEM_calloc_arrayN<float>(sys->numEdges, "ModLaplSmoothEWeight");
  if (!sys->eweights) {
    delete_laplacian_system(sys);
    return nullptr;
  }

  sys->fweights = static_cast<float (*)[3]>(
      MEM_callocN(sizeof(float[3]) * sys->numLoops, "ModLaplSmoothFWeight"));
  if (!sys->fweights) {
    delete_laplacian_system(sys);
    return nullptr;
  }

  sys->ring_areas = MEM_calloc_arrayN<float>(sys->numVerts, "ModLaplSmoothRingAreas");
  if (!sys->ring_areas) {
    delete_laplacian_system(sys);
    return nullptr;
  }

  sys->vlengths = MEM_calloc_arrayN<float>(sys->numVerts, "ModLaplSmoothVlengths");
  if (!sys->vlengths) {
    delete_laplacian_system(sys);
    return nullptr;
  }

  sys->vweights = MEM_calloc_arrayN<float>(sys->numVerts, "ModLaplSmoothVweights");
  if (!sys->vweights) {
    delete_laplacian_system(sys);
    return nullptr;
  }

  sys->zerola = MEM_calloc_arrayN<bool>(sys->numVerts, "ModLaplSmoothZeloa");
  if (!sys->zerola) {
    delete_laplacian_system(sys);
    return nullptr;
  }

  return sys;
}

/**
 * Compute weight between vertex v_i and all your neighbors
 * weight between v_i and v_neighbor
 * <pre>
 * Wij = cot(alpha) + cot(beta) / (4.0 * total area of all faces  * sum all weight)
 *
 *        v_i *
 *          / | \
 *         /  |  \
 *  v_beta*   |   * v_alpha
 *         \  |  /
 *          \ | /
 *            * v_neighbor
 * </pre>
 */

static void init_laplacian_matrix(LaplacianSystem *sys)
{
  BMEdge *e;
  BMFace *f;
  BMIter eiter;
  BMIter fiter;
  uint i;

  BM_ITER_MESH_INDEX (e, &eiter, sys->bm, BM_EDGES_OF_MESH, i) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT) || !BM_edge_is_boundary(e)) {
      continue;
    }

    const float *v1 = e->v1->co;
    const float *v2 = e->v2->co;
    const int idv1 = BM_elem_index_get(e->v1);
    const int idv2 = BM_elem_index_get(e->v2);

    float w1 = len_v3v3(v1, v2);
    if (w1 > sys->min_area) {
      w1 = 1.0f / w1;
      sys->eweights[i] = w1;
      sys->vlengths[idv1] += w1;
      sys->vlengths[idv2] += w1;
    }
    else {
      sys->zerola[idv1] = true;
      sys->zerola[idv2] = true;
    }
  }

  uint l_curr_index = 0;

  BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      l_curr_index += f->len;
      continue;
    }

    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter;

    l_iter = l_first;
    do {
      const int vi_prev = BM_elem_index_get(l_iter->prev->v);
      const int vi_curr = BM_elem_index_get(l_iter->v);
      const int vi_next = BM_elem_index_get(l_iter->next->v);

      const float *co_prev = l_iter->prev->v->co;
      const float *co_curr = l_iter->v->co;
      const float *co_next = l_iter->next->v->co;

      const float areaf = area_tri_v3(co_prev, co_curr, co_next);

      if (areaf < sys->min_area) {
        sys->zerola[vi_curr] = true;
      }

      sys->ring_areas[vi_prev] += areaf;
      sys->ring_areas[vi_curr] += areaf;
      sys->ring_areas[vi_next] += areaf;

      const float w1 = cotangent_tri_weight_v3(co_curr, co_next, co_prev) / 2.0f;
      const float w2 = cotangent_tri_weight_v3(co_next, co_prev, co_curr) / 2.0f;
      const float w3 = cotangent_tri_weight_v3(co_prev, co_curr, co_next) / 2.0f;

      sys->fweights[l_curr_index][0] += w1;
      sys->fweights[l_curr_index][1] += w2;
      sys->fweights[l_curr_index][2] += w3;

      sys->vweights[vi_prev] += w1 + w2;
      sys->vweights[vi_curr] += w2 + w3;
      sys->vweights[vi_next] += w1 + w3;
    } while ((void)(l_curr_index += 1), (l_iter = l_iter->next) != l_first);
  }
}

static void fill_laplacian_matrix(LaplacianSystem *sys)
{
  BMEdge *e;
  BMFace *f;
  BMIter eiter;
  BMIter fiter;
  int i;

  uint l_curr_index = 0;

  BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      l_curr_index += f->len;
      continue;
    }

    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    int vi_prev = BM_elem_index_get(l_iter->prev->v);
    int vi_curr = BM_elem_index_get(l_iter->v);

    bool ok_prev = (sys->zerola[vi_prev] == false) && !vert_is_boundary(l_iter->prev->v);
    bool ok_curr = (sys->zerola[vi_curr] == false) && !vert_is_boundary(l_iter->v);

    do {
      const int vi_next = BM_elem_index_get(l_iter->next->v);
      const bool ok_next = (sys->zerola[vi_next] == false) && !vert_is_boundary(l_iter->next->v);

      if (ok_prev) {
        EIG_linear_solver_matrix_add(sys->context,
                                     vi_prev,
                                     vi_curr,
                                     sys->fweights[l_curr_index][1] * sys->vweights[vi_prev]);
        EIG_linear_solver_matrix_add(sys->context,
                                     vi_prev,
                                     vi_next,
                                     sys->fweights[l_curr_index][0] * sys->vweights[vi_prev]);
      }
      if (ok_curr) {
        EIG_linear_solver_matrix_add(sys->context,
                                     vi_curr,
                                     vi_next,
                                     sys->fweights[l_curr_index][2] * sys->vweights[vi_curr]);
        EIG_linear_solver_matrix_add(sys->context,
                                     vi_curr,
                                     vi_prev,
                                     sys->fweights[l_curr_index][1] * sys->vweights[vi_curr]);
      }
      if (ok_next) {
        EIG_linear_solver_matrix_add(sys->context,
                                     vi_next,
                                     vi_curr,
                                     sys->fweights[l_curr_index][2] * sys->vweights[vi_next]);
        EIG_linear_solver_matrix_add(sys->context,
                                     vi_next,
                                     vi_prev,
                                     sys->fweights[l_curr_index][0] * sys->vweights[vi_next]);
      }

      vi_prev = vi_curr;
      vi_curr = vi_next;

      ok_prev = ok_curr;
      ok_curr = ok_next;

    } while ((void)(l_curr_index += 1), (l_iter = l_iter->next) != l_first);
  }
  BM_ITER_MESH_INDEX (e, &eiter, sys->bm, BM_EDGES_OF_MESH, i) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT) || !BM_edge_is_boundary(e)) {
      continue;
    }
    const uint idv1 = BM_elem_index_get(e->v1);
    const uint idv2 = BM_elem_index_get(e->v2);
    if (sys->zerola[idv1] == false && sys->zerola[idv2] == false) {
      EIG_linear_solver_matrix_add(
          sys->context, idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
      EIG_linear_solver_matrix_add(
          sys->context, idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
    }
  }
}

static bool vert_is_boundary(BMVert *v)
{
  BMEdge *ed;
  BMFace *f;
  BMIter ei;
  BMIter fi;
  BM_ITER_ELEM (ed, &ei, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(ed)) {
      return true;
    }
  }
  BM_ITER_ELEM (f, &fi, v, BM_FACES_OF_VERT) {
    if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      return true;
    }
  }
  return false;
}

static void volume_preservation(
    BMOperator *op, float vini, float vend, int usex, int usey, int usez)
{
  float beta;
  BMOIter siter;
  BMVert *v;

  if (vend != 0.0f) {
    beta = pow(vini / vend, 1.0f / 3.0f);
    BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
      if (usex) {
        v->co[0] *= beta;
      }
      if (usey) {
        v->co[1] *= beta;
      }
      if (usez) {
        v->co[2] *= beta;
      }
    }
  }
}

static void validate_solution(
    LaplacianSystem *sys, int usex, int usey, int usez, int preserve_volume)
{
  int m_vertex_id;
  float leni, lene;
  float vini, vend;
  float *vi1, *vi2, ve1[3], ve2[3];
  uint idv1, idv2;
  BMOIter siter;
  BMVert *v;
  BMEdge *e;
  BMIter eiter;

  BM_ITER_MESH (e, &eiter, sys->bm, BM_EDGES_OF_MESH) {
    idv1 = BM_elem_index_get(e->v1);
    idv2 = BM_elem_index_get(e->v2);
    vi1 = e->v1->co;
    vi2 = e->v2->co;
    ve1[0] = EIG_linear_solver_variable_get(sys->context, 0, idv1);
    ve1[1] = EIG_linear_solver_variable_get(sys->context, 1, idv1);
    ve1[2] = EIG_linear_solver_variable_get(sys->context, 2, idv1);
    ve2[0] = EIG_linear_solver_variable_get(sys->context, 0, idv2);
    ve2[1] = EIG_linear_solver_variable_get(sys->context, 1, idv2);
    ve2[2] = EIG_linear_solver_variable_get(sys->context, 2, idv2);
    leni = len_v3v3(vi1, vi2);
    lene = len_v3v3(ve1, ve2);
    if (lene > leni * SMOOTH_LAPLACIAN_MAX_EDGE_PERCENTAGE ||
        lene < leni * SMOOTH_LAPLACIAN_MIN_EDGE_PERCENTAGE)
    {
      sys->zerola[idv1] = true;
      sys->zerola[idv2] = true;
    }
  }

  if (preserve_volume) {
    vini = BM_mesh_calc_volume(sys->bm, false);
  }
  BMO_ITER (v, &siter, sys->op->slots_in, "verts", BM_VERT) {
    m_vertex_id = BM_elem_index_get(v);
    if (sys->zerola[m_vertex_id] == false) {
      if (usex) {
        v->co[0] = EIG_linear_solver_variable_get(sys->context, 0, m_vertex_id);
      }
      if (usey) {
        v->co[1] = EIG_linear_solver_variable_get(sys->context, 1, m_vertex_id);
      }
      if (usez) {
        v->co[2] = EIG_linear_solver_variable_get(sys->context, 2, m_vertex_id);
      }
    }
  }
  if (preserve_volume) {
    vend = BM_mesh_calc_volume(sys->bm, false);
    volume_preservation(sys->op, vini, vend, usex, usey, usez);
  }
}

void bmo_smooth_laplacian_vert_exec(BMesh *bm, BMOperator *op)
{
  int i;
  int m_vertex_id;
  bool usex, usey, usez, preserve_volume;
  float lambda_factor, lambda_border;
  float w;
  BMOIter siter;
  BMVert *v;
  LaplacianSystem *sys;

  if (bm->totface == 0) {
    return;
  }
  sys = init_laplacian_system(bm->totedge, bm->totloop, bm->totvert);
  if (!sys) {
    return;
  }
  sys->bm = bm;
  sys->op = op;

  memset_laplacian_system(sys, 0);

  BM_mesh_elem_index_ensure(bm, BM_VERT);
  lambda_factor = BMO_slot_float_get(op->slots_in, "lambda_factor");
  lambda_border = BMO_slot_float_get(op->slots_in, "lambda_border");
  sys->min_area = 0.00001f;
  usex = BMO_slot_bool_get(op->slots_in, "use_x");
  usey = BMO_slot_bool_get(op->slots_in, "use_y");
  usez = BMO_slot_bool_get(op->slots_in, "use_z");
  preserve_volume = BMO_slot_bool_get(op->slots_in, "preserve_volume");

  sys->context = EIG_linear_least_squares_solver_new(bm->totvert, bm->totvert, 3);

  for (i = 0; i < bm->totvert; i++) {
    EIG_linear_solver_variable_lock(sys->context, i);
  }
  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
    m_vertex_id = BM_elem_index_get(v);
    EIG_linear_solver_variable_unlock(sys->context, m_vertex_id);
    EIG_linear_solver_variable_set(sys->context, 0, m_vertex_id, v->co[0]);
    EIG_linear_solver_variable_set(sys->context, 1, m_vertex_id, v->co[1]);
    EIG_linear_solver_variable_set(sys->context, 2, m_vertex_id, v->co[2]);
  }

  init_laplacian_matrix(sys);
  BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
    m_vertex_id = BM_elem_index_get(v);
    EIG_linear_solver_right_hand_side_add(sys->context, 0, m_vertex_id, v->co[0]);
    EIG_linear_solver_right_hand_side_add(sys->context, 1, m_vertex_id, v->co[1]);
    EIG_linear_solver_right_hand_side_add(sys->context, 2, m_vertex_id, v->co[2]);
    i = m_vertex_id;
    if ((sys->zerola[i] == false) &&
        /* Non zero check is to account for vertices that aren't connected to a selected face.
         * Without this wire edges become `nan`, see #89214. */
        (sys->ring_areas[i] != 0.0f))
    {
      w = sys->vweights[i] * sys->ring_areas[i];
      sys->vweights[i] = (w == 0.0f) ? 0.0f : -lambda_factor / (4.0f * w);
      w = sys->vlengths[i];
      sys->vlengths[i] = (w == 0.0f) ? 0.0f : -lambda_border * 2.0f / w;

      if (!vert_is_boundary(v)) {
        EIG_linear_solver_matrix_add(
            sys->context, i, i, 1.0f + lambda_factor / (4.0f * sys->ring_areas[i]));
      }
      else {
        EIG_linear_solver_matrix_add(sys->context, i, i, 1.0f + lambda_border * 2.0f);
      }
    }
    else {
      EIG_linear_solver_matrix_add(sys->context, i, i, 1.0f);
    }
  }
  fill_laplacian_matrix(sys);

  if (EIG_linear_solver_solve(sys->context)) {
    validate_solution(sys, usex, usey, usez, preserve_volume);
  }

  delete_laplacian_system(sys);
}
