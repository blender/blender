/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_editmesh.hh"

#include "extract_mesh.hh"

#include "GPU_index_buffer.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Triangles Indices
 * \{ */

inline bool skip_bm_face(const BMFace &face, const bool sync_selection)
{
  if (BM_elem_flag_test(&face, BM_ELEM_HIDDEN)) {
    return true;
  }
  if (!sync_selection) {
    if (!BM_elem_flag_test_bool(&face, BM_ELEM_SELECT)) {
      return true;
    }
  }
  return false;
}

static void extract_edituv_tris_bm(const MeshRenderData &mr,
                                   const bool sync_selection,
                                   GPUIndexBufBuilder &builder)
{
  const Span<std::array<BMLoop *, 3>> looptris = mr.edit_bmesh->looptris;
  for (const int i : looptris.index_range()) {
    const std::array<BMLoop *, 3> &tri = looptris[i];
    if (skip_bm_face(*tri[0]->f, sync_selection)) {
      continue;
    }
    GPU_indexbuf_add_tri_verts(
        &builder, BM_elem_index_get(tri[0]), BM_elem_index_get(tri[1]), BM_elem_index_get(tri[2]));
  }
}

static void extract_edituv_tris_mesh(const MeshRenderData &mr,
                                     const bool sync_selection,
                                     GPUIndexBufBuilder &builder)
{
  const OffsetIndices faces = mr.faces;
  const Span<int3> corner_tris = mr.mesh->corner_tris();
  for (const int face : faces.index_range()) {
    const BMFace *face_orig = bm_original_face_get(mr, face);
    if (!face_orig) {
      continue;
    }
    if (skip_bm_face(*face_orig, sync_selection)) {
      continue;
    }
    const IndexRange tris = bke::mesh::face_triangles_range(faces, face);
    for (const int3 &tri : corner_tris.slice(tris)) {
      GPU_indexbuf_add_tri_verts(&builder, tri[0], tri[1], tri[2]);
    }
  }
}

void extract_edituv_tris(const MeshRenderData &mr, gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, mr.corner_tris_num, mr.corners_num);
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_tris_bm(mr, sync_selection, builder);
  }
  else {
    extract_edituv_tris_mesh(mr, sync_selection, builder);
  }

  GPU_indexbuf_build_in_place(&builder, &ibo);
}

static void extract_edituv_tris_subdiv_bm(const MeshRenderData &mr,
                                          const DRWSubdivCache &subdiv_cache,
                                          const bool sync_selection,
                                          GPUIndexBufBuilder &builder)
{
  const BMesh &bm = *mr.bm;
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);
  for (const int subdiv_quad_index : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const uint corner_start = subdiv_quad_index * 4;
    const int coarse_face = subdiv_loop_face_index[corner_start];
    const BMFace &face_orig = *BM_face_at_index(&const_cast<BMesh &>(bm), coarse_face);
    if (skip_bm_face(face_orig, sync_selection)) {
      continue;
    }
    GPU_indexbuf_add_tri_verts(&builder, corner_start, corner_start + 1, corner_start + 2);
    GPU_indexbuf_add_tri_verts(&builder, corner_start, corner_start + 2, corner_start + 3);
  }
}

static void extract_edituv_tris_subdiv_mesh(const MeshRenderData &mr,
                                            const DRWSubdivCache &subdiv_cache,
                                            const bool sync_selection,
                                            GPUIndexBufBuilder &builder)
{
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);
  for (const int subdiv_quad_index : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const uint corner_start = subdiv_quad_index * 4;
    const int coarse_face = subdiv_loop_face_index[corner_start];
    const BMFace *face_orig = bm_original_face_get(mr, coarse_face);
    if (!face_orig) {
      continue;
    }
    if (skip_bm_face(*face_orig, sync_selection)) {
      continue;
    }
    GPU_indexbuf_add_tri_verts(&builder, corner_start, corner_start + 1, corner_start + 2);
    GPU_indexbuf_add_tri_verts(&builder, corner_start, corner_start + 2, corner_start + 3);
  }
}

