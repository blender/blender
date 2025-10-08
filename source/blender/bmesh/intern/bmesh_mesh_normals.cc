/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BM mesh normal calculation functions.
 *
 * \see mesh_normals.cc for the equivalent #Mesh functionality.
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_mesh.hh"

#include "intern/bmesh_private.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;

/* Smooth angle to use when tagging edges is disabled entirely. */
#define EDGE_TAG_FROM_SPLIT_ANGLE_BYPASS -FLT_MAX

static void bm_edge_tag_from_smooth_and_set_sharp(Span<float3> fnos,
                                                  BMEdge *e,
                                                  const float split_angle_cos);
static void bm_edge_tag_from_smooth(Span<float3> fnos, BMEdge *e, const float split_angle_cos);

/* -------------------------------------------------------------------- */
/** \name Update Vertex & Face Normals
 * \{ */

/**
 * Helpers for #BM_mesh_normals_update and #BM_verts_calc_normal_vcos
 */

/* We use that existing internal API flag,
 * assuming no other tool using it would run concurrently to clnors editing. */
#define BM_LNORSPACE_UPDATE _FLAG_MF

struct BMVertsCalcNormalsWithCoordsData {
  /* Read-only data. */
  Span<float3> fnos;
  Span<float3> vcos;

  /* Write data. */
  MutableSpan<float3> vnos;
};

BLI_INLINE void bm_vert_calc_normals_accum_loop(const BMLoop *l_iter,
                                                const float e1diff[3],
                                                const float e2diff[3],
                                                const float f_no[3],
                                                float v_no[3])
{
  /* Calculate the dot product of the two edges that meet at the loop's vertex. */
  /* Edge vectors are calculated from `e->v1` to `e->v2`, so adjust the dot product if one but not
   * both loops actually runs from `e->v2` to `e->v1`. */
  float dotprod = dot_v3v3(e1diff, e2diff);
  if ((l_iter->prev->e->v1 == l_iter->prev->v) ^ (l_iter->e->v1 == l_iter->v)) {
    dotprod = -dotprod;
  }
  const float fac = blender::math::safe_acos_approx(-dotprod);
  /* Shouldn't happen as normalizing edge-vectors cause degenerate values to be zeroed out. */
  BLI_assert(!isnan(fac));
  madd_v3_v3fl(v_no, f_no, fac);
}

