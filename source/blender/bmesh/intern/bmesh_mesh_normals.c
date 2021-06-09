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
 * \ingroup bmesh
 *
 * BM mesh normal calculation functions.
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_stack.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"

#include "intern/bmesh_private.h"

/* -------------------------------------------------------------------- */
/** \name Update Vertex & Face Normals
 * \{ */

/**
 * Helpers for #BM_mesh_normals_update and #BM_verts_calc_normal_vcos
 */

/* We use that existing internal API flag,
 * assuming no other tool using it would run concurrently to clnors editing. */
#define BM_LNORSPACE_UPDATE _FLAG_MF

typedef struct BMEdgesCalcVectorsData {
  /* Read-only data. */
  const float (*vcos)[3];

  /* Read-write data, but no need to protect it, no concurrency to fear here. */
  float (*edgevec)[3];
} BMEdgesCalcVectorsData;

static void bm_edge_calc_vectors_cb(void *userdata,
                                    MempoolIterData *mp_e,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMEdge *e = (BMEdge *)mp_e;
  /* The edge vector will not be needed when the edge has no radial. */
  if (e->l != NULL) {
    float(*edgevec)[3] = userdata;
    float *e_diff = edgevec[BM_elem_index_get(e)];
    sub_v3_v3v3(e_diff, e->v2->co, e->v1->co);
    normalize_v3(e_diff);
  }
}

static void bm_edge_calc_vectors_with_coords_cb(void *userdata,
                                                MempoolIterData *mp_e,
                                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMEdge *e = (BMEdge *)mp_e;
  /* The edge vector will not be needed when the edge has no radial. */
  if (e->l != NULL) {
    BMEdgesCalcVectorsData *data = userdata;
    float *e_diff = data->edgevec[BM_elem_index_get(e)];
    sub_v3_v3v3(
        e_diff, data->vcos[BM_elem_index_get(e->v2)], data->vcos[BM_elem_index_get(e->v1)]);
    normalize_v3(e_diff);
  }
}

static void bm_mesh_edges_calc_vectors(BMesh *bm, float (*edgevec)[3], const float (*vcos)[3])
{
  BM_mesh_elem_index_ensure(bm, BM_EDGE | (vcos ? BM_VERT : 0));

  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);
  settings.use_threading = bm->totedge >= BM_OMP_LIMIT;

  if (vcos == NULL) {
    BM_iter_parallel(bm, BM_EDGES_OF_MESH, bm_edge_calc_vectors_cb, edgevec, &settings);
  }
  else {
    BMEdgesCalcVectorsData data = {
        .edgevec = edgevec,
        .vcos = vcos,
    };
    BM_iter_parallel(bm, BM_EDGES_OF_MESH, bm_edge_calc_vectors_with_coords_cb, &data, &settings);
  }
}

typedef struct BMVertsCalcNormalsWithCoordsData {
  /* Read-only data. */
  const float (*fnos)[3];
  const float (*edgevec)[3];
  const float (*vcos)[3];

  /* Write data. */
  float (*vnos)[3];
} BMVertsCalcNormalsWithCoordsData;

BLI_INLINE void bm_vert_calc_normals_accum_loop(const BMLoop *l_iter,
                                                const float (*edgevec)[3],
                                                const float f_no[3],
                                                float v_no[3])
{
  /* Calculate the dot product of the two edges that meet at the loop's vertex. */
  const float *e1diff = edgevec[BM_elem_index_get(l_iter->prev->e)];
  const float *e2diff = edgevec[BM_elem_index_get(l_iter->e)];
  /* Edge vectors are calculated from e->v1 to e->v2, so adjust the dot product if one but not
   * both loops actually runs from from e->v2 to e->v1. */
  float dotprod = dot_v3v3(e1diff, e2diff);
  if ((l_iter->prev->e->v1 == l_iter->prev->v) ^ (l_iter->e->v1 == l_iter->v)) {
    dotprod = -dotprod;
  }
  const float fac = saacos(-dotprod);
  /* NAN detection, otherwise this is a degenerated case, ignore that vertex in this case. */
  if (fac == fac) { /* NAN detection. */
    madd_v3_v3fl(v_no, f_no, fac);
  }
}