void extract_edituv_tris_subdiv(const MeshRenderData &mr,
                                const DRWSubdivCache &subdiv_cache,
                                gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_TRIS, subdiv_cache.num_subdiv_triangles, subdiv_cache.num_subdiv_loops);
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_tris_subdiv_bm(mr, subdiv_cache, sync_selection, builder);
  }
  else {
    extract_edituv_tris_subdiv_mesh(mr, subdiv_cache, sync_selection, builder);
  }

  GPU_indexbuf_build_in_place(&builder, &ibo);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Line Indices around faces
 * \{ */

static void extract_edituv_lines_bm(const MeshRenderData &mr,
                                    const bool sync_selection,
                                    GPUIndexBufBuilder &builder)
{
  const BMesh &bm = *mr.bm;
  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    if (skip_bm_face(*face, sync_selection)) {
      continue;
    }
    const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      GPU_indexbuf_add_line_verts(
          &builder, BM_elem_index_get(loop), BM_elem_index_get(loop->next));
      loop = loop->next;
    }
  }
}

static void extract_edituv_lines_mesh(const MeshRenderData &mr,
                                      const bool sync_selection,
                                      GPUIndexBufBuilder &builder)
{
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_edges = mr.corner_edges;
  const Span<int> orig_index_edge = mr.orig_index_edge ?
                                        Span<int>(mr.orig_index_edge, mr.edges_num) :
                                        Span<int>();
  if (mr.bm) {
    for (const int face_index : faces.index_range()) {
      const IndexRange face = faces[face_index];
      const BMFace *face_orig = bm_original_face_get(mr, face_index);
      if (!face_orig) {
        continue;
      }
      if (skip_bm_face(*face_orig, sync_selection)) {
        continue;
      }
      for (const int corner : face) {
        const int edge = corner_edges[corner];
        if (!orig_index_edge.is_empty() && orig_index_edge[edge] == ORIGINDEX_NONE) {
          continue;
        }
        const int corner_next = bke::mesh::face_corner_next(face, corner);
        GPU_indexbuf_add_line_verts(&builder, corner, corner_next);
      }
    }
  }
  else {
    IndexMaskMemory memory;
    IndexMask visible = faces.index_range();
    if (!mr.hide_poly.is_empty()) {
      visible = IndexMask::from_bools_inverse(visible, mr.hide_poly, memory);
    }
    if (!sync_selection) {
      if (mr.select_poly.is_empty()) {
        visible = {};
      }
      else {
        visible = IndexMask::from_bools(visible, mr.select_poly, memory);
      }
    }
    visible.foreach_index([&](const int face_index) {
      const IndexRange face = faces[face_index];
      for (const int corner : face) {
        const int edge = corner_edges[corner];
        if (!orig_index_edge.is_empty() && orig_index_edge[edge] == ORIGINDEX_NONE) {
          continue;
        }
        const int corner_next = bke::mesh::face_corner_next(face, corner);
        GPU_indexbuf_add_line_verts(&builder, corner, corner_next);
      }
    });
  }
}

void extract_edituv_lines(const MeshRenderData &mr, gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES, mr.corners_num, mr.corners_num);
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_lines_bm(mr, sync_selection, builder);
  }
  else {
    extract_edituv_lines_mesh(mr, sync_selection, builder);
  }

  GPU_indexbuf_build_in_place(&builder, &ibo);
}

static void extract_edituv_lines_subdiv_bm(const MeshRenderData &mr,
                                           const DRWSubdivCache &subdiv_cache,
                                           const bool sync_selection,
                                           GPUIndexBufBuilder &builder)
{
  const BMesh &bm = *mr.bm;
  const Span<int> subdiv_loop_edge_index = subdiv_cache.edges_orig_index->data<int>();
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);

  for (const int subdiv_quad : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const int coarse_face = subdiv_loop_face_index[subdiv_quad * 4];
    const BMFace &face_orig = *BM_face_at_index(&const_cast<BMesh &>(bm), coarse_face);
    if (skip_bm_face(face_orig, sync_selection)) {
      continue;
    }
    const IndexRange subdiv_face(subdiv_quad * 4, 4);
    for (const int subdiv_corner : subdiv_face) {
      const int coarse_edge = subdiv_loop_edge_index[subdiv_corner];
      if (coarse_edge == -1) {
        continue;
      }
      const int subdiv_corner_next = bke::mesh::face_corner_next(subdiv_face, subdiv_corner);
      GPU_indexbuf_add_line_verts(&builder, subdiv_corner, subdiv_corner_next);
    }
  }
}

