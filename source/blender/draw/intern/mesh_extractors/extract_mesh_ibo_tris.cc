/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_editmesh.hh"

#include "BLI_math_geom.h"

#include "GPU_index_buffer.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static gpu::IndexBufPtr extract_tris_mesh(const MeshRenderData &mr,
                                          const SortedFaceData &face_sorted)
{
  const Span<int3> corner_tris = mr.mesh->corner_tris();
  if (!face_sorted.face_tri_offsets) {
    /* There are no hidden faces and no reordering is necessary to group triangles with the same
     * material. The corner indices from #Mesh::corner_tris() can be copied directly to the GPU. */
    BLI_assert(face_sorted.visible_tris_num == corner_tris.size());
    return gpu::IndexBufPtr(GPU_indexbuf_build_from_memory(GPU_PRIM_TRIS,
                                                           corner_tris.cast<uint32_t>().data(),
                                                           corner_tris.size(),
                                                           0,
                                                           mr.corners_num,
                                                           false));
  }

  const OffsetIndices faces = mr.faces;
  const Span<bool> hide_poly = mr.hide_poly;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, face_sorted.visible_tris_num, mr.corners_num);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  const Span<int> face_tri_offsets = face_sorted.face_tri_offsets->as_span();
  threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
    for (const int face : range) {
      if (!hide_poly.is_empty() && hide_poly[face]) {
        continue;
      }
      const IndexRange mesh_range = bke::mesh::face_triangles_range(faces, face);
      const Span<uint3> mesh_tris = corner_tris.slice(mesh_range).cast<uint3>();
      MutableSpan<uint3> ibo_tris = data.slice(face_tri_offsets[face], mesh_tris.size());
      ibo_tris.copy_from(mesh_tris);
    }
  });

  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, mr.corners_num, false));
}

static gpu::IndexBufPtr extract_tris_bmesh(const MeshRenderData &mr,
                                           const SortedFaceData &face_sorted)
{
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, face_sorted.visible_tris_num, mr.corners_num);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  BMesh &bm = *mr.bm;
  const Span<std::array<BMLoop *, 3>> looptris = mr.edit_bmesh->looptris;
  const Span<int> face_tri_offsets = *face_sorted.face_tri_offsets;
  threading::parallel_for(IndexRange(bm.totface), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&bm, face_index);
      if (BM_elem_flag_test(&face, BM_ELEM_HIDDEN)) {
        continue;
      }
      const int corner_index = BM_elem_index_get(BM_FACE_FIRST_LOOP(&face));
      const IndexRange bm_tris(poly_to_tri_count(face_index, corner_index),
                               bke::mesh::face_triangles_num(face.len));
      const IndexRange ibo_tris(face_tri_offsets[face_index], bm_tris.size());
      for (const int i : bm_tris.index_range()) {
        data[ibo_tris[i]] = uint3(BM_elem_index_get(looptris[bm_tris[i]][0]),
                                  BM_elem_index_get(looptris[bm_tris[i]][1]),
                                  BM_elem_index_get(looptris[bm_tris[i]][2]));
      }
    }
  });

  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, bm.totloop, false));
}

void create_material_subranges(const SortedFaceData &face_sorted,
                               gpu::IndexBuf &tris_ibo,
                               MutableSpan<gpu::IndexBufPtr> ibos)
{
  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  int mat_start = 0;
  for (const int i : face_sorted.tris_num_by_material.index_range()) {
    /* These IBOs have not been queried yet but we create them just in case they are needed
     * later since they are not tracked by mesh_buffer_cache_create_requested(). */

    const int mat_tri_len = face_sorted.tris_num_by_material[i];
    /* Multiply by 3 because these are triangle indices. */
    const int start = mat_start * 3;
    const int len = mat_tri_len * 3;
    ibos[i] = gpu::IndexBufPtr(GPU_indexbuf_create_subrange(&tris_ibo, start, len));
    mat_start += mat_tri_len;
  }
}

gpu::IndexBufPtr extract_tris(const MeshRenderData &mr, const SortedFaceData &face_sorted)
{
  if (mr.extract_type == MeshExtractType::Mesh) {
    return extract_tris_mesh(mr, face_sorted);
  }
  return extract_tris_bmesh(mr, face_sorted);
}

gpu::IndexBufPtr extract_tris_subdiv(const DRWSubdivCache &subdiv_cache, MeshBatchCache &cache)
{
  /* Initialize the index buffer, it was already allocated, it will be filled on the device. */
  gpu::IndexBufPtr ibo = gpu::IndexBufPtr(
      GPU_indexbuf_build_on_device(subdiv_cache.num_subdiv_triangles * 3));

  if (!cache.tris_per_mat.is_empty()) {
    for (int i = 0; i < cache.mat_len; i++) {
      /* Multiply by 6 since we have 2 triangles per quad. */
      const int start = subdiv_cache.mat_start[i] * 6;
      const int len = (subdiv_cache.mat_end[i] - subdiv_cache.mat_start[i]) * 6;
      cache.tris_per_mat[i] = gpu::IndexBufPtr(
          GPU_indexbuf_create_subrange(ibo.get(), start, len));
    }
  }

  draw_subdiv_build_tris_buffer(subdiv_cache, ibo.get(), cache.mat_len);
  return ibo;
}

}  // namespace blender::draw