static void bm_vert_calc_normals_impl(const float (*edgevec)[3], BMVert *v)
{
  float *v_no = v->no;
  zero_v3(v_no);
  BMEdge *e_first = v->e;
  if (e_first != NULL) {
    BMEdge *e_iter = e_first;
    do {
      BMLoop *l_first = e_iter->l;
      if (l_first != NULL) {
        BMLoop *l_iter = l_first;
        do {
          if (l_iter->v == v) {
            bm_vert_calc_normals_accum_loop(l_iter, edgevec, l_iter->f->no, v_no);
          }
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
    } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);

    if (LIKELY(normalize_v3(v_no) != 0.0f)) {
      return;
    }
  }
  /* Fallback normal. */
  normalize_v3_v3(v_no, v->co);
}

static void bm_vert_calc_normals_cb(void *userdata,
                                    MempoolIterData *mp_v,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  const float(*edgevec)[3] = userdata;
  BMVert *v = (BMVert *)mp_v;
  bm_vert_calc_normals_impl(edgevec, v);
}

static void bm_vert_calc_normals_with_coords(BMVert *v, BMVertsCalcNormalsWithCoordsData *data)
{
  float *v_no = data->vnos[BM_elem_index_get(v)];
  zero_v3(v_no);

  /* Loop over edges. */
  BMEdge *e_first = v->e;
  if (e_first != NULL) {
    BMEdge *e_iter = e_first;
    do {
      BMLoop *l_first = e_iter->l;
      if (l_first != NULL) {
        BMLoop *l_iter = l_first;
        do {
          if (l_iter->v == v) {
            bm_vert_calc_normals_accum_loop(
                l_iter, data->edgevec, data->fnos[BM_elem_index_get(l_iter->f)], v_no);
          }
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
    } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);

    if (LIKELY(normalize_v3(v_no) != 0.0f)) {
      return;
    }
  }
  /* Fallback normal. */
  normalize_v3_v3(v_no, data->vcos[BM_elem_index_get(v)]);
}

static void bm_vert_calc_normals_with_coords_cb(void *userdata,
                                                MempoolIterData *mp_v,
                                                const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMVertsCalcNormalsWithCoordsData *data = userdata;
  BMVert *v = (BMVert *)mp_v;
  bm_vert_calc_normals_with_coords(v, data);
}

static void bm_mesh_verts_calc_normals(BMesh *bm,
                                       const float (*edgevec)[3],
                                       const float (*fnos)[3],
                                       const float (*vcos)[3],
                                       float (*vnos)[3])
{
  BM_mesh_elem_index_ensure(bm, (BM_EDGE | BM_FACE) | ((vnos || vcos) ? BM_VERT : 0));

  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);
  settings.use_threading = bm->totvert >= BM_OMP_LIMIT;

  if (vcos == NULL) {
    BM_iter_parallel(bm, BM_VERTS_OF_MESH, bm_vert_calc_normals_cb, (void *)edgevec, &settings);
  }
  else {
    BLI_assert(!ELEM(NULL, fnos, vnos));
    BMVertsCalcNormalsWithCoordsData data = {
        .edgevec = edgevec,
        .fnos = fnos,
        .vcos = vcos,
        .vnos = vnos,
    };
    BM_iter_parallel(bm, BM_VERTS_OF_MESH, bm_vert_calc_normals_with_coords_cb, &data, &settings);
  }
}

static void bm_face_calc_normals_cb(void *UNUSED(userdata),
                                    MempoolIterData *mp_f,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMFace *f = (BMFace *)mp_f;

  BM_face_calc_normal(f, f->no);
}

/**
 * \brief BMesh Compute Normals
 *
 * Updates the normals of a mesh.
 */
void BM_mesh_normals_update(BMesh *bm)
{
  float(*edgevec)[3] = MEM_mallocN(sizeof(*edgevec) * bm->totedge, __func__);

  /* Parallel mempool iteration does not allow generating indices inline anymore. */
  BM_mesh_elem_index_ensure(bm, (BM_EDGE | BM_FACE));

  /* Calculate all face normals. */
  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);
  settings.use_threading = bm->totedge >= BM_OMP_LIMIT;

  BM_iter_parallel(bm, BM_FACES_OF_MESH, bm_face_calc_normals_cb, NULL, &settings);

  bm_mesh_edges_calc_vectors(bm, edgevec, NULL);

  /* Add weighted face normals to vertices, and normalize vert normals. */
  bm_mesh_verts_calc_normals(bm, edgevec, NULL, NULL, NULL);
  MEM_freeN(edgevec);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Vertex & Face Normals (Partial Updates)
 * \{ */

static void bm_partial_faces_parallel_range_calc_normals_cb(
    void *userdata, const int iter, const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMFace *f = ((BMFace **)userdata)[iter];
  BM_face_calc_normal(f, f->no);
}

static void bm_partial_edges_parallel_range_calc_vectors_cb(
    void *userdata, const int iter, const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMEdge *e = ((BMEdge **)((void **)userdata)[0])[iter];
  float *r_edgevec = ((float(*)[3])((void **)userdata)[1])[iter];
  sub_v3_v3v3(r_edgevec, e->v1->co, e->v2->co);
  normalize_v3(r_edgevec);
}

static void bm_partial_verts_parallel_range_calc_normal_cb(
    void *userdata, const int iter, const TaskParallelTLS *__restrict UNUSED(tls))
{
  BMVert *v = ((BMVert **)((void **)userdata)[0])[iter];
  const float(*edgevec)[3] = (const float(*)[3])((void **)userdata)[1];
  bm_vert_calc_normals_impl(edgevec, v);
}

/**
 * A version of #BM_mesh_normals_update that updates a subset of geometry,
 * used to avoid the overhead of updating everything.
 */
void BM_mesh_normals_update_with_partial(BMesh *bm, const BMPartialUpdate *bmpinfo)
{
  BLI_assert(bmpinfo->params.do_normals);

  BMVert **verts = bmpinfo->verts;
  BMEdge **edges = bmpinfo->edges;
  BMFace **faces = bmpinfo->faces;
  const int verts_len = bmpinfo->verts_len;
  const int edges_len = bmpinfo->edges_len;
  const int faces_len = bmpinfo->faces_len;

  float(*edgevec)[3] = MEM_mallocN(sizeof(*edgevec) * edges_len, __func__);

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);

  /* Faces. */
  BLI_task_parallel_range(
      0, faces_len, faces, bm_partial_faces_parallel_range_calc_normals_cb, &settings);

  /* Temporarily override the edge indices,
   * storing the correct indices in the case they're not dirty.
   *
   * \note in most cases indices are modified and #BMesh.elem_index_dirty is set.
   * This is an exceptional case where indices are restored because the worst case downside
   * of marking the edge indices dirty would require a full loop over all edges to
   * correct the indices in other functions which need them to be valid.
   * When moving a few vertices on a high poly mesh setting and restoring connected
   * edges has very little overhead compared with restoring all edge indices. */
  int *edge_index_value = NULL;
  if ((bm->elem_index_dirty & BM_EDGE) == 0) {
    edge_index_value = MEM_mallocN(sizeof(*edge_index_value) * edges_len, __func__);

    for (int i = 0; i < edges_len; i++) {
      BMEdge *e = edges[i];
      edge_index_value[i] = BM_elem_index_get(e);
      BM_elem_index_set(e, i); /* set_dirty! (restore before this function exits). */
    }
  }
  else {
    for (int i = 0; i < edges_len; i++) {
      BMEdge *e = edges[i];
      BM_elem_index_set(e, i); /* set_dirty! (already dirty) */
    }
  }

  {
    /* Verts. */

    /* Compute normalized direction vectors for each edge.
     * Directions will be used for calculating the weights of the face normals on the vertex
     * normals. */
    void *data[2] = {edges, edgevec};
    BLI_task_parallel_range(
        0, edges_len, data, bm_partial_edges_parallel_range_calc_vectors_cb, &settings);

    /* Calculate vertex normals. */
    data[0] = verts;
    BLI_task_parallel_range(
        0, verts_len, data, bm_partial_verts_parallel_range_calc_normal_cb, &settings);
  }

  if (edge_index_value != NULL) {
    for (int i = 0; i < edges_len; i++) {
      BMEdge *e = edges[i];
      BM_elem_index_set(e, edge_index_value[i]); /* set_ok (restore) */
    }

    MEM_freeN(edge_index_value);
  }

  MEM_freeN(edgevec);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Vertex & Face Normals (Custom Coords)
 * \{ */

/**
 * \brief BMesh Compute Normals from/to external data.
 *
 * Computes the vertex normals of a mesh into vnos,
 * using given vertex coordinates (vcos) and polygon normals (fnos).
 */
void BM_verts_calc_normal_vcos(BMesh *bm,
                               const float (*fnos)[3],
                               const float (*vcos)[3],
                               float (*vnos)[3])
{
  float(*edgevec)[3] = MEM_mallocN(sizeof(*edgevec) * bm->totedge, __func__);

  /* Compute normalized direction vectors for each edge.
   * Directions will be used for calculating the weights of the face normals on the vertex normals.
   */
  bm_mesh_edges_calc_vectors(bm, edgevec, vcos);

  /* Add weighted face normals to vertices, and normalize vert normals. */
  bm_mesh_verts_calc_normals(bm, edgevec, fnos, vcos, vnos);
  MEM_freeN(edgevec);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tagging Utility Functions
 * \{ */

void BM_normals_loops_edges_tag(BMesh *bm, const bool do_edges)
{
  BMFace *f;
  BMEdge *e;
  BMIter fiter, eiter;
  BMLoop *l_curr, *l_first;

  if (do_edges) {
    int index_edge;
    BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, index_edge) {
      BMLoop *l_a, *l_b;

      BM_elem_index_set(e, index_edge); /* set_inline */
      BM_elem_flag_disable(e, BM_ELEM_TAG);
      if (BM_edge_loop_pair(e, &l_a, &l_b)) {
        if (BM_elem_flag_test(e, BM_ELEM_SMOOTH) && l_a->v != l_b->v) {
          BM_elem_flag_enable(e, BM_ELEM_TAG);
        }
      }
    }
    bm->elem_index_dirty &= ~BM_EDGE;
  }

  int index_face, index_loop = 0;
  BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, index_face) {
    BM_elem_index_set(f, index_face); /* set_inline */
    l_curr = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_elem_index_set(l_curr, index_loop++); /* set_inline */
      BM_elem_flag_disable(l_curr, BM_ELEM_TAG);
    } while ((l_curr = l_curr->next) != l_first);
  }
  bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);
}

/**
 * Helpers for #BM_mesh_loop_normals_update and #BM_loops_calc_normal_vcos
 */
static void bm_mesh_edges_sharp_tag(BMesh *bm,
                                    const float (*vnos)[3],
                                    const float (*fnos)[3],
                                    float (*r_lnos)[3],
                                    const float split_angle,
                                    const bool do_sharp_edges_tag)
{
  BMIter eiter;
  BMEdge *e;
  int i;

  const bool check_angle = (split_angle < (float)M_PI);
  const float split_angle_cos = check_angle ? cosf(split_angle) : -1.0f;

  {
    char htype = BM_VERT | BM_LOOP;
    if (fnos) {
      htype |= BM_FACE;
    }
    BM_mesh_elem_index_ensure(bm, htype);
  }

  /* This first loop checks which edges are actually smooth,
   * and pre-populate lnos with vnos (as if they were all smooth). */
  BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
    BMLoop *l_a, *l_b;

    BM_elem_index_set(e, i);              /* set_inline */
    BM_elem_flag_disable(e, BM_ELEM_TAG); /* Clear tag (means edge is sharp). */

    /* An edge with only two loops, might be smooth... */
    if (BM_edge_loop_pair(e, &l_a, &l_b)) {
      bool is_angle_smooth = true;
      if (check_angle) {
        const float *no_a = fnos ? fnos[BM_elem_index_get(l_a->f)] : l_a->f->no;
        const float *no_b = fnos ? fnos[BM_elem_index_get(l_b->f)] : l_b->f->no;
        is_angle_smooth = (dot_v3v3(no_a, no_b) >= split_angle_cos);
      }

      /* We only tag edges that are *really* smooth:
       * If the angle between both its polys' normals is below split_angle value,
       * and it is tagged as such,
       * and both its faces are smooth,
       * and both its faces have compatible (non-flipped) normals,
       * i.e. both loops on the same edge do not share the same vertex.
       */
      if (BM_elem_flag_test(e, BM_ELEM_SMOOTH) && BM_elem_flag_test(l_a->f, BM_ELEM_SMOOTH) &&
          BM_elem_flag_test(l_b->f, BM_ELEM_SMOOTH) && l_a->v != l_b->v) {
        if (is_angle_smooth) {
          const float *no;
          BM_elem_flag_enable(e, BM_ELEM_TAG);

          /* linked vertices might be fully smooth, copy their normals to loop ones. */
          if (r_lnos) {
            no = vnos ? vnos[BM_elem_index_get(l_a->v)] : l_a->v->no;
            copy_v3_v3(r_lnos[BM_elem_index_get(l_a)], no);
            no = vnos ? vnos[BM_elem_index_get(l_b->v)] : l_b->v->no;
            copy_v3_v3(r_lnos[BM_elem_index_get(l_b)], no);
          }
        }
        else if (do_sharp_edges_tag) {
          /* Note that we do not care about the other sharp-edge cases
           * (sharp poly, non-manifold edge, etc.),
           * only tag edge as sharp when it is due to angle threshold. */
          BM_elem_flag_disable(e, BM_ELEM_SMOOTH);
        }
      }
    }
  }

  bm->elem_index_dirty &= ~BM_EDGE;
}

