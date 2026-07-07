/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_editmesh.hh"

#include "BLI_math_geom.h"

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

static OffsetIndices<int> build_bmesh_face_offsets(const BMesh &bm, Array<int> &r_offset_data)
{
  r_offset_data.reinitialize(bm.totface + 1);
  threading::parallel_for(IndexRange(bm.totface), 4096, [&](const IndexRange range) {
    for (const int face : range) {
      r_offset_data[face] = BM_face_at_index(&const_cast<BMesh &>(bm), face)->len;
    }
  });
  return offset_indices::accumulate_counts_to_offsets(r_offset_data);
}

static gpu::IndexBufPtr extract_edituv_tris_bm(const MeshRenderData &mr, const bool sync_selection)
{
  const Span<std::array<BMLoop *, 3>> looptris = mr.edit_bmesh->looptris;
  const BMesh &bm = *mr.bm;

  IndexMaskMemory memory;
  const IndexMask selection = IndexMask::from_predicate(
      IndexRange(bm.totface), GrainSize(4096), memory, [&](const int face) {
        return !skip_bm_face(*BM_face_at_index(&const_cast<BMesh &>(bm), face), sync_selection);
      });

  if (selection.size() == bm.totface) {
    GPUIndexBufBuilder builder;
    GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, looptris.size(), mr.corners_num);
    MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();
    threading::parallel_for(looptris.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        data[i] = uint3(BM_elem_index_get(looptris[i][0]),
                        BM_elem_index_get(looptris[i][1]),
                        BM_elem_index_get(looptris[i][2]));
      }
    });
    return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, mr.corners_num, false));
  }

  Array<int> face_offset_data;
  const OffsetIndices faces = build_bmesh_face_offsets(bm, face_offset_data);

  Array<int> selected_face_offset_data(selection.size() + 1);
  const OffsetIndices selected_faces = offset_indices::gather_selected_offsets(
      faces, selection, selected_face_offset_data);

  const int tris_num = poly_to_tri_count(selected_faces.size(), selected_faces.total_size());

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, tris_num, mr.corners_num);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  selection.foreach_index(GrainSize(4096), [&](const int face, const int mask) {
    const IndexRange tris = bke::mesh::face_triangles_range(faces, face);
    const IndexRange ibo_tris = bke::mesh::face_triangles_range(selected_faces, mask);
    for (const int i : tris.index_range()) {
      data[ibo_tris[i]] = uint3(BM_elem_index_get(looptris[tris[i]][0]),
                                BM_elem_index_get(looptris[tris[i]][1]),
                                BM_elem_index_get(looptris[tris[i]][2]));
    }
  });

  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, mr.corners_num, false));
}

static gpu::IndexBufPtr extract_edituv_tris_mesh(const MeshRenderData &mr,
                                                 const bool sync_selection)
{
  const OffsetIndices faces = mr.faces;
  const Span<int3> corner_tris = mr.mesh->corner_tris();

  IndexMaskMemory memory;
  IndexMask selection;
  if (mr.bm) {
    selection = IndexMask::from_predicate(
        faces.index_range(), GrainSize(4096), memory, [&](const int face) {
          const BMFace *face_orig = bm_original_face_get(mr, face);
          if (!face_orig) {
            return false;
          }
          if (skip_bm_face(*face_orig, sync_selection)) {
            return false;
          }
          return true;
        });
  }
  else {
    if (mr.hide_poly.is_empty()) {
      selection = faces.index_range();
    }
    else {
      selection = IndexMask::from_bools_inverse(faces.index_range(), mr.hide_poly, memory);
    }

    if (!sync_selection && !mr.select_poly.is_empty()) {
      selection = IndexMask::from_bools(selection, mr.select_poly, memory);
    }
  }

  if (selection.size() == faces.size()) {
    return gpu::IndexBufPtr(GPU_indexbuf_build_from_memory(GPU_PRIM_TRIS,
                                                           corner_tris.cast<uint32_t>().data(),
                                                           corner_tris.size(),
                                                           0,
                                                           mr.corners_num,
                                                           false));
  }

  Array<int> selected_face_offset_data(selection.size() + 1);
  const OffsetIndices selected_faces = offset_indices::gather_selected_offsets(
      faces, selection, selected_face_offset_data);

  const int tris_num = poly_to_tri_count(selected_faces.size(), selected_faces.total_size());

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, tris_num, mr.corners_num);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  selection.foreach_index(GrainSize(4096), [&](const int face, const int mask) {
    const IndexRange tris = bke::mesh::face_triangles_range(faces, face);
    const IndexRange ibo_tris = bke::mesh::face_triangles_range(selected_faces, mask);
    for (const int i : tris.index_range()) {
      data[ibo_tris[i]] = uint3(
          corner_tris[tris[i]][0], corner_tris[tris[i]][1], corner_tris[tris[i]][2]);
    }
  });

  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, mr.corners_num, false));
}

