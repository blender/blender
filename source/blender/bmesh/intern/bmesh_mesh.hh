/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "bmesh_class.hh"

#include "intern/bmesh_operator_api.hh"

struct BMAllocTemplate;

void BM_mesh_elem_toolflags_ensure(BMesh *bm);
void BM_mesh_elem_toolflags_clear(BMesh *bm);

struct BMeshCreateParams {
  bool use_toolflags : 1;
};

/**
 * \brief BMesh Make Mesh
 *
 * Allocates a new BMesh structure.
 *
 * \return The New bmesh
 *
 * \note ob is needed by multires
 */
BMesh *BM_mesh_create(const BMAllocTemplate *allocsize, const BMeshCreateParams *params);

/**
 * \brief BMesh Free Mesh
 *
 * Frees a BMesh data and its structure.
 */
void BM_mesh_free(BMesh *bm);
/**
 * \brief BMesh Free Mesh Data
 *
 * Frees a BMesh structure.
 *
 * \note frees mesh, but not actual BMesh struct
 */
void BM_mesh_data_free(BMesh *bm);
/**
 * \brief BMesh Clear Mesh
 *
 * Clear all data in bm
 */
void BM_mesh_clear(BMesh *bm);

/**
 * \brief BMesh Begin Edit
 *
 * Functions for setting up a mesh for editing and cleaning up after
 * the editing operations are done. These are called by the tools/operator
 * API for each time a tool is executed.
 */
void bmesh_edit_begin(BMesh *bm, BMOpTypeFlag type_flag);
/**
 * \brief BMesh End Edit
 */
void bmesh_edit_end(BMesh *bm, BMOpTypeFlag type_flag);

void BM_mesh_elem_index_ensure_ex(BMesh *bm, char htype, int elem_offset[4]);
void BM_mesh_elem_index_ensure(BMesh *bm, char htype);
/**
 * Array checking/setting macros.
 *
 * Currently vert/edge/loop/face index data is being abused, in a few areas of the code.
 *
 * To avoid correcting them afterwards, set 'bm->elem_index_dirty' however its possible
 * this flag is set incorrectly which could crash blender.
 *
 * Functions that calls this function may depend on dirty indices on being set.
 *
 * This is read-only, so it can be used for assertions that don't impact behavior.
 */
void BM_mesh_elem_index_validate(
    BMesh *bm, const char *location, const char *func, const char *msg_a, const char *msg_b);

#ifndef NDEBUG
/**
 * \see #BM_mesh_elem_index_validate the same rationale applies to this function.
 */
bool BM_mesh_elem_table_check(BMesh *bm);
#endif

/**
 * Re-allocates mesh data with/without toolflags.
 */
void BM_mesh_toolflags_set(BMesh *bm, bool use_toolflags);

void BM_mesh_elem_table_ensure(BMesh *bm, char htype);
/* use BM_mesh_elem_table_ensure where possible to avoid full rebuild */
void BM_mesh_elem_table_init(BMesh *bm, char htype);
void BM_mesh_elem_table_free(BMesh *bm, char htype);

BLI_INLINE BMVert *BM_vert_at_index(BMesh *bm, const int index)
{
  BLI_assert((index >= 0) && (index < bm->totvert));
  BLI_assert((bm->elem_table_dirty & BM_VERT) == 0);
  return bm->vtable[index];
}
BLI_INLINE BMEdge *BM_edge_at_index(BMesh *bm, const int index)
{
  BLI_assert((index >= 0) && (index < bm->totedge));
  BLI_assert((bm->elem_table_dirty & BM_EDGE) == 0);
  return bm->etable[index];
}
BLI_INLINE BMFace *BM_face_at_index(BMesh *bm, const int index)
{
  BLI_assert((index >= 0) && (index < bm->totface));
  BLI_assert((bm->elem_table_dirty & BM_FACE) == 0);
  return bm->ftable[index];
}