/**
 * Define sharp edges as needed to mimic 'autosmooth' from angle threshold.
 *
 * Used when defining an empty custom loop normals data layer,
 * to keep same shading as with auto-smooth!
 */
void BM_edges_sharp_from_angle_set(BMesh *bm, const float split_angle)
{
  if (split_angle >= (float)M_PI) {
    /* Nothing to do! */
    return;
  }

  bm_mesh_edges_sharp_tag(bm, NULL, NULL, NULL, split_angle, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Normals Calculation API
 * \{ */

/**
 * Check whether given loop is part of an unknown-so-far cyclic smooth fan, or not.
 * Needed because cyclic smooth fans have no obvious 'entry point',
 * and yet we need to walk them once, and only once.
 */
bool BM_loop_check_cyclic_smooth_fan(BMLoop *l_curr)
{
  BMLoop *lfan_pivot_next = l_curr;
  BMEdge *e_next = l_curr->e;

  BLI_assert(!BM_elem_flag_test(lfan_pivot_next, BM_ELEM_TAG));
  BM_elem_flag_enable(lfan_pivot_next, BM_ELEM_TAG);

  while (true) {
    /* Much simpler than in sibling code with basic Mesh data! */
    lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot_next, &e_next);

    if (!lfan_pivot_next || !BM_elem_flag_test(e_next, BM_ELEM_TAG)) {
      /* Sharp loop/edge, so not a cyclic smooth fan... */
      return false;
    }
    /* Smooth loop/edge... */
    if (BM_elem_flag_test(lfan_pivot_next, BM_ELEM_TAG)) {
      if (lfan_pivot_next == l_curr) {
        /* We walked around a whole cyclic smooth fan
         * without finding any already-processed loop,
         * means we can use initial l_curr/l_prev edge as start for this smooth fan. */
        return true;
      }
      /* ... already checked in some previous looping, we can abort. */
      return false;
    }
    /* ... we can skip it in future, and keep checking the smooth fan. */
    BM_elem_flag_enable(lfan_pivot_next, BM_ELEM_TAG);
  }
}

/**
 * BMesh version of BKE_mesh_normals_loop_split() in mesh_evaluate.c
 * Will use first clnors_data array, and fallback to cd_loop_clnors_offset
 * (use NULL and -1 to not use clnors).
 *
 * \note This sets #BM_ELEM_TAG which is used in tool code (e.g. T84426).
 * we could add a low-level API flag for this, see #BM_ELEM_API_FLAG_ENABLE and friends.
 */
static void bm_mesh_loops_calc_normals(BMesh *bm,
                                       const float (*vcos)[3],
                                       const float (*fnos)[3],
                                       float (*r_lnos)[3],
                                       MLoopNorSpaceArray *r_lnors_spacearr,
                                       const short (*clnors_data)[2],
                                       const int cd_loop_clnors_offset,
                                       const bool do_rebuild)
{
  BMIter fiter;
  BMFace *f_curr;
  const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);

  MLoopNorSpaceArray _lnors_spacearr = {NULL};

  /* Temp normal stack. */
  BLI_SMALLSTACK_DECLARE(normal, float *);
  /* Temp clnors stack. */
  BLI_SMALLSTACK_DECLARE(clnors, short *);
  /* Temp edge vectors stack, only used when computing lnor spacearr. */
  BLI_Stack *edge_vectors = NULL;

  {
    char htype = 0;
    if (vcos) {
      htype |= BM_VERT;
    }
    /* Face/Loop indices are set inline below. */
    BM_mesh_elem_index_ensure(bm, htype);
  }

  if (!r_lnors_spacearr && has_clnors) {
    /* We need to compute lnor spacearr if some custom lnor data are given to us! */
    r_lnors_spacearr = &_lnors_spacearr;
  }
  if (r_lnors_spacearr) {
    BKE_lnor_spacearr_init(r_lnors_spacearr, bm->totloop, MLNOR_SPACEARR_BMLOOP_PTR);
    edge_vectors = BLI_stack_new(sizeof(float[3]), __func__);
  }

  /* Clear all loops' tags (means none are to be skipped for now). */
  int index_face, index_loop = 0;
  BM_ITER_MESH_INDEX (f_curr, &fiter, bm, BM_FACES_OF_MESH, index_face) {
    BMLoop *l_curr, *l_first;

    BM_elem_index_set(f_curr, index_face); /* set_inline */

    l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
    do {
      BM_elem_index_set(l_curr, index_loop++); /* set_inline */
      BM_elem_flag_disable(l_curr, BM_ELEM_TAG);
    } while ((l_curr = l_curr->next) != l_first);
  }
  bm->elem_index_dirty &= ~(BM_FACE | BM_LOOP);

  /* We now know edges that can be smoothed (they are tagged),
   * and edges that will be hard (they aren't).
   * Now, time to generate the normals.
   */
  BM_ITER_MESH (f_curr, &fiter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_curr, *l_first;

    l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
    do {
      if (do_rebuild && !BM_ELEM_API_FLAG_TEST(l_curr, BM_LNORSPACE_UPDATE) &&
          !(bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL)) {
        continue;
      }
      /* A smooth edge, we have to check for cyclic smooth fan case.
       * If we find a new, never-processed cyclic smooth fan, we can do it now using that loop/edge
       * as 'entry point', otherwise we can skip it. */

      /* Note: In theory, we could make bm_mesh_loop_check_cyclic_smooth_fan() store
       * mlfan_pivot's in a stack, to avoid having to fan again around
       * the vert during actual computation of clnor & clnorspace. However, this would complicate
       * the code, add more memory usage, and
       * BM_vert_step_fan_loop() is quite cheap in term of CPU cycles,
       * so really think it's not worth it. */
      if (BM_elem_flag_test(l_curr->e, BM_ELEM_TAG) &&
          (BM_elem_flag_test(l_curr, BM_ELEM_TAG) || !BM_loop_check_cyclic_smooth_fan(l_curr))) {
      }
      else if (!BM_elem_flag_test(l_curr->e, BM_ELEM_TAG) &&
               !BM_elem_flag_test(l_curr->prev->e, BM_ELEM_TAG)) {
        /* Simple case (both edges around that vertex are sharp in related polygon),
         * this vertex just takes its poly normal.
         */
        const int l_curr_index = BM_elem_index_get(l_curr);
        const float *no = fnos ? fnos[BM_elem_index_get(f_curr)] : f_curr->no;
        copy_v3_v3(r_lnos[l_curr_index], no);

        /* If needed, generate this (simple!) lnor space. */
        if (r_lnors_spacearr) {
          float vec_curr[3], vec_prev[3];
          MLoopNorSpace *lnor_space = BKE_lnor_space_create(r_lnors_spacearr);

          {
            const BMVert *v_pivot = l_curr->v;
            const float *co_pivot = vcos ? vcos[BM_elem_index_get(v_pivot)] : v_pivot->co;
            const BMVert *v_1 = BM_edge_other_vert(l_curr->e, v_pivot);
            const float *co_1 = vcos ? vcos[BM_elem_index_get(v_1)] : v_1->co;
            const BMVert *v_2 = BM_edge_other_vert(l_curr->prev->e, v_pivot);
            const float *co_2 = vcos ? vcos[BM_elem_index_get(v_2)] : v_2->co;

            sub_v3_v3v3(vec_curr, co_1, co_pivot);
            normalize_v3(vec_curr);
            sub_v3_v3v3(vec_prev, co_2, co_pivot);
            normalize_v3(vec_prev);
          }

          BKE_lnor_space_define(lnor_space, r_lnos[l_curr_index], vec_curr, vec_prev, NULL);
          /* We know there is only one loop in this space,
           * no need to create a linklist in this case... */
          BKE_lnor_space_add_loop(r_lnors_spacearr, lnor_space, l_curr_index, l_curr, true);

          if (has_clnors) {
            const short(*clnor)[2] = clnors_data ? &clnors_data[l_curr_index] :
                                                   (const void *)BM_ELEM_CD_GET_VOID_P(
                                                       l_curr, cd_loop_clnors_offset);
            BKE_lnor_space_custom_data_to_normal(lnor_space, *clnor, r_lnos[l_curr_index]);
          }
        }
      }
      /* We *do not need* to check/tag loops as already computed!
       * Due to the fact a loop only links to one of its two edges,
       * a same fan *will never be walked more than once!*
       * Since we consider edges having neighbor faces with inverted (flipped) normals as sharp,
       * we are sure that no fan will be skipped, even only considering the case
       * (sharp curr_edge, smooth prev_edge), and not the alternative
       * (smooth curr_edge, sharp prev_edge).
       * All this due/thanks to link between normals and loop ordering.
       */
      else {
        /* We have to fan around current vertex, until we find the other non-smooth edge,
         * and accumulate face normals into the vertex!
         * Note in case this vertex has only one sharp edge,
         * this is a waste because the normal is the same as the vertex normal,
         * but I do not see any easy way to detect that (would need to count number of sharp edges
         * per vertex, I doubt the additional memory usage would be worth it, especially as it
         * should not be a common case in real-life meshes anyway).
         */
        BMVert *v_pivot = l_curr->v;
        BMEdge *e_next;
        const BMEdge *e_org = l_curr->e;
        BMLoop *lfan_pivot, *lfan_pivot_next;
        int lfan_pivot_index;
        float lnor[3] = {0.0f, 0.0f, 0.0f};
        float vec_curr[3], vec_next[3], vec_org[3];

        /* We validate clnors data on the fly - cheapest way to do! */
        int clnors_avg[2] = {0, 0};
        const short(*clnor_ref)[2] = NULL;
        int clnors_nbr = 0;
        bool clnors_invalid = false;

        const float *co_pivot = vcos ? vcos[BM_elem_index_get(v_pivot)] : v_pivot->co;

        MLoopNorSpace *lnor_space = r_lnors_spacearr ? BKE_lnor_space_create(r_lnors_spacearr) :
                                                       NULL;

        BLI_assert((edge_vectors == NULL) || BLI_stack_is_empty(edge_vectors));

        lfan_pivot = l_curr;
        lfan_pivot_index = BM_elem_index_get(lfan_pivot);
        e_next = lfan_pivot->e; /* Current edge here, actually! */

        /* Only need to compute previous edge's vector once,
         * then we can just reuse old current one! */
        {
          const BMVert *v_2 = BM_edge_other_vert(e_next, v_pivot);
          const float *co_2 = vcos ? vcos[BM_elem_index_get(v_2)] : v_2->co;

          sub_v3_v3v3(vec_org, co_2, co_pivot);
          normalize_v3(vec_org);
          copy_v3_v3(vec_curr, vec_org);

          if (r_lnors_spacearr) {
            BLI_stack_push(edge_vectors, vec_org);
          }
        }

        while (true) {
          /* Much simpler than in sibling code with basic Mesh data! */
          lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot, &e_next);
          if (lfan_pivot_next) {
            BLI_assert(lfan_pivot_next->v == v_pivot);
          }
          else {
            /* next edge is non-manifold, we have to find it ourselves! */
            e_next = (lfan_pivot->e == e_next) ? lfan_pivot->prev->e : lfan_pivot->e;
          }

          /* Compute edge vector.
           * NOTE: We could pre-compute those into an array, in the first iteration,
           * instead of computing them twice (or more) here.
           * However, time gained is not worth memory and time lost,
           * given the fact that this code should not be called that much in real-life meshes.
           */
          {
            const BMVert *v_2 = BM_edge_other_vert(e_next, v_pivot);
            const float *co_2 = vcos ? vcos[BM_elem_index_get(v_2)] : v_2->co;

            sub_v3_v3v3(vec_next, co_2, co_pivot);
            normalize_v3(vec_next);
          }

          {
            /* Code similar to accumulate_vertex_normals_poly_v3. */
            /* Calculate angle between the two poly edges incident on this vertex. */
            const BMFace *f = lfan_pivot->f;
            const float fac = saacos(dot_v3v3(vec_next, vec_curr));
            const float *no = fnos ? fnos[BM_elem_index_get(f)] : f->no;
            /* Accumulate */
            madd_v3_v3fl(lnor, no, fac);

            if (has_clnors) {
              /* Accumulate all clnors, if they are not all equal we have to fix that! */
              const short(*clnor)[2] = clnors_data ? &clnors_data[lfan_pivot_index] :
                                                     (const void *)BM_ELEM_CD_GET_VOID_P(
                                                         lfan_pivot, cd_loop_clnors_offset);
              if (clnors_nbr) {
                clnors_invalid |= ((*clnor_ref)[0] != (*clnor)[0] ||
                                   (*clnor_ref)[1] != (*clnor)[1]);
              }
              else {
                clnor_ref = clnor;
              }
              clnors_avg[0] += (*clnor)[0];
              clnors_avg[1] += (*clnor)[1];
              clnors_nbr++;
              /* We store here a pointer to all custom lnors processed. */
              BLI_SMALLSTACK_PUSH(clnors, (short *)*clnor);
            }
          }

          /* We store here a pointer to all loop-normals processed. */
          BLI_SMALLSTACK_PUSH(normal, (float *)r_lnos[lfan_pivot_index]);

          if (r_lnors_spacearr) {
            /* Assign current lnor space to current 'vertex' loop. */
            BKE_lnor_space_add_loop(
                r_lnors_spacearr, lnor_space, lfan_pivot_index, lfan_pivot, false);
            if (e_next != e_org) {
              /* We store here all edges-normalized vectors processed. */
              BLI_stack_push(edge_vectors, vec_next);
            }
          }

          if (!BM_elem_flag_test(e_next, BM_ELEM_TAG) || (e_next == e_org)) {
            /* Next edge is sharp, we have finished with this fan of faces around this vert! */
            break;
          }

          /* Copy next edge vector to current one. */
          copy_v3_v3(vec_curr, vec_next);
          /* Next pivot loop to current one. */
          lfan_pivot = lfan_pivot_next;
          lfan_pivot_index = BM_elem_index_get(lfan_pivot);
        }

        {
          float lnor_len = normalize_v3(lnor);

          /* If we are generating lnor spacearr, we can now define the one for this fan. */
          if (r_lnors_spacearr) {
            if (UNLIKELY(lnor_len == 0.0f)) {
              /* Use vertex normal as fallback! */
              copy_v3_v3(lnor, r_lnos[lfan_pivot_index]);
              lnor_len = 1.0f;
            }

            BKE_lnor_space_define(lnor_space, lnor, vec_org, vec_next, edge_vectors);

            if (has_clnors) {
              if (clnors_invalid) {
                short *clnor;

                clnors_avg[0] /= clnors_nbr;
                clnors_avg[1] /= clnors_nbr;
                /* Fix/update all clnors of this fan with computed average value. */

                /* Prints continuously when merge custom normals, so commenting. */
                /* printf("Invalid clnors in this fan!\n"); */

                while ((clnor = BLI_SMALLSTACK_POP(clnors))) {
                  // print_v2("org clnor", clnor);
                  clnor[0] = (short)clnors_avg[0];
                  clnor[1] = (short)clnors_avg[1];
                }
                // print_v2("new clnors", clnors_avg);
              }
              else {
                /* We still have to consume the stack! */
                while (BLI_SMALLSTACK_POP(clnors)) {
                  /* pass */
                }
              }
              BKE_lnor_space_custom_data_to_normal(lnor_space, *clnor_ref, lnor);
            }
          }

          /* In case we get a zero normal here, just use vertex normal already set! */
          if (LIKELY(lnor_len != 0.0f)) {
            /* Copy back the final computed normal into all related loop-normals. */
            float *nor;

            while ((nor = BLI_SMALLSTACK_POP(normal))) {
              copy_v3_v3(nor, lnor);
            }
          }
          else {
            /* We still have to consume the stack! */
            while (BLI_SMALLSTACK_POP(normal)) {
              /* pass */
            }
          }
        }

        /* Tag related vertex as sharp, to avoid fanning around it again
         * (in case it was a smooth one). */
        if (r_lnors_spacearr) {
          BM_elem_flag_enable(l_curr->v, BM_ELEM_TAG);
        }
      }
    } while ((l_curr = l_curr->next) != l_first);
  }

  if (r_lnors_spacearr) {
    BLI_stack_free(edge_vectors);
    if (r_lnors_spacearr == &_lnors_spacearr) {
      BKE_lnor_spacearr_free(r_lnors_spacearr);
    }
  }
}