gpu::IndexBufPtr extract_edituv_tris(const MeshRenderData &mr, const bool edit_uvs)
{
  const bool sync_selection = edit_uvs ? (mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) : false;
  if (mr.extract_type == MeshExtractType::BMesh) {
    return extract_edituv_tris_bm(mr, sync_selection);
  }
  return extract_edituv_tris_mesh(mr, sync_selection);
}

static gpu::IndexBufPtr build_tris_from_subdiv_quad_selection(const DRWSubdivCache &subdiv_cache,
                                                              const IndexMask &selection)
{
  const int tris_num = selection.size() * 2;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, tris_num, subdiv_cache.num_subdiv_loops);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  selection.foreach_index(GrainSize(4096), [&](const int subdiv_quad_index, const int mask) {
    const uint corner_start = subdiv_quad_index * 4;
    data[mask * 2 + 0] = uint3(corner_start, corner_start + 1, corner_start + 2);
    data[mask * 2 + 1] = uint3(corner_start, corner_start + 2, corner_start + 3);
  });

  return gpu::IndexBufPtr(
      GPU_indexbuf_build_ex(&builder, 0, subdiv_cache.num_subdiv_loops, false));
}

static gpu::IndexBufPtr extract_edituv_tris_subdiv_bm(const MeshRenderData &mr,
                                                      const DRWSubdivCache &subdiv_cache,
                                                      const bool sync_selection)
{
  const BMesh &bm = *mr.bm;
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);

  IndexMaskMemory memory;
  const IndexMask selection = IndexMask::from_predicate(
      IndexRange(subdiv_cache.num_subdiv_quads),
      GrainSize(4096),
      memory,
      [&](const int subdiv_quad_index) {
        const uint corner_start = subdiv_quad_index * 4;
        const int coarse_face = subdiv_loop_face_index[corner_start];
        const BMFace &bm_face = *BM_face_at_index(&const_cast<BMesh &>(bm), coarse_face);
        return !skip_bm_face(bm_face, sync_selection);
      });

  return build_tris_from_subdiv_quad_selection(subdiv_cache, selection);
}

static gpu::IndexBufPtr extract_edituv_tris_subdiv_mesh(const MeshRenderData &mr,
                                                        const DRWSubdivCache &subdiv_cache,
                                                        const bool sync_selection)
{
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);

  IndexMaskMemory memory;
  const IndexMask selection = IndexMask::from_predicate(
      IndexRange(subdiv_cache.num_subdiv_quads),
      GrainSize(4096),
      memory,
      [&](const int subdiv_quad_index) {
        const uint corner_start = subdiv_quad_index * 4;
        const int coarse_face = subdiv_loop_face_index[corner_start];
        const BMFace *face_orig = bm_original_face_get(mr, coarse_face);
        if (!face_orig) {
          return false;
        }
        if (skip_bm_face(*face_orig, sync_selection)) {
          return false;
        }
        return true;
      });

  return build_tris_from_subdiv_quad_selection(subdiv_cache, selection);
}