static void bm_vert_calc_normals_impl(BMVert *v)
{
  /* NOTE(@ideasman42): Regarding redundant unit-length edge-vector calculation:
   *
   * This functions calculates unit-length edge-vector for every loop edge
   * in practice this means 2x `sqrt` calls per face-corner connected to each vertex.
   *
   * Previously (2.9x and older), the edge vectors were calculated and stored for reuse.
   * However the overhead of did not perform well (~16% slower - single & multi-threaded)
   * when compared with calculating the values as they are needed.
   *
   * For simple grid topologies this function calculates the edge-vectors 4x times.
   * There is some room for improved performance by storing the edge-vectors for reuse locally
   * in this function, reducing the number of redundant `sqrtf` in half (2x instead of 4x).
   * so face loops that share an edge would not calculate it multiple times.
   * From my tests the performance improvements are so small they're difficult to measure,
   * the time saved removing `sqrtf` calls is lost on storing and looking up the information,
   * even in the case of small inline lookup tables.
   *
   * Further, local data structures would need to support cases where
   * stack memory isn't sufficient - adding additional complexity for corner-cases
   * (a vertex that has thousands of connected edges for example).
   * Unless there are important use-cases that benefit from edge-vector caching,
   * keep this simple and calculate ~4x as many edge-vectors.
   *
   * In conclusion, the cost of caching & looking up edge-vectors both globally or per-vertex
   * doesn't save enough time to make it worthwhile.
   */

  float *v_no = v->no;
  zero_v3(v_no);

  BMEdge *e_first = v->e;
  if (e_first != nullptr) {
    float e1diff[3], e2diff[3];
    BMEdge *e_iter = e_first;
    do {
      BMLoop *l_first = e_iter->l;
      if (l_first != nullptr) {
        sub_v3_v3v3(e2diff, e_iter->v1->co, e_iter->v2->co);
        normalize_v3(e2diff);

        BMLoop *l_iter = l_first;
        do {
          if (l_iter->v == v) {
            BMEdge *e_prev = l_iter->prev->e;
            sub_v3_v3v3(e1diff, e_prev->v1->co, e_prev->v2->co);
            normalize_v3(e1diff);

            bm_vert_calc_normals_accum_loop(l_iter, e1diff, e2diff, l_iter->f->no, v_no);
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

static void bm_vert_calc_normals_cb(void * /*userdata*/,
                                    MempoolIterData *mp_v,
                                    const TaskParallelTLS *__restrict /*tls*/)
{
  BMVert *v = (BMVert *)mp_v;
  bm_vert_calc_normals_impl(v);
}

static void bm_vert_calc_normals_with_coords(BMVert *v, BMVertsCalcNormalsWithCoordsData *data)
{
  /* See #bm_vert_calc_normals_impl note on performance. */
  float *v_no = data->vnos[BM_elem_index_get(v)];
  zero_v3(v_no);

  /* Loop over edges. */
  BMEdge *e_first = v->e;
  if (e_first != nullptr) {
    float e1diff[3], e2diff[3];
    BMEdge *e_iter = e_first;
    do {
      BMLoop *l_first = e_iter->l;
      if (l_first != nullptr) {
        sub_v3_v3v3(e2diff,
                    data->vcos[BM_elem_index_get(e_iter->v1)],
                    data->vcos[BM_elem_index_get(e_iter->v2)]);
        normalize_v3(e2diff);

        BMLoop *l_iter = l_first;
        do {
          if (l_iter->v == v) {
            BMEdge *e_prev = l_iter->prev->e;
            sub_v3_v3v3(e1diff,
                        data->vcos[BM_elem_index_get(e_prev->v1)],
                        data->vcos[BM_elem_index_get(e_prev->v2)]);
            normalize_v3(e1diff);

            bm_vert_calc_normals_accum_loop(
                l_iter, e1diff, e2diff, data->fnos[BM_elem_index_get(l_iter->f)], v_no);
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
                                                const TaskParallelTLS *__restrict /*tls*/)
{
  BMVertsCalcNormalsWithCoordsData *data = static_cast<BMVertsCalcNormalsWithCoordsData *>(
      userdata);
  BMVert *v = (BMVert *)mp_v;
  bm_vert_calc_normals_with_coords(v, data);
}

static void bm_mesh_verts_calc_normals(BMesh *bm,
                                       const Span<float3> fnos,
                                       const Span<float3> vcos,
                                       MutableSpan<float3> vnos)
{
  BM_mesh_elem_index_ensure(bm, BM_FACE | ((!vnos.is_empty() || !vcos.is_empty()) ? BM_VERT : 0));

  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);
  settings.use_threading = bm->totvert >= BM_THREAD_LIMIT;

  if (vcos.is_empty()) {
    BM_iter_parallel(bm, BM_VERTS_OF_MESH, bm_vert_calc_normals_cb, nullptr, &settings);
  }
  else {
    BLI_assert(!fnos.is_empty() || !vnos.is_empty());
    BMVertsCalcNormalsWithCoordsData data{};
    data.fnos = fnos;
    data.vcos = vcos;
    data.vnos = vnos;
    BM_iter_parallel(bm, BM_VERTS_OF_MESH, bm_vert_calc_normals_with_coords_cb, &data, &settings);
  }
}

static void bm_face_calc_normals_cb(void * /*userdata*/,
                                    MempoolIterData *mp_f,
                                    const TaskParallelTLS *__restrict /*tls*/)
{
  BMFace *f = (BMFace *)mp_f;

  BM_face_calc_normal(f, f->no);
}

void BM_mesh_normals_update_ex(BMesh *bm, const BMeshNormalsUpdate_Params *params)
{
  if (params->face_normals) {
    /* Calculate all face normals. */
    TaskParallelSettings settings;
    BLI_parallel_mempool_settings_defaults(&settings);
    settings.use_threading = bm->totedge >= BM_THREAD_LIMIT;

    BM_iter_parallel(bm, BM_FACES_OF_MESH, bm_face_calc_normals_cb, nullptr, &settings);
  }

  /* Add weighted face normals to vertices, and normalize vert normals. */
  bm_mesh_verts_calc_normals(bm, {}, {}, {});
}

void BM_mesh_normals_update(BMesh *bm)
{
  BMeshNormalsUpdate_Params params{};
  params.face_normals = true;
  BM_mesh_normals_update_ex(bm, &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Vertex & Face Normals (Partial Updates)
 * \{ */

void BM_mesh_normals_update_with_partial_ex(BMesh * /*bm*/,
                                            const BMPartialUpdate *bmpinfo,
                                            const BMeshNormalsUpdate_Params *params)
{
  using namespace blender;
  BLI_assert(bmpinfo->params.do_normals);
  /* While harmless, exit early if there is nothing to do. */
  if (UNLIKELY(bmpinfo->verts.is_empty() && bmpinfo->faces.is_empty())) {
    return;
  }

  if (params->face_normals) {
    threading::parallel_for(bmpinfo->faces.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        BMFace *f = bmpinfo->faces[i];
        BM_face_calc_normal(f, f->no);
      }
    });
  }

  threading::parallel_for(bmpinfo->verts.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      BMVert *v = bmpinfo->verts[i];
      bm_vert_calc_normals_impl(v);
    }
  });
}

void BM_mesh_normals_update_with_partial(BMesh *bm, const BMPartialUpdate *bmpinfo)
{
  BMeshNormalsUpdate_Params params{};
  params.face_normals = true;
  BM_mesh_normals_update_with_partial_ex(bm, bmpinfo, &params);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update Vertex & Face Normals (Custom Coords)
 * \{ */

void BM_verts_calc_normal_vcos(BMesh *bm,
                               const Span<float3> fnos,
                               const Span<float3> vcos,
                               MutableSpan<float3> vnos)
{
  /* Add weighted face normals to vertices, and normalize vert normals. */
  bm_mesh_verts_calc_normals(bm, fnos, vcos, vnos);
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
                                    const Span<float3> fnos,
                                    float split_angle_cos,
                                    const bool do_sharp_edges_tag)
{
  BMIter eiter;
  BMEdge *e;
  int i;

  if (!fnos.is_empty()) {
    BM_mesh_elem_index_ensure(bm, BM_FACE);
  }

  if (do_sharp_edges_tag) {
    BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
      BM_elem_index_set(e, i); /* set_inline */
      if (e->l != nullptr) {
        bm_edge_tag_from_smooth_and_set_sharp(fnos, e, split_angle_cos);
      }
    }
  }
  else {
    BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
      BM_elem_index_set(e, i); /* set_inline */
      if (e->l != nullptr) {
        bm_edge_tag_from_smooth(fnos, e, split_angle_cos);
      }
    }
  }

  bm->elem_index_dirty &= ~BM_EDGE;
}

void BM_edges_sharp_from_angle_set(BMesh *bm, const float split_angle)
{
  if (split_angle >= float(M_PI)) {
    /* Nothing to do! */
    return;
  }

  bm_mesh_edges_sharp_tag(bm, {}, cosf(split_angle), true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop Normals Calculation API
 * \{ */

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
 * Called for all faces loops.
 *
 * - All loops must have #BM_ELEM_TAG cleared.
 * - Loop indices must be valid.
 *
 * \note When custom normals are present, the order of loops can be important.
 * Loops with lower indices must be passed before loops with higher indices (for each vertex).
 * This is needed since the first loop sets the reference point for the custom normal offsets.
 *
 * \return The number of loops that were handled (for early exit when all have been handled).
 */
static int bm_mesh_loops_calc_normals_for_loop(BMesh *bm,
                                               const Span<float3> vcos,
                                               const Span<float3> fnos,
                                               const short (*clnors_data)[2],
                                               const int cd_loop_clnors_offset,
                                               const bool has_clnors,
                                               /* Cache. */
                                               blender::Vector<blender::float3, 16> *edge_vectors,
                                               /* Iterate. */
                                               BMLoop *l_curr,
                                               /* Result. */
                                               MutableSpan<float3> r_lnos,
                                               MLoopNorSpaceArray *r_lnors_spacearr)
{
  BLI_assert((bm->elem_index_dirty & BM_LOOP) == 0);
  BLI_assert(fnos.is_empty() || ((bm->elem_index_dirty & BM_FACE) == 0));
  BLI_assert(vcos.is_empty() || ((bm->elem_index_dirty & BM_VERT) == 0));
  UNUSED_VARS_NDEBUG(bm);

  int handled = 0;

  /* Temp normal stack. */
  BLI_SMALLSTACK_DECLARE(normal, float *);
  /* Temp clnors stack. */
  BLI_SMALLSTACK_DECLARE(clnors, short *);
  /* Temp edge vectors stack, only used when computing lnor spacearr. */

  /* A smooth edge, we have to check for cyclic smooth fan case.
   * If we find a new, never-processed cyclic smooth fan, we can do it now using that loop/edge
   * as 'entry point', otherwise we can skip it. */

  /* NOTE: In theory, we could make bm_mesh_loop_check_cyclic_smooth_fan() store
   * mlfan_pivot's in a stack, to avoid having to fan again around
   * the vert during actual computation of clnor & clnorspace. However, this would complicate
   * the code, add more memory usage, and
   * BM_vert_step_fan_loop() is quite cheap in term of CPU cycles,
   * so really think it's not worth it. */
  if (BM_elem_flag_test(l_curr->e, BM_ELEM_TAG) &&
      (BM_elem_flag_test(l_curr, BM_ELEM_TAG) || !BM_loop_check_cyclic_smooth_fan(l_curr)))
  {
  }
  else if (!BM_elem_flag_test(l_curr->e, BM_ELEM_TAG) &&
           !BM_elem_flag_test(l_curr->prev->e, BM_ELEM_TAG))
  {
    /* Simple case (both edges around that vertex are sharp in related face),
     * this vertex just takes its face normal.
     */
    const int l_curr_index = BM_elem_index_get(l_curr);
    const float3 &no = !fnos.is_empty() ? fnos[BM_elem_index_get(l_curr->f)] :
                                          float3(l_curr->f->no);
    copy_v3_v3(r_lnos[l_curr_index], no);

    /* If needed, generate this (simple!) lnor space. */
    if (r_lnors_spacearr) {
      float vec_curr[3], vec_prev[3];
      MLoopNorSpace *lnor_space = BKE_lnor_space_create(r_lnors_spacearr);

      {
        const BMVert *v_pivot = l_curr->v;
        const float3 &co_pivot = !vcos.is_empty() ? vcos[BM_elem_index_get(v_pivot)] :
                                                    float3(v_pivot->co);
        const BMVert *v_1 = l_curr->next->v;
        const float3 co_1 = !vcos.is_empty() ? vcos[BM_elem_index_get(v_1)] : float3(v_1->co);
        const BMVert *v_2 = l_curr->prev->v;
        const float3 co_2 = !vcos.is_empty() ? vcos[BM_elem_index_get(v_2)] : float3(v_2->co);

        BLI_assert(v_1 == BM_edge_other_vert(l_curr->e, v_pivot));
        BLI_assert(v_2 == BM_edge_other_vert(l_curr->prev->e, v_pivot));

        sub_v3_v3v3(vec_curr, co_1, co_pivot);
        normalize_v3(vec_curr);
        sub_v3_v3v3(vec_prev, co_2, co_pivot);
        normalize_v3(vec_prev);
      }

      BKE_lnor_space_define(lnor_space, r_lnos[l_curr_index], vec_curr, vec_prev, {});
      /* We know there is only one loop in this space,
       * no need to create a linklist in this case... */
      BKE_lnor_space_add_loop(r_lnors_spacearr, lnor_space, l_curr_index, l_curr, true);

      if (has_clnors) {
        const short (*clnor)[2] = clnors_data ?
                                      &clnors_data[l_curr_index] :
                                      static_cast<const short (*)[2]>(
                                          BM_ELEM_CD_GET_VOID_P(l_curr, cd_loop_clnors_offset));
        BKE_lnor_space_custom_data_to_normal(lnor_space, *clnor, r_lnos[l_curr_index]);
      }
    }
    handled = 1;
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
    const short (*clnor_ref)[2] = nullptr;
    int clnors_count = 0;
    bool clnors_invalid = false;

    const float3 &co_pivot = !vcos.is_empty() ? vcos[BM_elem_index_get(v_pivot)] :
                                                float3(v_pivot->co);

    MLoopNorSpace *lnor_space = r_lnors_spacearr ? BKE_lnor_space_create(r_lnors_spacearr) :
                                                   nullptr;

    BLI_assert((edge_vectors == nullptr) || edge_vectors->is_empty());

    lfan_pivot = l_curr;
    lfan_pivot_index = BM_elem_index_get(lfan_pivot);
    e_next = lfan_pivot->e; /* Current edge here, actually! */

    /* Only need to compute previous edge's vector once,
     * then we can just reuse old current one! */
    {
      const BMVert *v_2 = lfan_pivot->next->v;
      const float3 co_2 = !vcos.is_empty() ? vcos[BM_elem_index_get(v_2)] : float3(v_2->co);

      BLI_assert(v_2 == BM_edge_other_vert(e_next, v_pivot));

      sub_v3_v3v3(vec_org, co_2, co_pivot);
      normalize_v3(vec_org);
      copy_v3_v3(vec_curr, vec_org);

      if (r_lnors_spacearr) {
        edge_vectors->append(vec_org);
      }
    }

    while (true) {
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
        const float3 co_2 = !vcos.is_empty() ? vcos[BM_elem_index_get(v_2)] : float3(v_2->co);

        sub_v3_v3v3(vec_next, co_2, co_pivot);
        normalize_v3(vec_next);
      }

      {
        /* Code similar to accumulate_vertex_normals_poly_v3. */
        /* Calculate angle between the two face edges incident on this vertex. */
        const BMFace *f = lfan_pivot->f;
        const float fac = blender::math::safe_acos_approx(dot_v3v3(vec_next, vec_curr));
        const float3 &no = !fnos.is_empty() ? fnos[BM_elem_index_get(f)] : float3(f->no);
        /* Accumulate */
        madd_v3_v3fl(lnor, no, fac);

        if (has_clnors) {
          /* Accumulate all clnors, if they are not all equal we have to fix that! */
          const short (*clnor)[2] = clnors_data ?
                                        &clnors_data[lfan_pivot_index] :
                                        static_cast<const short (*)[2]>(BM_ELEM_CD_GET_VOID_P(
                                            lfan_pivot, cd_loop_clnors_offset));
          if (clnors_count) {
            clnors_invalid |= ((*clnor_ref)[0] != (*clnor)[0] || (*clnor_ref)[1] != (*clnor)[1]);
          }
          else {
            clnor_ref = clnor;
          }
          clnors_avg[0] += (*clnor)[0];
          clnors_avg[1] += (*clnor)[1];
          clnors_count++;
          /* We store here a pointer to all custom lnors processed. */
          BLI_SMALLSTACK_PUSH(clnors, (short *)*clnor);
        }
      }

      /* We store here a pointer to all loop-normals processed. */
      BLI_SMALLSTACK_PUSH(normal, (float *)r_lnos[lfan_pivot_index]);

      if (r_lnors_spacearr) {
        /* Assign current lnor space to current 'vertex' loop. */
        BKE_lnor_space_add_loop(r_lnors_spacearr, lnor_space, lfan_pivot_index, lfan_pivot, false);
        if (e_next != e_org) {
          /* We store here all edges-normalized vectors processed. */
          edge_vectors->append(vec_next);
        }
      }

      handled += 1;

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

        BKE_lnor_space_define(lnor_space, lnor, vec_org, vec_next, *edge_vectors);
        edge_vectors->clear();

        if (has_clnors) {
          if (clnors_invalid) {
            short *clnor;

            clnors_avg[0] /= clnors_count;
            clnors_avg[1] /= clnors_count;
            /* Fix/update all clnors of this fan with computed average value. */

            /* Prints continuously when merge custom normals, so commenting. */
            // printf("Invalid clnors in this fan!\n");

            while ((clnor = static_cast<short *>(BLI_SMALLSTACK_POP(clnors)))) {
              // print_v2("org clnor", clnor);
              clnor[0] = short(clnors_avg[0]);
              clnor[1] = short(clnors_avg[1]);
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

        while ((nor = static_cast<float *>(BLI_SMALLSTACK_POP(normal)))) {
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
  return handled;
}

static int bm_loop_index_cmp(const void *a, const void *b)
{
  BLI_assert(BM_elem_index_get((BMLoop *)a) != BM_elem_index_get((BMLoop *)b));
  if (BM_elem_index_get((BMLoop *)a) < BM_elem_index_get((BMLoop *)b)) {
    return -1;
  }
  return 1;
}

/**
 * We only tag edges that are *really* smooth when the following conditions are met:
 * - The angle between both its polygons normals is below split_angle value.
 * - The edge is tagged as smooth.
 * - The faces of the edge are tagged as smooth.
 * - The faces of the edge have compatible (non-flipped) topological normal (winding),
 *   i.e. both loops on the same edge do not share the same vertex.
 */
BLI_INLINE bool bm_edge_is_smooth_no_angle_test(const BMEdge *e,
                                                const BMLoop *l_a,
                                                const BMLoop *l_b)
{
  BLI_assert(l_a->radial_next == l_b);
  return (
      /* The face is manifold. */
      (l_b->radial_next == l_a) &&
      /* Faces have winding that faces the same way. */
      (l_a->v != l_b->v) &&
      /* The edge is smooth. */
      BM_elem_flag_test(e, BM_ELEM_SMOOTH) &&
      /* Both faces are smooth. */
      BM_elem_flag_test(l_a->f, BM_ELEM_SMOOTH) && BM_elem_flag_test(l_b->f, BM_ELEM_SMOOTH));
}

static void bm_edge_tag_from_smooth(const Span<float3> fnos,
                                    BMEdge *e,
                                    const float split_angle_cos)
{
  BLI_assert(e->l != nullptr);
  BMLoop *l_a = e->l, *l_b = l_a->radial_next;
  bool is_smooth = false;
  if (bm_edge_is_smooth_no_angle_test(e, l_a, l_b)) {
    if (split_angle_cos != -1.0f) {
      const float dot = fnos.is_empty() ? dot_v3v3(l_a->f->no, l_b->f->no) :
                                          dot_v3v3(fnos[BM_elem_index_get(l_a->f)],
                                                   fnos[BM_elem_index_get(l_b->f)]);
      if (dot >= split_angle_cos) {
        is_smooth = true;
      }
    }
    else {
      is_smooth = true;
    }
  }

  /* Perform `BM_elem_flag_set(e, BM_ELEM_TAG, is_smooth)`
   * NOTE: This will be set by multiple threads however it will be set to the same value. */

  /* No need for atomics here as this is a single byte. */
  char *hflag_p = &e->head.hflag;
  if (is_smooth) {
    *hflag_p = *hflag_p | BM_ELEM_TAG;
  }
  else {
    *hflag_p = *hflag_p & ~BM_ELEM_TAG;
  }
}

/**
 * A version of #bm_edge_tag_from_smooth that sets sharp edges
 * when they would be considered smooth but exceed the split angle .
 *
 * \note This doesn't have the same atomic requirement as #bm_edge_tag_from_smooth
 * since it isn't run from multiple threads at once.
 */
static void bm_edge_tag_from_smooth_and_set_sharp(const Span<float3> fnos,
                                                  BMEdge *e,
                                                  const float split_angle_cos)
{
  BLI_assert(e->l != nullptr);
  BMLoop *l_a = e->l, *l_b = l_a->radial_next;
  bool is_smooth = false;
  if (bm_edge_is_smooth_no_angle_test(e, l_a, l_b)) {
    if (split_angle_cos != -1.0f) {
      const float dot = fnos.is_empty() ? dot_v3v3(l_a->f->no, l_b->f->no) :
                                          dot_v3v3(fnos[BM_elem_index_get(l_a->f)],
                                                   fnos[BM_elem_index_get(l_b->f)]);
      if (dot >= split_angle_cos) {
        is_smooth = true;
      }
      else {
        /* Note that we do not care about the other sharp-edge cases
         * (sharp face, non-manifold edge, etc.),
         * only tag edge as sharp when it is due to angle threshold. */
        BM_elem_flag_disable(e, BM_ELEM_SMOOTH);
      }
    }
    else {
      is_smooth = true;
    }
  }

  BM_elem_flag_set(e, BM_ELEM_TAG, is_smooth);
}

/**
 * Operate on all vertices loops.
 * operating on vertices this is needed for multi-threading
 * so there is a guarantee that each thread has isolated loops.
 */
static void bm_mesh_loops_calc_normals_for_vert_with_clnors(
    BMesh *bm,
    const Span<float3> vcos,
    const Span<float3> fnos,
    MutableSpan<float3> r_lnos,
    const short (*clnors_data)[2],
    const int cd_loop_clnors_offset,
    const bool do_rebuild,
    const float split_angle_cos,
    /* TLS */
    MLoopNorSpaceArray *r_lnors_spacearr,
    blender::Vector<blender::float3, 16> *edge_vectors,
    /* Iterate over. */
    BMVert *v)
{
  /* Respecting face order is necessary so the initial starting loop is consistent
   * with looping over loops of all faces.
   *
   * Logically we could sort the loops by their index & loop over them
   * however it's faster to use the lowest index of an un-ordered list
   * since it's common that smooth vertices only ever need to pick one loop
   * which then handles all the others.
   *
   * Sorting is only performed when multiple fans are found. */
  const bool has_clnors = true;
  LinkNode *loops_of_vert = nullptr;
  int loops_of_vert_count = 0;
  /* When false the caller must have already tagged the edges. */
  const bool do_edge_tag = (split_angle_cos != EDGE_TAG_FROM_SPLIT_ANGLE_BYPASS);

  /* The loop with the lowest index. */
  {
    LinkNode *link_best;
    uint index_best = UINT_MAX;
    BMEdge *e_curr_iter = v->e;
    do { /* Edges of vertex. */
      BMLoop *l_curr = e_curr_iter->l;
      if (l_curr == nullptr) {
        continue;
      }

      if (do_edge_tag) {
        bm_edge_tag_from_smooth(fnos, e_curr_iter, split_angle_cos);
      }

      do { /* Radial loops. */
        if (l_curr->v != v) {
          continue;
        }
        if (do_rebuild && !BM_ELEM_API_FLAG_TEST(l_curr, BM_LNORSPACE_UPDATE) &&
            !(bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL))
        {
          continue;
        }
        BM_elem_flag_disable(l_curr, BM_ELEM_TAG);
        BLI_linklist_prepend_alloca(&loops_of_vert, l_curr);
        loops_of_vert_count += 1;

        const uint index_test = uint(BM_elem_index_get(l_curr));
        if (index_best > index_test) {
          index_best = index_test;
          link_best = loops_of_vert;
        }
      } while ((l_curr = l_curr->radial_next) != e_curr_iter->l);
    } while ((e_curr_iter = BM_DISK_EDGE_NEXT(e_curr_iter, v)) != v->e);

    if (UNLIKELY(loops_of_vert == nullptr)) {
      return;
    }

    /* Immediately pop the best element.
     * The order doesn't matter, so swap the links as it's simpler than tracking
     * reference to `link_best`. */
    if (link_best != loops_of_vert) {
      std::swap(link_best->link, loops_of_vert->link);
    }
  }

  bool loops_of_vert_is_sorted = false;

  /* Keep track of the number of loops that have been assigned. */
  int loops_of_vert_handled = 0;

  while (loops_of_vert != nullptr) {
    BMLoop *l_best = static_cast<BMLoop *>(loops_of_vert->link);
    loops_of_vert = loops_of_vert->next;

    BLI_assert(l_best->v == v);
    loops_of_vert_handled += bm_mesh_loops_calc_normals_for_loop(bm,
                                                                 vcos,
                                                                 fnos,
                                                                 clnors_data,
                                                                 cd_loop_clnors_offset,
                                                                 has_clnors,
                                                                 edge_vectors,
                                                                 l_best,
                                                                 r_lnos,
                                                                 r_lnors_spacearr);

    /* Check if an early exit is possible without an exhaustive inspection of every loop
     * where 1 loop's fan extends out to all remaining loops.
     * This is a common case for smooth vertices. */
    BLI_assert(loops_of_vert_handled <= loops_of_vert_count);
    if (loops_of_vert_handled == loops_of_vert_count) {
      break;
    }

    /* Note on sorting, in some cases it will be faster to scan for the lowest index each time.
     * However in the worst case this is `O(N^2)`, so use a single sort call instead. */
    if (!loops_of_vert_is_sorted) {
      if (loops_of_vert && loops_of_vert->next) {
        loops_of_vert = BLI_linklist_sort(loops_of_vert, bm_loop_index_cmp);
        loops_of_vert_is_sorted = true;
      }
    }
  }
}

/**
 * A simplified version of #bm_mesh_loops_calc_normals_for_vert_with_clnors
 * that can operate on loops in any order.
 */
static void bm_mesh_loops_calc_normals_for_vert_without_clnors(
    BMesh *bm,
    const Span<float3> vcos,
    const Span<float3> fnos,
    MutableSpan<float3> r_lnos,
    const bool do_rebuild,
    const float split_angle_cos,
    /* TLS */
    MLoopNorSpaceArray *r_lnors_spacearr,
    blender::Vector<blender::float3, 16> *edge_vectors,
    /* Iterate over. */
    BMVert *v)
{
  const bool has_clnors = false;
  const short (*clnors_data)[2] = nullptr;
  /* When false the caller must have already tagged the edges. */
  const bool do_edge_tag = (split_angle_cos != EDGE_TAG_FROM_SPLIT_ANGLE_BYPASS);
  const int cd_loop_clnors_offset = -1;

  BMEdge *e_curr_iter;

  /* Unfortunately a loop is needed just to clear loop-tags. */
  e_curr_iter = v->e;
  do { /* Edges of vertex. */
    BMLoop *l_curr = e_curr_iter->l;
    if (l_curr == nullptr) {
      continue;
    }

    if (do_edge_tag) {
      bm_edge_tag_from_smooth(fnos, e_curr_iter, split_angle_cos);
    }

    do { /* Radial loops. */
      if (l_curr->v != v) {
        continue;
      }
      BM_elem_flag_disable(l_curr, BM_ELEM_TAG);
    } while ((l_curr = l_curr->radial_next) != e_curr_iter->l);
  } while ((e_curr_iter = BM_DISK_EDGE_NEXT(e_curr_iter, v)) != v->e);

  e_curr_iter = v->e;
  do { /* Edges of vertex. */
    BMLoop *l_curr = e_curr_iter->l;
    if (l_curr == nullptr) {
      continue;
    }
    do { /* Radial loops. */
      if (l_curr->v != v) {
        continue;
      }
      if (do_rebuild && !BM_ELEM_API_FLAG_TEST(l_curr, BM_LNORSPACE_UPDATE) &&
          !(bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL))
      {
        continue;
      }
      bm_mesh_loops_calc_normals_for_loop(bm,
                                          vcos,
                                          fnos,
                                          clnors_data,
                                          cd_loop_clnors_offset,
                                          has_clnors,
                                          edge_vectors,
                                          l_curr,
                                          r_lnos,
                                          r_lnors_spacearr);
    } while ((l_curr = l_curr->radial_next) != e_curr_iter->l);
  } while ((e_curr_iter = BM_DISK_EDGE_NEXT(e_curr_iter, v)) != v->e);
}

/**
 * BMesh version of bke::mesh::normals_calc_corners() in `mesh_evaluate.cc`
 * Will use first clnors_data array, and fallback to cd_loop_clnors_offset
 * (use nullptr and -1 to not use clnors).
 *
 * \note This sets #BM_ELEM_TAG which is used in tool code (e.g. #84426).
 * we could add a low-level API flag for this, see #BM_ELEM_API_FLAG_ENABLE and friends.
 */
static void bm_mesh_loops_calc_normals__single_threaded(BMesh *bm,
                                                        const Span<float3> vcos,
                                                        const Span<float3> fnos,
                                                        MutableSpan<float3> r_lnos,
                                                        MLoopNorSpaceArray *r_lnors_spacearr,
                                                        const short (*clnors_data)[2],
                                                        const int cd_loop_clnors_offset,
                                                        const bool do_rebuild,
                                                        const float split_angle_cos)
{
  BMIter fiter;
  BMFace *f_curr;
  const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);
  /* When false the caller must have already tagged the edges. */
  const bool do_edge_tag = (split_angle_cos != EDGE_TAG_FROM_SPLIT_ANGLE_BYPASS);

  MLoopNorSpaceArray _lnors_spacearr = {nullptr};

  std::unique_ptr<blender::Vector<blender::float3, 16>> edge_vectors = nullptr;

  {
    char htype = 0;
    if (!vcos.is_empty()) {
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
    edge_vectors = std::make_unique<blender::Vector<blender::float3, 16>>();
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

  /* Always tag edges based on winding & sharp edge flag
   * (even when the auto-smooth angle doesn't need to be calculated). */
  if (do_edge_tag) {
    bm_mesh_edges_sharp_tag(bm, fnos, has_clnors ? -1.0f : split_angle_cos, false);
  }

  /* We now know edges that can be smoothed (they are tagged),
   * and edges that will be hard (they aren't).
   * Now, time to generate the normals.
   */
  BM_ITER_MESH (f_curr, &fiter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_curr, *l_first;

    l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
    do {
      if (do_rebuild && !BM_ELEM_API_FLAG_TEST(l_curr, BM_LNORSPACE_UPDATE) &&
          !(bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL))
      {
        continue;
      }
      bm_mesh_loops_calc_normals_for_loop(bm,
                                          vcos,
                                          fnos,
                                          clnors_data,
                                          cd_loop_clnors_offset,
                                          has_clnors,
                                          edge_vectors.get(),
                                          l_curr,
                                          r_lnos,
                                          r_lnors_spacearr);
    } while ((l_curr = l_curr->next) != l_first);
  }

  if (r_lnors_spacearr) {
    if (r_lnors_spacearr == &_lnors_spacearr) {
      BKE_lnor_spacearr_free(r_lnors_spacearr);
    }
  }
}

struct BMLoopsCalcNormalsWithCoordsData {
  /* Read-only data. */
  Span<float3> vcos;
  Span<float3> fnos;
  BMesh *bm;
  const short (*clnors_data)[2];
  int cd_loop_clnors_offset;
  bool do_rebuild;
  float split_angle_cos;

  /* Output. */
  MutableSpan<float3> r_lnos;
  MLoopNorSpaceArray *r_lnors_spacearr;
};

struct BMLoopsCalcNormalsWithCoords_TLS {
  blender::Vector<blender::float3, 16> *edge_vectors;

  /** Copied from #BMLoopsCalcNormalsWithCoordsData.r_lnors_spacearr when it's not nullptr. */
  MLoopNorSpaceArray *lnors_spacearr;
  MLoopNorSpaceArray lnors_spacearr_buf;
};

static void bm_mesh_loops_calc_normals_for_vert_init_fn(const void *__restrict userdata,
                                                        void *__restrict chunk)
{
  const auto *data = static_cast<const BMLoopsCalcNormalsWithCoordsData *>(userdata);
  auto *tls_data = static_cast<BMLoopsCalcNormalsWithCoords_TLS *>(chunk);
  if (data->r_lnors_spacearr) {
    tls_data->edge_vectors = MEM_new<blender::Vector<blender::float3, 16>>(__func__);
    BKE_lnor_spacearr_tls_init(data->r_lnors_spacearr, &tls_data->lnors_spacearr_buf);
    tls_data->lnors_spacearr = &tls_data->lnors_spacearr_buf;
  }
  else {
    tls_data->lnors_spacearr = nullptr;
  }
}

static void bm_mesh_loops_calc_normals_for_vert_reduce_fn(const void *__restrict userdata,
                                                          void *__restrict /*chunk_join*/,
                                                          void *__restrict chunk)
{
  const auto *data = static_cast<const BMLoopsCalcNormalsWithCoordsData *>(userdata);
  auto *tls_data = static_cast<BMLoopsCalcNormalsWithCoords_TLS *>(chunk);

  if (data->r_lnors_spacearr) {
    BKE_lnor_spacearr_tls_join(data->r_lnors_spacearr, tls_data->lnors_spacearr);
  }
}

static void bm_mesh_loops_calc_normals_for_vert_free_fn(const void *__restrict userdata,
                                                        void *__restrict chunk)
{
  const auto *data = static_cast<const BMLoopsCalcNormalsWithCoordsData *>(userdata);
  auto *tls_data = static_cast<BMLoopsCalcNormalsWithCoords_TLS *>(chunk);

  if (data->r_lnors_spacearr) {
    MEM_delete(tls_data->edge_vectors);
  }
}

static void bm_mesh_loops_calc_normals_for_vert_with_clnors_fn(
    void *userdata, MempoolIterData *mp_v, const TaskParallelTLS *__restrict tls)
{
  BMVert *v = (BMVert *)mp_v;
  if (v->e == nullptr) {
    return;
  }
  auto *data = static_cast<BMLoopsCalcNormalsWithCoordsData *>(userdata);
  auto *tls_data = static_cast<BMLoopsCalcNormalsWithCoords_TLS *>(tls->userdata_chunk);
  bm_mesh_loops_calc_normals_for_vert_with_clnors(data->bm,
                                                  data->vcos,
                                                  data->fnos,
                                                  data->r_lnos,

                                                  data->clnors_data,
                                                  data->cd_loop_clnors_offset,
                                                  data->do_rebuild,
                                                  data->split_angle_cos,
                                                  /* Thread local. */
                                                  tls_data->lnors_spacearr,
                                                  tls_data->edge_vectors,
                                                  /* Iterate over. */
                                                  v);
}

static void bm_mesh_loops_calc_normals_for_vert_without_clnors_fn(
    void *userdata, MempoolIterData *mp_v, const TaskParallelTLS *__restrict tls)
{
  BMVert *v = (BMVert *)mp_v;
  if (v->e == nullptr) {
    return;
  }
  auto *data = static_cast<BMLoopsCalcNormalsWithCoordsData *>(userdata);
  auto *tls_data = static_cast<BMLoopsCalcNormalsWithCoords_TLS *>(tls->userdata_chunk);
  bm_mesh_loops_calc_normals_for_vert_without_clnors(data->bm,
                                                     data->vcos,
                                                     data->fnos,
                                                     data->r_lnos,

                                                     data->do_rebuild,
                                                     data->split_angle_cos,
                                                     /* Thread local. */
                                                     tls_data->lnors_spacearr,
                                                     tls_data->edge_vectors,
                                                     /* Iterate over. */
                                                     v);
}

static void bm_mesh_loops_calc_normals__multi_threaded(BMesh *bm,
                                                       const Span<float3> vcos,
                                                       const Span<float3> fnos,
                                                       MutableSpan<float3> r_lnos,
                                                       MLoopNorSpaceArray *r_lnors_spacearr,
                                                       const short (*clnors_data)[2],
                                                       const int cd_loop_clnors_offset,
                                                       const bool do_rebuild,
                                                       const float split_angle_cos)
{
  const bool has_clnors = clnors_data || (cd_loop_clnors_offset != -1);
  MLoopNorSpaceArray _lnors_spacearr = {nullptr};

  {
    char htype = BM_LOOP;
    if (!vcos.is_empty()) {
      htype |= BM_VERT;
    }
    if (!fnos.is_empty()) {
      htype |= BM_FACE;
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
  }

  /* We now know edges that can be smoothed (they are tagged),
   * and edges that will be hard (they aren't).
   * Now, time to generate the normals.
   */

  TaskParallelSettings settings;
  BLI_parallel_mempool_settings_defaults(&settings);

  BMLoopsCalcNormalsWithCoords_TLS tls = {nullptr};

  settings.userdata_chunk = &tls;
  settings.userdata_chunk_size = sizeof(tls);

  settings.func_init = bm_mesh_loops_calc_normals_for_vert_init_fn;
  settings.func_reduce = bm_mesh_loops_calc_normals_for_vert_reduce_fn;
  settings.func_free = bm_mesh_loops_calc_normals_for_vert_free_fn;

  BMLoopsCalcNormalsWithCoordsData data{};
  data.bm = bm;
  data.vcos = vcos;
  data.fnos = fnos;
  data.r_lnos = r_lnos;
  data.r_lnors_spacearr = r_lnors_spacearr;
  data.clnors_data = clnors_data;
  data.cd_loop_clnors_offset = cd_loop_clnors_offset;
  data.do_rebuild = do_rebuild;
  data.split_angle_cos = split_angle_cos;

  BM_iter_parallel(bm,
                   BM_VERTS_OF_MESH,
                   has_clnors ? bm_mesh_loops_calc_normals_for_vert_with_clnors_fn :
                                bm_mesh_loops_calc_normals_for_vert_without_clnors_fn,
                   &data,
                   &settings);

  if (r_lnors_spacearr) {
    if (r_lnors_spacearr == &_lnors_spacearr) {
      BKE_lnor_spacearr_free(r_lnors_spacearr);
    }
  }
}

static void bm_mesh_loops_calc_normals(BMesh *bm,
                                       const Span<float3> vcos,
                                       const Span<float3> fnos,
                                       MutableSpan<float3> r_lnos,
                                       MLoopNorSpaceArray *r_lnors_spacearr,
                                       const short (*clnors_data)[2],
                                       const int cd_loop_clnors_offset,
                                       const bool do_rebuild,
                                       const float split_angle_cos)
{
  if (bm->totloop < BM_THREAD_LIMIT) {
    bm_mesh_loops_calc_normals__single_threaded(bm,
                                                vcos,
                                                fnos,
                                                r_lnos,
                                                r_lnors_spacearr,
                                                clnors_data,
                                                cd_loop_clnors_offset,
                                                do_rebuild,
                                                split_angle_cos);
  }
  else {
    bm_mesh_loops_calc_normals__multi_threaded(bm,
                                               vcos,
                                               fnos,
                                               r_lnos,
                                               r_lnors_spacearr,
                                               clnors_data,
                                               cd_loop_clnors_offset,
                                               do_rebuild,
                                               split_angle_cos);
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
  BLI_bitmap *done_loops = BLI_BITMAP_NEW(size_t(bm->totloop), __func__);
  bool changed = false;

  BLI_assert(lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);

  for (int i = 0; i < bm->totloop; i++) {
    if (!lnors_spacearr->lspacearr[i]) {
      /* This should not happen in theory, but in some rare case (probably ugly geometry)
       * we can get some nullptr loopspacearr at this point. :/
       * Maybe we should set those loops' edges as sharp?
       */
      BLI_BITMAP_ENABLE(done_loops, i);
      if (G.debug & G_DEBUG) {
        printf("WARNING! Getting invalid nullptr loop space for loop %d!\n", i);
      }
      continue;
    }

    if (!BLI_BITMAP_TEST(done_loops, i)) {
      /* Notes:
       * * In case of mono-loop smooth fan, we have nothing to do.
       * * Loops in this linklist are ordered (in reversed order compared to how they were
       *   discovered by bke::mesh::normals_calc_corners(), but this is not a problem).
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
      BMLoop *prev_ml = nullptr;
      const float *org_nor = nullptr;

      while (loops) {
        BMLoop *ml = static_cast<BMLoop *>(loops->link);
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
       * See #45984. */
      loops = lnors_spacearr->lspacearr[i]->loops;
      if (loops && org_nor) {
        BMLoop *ml = static_cast<BMLoop *>(loops->link);
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
  BLI_bitmap *done_loops = BLI_BITMAP_NEW(size_t(bm->totloop), __func__);

  BLI_SMALLSTACK_DECLARE(clnors_data, short *);

  BLI_assert(lnors_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);

  for (int i = 0; i < bm->totloop; i++) {
    if (!lnors_spacearr->lspacearr[i]) {
      BLI_BITMAP_ENABLE(done_loops, i);
      if (G.debug & G_DEBUG) {
        printf("WARNING! Still getting invalid nullptr loop space in second loop for loop %d!\n",
               i);
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
        short *clnor = static_cast<short *>(r_clnors_data ?
                                                &r_clnors_data[lidx] :
                                                BM_ELEM_CD_GET_VOID_P(ml, cd_loop_clnors_offset));

        BKE_lnor_space_custom_normal_to_data(lnors_spacearr->lspacearr[i], nor, clnor);
        BLI_BITMAP_ENABLE(done_loops, i);
      }
      else {
        int avg_nor_count = 0;
        float avg_nor[3];
        short clnor_data_tmp[2], *clnor_data;

        zero_v3(avg_nor);

        while (loops) {
          BMLoop *ml = static_cast<BMLoop *>(loops->link);
          const int lidx = BM_elem_index_get(ml);
          const float *nor = new_lnors[lidx];
          short *clnor = static_cast<short *>(
              r_clnors_data ? &r_clnors_data[lidx] :
                              BM_ELEM_CD_GET_VOID_P(ml, cd_loop_clnors_offset));

          avg_nor_count++;
          add_v3_v3(avg_nor, nor);
          BLI_SMALLSTACK_PUSH(clnors_data, clnor);

          loops = loops->next;
          BLI_BITMAP_ENABLE(done_loops, lidx);
        }

        mul_v3_fl(avg_nor, 1.0f / float(avg_nor_count));
        BKE_lnor_space_custom_normal_to_data(
            lnors_spacearr->lspacearr[i], avg_nor, clnor_data_tmp);

        while ((clnor_data = static_cast<short *>(BLI_SMALLSTACK_POP(clnors_data)))) {
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
                                             const Span<float3> vcos,
                                             const Span<float3> fnos,
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
  Array<float3> cur_lnors(bm->totloop);

  BKE_lnor_spacearr_clear(r_lnors_spacearr);

  /* Tag smooth edges and set lnos from vnos when they might be completely smooth...
   * When using custom loop normals, disable the angle feature! */
  bm_mesh_edges_sharp_tag(bm, fnos, -1.0f, false);

  /* Finish computing lnos by accumulating face normals
   * in each fan of faces defined by sharp edges. */
  bm_mesh_loops_calc_normals(bm,
                             vcos,
                             fnos,
                             cur_lnors,
                             r_lnors_spacearr,
                             r_clnors_data,
                             cd_loop_clnors_offset,
                             false,
                             EDGE_TAG_FROM_SPLIT_ANGLE_BYPASS);

  /* Extract new normals from the data layer if necessary. */
  float (*custom_lnors)[3] = new_lnors;

  if (new_lnors == nullptr) {
    custom_lnors = static_cast<float (*)[3]>(
        MEM_mallocN(sizeof(*new_lnors) * bm->totloop, __func__));

    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        const float *normal = static_cast<float *>(BM_ELEM_CD_GET_VOID_P(l, cd_new_lnors_offset));
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

    bm_mesh_loops_calc_normals(bm,
                               vcos,
                               fnos,
                               cur_lnors,
                               r_lnors_spacearr,
                               r_clnors_data,
                               cd_loop_clnors_offset,
                               false,
                               EDGE_TAG_FROM_SPLIT_ANGLE_BYPASS);
  }

  /* And we just have to convert plain object-space custom normals to our
   * lnor space-encoded ones. */
  bm_mesh_loops_assign_normal_data(
      bm, r_lnors_spacearr, r_clnors_data, cd_loop_clnors_offset, custom_lnors);

  if (custom_lnors != new_lnors) {
    MEM_freeN(custom_lnors);
  }
}

static void bm_mesh_loops_calc_normals_no_autosmooth(BMesh *bm,
                                                     const Span<float3> vnos,
                                                     const Span<float3> fnos,
                                                     MutableSpan<float3> r_lnos)
{
  BMIter fiter;
  BMFace *f_curr;

  {
    char htype = BM_LOOP;
    if (!vnos.is_empty()) {
      htype |= BM_VERT;
    }
    if (!fnos.is_empty()) {
      htype |= BM_FACE;
    }
    BM_mesh_elem_index_ensure(bm, htype);
  }

  BM_ITER_MESH (f_curr, &fiter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_curr, *l_first;
    const bool is_face_flat = !BM_elem_flag_test(f_curr, BM_ELEM_SMOOTH);

    l_curr = l_first = BM_FACE_FIRST_LOOP(f_curr);
    do {
      const float3 &no = is_face_flat ? (!fnos.is_empty() ? fnos[BM_elem_index_get(f_curr)] :
                                                            float3(f_curr->no)) :
                                        (!vnos.is_empty() ? vnos[BM_elem_index_get(l_curr->v)] :
                                                            float3(l_curr->v->no));
      copy_v3_v3(r_lnos[BM_elem_index_get(l_curr)], no);

    } while ((l_curr = l_curr->next) != l_first);
  }
}

void BM_loops_calc_normal_vcos(BMesh *bm,
                               const Span<float3> vcos,
                               const Span<float3> vnos,
                               const Span<float3> fnos,
                               const bool use_split_normals,
                               MutableSpan<float3> r_lnos,
                               MLoopNorSpaceArray *r_lnors_spacearr,
                               short (*clnors_data)[2],
                               const int cd_loop_clnors_offset,
                               const bool do_rebuild)
{

  if (use_split_normals) {
    bm_mesh_loops_calc_normals(bm,
                               vcos,
                               fnos,
                               r_lnos,
                               r_lnors_spacearr,
                               clnors_data,
                               cd_loop_clnors_offset,
                               do_rebuild,
                               -1.0f);
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

void BM_lnorspacearr_store(BMesh *bm, MutableSpan<float3> r_lnors)
{
  BLI_assert(bm->lnor_spacearr != nullptr);

  BM_data_layer_ensure_named(bm, &bm->ldata, CD_PROP_INT16_2D, "custom_normal");

  int cd_loop_clnors_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");

  BM_loops_calc_normal_vcos(
      bm, {}, {}, {}, true, r_lnors, bm->lnor_spacearr, nullptr, cd_loop_clnors_offset, false);
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
  if (bm->lnor_spacearr == nullptr) {
    bm->spacearr_dirty |= BM_SPACEARR_DIRTY_ALL;
    return;
  }

  BMVert *v;
  BMLoop *l;
  BMIter viter, liter;
  /* NOTE: we could use temp tag of BMItem for that,
   * but probably better not use it in such a low-level func?
   * --mont29 */
  BLI_bitmap *done_verts = BLI_BITMAP_NEW(bm->totvert, __func__);

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  /* When we affect a given vertex, we may affect following smooth fans:
   * - all smooth fans of said vertex;
   * - all smooth fans of all immediate loop-neighbors vertices;
   * This can be simplified as 'all loops of selected vertices and their immediate neighbors'
   * need to be tagged for update.
   */
  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
        BM_ELEM_API_FLAG_ENABLE(l, BM_LNORSPACE_UPDATE);

        /* Note that we only handle unselected neighbor vertices here, main loop will take care of
         * selected ones. */
        if (!BM_elem_flag_test(l->prev->v, BM_ELEM_SELECT) &&
            !BLI_BITMAP_TEST(done_verts, BM_elem_index_get(l->prev->v)))
        {

          BMLoop *l_prev;
          BMIter liter_prev;
          BM_ITER_ELEM (l_prev, &liter_prev, l->prev->v, BM_LOOPS_OF_VERT) {
            BM_ELEM_API_FLAG_ENABLE(l_prev, BM_LNORSPACE_UPDATE);
          }
          BLI_BITMAP_ENABLE(done_verts, BM_elem_index_get(l_prev->v));
        }

        if (!BM_elem_flag_test(l->next->v, BM_ELEM_SELECT) &&
            !BLI_BITMAP_TEST(done_verts, BM_elem_index_get(l->next->v)))
        {

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
  BLI_assert(bm->lnor_spacearr != nullptr);

  if (!(bm->spacearr_dirty & (BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL))) {
    return;
  }
  BMFace *f;
  BMLoop *l;
  BMIter fiter, liter;

  Array<float3> r_lnors(bm->totloop, float3(0));
  Array<float3> oldnors(preserve_clnor ? bm->totloop : 0, float3(0));

  int cd_loop_clnors_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  if (preserve_clnor) {
    BLI_assert(bm->lnor_spacearr->lspacearr != nullptr);

    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (BM_ELEM_API_FLAG_TEST(l, BM_LNORSPACE_UPDATE) ||
            bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL)
        {
          short (*clnor)[2] = static_cast<short (*)[2]>(
              BM_ELEM_CD_GET_VOID_P(l, cd_loop_clnors_offset));
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
  BM_loops_calc_normal_vcos(
      bm, {}, {}, {}, true, r_lnors, bm->lnor_spacearr, nullptr, cd_loop_clnors_offset, true);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      if (BM_ELEM_API_FLAG_TEST(l, BM_LNORSPACE_UPDATE) ||
          bm->spacearr_dirty & BM_SPACEARR_DIRTY_ALL)
      {
        if (preserve_clnor) {
          short (*clnor)[2] = static_cast<short (*)[2]>(
              BM_ELEM_CD_GET_VOID_P(l, cd_loop_clnors_offset));
          int l_index = BM_elem_index_get(l);
          BKE_lnor_space_custom_normal_to_data(
              bm->lnor_spacearr->lspacearr[l_index], oldnors[l_index], *clnor);
        }
        BM_ELEM_API_FLAG_DISABLE(l, BM_LNORSPACE_UPDATE);
      }
    }
  }

  bm->spacearr_dirty &= ~(BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL);

#ifndef NDEBUG
  BM_lnorspace_err(bm);
#endif
}

/**
 * Make sure the corner fan (tangent space) style custom normals exist on the BMesh. If free vector
 * custom normals exist, they'll be converted. This is often necessary for BMesh editing tools that
 * don't (yet) support free normals.
 */
static void bm_lnorspace_ensure_from_free_normals(BMesh *bm)
{
  /* Zero values tell the normals calculation code to use the automatic normals (rather than any
   * custom normal vector). */
  Array<float3> lnors(bm->totloop, float3(0));
  const int vert_free_offset = CustomData_get_offset_named(
      &bm->vdata, CD_PROP_FLOAT3, "custom_normal");
  const int edge_free_offset = CustomData_get_offset_named(
      &bm->edata, CD_PROP_FLOAT3, "custom_normal");
  const int face_free_offset = CustomData_get_offset_named(
      &bm->pdata, CD_PROP_FLOAT3, "custom_normal");
  const int loop_free_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_FLOAT3, "custom_normal");
  if (vert_free_offset != -1) {
    int loop_index = 0;
    BMFace *f;
    BMLoop *l;
    BMIter fiter, liter;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        lnors[loop_index++] = float3(BM_ELEM_CD_GET_FLOAT_P(l->v, vert_free_offset));
      }
    }
    BM_data_layer_free_named(bm, &bm->vdata, "custom_normal");
  }
  else if (edge_free_offset != -1) {
    BM_data_layer_free_named(bm, &bm->edata, "custom_normal");
  }
  else if (face_free_offset != -1) {
    int loop_index = 0;
    BMFace *f;
    BMLoop *l;
    BMIter fiter, liter;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        lnors[loop_index++] = float3(BM_ELEM_CD_GET_FLOAT_P(f, face_free_offset));
      }
    }
    BM_data_layer_free_named(bm, &bm->pdata, "custom_normal");
  }
  else if (loop_free_offset != -1) {
    int loop_index = 0;
    BMFace *f;
    BMLoop *l;
    BMIter fiter, liter;
    BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        lnors[loop_index++] = float3(BM_ELEM_CD_GET_FLOAT_P(l, loop_free_offset));
      }
    }
    BM_data_layer_free_named(bm, &bm->ldata, "custom_normal");
  }
  BM_lnorspacearr_store(bm, lnors);
}

void BM_lnorspace_update(BMesh *bm)
{
  if (bm->lnor_spacearr == nullptr) {
    bm->lnor_spacearr = MEM_callocN<MLoopNorSpaceArray>(__func__);
  }
  if (bm->lnor_spacearr->lspacearr == nullptr) {
    bm_lnorspace_ensure_from_free_normals(bm);
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

  MLoopNorSpaceArray *temp = MEM_callocN<MLoopNorSpaceArray>(__func__);
  temp->lspacearr = nullptr;

  BKE_lnor_spacearr_init(temp, bm->totloop, MLNOR_SPACEARR_BMLOOP_PTR);

  int cd_loop_clnors_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");
  Array<float3> lnors(bm->totloop, float3(0));
  BM_loops_calc_normal_vcos(
      bm, {}, {}, {}, true, lnors, temp, nullptr, cd_loop_clnors_offset, true);

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
  if (l != nullptr) {
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

static void bm_loop_normal_mark_verts_impl(BMesh *bm,
                                           BLI_bitmap *loops,
                                           const bool do_all_loops_of_vert,
                                           int *totloopsel_p)
{
  /* Select all loops of selected verts. */
  BMLoop *l;
  BMVert *v;
  BMIter liter, viter;
  BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
        bm_loop_normal_mark_indiv_do_loop(
            l, loops, bm->lnor_spacearr, totloopsel_p, do_all_loops_of_vert);
      }
    }
  }
}

static void bm_loop_normal_mark_edges_impl(BMesh *bm,
                                           BLI_bitmap *loops,
                                           const bool do_all_loops_of_vert,
                                           int *totloopsel_p)
{
  /* Only select all loops of selected edges. */
  BMLoop *l;
  BMEdge *e;
  BMIter liter, eiter;
  BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
        bm_loop_normal_mark_indiv_do_loop(
            l, loops, bm->lnor_spacearr, totloopsel_p, do_all_loops_of_vert);
        /* Loops actually 'have' two edges, or said otherwise, a selected edge actually selects
         * *two* loops in each of its faces. We have to find the other one too. */
        if (BM_vert_in_edge(e, l->next->v)) {
          bm_loop_normal_mark_indiv_do_loop(
              l->next, loops, bm->lnor_spacearr, totloopsel_p, do_all_loops_of_vert);
        }
        else {
          BLI_assert(BM_vert_in_edge(e, l->prev->v));
          bm_loop_normal_mark_indiv_do_loop(
              l->prev, loops, bm->lnor_spacearr, totloopsel_p, do_all_loops_of_vert);
        }
      }
    }
  }
}

static void bm_loop_normal_mark_faces_impl(BMesh *bm,
                                           BLI_bitmap *loops,
                                           const bool do_all_loops_of_vert,
                                           int *totloopsel_p)
{
  /* Only select all loops of selected faces. */
  BMLoop *l;
  BMFace *f;
  BMIter liter, fiter;
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        bm_loop_normal_mark_indiv_do_loop(
            l, loops, bm->lnor_spacearr, totloopsel_p, do_all_loops_of_vert);
      }
    }
  }
}

static int bm_loop_normal_mark_verts(BMesh *bm, BLI_bitmap *loops, const bool do_all_loops_of_vert)
{
  BM_mesh_elem_index_ensure(bm, BM_LOOP);
  BLI_assert(bm->lnor_spacearr != nullptr);
  BLI_assert(bm->lnor_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);
  int totloopsel = 0;
  bm_loop_normal_mark_verts_impl(bm, loops, do_all_loops_of_vert, &totloopsel);
  return totloopsel;
}

static int bm_loop_normal_mark_edges(BMesh *bm, BLI_bitmap *loops, const bool do_all_loops_of_vert)
{
  BM_mesh_elem_index_ensure(bm, BM_LOOP);
  BLI_assert(bm->lnor_spacearr != nullptr);
  BLI_assert(bm->lnor_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);
  int totloopsel = 0;
  bm_loop_normal_mark_edges_impl(bm, loops, do_all_loops_of_vert, &totloopsel);
  return totloopsel;
}

static int bm_loop_normal_mark_faces(BMesh *bm, BLI_bitmap *loops, const bool do_all_loops_of_vert)
{
  BM_mesh_elem_index_ensure(bm, BM_LOOP);
  BLI_assert(bm->lnor_spacearr != nullptr);
  BLI_assert(bm->lnor_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);
  int totloopsel = 0;
  bm_loop_normal_mark_faces_impl(bm, loops, do_all_loops_of_vert, &totloopsel);
  return totloopsel;
}

/* Mark the individual clnors to be edited, if multiple selection methods are used. */
static int bm_loop_normal_mark_indiv(BMesh *bm, BLI_bitmap *loops, const bool do_all_loops_of_vert)
{
  int totloopsel = 0;

  const bool sel_verts = (bm->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool sel_edges = (bm->selectmode & SCE_SELECT_EDGE) != 0;
  const bool sel_faces = (bm->selectmode & SCE_SELECT_FACE) != 0;
  const bool use_sel_face_history = sel_faces && (sel_edges || sel_verts);

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  BLI_assert(bm->lnor_spacearr != nullptr);
  BLI_assert(bm->lnor_spacearr->data_type == MLNOR_SPACEARR_BMLOOP_PTR);

  if (use_sel_face_history) {
    /* Using face history allows to select a single loop from a single face...
     * Note that this is O(n^2) piece of code,
     * but it is not designed to be used with huge selection sets,
     * rather with only a few items selected at most. */
    /* Goes from last selected to the first selected element. */
    LISTBASE_FOREACH_BACKWARD (BMEditSelection *, ese, &bm->selected) {
      if (ese->htype == BM_FACE) {
        /* If current face is selected,
         * then any verts to be edited must have been selected before it. */
        for (BMEditSelection *ese_prev = ese->prev; ese_prev; ese_prev = ese_prev->prev) {
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

  /* If the selection history could not be used, fall back to regular selection. */
  if (totloopsel == 0) {
    if (sel_faces) {
      bm_loop_normal_mark_faces_impl(bm, loops, do_all_loops_of_vert, &totloopsel);
    }
    if (sel_edges) {
      bm_loop_normal_mark_edges_impl(bm, loops, do_all_loops_of_vert, &totloopsel);
    }
    if (sel_verts) {
      bm_loop_normal_mark_verts_impl(bm, loops, do_all_loops_of_vert, &totloopsel);
    }
  }

  return totloopsel;
}

static void loop_normal_editdata_init(
    BMesh *bm, BMLoopNorEditData *lnor_ed, BMVert *v, BMLoop *l, const int offset)
{
  BLI_assert(bm->lnor_spacearr != nullptr);
  BLI_assert(bm->lnor_spacearr->lspacearr != nullptr);

  const int l_index = BM_elem_index_get(l);
  short *clnors_data = static_cast<short *>(BM_ELEM_CD_GET_VOID_P(l, offset));

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

BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init_with_htype(
    BMesh *bm, const bool do_all_loops_of_vert, const char htype_override)
{
  BMLoop *l;
  BMVert *v;
  BMIter liter, viter;

  int totloopsel = 0;

  BLI_assert(bm->spacearr_dirty == 0);

  BMLoopNorEditDataArray *lnors_ed_arr = MEM_callocN<BMLoopNorEditDataArray>(__func__);
  lnors_ed_arr->lidx_to_lnor_editdata = MEM_calloc_arrayN<BMLoopNorEditData *>(bm->totloop,
                                                                               __func__);

  BM_data_layer_ensure_named(bm, &bm->ldata, CD_PROP_INT16_2D, "custom_normal");
  const int cd_custom_normal_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  BLI_bitmap *loops = BLI_BITMAP_NEW(bm->totloop, __func__);

  /* This function define loop normals to edit, based on selection modes and history. */
  if (htype_override != 0) {
    BLI_assert(ELEM(htype_override, BM_VERT, BM_EDGE, BM_FACE));
    switch (htype_override) {
      case BM_VERT: {
        totloopsel = bm_loop_normal_mark_verts(bm, loops, do_all_loops_of_vert);
        break;
      }
      case BM_EDGE: {
        totloopsel = bm_loop_normal_mark_edges(bm, loops, do_all_loops_of_vert);
        break;
      }
      case BM_FACE: {
        totloopsel = bm_loop_normal_mark_faces(bm, loops, do_all_loops_of_vert);
        break;
      }
    }
  }
  else {
    totloopsel = bm_loop_normal_mark_indiv(bm, loops, do_all_loops_of_vert);
  }

  if (totloopsel) {
    BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata =
        MEM_malloc_arrayN<BMLoopNorEditData>(totloopsel, __func__);

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

BMLoopNorEditDataArray *BM_loop_normal_editdata_array_init(BMesh *bm,
                                                           const bool do_all_loops_of_vert)
{
  return BM_loop_normal_editdata_array_init_with_htype(bm, do_all_loops_of_vert, 0);
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

bool BM_custom_loop_normals_to_vector_layer(BMesh *bm)
{
  BMFace *f;
  BMLoop *l;
  BMIter liter, fiter;

  if (!CustomData_has_layer_named(&bm->ldata, CD_PROP_INT16_2D, "custom_normal")) {
    return false;
  }

  BM_lnorspace_update(bm);

  /* Create a loop normal layer. */
  if (!CustomData_has_layer(&bm->ldata, CD_NORMAL)) {
    BM_data_layer_add(bm, &bm->ldata, CD_NORMAL);

    CustomData_set_layer_flag(&bm->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }

  const int cd_custom_normal_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");
  const int cd_normal_offset = CustomData_get_offset(&bm->ldata, CD_NORMAL);

  int l_index = 0;
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      const short *clnors_data = static_cast<const short *>(
          BM_ELEM_CD_GET_VOID_P(l, cd_custom_normal_offset));
      float *normal = static_cast<float *>(BM_ELEM_CD_GET_VOID_P(l, cd_normal_offset));

      BKE_lnor_space_custom_data_to_normal(
          bm->lnor_spacearr->lspacearr[l_index], clnors_data, normal);
      l_index += 1;
    }
  }

  return true;
}

void BM_custom_loop_normals_from_vector_layer(BMesh *bm, bool add_sharp_edges)
{
  const int cd_custom_normal_offset = CustomData_get_offset_named(
      &bm->ldata, CD_PROP_INT16_2D, "custom_normal");
  if (cd_custom_normal_offset == -1) {
    return;
  }
  const int cd_normal_offset = CustomData_get_offset(&bm->ldata, CD_NORMAL);
  if (cd_normal_offset == -1) {
    return;
  }

  if (bm->lnor_spacearr == nullptr) {
    bm->lnor_spacearr = MEM_callocN<MLoopNorSpaceArray>(__func__);
  }

  bm_mesh_loops_custom_normals_set(bm,
                                   {},
                                   {},
                                   bm->lnor_spacearr,
                                   nullptr,
                                   cd_custom_normal_offset,
                                   nullptr,
                                   cd_normal_offset,
                                   add_sharp_edges);

  bm->spacearr_dirty &= ~(BM_SPACEARR_DIRTY | BM_SPACEARR_DIRTY_ALL);
}

/** \} */