/* This threshold is a bit touchy (usual float precision issue), this value seems OK. */
#define LNOR_SPACE_TRIGO_THRESHOLD (1.0f - 1e-4f)

/**
 * Check each current smooth fan (one lnor space per smooth fan!), and if all its
 * matching custom lnors are not (enough) equal, add sharp edges as needed.
 */
static bool bm_mesh_loops_split_lnor_fans(BMesh *bm,
                                          MLoopNorSpaceArray *lnors_spacearr,
                                          const float (*new_lnors)[3])
{
  BLI_bitmap *done_loops = BLI_BITMAP_NEW((size_t)bm->totloop, __func__);
  bool changed = false;

  BLI_assert(lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);

  for (int i = 0; i < bm->totloop; i++) {
    if (!lnors_spacearr->lspacearr[i]) {
      /* This should not happen in theory, but in some rare case (probably ugly geometry)
       * we can get some NULL loopspacearr at this point. :/
       * Maybe we should set those loops' edges as sharp?
       */
      BLI_BITMAP_ENABLE(done_loops, i);
      if (G.debug & G_DEBUG) {
        printf("WARNING! Getting invalid NULL loop space for loop %d!\n", i);
      }
      continue;
    }

    if (!BLI_BITMAP_TEST(done_loops, i)) {
      /* Notes:
       * * In case of mono-loop smooth fan, we have nothing to do.
       * * Loops in this linklist are ordered (in reversed order compared to how they were
       *   discovered by BKE_mesh_normals_loop_split(), but this is not a problem).
       *   Which means if we find a mismatching clnor,
       *   we know all remaining loops will have to be in a new, different smooth fan/lnor space.
       * * In smooth fan case, we compare each clnor against a ref one,
       *   to avoid small differences adding up into a real big one in the end!
       */
      if (lnors_spacearr->lspacearr[i]->flags & MLNOR_SPACE_IS_SINGLE) {
        BLI_BITMAP_ENABLE(done_loops, i);
        continue;
      }

      LinkNode *loops = lnors_spacearr->lspacearr[i]->loops;
      BMLoop *prev_ml = NULL;
      const float *org_nor = NULL;

      while (loops) {
        BMLoop *ml = loops->link;
        const int lidx = BM_elem_index_get(ml);
        const float *nor = new_lnors[lidx];

        if (!org_nor) {
          org_nor = nor;
        }
        else if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          /* Current normal differs too much from org one, we have to tag the edge between
           * previous loop's face and current's one as sharp.
           * We know those two loops do not point to the same edge,
           * since we do not allow reversed winding in a same smooth fan.
           */
          BMEdge *e = (prev_ml->e == ml->prev->e) ? prev_ml->e : ml->e;

          BM_elem_flag_disable(e, BM_ELEM_TAG | BM_ELEM_SMOOTH);
          changed = true;

          org_nor = nor;
        }

        prev_ml = ml;
        loops = loops->next;
        BLI_BITMAP_ENABLE(done_loops, lidx);
      }

      /* We also have to check between last and first loops,
       * otherwise we may miss some sharp edges here!
       * This is just a simplified version of above while loop.
       * See T45984. */
      loops = lnors_spacearr->lspacearr[i]->loops;
      if (loops && org_nor) {
        BMLoop *ml = loops->link;
        const int lidx = BM_elem_index_get(ml);
        const float *nor = new_lnors[lidx];

        if (dot_v3v3(org_nor, nor) < LNOR_SPACE_TRIGO_THRESHOLD) {
          BMEdge *e = (prev_ml->e == ml->prev->e) ? prev_ml->e : ml->e;

          BM_elem_flag_disable(e, BM_ELEM_TAG | BM_ELEM_SMOOTH);
          changed = true;
        }
      }
    }
  }

  MEM_freeN(done_loops);
  return changed;
}