gpu::IndexBufPtr extract_edituv_tris_subdiv(const MeshRenderData &mr,
                                            const DRWSubdivCache &subdiv_cache)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0;
  if (mr.extract_type == MeshExtractType::BMesh) {
    return extract_edituv_tris_subdiv_bm(mr, subdiv_cache, sync_selection);
  }
  return extract_edituv_tris_subdiv_mesh(mr, subdiv_cache, sync_selection);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Line Indices around faces
 * \{ */

static gpu::IndexBufPtr extract_edituv_lines_bm(const MeshRenderData &mr,
                                                const bool sync_selection)
{
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES, mr.corners_num, mr.corners_num);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();
  int line_index = 0;

  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, mr.bm, BM_FACES_OF_MESH) {
    if (skip_bm_face(*face, sync_selection)) {
      continue;
    }
    const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      data[line_index++] = uint2(BM_elem_index_get(loop), BM_elem_index_get(loop->next));
      loop = loop->next;
    }
  }

  /* Only upload the part of the index buffer that is used. Alternatively it might be beneficial to
   * count the number of visible edges first, especially if that allows parallelizing filling the
   * data array. */
  builder.index_len = line_index * 2;
  builder.index_min = 0;
  builder.index_max = mr.corners_num;
  builder.uses_restart_indices = false;
  gpu::IndexBufPtr result = gpu::IndexBufPtr(GPU_indexbuf_calloc());
  GPU_indexbuf_build_in_place(&builder, result.get());
  return result;
}

static gpu::IndexBufPtr extract_edituv_lines_mesh(const MeshRenderData &mr,
                                                  const bool sync_selection)
{
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_edges = mr.corner_edges;
  const Span<int> orig_index_edge = mr.orig_index_edge ?
                                        Span<int>(mr.orig_index_edge, mr.edges_num) :
                                        Span<int>();

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_LINES, mr.corners_num, mr.corners_num);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();
  int line_index = 0;

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
        data[line_index++] = edge_from_corners(face, corner);
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
        data[line_index++] = edge_from_corners(face, corner);
      }
    });
  }

  /* Only upload the part of the index buffer that is used. Alternatively it might be beneficial to
   * count the number of visible edges first, especially if that allows parallelizing filling the
   * data array. */
  builder.index_len = line_index * 2;
  builder.index_min = 0;
  builder.index_max = mr.corners_num;
  builder.uses_restart_indices = false;
  gpu::IndexBufPtr result = gpu::IndexBufPtr(GPU_indexbuf_calloc());
  GPU_indexbuf_build_in_place(&builder, result.get());
  return result;
}

gpu::IndexBufPtr extract_edituv_lines(const MeshRenderData &mr, const UvExtractionMode mode)
{
  bool sync_selection = false;
  switch (mode) {
    case UvExtractionMode::All:
      sync_selection = true;
      break;
    case UvExtractionMode::Edit:
      sync_selection = ((mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0);
      break;
    case UvExtractionMode::Selection:
      sync_selection = false;
      break;
  }

  if (mr.extract_type == MeshExtractType::BMesh) {
    return extract_edituv_lines_bm(mr, sync_selection);
  }
  return extract_edituv_lines_mesh(mr, sync_selection);
}

static gpu::IndexBufPtr extract_edituv_lines_subdiv_bm(const MeshRenderData &mr,
                                                       const DRWSubdivCache &subdiv_cache,
                                                       const bool sync_selection)
{
  const BMesh &bm = *mr.bm;
  const Span<int> subdiv_loop_edge_index = subdiv_cache.edges_orig_index->data<int>();
  const Span<int> subdiv_loop_face_index(subdiv_cache.subdiv_loop_face_index,
                                         subdiv_cache.num_subdiv_loops);

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_LINES, subdiv_cache.num_subdiv_loops, subdiv_cache.num_subdiv_loops);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();
  int line_index = 0;

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
      data[line_index++] = edge_from_corners(subdiv_face, subdiv_corner);
    }
  }

  return gpu::IndexBufPtr(
      GPU_indexbuf_build_ex(&builder, 0, subdiv_cache.num_subdiv_loops, false));
}