static void extract_edituv_lines_subdiv_mesh(const MeshRenderData &mr,
                                             const DRWSubdivCache &subdiv_cache,
                                             const bool sync_selection,
                                             GPUIndexBufBuilder &builder)
{
  /* NOTE: #subdiv_loop_edge_index already has the #CD_ORIGINDEX layer baked in. */
  const Span<int> subdiv_loop_edge_index = subdiv_cache.edges_orig_index->data<int>();
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);
  /* TODO: Replace subdiv quad iteration with coarse face iteration. */
  for (const int subdiv_quad : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const int coarse_face = subdiv_loop_face_index[subdiv_quad * 4];
    if (const BMesh *bm = mr.bm) {
      const int orig_coarse_face = mr.orig_index_face ? mr.orig_index_face[coarse_face] :
                                                        coarse_face;
      const BMFace &face_orig = *BM_face_at_index(const_cast<BMesh *>(bm), orig_coarse_face);
      if (skip_bm_face(face_orig, sync_selection)) {
        continue;
      }
    }
    else {
      if (!mr.hide_poly.is_empty() && mr.hide_poly[coarse_face]) {
        continue;
      }
      if (!sync_selection) {
        if (mr.select_poly.is_empty() || !mr.select_poly[coarse_face]) {
          continue;
        }
      }
    }
    const IndexRange subdiv_face(subdiv_quad * 4, 4);
    for (const int subdiv_corner : subdiv_face) {
      const int coarse_edge = subdiv_loop_edge_index[subdiv_corner];
      if (coarse_edge == -1) {
        continue;
      }
      const int subdiv_corner_next = bke::mesh::face_corner_next(subdiv_face, subdiv_corner);
      GPU_indexbuf_add_line_verts(&builder, subdiv_corner, subdiv_corner_next);
    }
  }
}

void extract_edituv_lines_subdiv(const MeshRenderData &mr,
                                 const DRWSubdivCache &subdiv_cache,
                                 gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_LINES, subdiv_cache.num_subdiv_loops, subdiv_cache.num_subdiv_loops);
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_lines_subdiv_bm(mr, subdiv_cache, sync_selection, builder);
  }
  else {
    extract_edituv_lines_subdiv_mesh(mr, subdiv_cache, sync_selection, builder);
  }

  GPU_indexbuf_build_in_place(&builder, &ibo);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Points Indices
 * \{ */

static void extract_edituv_points_bm(const MeshRenderData &mr,
                                     const bool sync_selection,
                                     GPUIndexBufBuilder &builder)
{
  const BMesh &bm = *mr.bm;
  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    if (skip_bm_face(*face, sync_selection)) {
      continue;
    }
    const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      GPU_indexbuf_add_point_vert(&builder, BM_elem_index_get(loop));
      loop = loop->next;
    }
  }
}

static void extract_edituv_points_mesh(const MeshRenderData &mr,
                                       const bool sync_selection,
                                       GPUIndexBufBuilder &builder)
{
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  const Span<int> orig_index_vert = mr.orig_index_vert ?
                                        Span<int>(mr.orig_index_vert, mr.verts_num) :
                                        Span<int>();
  for (const int face_index : faces.index_range()) {
    const BMFace *face_orig = bm_original_face_get(mr, face_index);
    if (!face_orig) {
      continue;
    }
    if (skip_bm_face(*face_orig, sync_selection)) {
      continue;
    }
    for (const int corner : faces[face_index]) {
      const int vert = corner_verts[corner];
      if (!orig_index_vert.is_empty() && orig_index_vert[vert] == ORIGINDEX_NONE) {
        continue;
      }
      GPU_indexbuf_add_point_vert(&builder, corner);
    }
  }
}

void extract_edituv_points(const MeshRenderData &mr, gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, mr.corners_num, mr.corners_num);
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_points_bm(mr, sync_selection, builder);
  }
  else {
    extract_edituv_points_mesh(mr, sync_selection, builder);
  }
  GPU_indexbuf_build_in_place(&builder, &ibo);
}

static void extract_edituv_points_subdiv_bm(const MeshRenderData &mr,
                                            const DRWSubdivCache &subdiv_cache,
                                            const bool sync_selection,
                                            GPUIndexBufBuilder &builder)
{
  const BMesh &bm = *mr.bm;
  const Span<int> subdiv_loop_vert_index = subdiv_cache.verts_orig_index->data<int>();
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);

  for (const int subdiv_quad : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const int coarse_face = subdiv_loop_face_index[subdiv_quad * 4];
    const BMFace &face_orig = *BM_face_at_index(&const_cast<BMesh &>(bm), coarse_face);
    if (skip_bm_face(face_orig, sync_selection)) {
      continue;
    }
    for (const int subdiv_corner : IndexRange(subdiv_quad * 4, 4)) {
      const int coarse_vert = subdiv_loop_vert_index[subdiv_corner];
      if (coarse_vert == -1) {
        continue;
      }
      GPU_indexbuf_add_point_vert(&builder, subdiv_corner);
    }
  }
}