/**
 * Assign custom normal data from given normal vectors, averaging normals
 * from one smooth fan as necessary.
 */
static void bm_mesh_loops_assign_normal_data(BMesh *bm,
                                             MLoopNorSpaceArray *lnors_spacearr,
                                             short (*r_clnors_data)[2],
                                             const int cd_loop_clnors_offset,
                                             const float (*new_lnors)[3])
{
  BLI_bitmap *done_loops = BLI_BITMAP_NEW((size_t)bm->totloop, __func__);

  BLI_SMALLSTACK_DECLARE(clnors_data, short *);

  BLI_assert(lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);

  for (int i = 0; i < bm->totloop; i++) {
    if (!lnors_spacearr->lspacearr[i]) {
      BLI_BITMAP_ENABLE(done_loops, i);
      if (G.debug & G_DEBUG) {
        printf("WARNING! Still getting invalid NULL loop space in second loop for loop %d!\n", i);
      }
      continue;
    }

    if (!BLI_BITMAP_TEST(done_loops, i)) {
      /* Note we accumulate and average all custom normals in current smooth fan,
       * to avoid getting different clnors data (tiny differences in plain custom normals can
       * give rather huge differences in computed 2D factors).
       */
      LinkNode *loops = lnors_spacearr->lspacearr[i]->loops;

      if (lnors_spacearr->lspacearr[i]->flags & MLNOR_SPACE_IS_SINGLE) {
        BMLoop *ml = (BMLoop *)loops;
        const int lidx = BM_elem_index_get(ml);

        BLI_assert(lidx == i);

        const float *nor = new_lnors[lidx];
        short *clnor = r_clnors_data ? &r_clnors_data[lidx] :
                                       BM_ELEM_CD_GET_VOID_P(ml, cd_loop_clnors_offset);

        BKE_lnor_space_custom_normal_to_data(lnors_spacearr->lspacearr[i], nor, clnor);
        BLI_BITMAP_ENABLE(done_loops, i);
      }
      else {
        int nbr_nors = 0;
        float avg_nor[3];
        short clnor_data_tmp[2], *clnor_data;

        zero_v3(avg_nor);

        while (loops) {
          BMLoop *ml = loops->link;
          const int lidx = BM_elem_index_get(ml);
          const float *nor = new_lnors[lidx];
          short *clnor = r_clnors_data ? &r_clnors_data[lidx] :
                                         BM_ELEM_CD_GET_VOID_P(ml, cd_loop_clnors_offset);

          nbr_nors++;
          add_v3_v3(avg_nor, nor);
          BLI_SMALLSTACK_PUSH(clnors_data, clnor);

          loops = loops->next;
          BLI_BITMAP_ENABLE(done_loops, lidx);
        }

        mul_v3_fl(avg_nor, 1.0f / (float)nbr_nors);
        BKE_lnor_space_custom_normal_to_data(
            lnors_spacearr->lspacearr[i], avg_nor, clnor_data_tmp);

        while ((clnor_data = BLI_SMALLSTACK_POP(clnors_data))) {
          clnor_data[0] = clnor_data_tmp[0];
          clnor_data[1] = clnor_data_tmp[1];
        }
      }
    }
  }

  MEM_freeN(done_loops);
}

/**
 * Compute internal representation of given custom normals (as an array of float[2] or data layer).
 *
 * It also makes sure the mesh matches those custom normals, by marking new sharp edges to split
 * the smooth fans when loop normals for the same vertex are different, or averaging the normals
 * instead, depending on the do_split_fans parameter.
 */