static gpu::IndexBufPtr extract_edituv_lines_subdiv_mesh(const MeshRenderData &mr,
                                                         const DRWSubdivCache &subdiv_cache,
                                                         const bool sync_selection)
{
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_LINES, subdiv_cache.num_subdiv_loops, subdiv_cache.num_subdiv_loops);
  MutableSpan<uint2> data = GPU_indexbuf_get_data(&builder).cast<uint2>();
  int line_index = 0;

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
      data[line_index++] = edge_from_corners(subdiv_face, subdiv_corner);
    }
  }

  /* Only upload the part of the index buffer that is used. Alternatively it might be beneficial to
   * count the number of visible edges first, especially if that allows parallelizing filling the
   * data array. */
  builder.index_len = line_index * 2;
  builder.index_min = 0;
  builder.index_max = subdiv_cache.num_subdiv_loops;
  builder.uses_restart_indices = false;
  gpu::IndexBufPtr result = gpu::IndexBufPtr(GPU_indexbuf_calloc());
  GPU_indexbuf_build_in_place(&builder, result.get());
  return result;
}

gpu::IndexBufPtr extract_edituv_lines_subdiv(const MeshRenderData &mr,
                                             const DRWSubdivCache &subdiv_cache,
                                             const UvExtractionMode mode)
{
  bool sync_selection = false;
  switch (mode) {
    case UvExtractionMode::All:
      sync_selection = true;
      break;
    case UvExtractionMode::Edit:
      sync_selection = ((mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0);
      break;
    case UvExtractionMode::Selection:
      sync_selection = false;
      break;
  }

  if (mr.extract_type == MeshExtractType::BMesh) {
    return extract_edituv_lines_subdiv_bm(mr, subdiv_cache, sync_selection);
  }
  return extract_edituv_lines_subdiv_mesh(mr, subdiv_cache, sync_selection);
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

gpu::IndexBufPtr extract_edituv_points(const MeshRenderData &mr)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, mr.corners_num, mr.corners_num);
  if (mr.extract_type == MeshExtractType::BMesh) {
    extract_edituv_points_bm(mr, sync_selection, builder);
  }
  else {
    extract_edituv_points_mesh(mr, sync_selection, builder);
  }
  return gpu::IndexBufPtr(GPU_indexbuf_build(&builder));
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

gpu::IndexBufPtr extract_edituv_points_subdiv(const MeshRenderData &mr,
                                              const DRWSubdivCache &subdiv_cache)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(
      &builder, GPU_PRIM_POINTS, subdiv_cache.num_subdiv_loops, subdiv_cache.num_subdiv_loops);
  if (mr.extract_type == MeshExtractType::BMesh) {
    extract_edituv_points_subdiv_bm(mr, subdiv_cache, sync_selection, builder);
  }
  else {
    extract_edituv_points_subdiv_mesh(mr, subdiv_cache, sync_selection, builder);
  }
  return gpu::IndexBufPtr(GPU_indexbuf_build(&builder));
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Face-dots Indices
 * \{ */

static gpu::IndexBufPtr extract_edituv_face_dots_bm(const MeshRenderData &mr,
                                                    const bool sync_selection)
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
  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, bm.totface, false));
}

static gpu::IndexBufPtr extract_edituv_face_dots_mesh(const MeshRenderData &mr,
                                                      const bool sync_selection)
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
  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, faces.size(), false));
}

gpu::IndexBufPtr extract_edituv_face_dots(const MeshRenderData &mr)
{
  const bool sync_selection = (mr.toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0;
  if (mr.extract_type == MeshExtractType::BMesh) {
    return extract_edituv_face_dots_bm(mr, sync_selection);
  }
  return extract_edituv_face_dots_mesh(mr, sync_selection);
}

/** \} */

}  // namespace blender::draw