static void extract_edituv_points_subdiv_mesh(const MeshRenderData &mr,
                                              const DRWSubdivCache &subdiv_cache,
                                              const bool sync_selection,
                                              GPUIndexBufBuilder &builder)
{
  const Span<int> subdiv_loop_vert_index = subdiv_cache.verts_orig_index->data<int>();
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);

  for (const int subdiv_quad : IndexRange(subdiv_cache.num_subdiv_quads)) {
    const int coarse_face = subdiv_loop_face_index[subdiv_quad * 4];
    const BMFace *face_orig = bm_original_face_get(mr, coarse_face);
    if (!face_orig) {
      continue;
    }
    if (skip_bm_face(*face_orig, sync_selection)) {
      continue;
    }
    for (const int subdiv_corner : IndexRange(subdiv_quad * 4, 4)) {
      const int coarse_vert = subdiv_loop_vert_index[subdiv_corner];
      if (coarse_vert == -1) {
        continue;
      }
      GPU_indexbuf_add_point_vert(&builder, subdiv_corner);
    }
  }
}

void extract_edituv_points_subdiv(const MeshRenderData &mr,
                                  const DRWSubdivCache &subdiv_cache,
                                  gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_POINTS, subdiv_cache.num_subdiv_loops, subdiv_cache.num_subdiv_loops);
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_points_subdiv_bm(mr, subdiv_cache, sync_selection, builder);
  }
  else {
    extract_edituv_points_subdiv_mesh(mr, subdiv_cache, sync_selection, builder);
  }
  GPU_indexbuf_build_in_place(&builder, &ibo);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Face-dots Indices
 * \{ */

static void extract_edituv_face_dots_bm(const MeshRenderData &mr,
                                        const bool sync_selection,
                                        gpu::IndexBuf &ibo)
{
  const BMesh &bm = *mr.bm;
  IndexMaskMemory memory;
  const IndexMask visible = IndexMask::from_predicate(
      IndexMask(bm.totface), GrainSize(4096), memory, [&](const int i) {
        return !skip_bm_face(*BM_face_at_index(&const_cast<BMesh &>(bm), i), sync_selection);
      });

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, visible.size(), bm.totface);
  visible.to_indices(GPU_indexbuf_get_data(&builder).cast<int>());
  GPU_indexbuf_build_in_place_ex(&builder, 0, bm.totface, false, &ibo);
}

static void extract_edituv_face_dots_mesh(const MeshRenderData &mr,
                                          const bool sync_selection,
                                          gpu::IndexBuf &ibo)
{
  const OffsetIndices faces = mr.faces;
  IndexMaskMemory memory;
  IndexMask visible = IndexMask::from_predicate(
      faces.index_range(), GrainSize(4096), memory, [&](const int i) {
        const BMFace *face_orig = bm_original_face_get(mr, i);
        if (!face_orig) {
          return false;
        }
        if (skip_bm_face(*face_orig, sync_selection)) {
          return false;
        }
        return true;
      });
  if (mr.use_subsurf_fdots) {
    const BitSpan facedot_tags = mr.mesh->runtime->subsurf_face_dot_tags;
    const Span<int> corner_verts = mr.corner_verts;
    visible = IndexMask::from_predicate(visible, GrainSize(4096), memory, [&](const int i) {
      const Span<int> face_verts = corner_verts.slice(faces[i]);
      return std::any_of(face_verts.begin(), face_verts.end(), [&](const int vert) {
        return facedot_tags[vert];
      });
    });
  }

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, visible.size(), faces.size());
  visible.to_indices(GPU_indexbuf_get_data(&builder).cast<int>());
  GPU_indexbuf_build_in_place_ex(&builder, 0, faces.size(), false, &ibo);
}

void extract_edituv_face_dots(const MeshRenderData &mr, gpu::IndexBuf &ibo)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    extract_edituv_face_dots_bm(mr, sync_selection, ibo);
  }
  else {
    extract_edituv_face_dots_mesh(mr, sync_selection, ibo);
  }
}

/** \} */

}  // namespace blender::draw