static void bm_mesh_loops_custom_normals_set(BMesh *bm,
                                             const float (*vcos)[3],
                                             const float (*vnos)[3],
                                             const float (*fnos)[3],
                                             MLoopNorSpaceArray *r_lnors_spacearr,
                                             short (*r_clnors_data)[2],
                                             const int cd_loop_clnors_offset,
                                             float (*new_lnors)[3],
                                             const int cd_new_lnors_offset,
                                             bool do_split_fans)
{
  BMFace *f;
  BMLoop *l;
  BMIter liter, fiter;
  float(*cur_lnors)[3] = MEM_mallocN(sizeof(*cur_lnors) * bm->totloop, __func__);

  BKE_lnor_spacearr_clear(r_lnors_spacearr);

  /* Tag smooth edges and set lnos from vnos when they might be completely smooth...
   * When using custom loop normals, disable the angle feature! */
  bm_mesh_edges_sharp_tag(bm, vnos, fnos, cur_lnors, (float)M_PI, false);

  /* Finish computing lnos by accumulating face normals
   * in each fan of faces defined by sharp edges. */
  bm_mesh_loops_calc_normals(
      bm, vcos, fnos, cur_lnors, r_lnors_spacearr, r_clnors_data, cd_loop_clnors_offset, false);

  /* Extract new normals from the data layer if necessary. */
  float(*custom_lnors)[3] = new_lnors;

  if (new_lnors == NULL) {
    custom_lnors = MEM_mallocN(sizeof(*new_lnors) * bm->totloop, __func__);

    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        const float *normal = BM_ELEM_CD_GET_VOID_P(l, cd_new_lnors_offset);
        copy_v3_v3(custom_lnors[BM_elem_index_get(l)], normal);
      }
    }
  }

  /* Validate the new normals. */
  for (int i = 0; i < bm->totloop; i++) {
    if (is_zero_v3(custom_lnors[i])) {
      copy_v3_v3(custom_lnors[i], cur_lnors[i]);
    }
    else {
      normalize_v3(custom_lnors[i]);
    }
  }

  /* Now, check each current smooth fan (one lnor space per smooth fan!),
   * and if all its matching custom lnors are not equal, add sharp edges as needed. */
  if (do_split_fans && bm_mesh_loops_split_lnor_fans(bm, r_lnors_spacearr, custom_lnors)) {
    /* If any sharp edges were added, run bm_mesh_loops_calc_normals() again to get lnor
     * spacearr/smooth fans matching the given custom lnors. */
    BKE_lnor_spacearr_clear(r_lnors_spacearr);

    bm_mesh_loops_calc_normals(
        bm, vcos, fnos, cur_lnors, r_lnors_spacearr, r_clnors_data, cd_loop_clnors_offset, false);
  }

  /* And we just have to convert plain object-space custom normals to our
   * lnor space-encoded ones. */
  bm_mesh_loops_assign_normal_data(
      bm, r_lnors_spacearr, r_clnors_data, cd_loop_clnors_offset, custom_lnors);

  MEM_freeN(cur_lnors);

  if (custom_lnors != new_lnors) {
    MEM_freeN(custom_lnors);
  }
}

static void bm_mesh_loops_calc_normals_no_autosmooth(BMesh *bm,
                                                     const float (*vnos)[3],
                                                     const float (*fnos)[3],
                                                     float (*r_lnos)[3])
{
  BMIter fiter;
  BMFace *f_curr;

  {
    char htype = BM_LOOP;
    if (vnos) {
      htype |= BM_VERT;
    }
    if (fnos) {
      htype |= BM_FACE;
    }
    BM_mesh_elem_index_ensure(bm, htype);
  }

  BM_ITER_MESH (f_curr, &fiter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_curr, *l_first;
    const bool is_face_flat = !BM_elem_flag_test(f_curr, BM_ELEM_SMOOTH);

    l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
    do {
      const float *no = is_face_flat ? (fnos ? fnos[BM_elem_index_get(f_curr)] : f_curr->no) :
                                       (vnos ? vnos[BM_elem_index_get(l_curr->v)] : l_curr->v->no);
      copy_v3_v3(r_lnos[BM_elem_index_get(l_curr)], no);

    } while ((l_curr = l_curr->next) != l_first);
  }
}

#if 0 /* Unused currently */
/**
 * \brief BMesh Compute Loop Normals
 *
 * Updates the loop normals of a mesh.
 * Assumes vertex and face normals are valid (else call BM_mesh_normals_update() first)!
 */
void BM_mesh_loop_normals_update(BMesh *bm,
                                 const bool use_split_normals,
                                 const float split_angle,
                                 float (*r_lnos)[3],
                                 MLoopNorSpaceArray *r_lnors_spacearr,
                                 const short (*clnors_data)[2],
                                 const int cd_loop_clnors_offset)
{
  const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);

  if (use_split_normals) {
    /* Tag smooth edges and set lnos from vnos when they might be completely smooth...
     * When using custom loop normals, disable the angle feature! */
    bm_mesh_edges_sharp_tag(bm, NULL, NULL, has_clnors ? (float)M_PI : split_angle, r_lnos);

    /* Finish computing lnos by accumulating face normals
     * in each fan of faces defined by sharp edges. */
    bm_mesh_loops_calc_normals(
        bm, NULL, NULL, r_lnos, r_lnors_spacearr, clnors_data, cd_loop_clnors_offset);
  }
  else {
    BLI_assert(!r_lnors_spacearr);
    bm_mesh_loops_calc_normals_no_autosmooth(bm, NULL, NULL, r_lnos);
  }
}
#endif

/**
 * \brief BMesh Compute Loop Normals from/to external data.
 *
 * Compute split normals, i.e. vertex normals associated with each poly (hence 'loop normals').
 * Useful to materialize sharp edges (or non-smooth faces) without actually modifying the geometry
 * (splitting edges).
 */
void BM_loops_calc_normal_vcos(BMesh *bm,
                               const float (*vcos)[3],
                               const float (*vnos)[3],
                               const float (*fnos)[3],
                               const bool use_split_normals,
                               const float split_angle,
                               float (*r_lnos)[3],
                               MLoopNorSpaceArray *r_lnors_spacearr,
                               short (*clnors_data)[2],
                               const int cd_loop_clnors_offset,
                               const bool do_rebuild)
{
  const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);

  if (use_split_normals) {
    /* Tag smooth edges and set lnos from vnos when they might be completely smooth...
     * When using custom loop normals, disable the angle feature! */
    bm_mesh_edges_sharp_tag(bm, vnos, fnos, r_lnos, has_clnors ? (float)M_PI : split_angle, false);

    /* Finish computing lnos by accumulating face normals
     * in each fan of faces defined by sharp edges. */
    bm_mesh_loops_calc_normals(
        bm, vcos, fnos, r_lnos, r_lnors_spacearr, clnors_data, cd_loop_clnors_offset, do_rebuild);
  }
  else {
    BLI_assert(!r_lnors_spacearr);
    bm_mesh_loops_calc_normals_no_autosmooth(bm, vnos, fnos, r_lnos);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Normal Space API
 * \{ */

void BM_lnorspacearr_store(BMesh *bm, float (*r_lnors)[3])
{
  BLI_assert(bm->lnor_spacearr != NULL);

  if (!CustomData_has_layer(&bm->ldata, CD_CUSTOMLOOPNORMAL)) {
    BM_data_layer_add(bm, &bm->ldata, CD_CUSTOMLOOPNORMAL);
  }

  int cd_loop_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);

  BM_loops_calc_normal_vcos(bm,
                            NULL,
                            NULL,
                            NULL,
                            true,
                            M_PI,
                            r_lnors,
                            bm->lnor_spacearr,
                            NULL,
                            cd_loop_clnors_offset,
                            false);
  bm->spacearr_dirty &= ~(BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL);
}

#define CLEAR_SPACEARRAY_THRESHOLD(x) ((x) / 2)

void BM_lnorspace_invalidate(BMesh *bm, const bool do_invalidate_all)
{
  if (bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL) {
    return;
  }
  if (do_invalidate_all || bm->totvertsel > CLEAR_SPACEARRAY_THRESHOLD(bm->totvert)) {
    bm->spacearr_dirty |= BM_SPACEARR_DIRTY_ALL;
    return;
  }
  if (bm->lnor_spacearr == NULL) {
    bm->spacearr_dirty |= BM_SPACEARR_DIRTY_ALL;
    return;
  }

  BMVert *v;
  BMLoop *l;
  BMIter viter, liter;
  /* Note: we could use temp tag of BMItem for that,
   * but probably better not use it in such a low-level func?
   * --mont29 */
  BLI_bitmap *done_verts = BLI_BITMAP_NEW(bm->totvert, __func__);

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  /* When we affect a given vertex, we may affect following smooth fans:
   *     - all smooth fans of said vertex;
   *     - all smooth fans of all immediate loop-neighbors vertices;
   * This can be simplified as 'all loops of selected vertices and their immediate neighbors'
   * need to be tagged for update.
   */
  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
        BM_ELEM_API_FLAG_ENABLE(l, BM_LNORSPACE_UPDATE);

        /* Note that we only handle unselected neighbor vertices here, main loop will take care of
         * selected ones. */
        if ((!BM_elem_flag_test(l->prev->v, BM_ELEM_SELECT)) &&
            !BLI_BITMAP_TEST(done_verts, BM_elem_index_get(l->prev->v))) {

          BMLoop *l_prev;
          BMIter liter_prev;
          BM_ITER_ELEM (l_prev, &liter_prev, l->prev->v, BM_LOOPS_OF_VERT) {
            BM_ELEM_API_FLAG_ENABLE(l_prev, BM_LNORSPACE_UPDATE);
          }
          BLI_BITMAP_ENABLE(done_verts, BM_elem_index_get(l_prev->v));
        }

        if ((!BM_elem_flag_test(l->next->v, BM_ELEM_SELECT)) &&
            !BLI_BITMAP_TEST(done_verts, BM_elem_index_get(l->next->v))) {

          BMLoop *l_next;
          BMIter liter_next;
          BM_ITER_ELEM (l_next, &liter_next, l->next->v, BM_LOOPS_OF_VERT) {
            BM_ELEM_API_FLAG_ENABLE(l_next, BM_LNORSPACE_UPDATE);
          }
          BLI_BITMAP_ENABLE(done_verts, BM_elem_index_get(l_next->v));
        }
      }

      BLI_BITMAP_ENABLE(done_verts, BM_elem_index_get(v));
    }
  }

  MEM_freeN(done_verts);
  bm->spacearr_dirty |= BM_SPACEARR_DIRTY;
}