BMVert *BM_vert_at_index_find(BMesh *bm, int index);
BMEdge *BM_edge_at_index_find(BMesh *bm, int index);
BMFace *BM_face_at_index_find(BMesh *bm, int index);
BMLoop *BM_loop_at_index_find(BMesh *bm, int index);

/**
 * Use lookup table when available, else use slower find functions.
 *
 * \note Try to use #BM_mesh_elem_table_ensure instead.
 */
BMVert *BM_vert_at_index_find_or_table(BMesh *bm, int index);
BMEdge *BM_edge_at_index_find_or_table(BMesh *bm, int index);
BMFace *BM_face_at_index_find_or_table(BMesh *bm, int index);

// XXX

/**
 * Return the amount of element of type 'type' in a given bmesh.
 */
int BM_mesh_elem_count(BMesh *bm, char htype);

/**
 * Remaps the vertices, edges and/or faces of the bmesh as indicated by vert/edge/face_idx arrays
 * (xxx_idx[org_index] = new_index).
 *
 * A NULL array means no changes.
 *
 * \note
 * - Does not mess with indices, just sets elem_index_dirty flag.
 * - For verts/edges/faces only (as loops must remain "ordered" and "aligned"
 *   on a per-face basis...).
 *
 * \warning Be careful if you keep pointers to affected BM elements,
 * or arrays, when using this func!
 */
void BM_mesh_remap(BMesh *bm, const uint *vert_idx, const uint *edge_idx, const uint *face_idx);

/**
 * Use new memory pools for this mesh.
 *
 * \note needed for re-sizing elements (adding/removing tool flags)
 * but could also be used for packing fragmented bmeshes.
 */
void BM_mesh_rebuild(BMesh *bm,
                     const BMeshCreateParams *params,
                     BLI_mempool *vpool,
                     BLI_mempool *epool,
                     BLI_mempool *lpool,
                     BLI_mempool *fpool);

struct BMAllocTemplate {
  int totvert, totedge, totloop, totface;
};

/* used as an extern, defined in bmesh.h */
extern const BMAllocTemplate bm_mesh_allocsize_default;
extern const BMAllocTemplate bm_mesh_chunksize_default;

#define BMALLOC_TEMPLATE_FROM_BM(bm) \
  {(CHECK_TYPE_INLINE(bm, BMesh *), (bm)->totvert), (bm)->totedge, (bm)->totloop, (bm)->totface}

#define _VA_BMALLOC_TEMPLATE_FROM_ME_1(me) \
  { \
      (CHECK_TYPE_INLINE(me, Mesh *), (me)->verts_num), \
      (me)->edges_num, \
      (me)->corners_num, \
      (me)->faces_num, \
  }
#define _VA_BMALLOC_TEMPLATE_FROM_ME_2(me_a, me_b) \
  { \
      (CHECK_TYPE_INLINE(me_a, Mesh *), \
       CHECK_TYPE_INLINE(me_b, Mesh *), \
       (me_a)->verts_num + (me_b)->verts_num), \
      (me_a)->edges_num + (me_b)->edges_num, \
      (me_a)->corners_num + (me_b)->corners_num, \
      (me_a)->faces_num + (me_b)->faces_num, \
  }
#define BMALLOC_TEMPLATE_FROM_ME(...) \
  VA_NARGS_CALL_OVERLOAD(_VA_BMALLOC_TEMPLATE_FROM_ME_, __VA_ARGS__)

void BM_mesh_vert_normals_get(BMesh *bm, blender::MutableSpan<blender::float3> normals);

/* Vertex coords access. */
void BM_mesh_vert_coords_get(BMesh *bm, blender::MutableSpan<blender::float3> positions);
blender::Array<blender::float3> BM_mesh_vert_coords_alloc(BMesh *bm);
void BM_mesh_vert_coords_apply(BMesh *bm, blender::Span<blender::float3> vert_coords);
void BM_mesh_vert_coords_apply_with_mat4(BMesh *bm,
                                         blender::Span<blender::float3> vert_coords,
                                         const blender::float4x4 &transform);