void BM_lnorspace_rebuild(BMesh *bm, bool preserve_clnor)
{
  BLI_assert(bm->lnor_spacearr != NULL);

  if (!(bm->spacearr_dirty & (BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL))) {
    return;
  }
  BMFace *f;
  BMLoop *l;
  BMIter fiter, liter;

  float(*r_lnors)[3] = MEM_callocN(sizeof(*r_lnors) * bm->totloop, __func__);
  float(*oldnors)[3] = preserve_clnor ? MEM_mallocN(sizeof(*oldnors) * bm->totloop, __func__) :
                                        NULL;

  int cd_loop_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  if (preserve_clnor) {
    BLI_assert(bm->lnor_spacearr->lspacearr != NULL);

    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (BM_ELEM_API_FLAG_TEST(l, BM_LNORSPACE_UPDATE) ||
            bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL) {
          short(*clnor)[2] = BM_ELEM_CD_GET_VOID_P(l, cd_loop_clnors_offset);
          int l_index = BM_elem_index_get(l);

          BKE_lnor_space_custom_data_to_normal(
              bm->lnor_spacearr->lspacearr[l_index], *clnor, oldnors[l_index]);
        }
      }
    }
  }

  if (bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL) {
    BKE_lnor_spacearr_clear(bm->lnor_spacearr);
  }
  BM_loops_calc_normal_vcos(bm,
                            NULL,
                            NULL,
                            NULL,
                            true,
                            M_PI,
                            r_lnors,
                            bm->lnor_spacearr,
                            NULL,
                            cd_loop_clnors_offset,
                            true);
  MEM_freeN(r_lnors);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      if (BM_ELEM_API_FLAG_TEST(l, BM_LNORSPACE_UPDATE) ||
          bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL) {
        if (preserve_clnor) {
          short(*clnor)[2] = BM_ELEM_CD_GET_VOID_P(l, cd_loop_clnors_offset);
          int l_index = BM_elem_index_get(l);
          BKE_lnor_space_custom_normal_to_data(
              bm->lnor_spacearr->lspacearr[l_index], oldnors[l_index], *clnor);
        }
        BM_ELEM_API_FLAG_DISABLE(l, BM_LNORSPACE_UPDATE);
      }
    }
  }

  MEM_SAFE_FREE(oldnors);
  bm->spacearr_dirty &= ~(BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL);

#ifndef NDEBUG
  BM_lnorspace_err(bm);
#endif
}

/**
 * \warning This function sets #BM_ELEM_TAG on loops & edges via #bm_mesh_loops_calc_normals,
 * take care to run this before setting up tags.
 */
void BM_lnorspace_update(BMesh *bm)
{
  if (bm->lnor_spacearr == NULL) {
    bm->lnor_spacearr = MEM_callocN(sizeof(*bm->lnor_spacearr), __func__);
  }
  if (bm->lnor_spacearr->lspacearr == NULL) {
    float(*lnors)[3] = MEM_callocN(sizeof(*lnors) * bm->totloop, __func__);

    BM_lnorspacearr_store(bm, lnors);

    MEM_freeN(lnors);
  }
  else if (bm->spacearr_dirty & (BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL)) {
    BM_lnorspace_rebuild(bm, false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Normal Edit Data Array API
 *
 * Utilities for creating/freeing #BMLoopNorEditDataArray.
 * \{ */

/**
 * Auxiliary function only used by rebuild to detect if any spaces were not marked as invalid.
 * Reports error if any of the lnor spaces change after rebuilding, meaning that all the possible
 * lnor spaces to be rebuilt were not correctly marked.
 */
#ifndef NDEBUG
void BM_lnorspace_err(BMesh *bm)
{
  bm->spacearr_dirty |= BM_SPACEARR_DIRTY_ALL;
  bool clear = true;

  MLoopNorSpaceArray *temp = MEM_callocN(sizeof(*temp), __func__);
  temp->lspacearr = NULL;

  BKE_lnor_spacearr_init(temp, bm->totloop, MLNOR_SPACEARR_BMLOOP_PTR);

  int cd_loop_clnors_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
  float(*lnors)[3] = MEM_callocN(sizeof(*lnors) * bm->totloop, __func__);
  BM_loops_calc_normal_vcos(
      bm, NULL, NULL, NULL, true, M_PI, lnors, temp, NULL, cd_loop_clnors_offset, true);

  for (int i = 0; i < bm->totloop; i++) {
    int j = 0;
    j += compare_ff(
        temp->lspacearr[i]->ref_alpha, bm->lnor_spacearr->lspacearr[i]->ref_alpha, 1e-4f);
    j += compare_ff(
        temp->lspacearr[i]->ref_beta, bm->lnor_spacearr->lspacearr[i]->ref_beta, 1e-4f);
    j += compare_v3v3(
        temp->lspacearr[i]->vec_lnor, bm->lnor_spacearr->lspacearr[i]->vec_lnor, 1e-4f);
    j += compare_v3v3(
        temp->lspacearr[i]->vec_ortho, bm->lnor_spacearr->lspacearr[i]->vec_ortho, 1e-4f);
    j += compare_v3v3(
        temp->lspacearr[i]->vec_ref, bm->lnor_spacearr->lspacearr[i]->vec_ref, 1e-4f);

    if (j != 5) {
      clear = false;
      break;
    }
  }
  BKE_lnor_spacearr_free(temp);
  MEM_freeN(temp);
  MEM_freeN(lnors);
  BLI_assert(clear);

  bm->spacearr_dirty &= ~BM_SPACEARR_DIRTY_ALL;
}
#endif

static void bm_loop_normal_mark_indiv_do_loop(BMLoop *l,
                                              BLI_bitmap *loops,
                                              MLoopNorSpaceArray *lnor_spacearr,
                                              int *totloopsel,
                                              const bool do_all_loops_of_vert)
{
  if (l != NULL) {
    const int l_idx = BM_elem_index_get(l);

    if (!BLI_BITMAP_TEST(loops, l_idx)) {
      /* If vert and face selected share a loop, mark it for editing. */
      BLI_BITMAP_ENABLE(loops, l_idx);
      (*totloopsel)++;

      if (do_all_loops_of_vert) {
        /* If required, also mark all loops shared by that vertex.
         * This is needed when loop spaces may change
         * (i.e. when some faces or edges might change of smooth/sharp status). */
        BMIter liter;
        BMLoop *lfan;
        BM_ITER_ELEM (lfan, &liter, l->v, BM_LOOPS_OF_VERT) {
          const int lfan_idx = BM_elem_index_get(lfan);
          if (!BLI_BITMAP_TEST(loops, lfan_idx)) {
            BLI_BITMAP_ENABLE(loops, lfan_idx);
            (*totloopsel)++;
          }
        }
      }
      else {
        /* Mark all loops in same loop normal space (aka smooth fan). */
        if ((lnor_spacearr->lspacearr[l_idx]->flags & MLNOR_SPACE_IS_SINGLE) == 0) {
          for (LinkNode *node = lnor_spacearr->lspacearr[l_idx]->loops; node; node = node->next) {
            const int lfan_idx = BM_elem_index_get((BMLoop *)node->link);
            if (!BLI_BITMAP_TEST(loops, lfan_idx)) {
              BLI_BITMAP_ENABLE(loops, lfan_idx);
              (*totloopsel)++;
            }
          }
        }
      }
    }
  }
}

/* Mark the individual clnors to be edited, if multiple selection methods are used. */
static int bm_loop_normal_mark_indiv(BMesh *bm, BLI_bitmap *loops, const bool do_all_loops_of_vert)
{
  BMEditSelection *ese, *ese_prev;
  int totloopsel = 0;

  const bool sel_verts = (bm->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool sel_edges = (bm->selectmode & SCE_SELECT_EDGE) != 0;
  const bool sel_faces = (bm->selectmode & SCE_SELECT_FACE) != 0;
  const bool use_sel_face_history = sel_faces && (sel_edges || sel_verts);

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  BLI_assert(bm->lnor_spacearr != NULL);
  BLI_assert(bm->lnor_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);

  if (use_sel_face_history) {
    /* Using face history allows to select a single loop from a single face...
     * Note that this is O(n^2) piece of code,
     * but it is not designed to be used with huge selection sets,
     * rather with only a few items selected at most.*/
    /* Goes from last selected to the first selected element. */
    for (ese = bm->selected.last; ese; ese = ese->prev) {
      if (ese->htype == BM_FACE) {
        /* If current face is selected,
         * then any verts to be edited must have been selected before it. */
        for (ese_prev = ese->prev; ese_prev; ese_prev = ese_prev->prev) {
          if (ese_prev->htype == BM_VERT) {
            bm_loop_normal_mark_indiv_do_loop(
                BM_face_vert_share_loop((BMFace *)ese->ele, (BMVert *)ese_prev->ele),
                loops,
                bm->lnor_spacearr,
                &totloopsel,
                do_all_loops_of_vert);
          }
          else if (ese_prev->htype == BM_EDGE) {
            BMEdge *e = (BMEdge *)ese_prev->ele;
            bm_loop_normal_mark_indiv_do_loop(BM_face_vert_share_loop((BMFace *)ese->ele, e->v1),
                                              loops,
                                              bm->lnor_spacearr,
                                              &totloopsel,
                                              do_all_loops_of_vert);

            bm_loop_normal_mark_indiv_do_loop(BM_face_vert_share_loop((BMFace *)ese->ele, e->v2),
                                              loops,
                                              bm->lnor_spacearr,
                                              &totloopsel,
                                              do_all_loops_of_vert);
          }
        }
      }
    }
  }
  else {
    if (sel_faces) {
      /* Only select all loops of selected faces. */
      BMLoop *l;
      BMFace *f;
      BMIter liter, fiter;
      BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
            bm_loop_normal_mark_indiv_do_loop(
                l, loops, bm->lnor_spacearr, &totloopsel, do_all_loops_of_vert);
          }
        }
      }
    }
    if (sel_edges) {
      /* Only select all loops of selected edges. */
      BMLoop *l;
      BMEdge *e;
      BMIter liter, eiter;
      BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
            bm_loop_normal_mark_indiv_do_loop(
                l, loops, bm->lnor_spacearr, &totloopsel, do_all_loops_of_vert);
            /* Loops actually 'have' two edges, or said otherwise, a selected edge actually selects
             * *two* loops in each of its faces. We have to find the other one too. */
            if (BM_vert_in_edge(e, l->next->v)) {
              bm_loop_normal_mark_indiv_do_loop(
                  l->next, loops, bm->lnor_spacearr, &totloopsel, do_all_loops_of_vert);
            }
            else {
              BLI_assert(BM_vert_in_edge(e, l->prev->v));
              bm_loop_normal_mark_indiv_do_loop(
                  l->prev, loops, bm->lnor_spacearr, &totloopsel, do_all_loops_of_vert);
            }
          }
        }
      }
    }
    if (sel_verts) {
      /* Select all loops of selected verts. */
      BMLoop *l;
      BMVert *v;
      BMIter liter, viter;
      BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
          BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
            bm_loop_normal_mark_indiv_do_loop(
                l, loops, bm->lnor_spacearr, &totloopsel, do_all_loops_of_vert);
          }
        }
      }
    }
  }

  return totloopsel;
}

static void loop_normal_editdata_init(
    BMesh *bm, BMLoopNorEditData *lnor_ed, BMVert *v, BMLoop *l, const int offset)
{
  BLI_assert(bm->lnor_spacearr != NULL);
  BLI_assert(bm->lnor_spacearr->lspacearr != NULL);

  const int l_index = BM_elem_index_get(l);
  short *clnors_data = BM_ELEM_CD_GET_VOID_P(l, offset);

  lnor_ed->loop_index = l_index;
  lnor_ed->loop = l;

  float custom_normal[3];
  BKE_lnor_space_custom_data_to_normal(
      bm->lnor_spacearr->lspacearr[l_index], clnors_data, custom_normal);

  lnor_ed->clnors_data = clnors_data;
  copy_v3_v3(lnor_ed->nloc, custom_normal);
  copy_v3_v3(lnor_ed->niloc, custom_normal);

  lnor_ed->loc = v->co;
}

BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init(BMesh *bm,
                                                           const bool do_all_loops_of_vert)
{
  BMLoop *l;
  BMVert *v;
  BMIter liter, viter;

  int totloopsel = 0;

  BLI_assert(bm->spacearr_dirty == 0);

  BMLoopNorEditDataArray *lnors_ed_arr = MEM_callocN(sizeof(*lnors_ed_arr), __func__);
  lnors_ed_arr->lidx_to_lnor_editdata = MEM_callocN(
      sizeof(*lnors_ed_arr->lidx_to_lnor_editdata) * bm->totloop, __func__);

  if (!CustomData_has_layer(&bm->ldata, CD_CUSTOMLOOPNORMAL)) {
    BM_data_layer_add(bm, &bm->ldata, CD_CUSTOMLOOPNORMAL);
  }
  const int cd_custom_normal_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  BLI_bitmap *loops = BLI_BITMAP_NEW(bm->totloop, __func__);

  /* This function define loop normals to edit, based on selection modes and history. */
  totloopsel = bm_loop_normal_mark_indiv(bm, loops, do_all_loops_of_vert);

  if (totloopsel) {
    BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata = MEM_mallocN(
        sizeof(*lnor_ed) * totloopsel, __func__);

    BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
      BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
        if (BLI_BITMAP_TEST(loops, BM_elem_index_get(l))) {
          loop_normal_editdata_init(bm, lnor_ed, v, l, cd_custom_normal_offset);
          lnors_ed_arr->lidx_to_lnor_editdata[BM_elem_index_get(l)] = lnor_ed;
          lnor_ed++;
        }
      }
    }
    lnors_ed_arr->totloop = totloopsel;
  }

  MEM_freeN(loops);
  lnors_ed_arr->cd_custom_normal_offset = cd_custom_normal_offset;
  return lnors_ed_arr;
}

void BM_loop_normal_editdata_array_free(BMLoopNorEditDataArray *lnors_ed_arr)
{
  MEM_SAFE_FREE(lnors_ed_arr->lnor_editdata);
  MEM_SAFE_FREE(lnors_ed_arr->lidx_to_lnor_editdata);
  MEM_freeN(lnors_ed_arr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Custom Normals / Vector Layer Conversion
 * \{ */

/**
 * \warning This function sets #BM_ELEM_TAG on loops & edges via #bm_mesh_loops_calc_normals,
 * take care to run this before setting up tags.
 */
bool BM_custom_loop_normals_to_vector_layer(BMesh *bm)
{
  BMFace *f;
  BMLoop *l;
  BMIter liter, fiter;

  if (!CustomData_has_layer(&bm->ldata, CD_CUSTOMLOOPNORMAL)) {
    return false;
  }

  BM_lnorspace_update(bm);
  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  /* Create a loop normal layer. */
  if (!CustomData_has_layer(&bm->ldata, CD_NORMAL)) {
    BM_data_layer_add(bm, &bm->ldata, CD_NORMAL);

    CustomData_set_layer_flag(&bm->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }

  const int cd_custom_normal_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
  const int cd_normal_offset = CustomData_get_offset(&bm->ldata, CD_NORMAL);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      const int l_index = BM_elem_index_get(l);
      const short *clnors_data = BM_ELEM_CD_GET_VOID_P(l, cd_custom_normal_offset);
      float *normal = BM_ELEM_CD_GET_VOID_P(l, cd_normal_offset);

      BKE_lnor_space_custom_data_to_normal(
          bm->lnor_spacearr->lspacearr[l_index], clnors_data, normal);
    }
  }

  return true;
}

void BM_custom_loop_normals_from_vector_layer(BMesh *bm, bool add_sharp_edges)
{
  if (!CustomData_has_layer(&bm->ldata, CD_CUSTOMLOOPNORMAL) ||
      !CustomData_has_layer(&bm->ldata, CD_NORMAL)) {
    return;
  }

  const int cd_custom_normal_offset = CustomData_get_offset(&bm->ldata, CD_CUSTOMLOOPNORMAL);
  const int cd_normal_offset = CustomData_get_offset(&bm->ldata, CD_NORMAL);

  if (bm->lnor_spacearr == NULL) {
    bm->lnor_spacearr = MEM_callocN(sizeof(*bm->lnor_spacearr), __func__);
  }

  bm_mesh_loops_custom_normals_set(bm,
                                   NULL,
                                   NULL,
                                   NULL,
                                   bm->lnor_spacearr,
                                   NULL,
                                   cd_custom_normal_offset,
                                   NULL,
                                   cd_normal_offset,
                                   add_sharp_edges);

  bm->spacearr_dirty &= ~(BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL);
}

/** \} */
