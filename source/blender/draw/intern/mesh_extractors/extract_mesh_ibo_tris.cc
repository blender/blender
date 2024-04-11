/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_editmesh.hh"

#include "GPU_index_buffer.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Triangles Indices (multi material)
 * \{ */

static void extract_tris_mesh(const MeshRenderData &mr, gpu::IndexBuf &ibo)
{
  const Span<int3> corner_tris = mr.corner_tris;
  if (!mr.face_sorted->face_tri_offsets) {
    /* There are no hidden faces and no reordering is necessary to group triangles with the same
     * material. The corner indices from #Mesh::corner_tris() can be copied directly to the GPU. */
    BLI_assert(mr.face_sorted->visible_tris_num == corner_tris.size());
    GPU_indexbuf_build_in_place_from_memory(&ibo,
                                            GPU_PRIM_TRIS,
                                            corner_tris.cast<uint32_t>().data(),
                                            corner_tris.size(),
                                            0,
                                            mr.corners_num,
                                            false);
    return;
  }

  const OffsetIndices faces = mr.faces;
  const Span<bool> hide_poly = mr.hide_poly;

  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, mr.face_sorted->visible_tris_num, mr.corners_num);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  const Span<int> face_tri_offsets = mr.face_sorted->face_tri_offsets->as_span();
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

  GPU_indexbuf_build_in_place_ex(&builder, 0, mr.face_sorted->visible_tris_num, false, &ibo);
}

static void extract_tris_bmesh(const MeshRenderData &mr, gpu::IndexBuf &ibo)
{
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_TRIS, mr.face_sorted->visible_tris_num, mr.corners_num);
  MutableSpan<uint3> data = GPU_indexbuf_get_data(&builder).cast<uint3>();

  BMesh &bm = *mr.bm;
  const Span<std::array<BMLoop *, 3>> looptris = mr.edit_bmesh->looptris;
  const Span<int> face_tri_offsets = *mr.face_sorted->face_tri_offsets;
  threading::parallel_for(IndexRange(bm.totface), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&bm, face_index);
      if (BM_elem_flag_test(&face, BM_ELEM_HIDDEN)) {
        continue;
      }
      const int loop_index = BM_elem_index_get(BM_FACE_FIRST_LOOP(&face));
      const IndexRange bm_tris(poly_to_tri_count(face_index, loop_index),
                               bke::mesh::face_triangles_num(face.len));
      const IndexRange ibo_tris(face_tri_offsets[face_index], bm_tris.size());
      for (const int i : bm_tris.index_range()) {
        data[ibo_tris[i]] = uint3(BM_elem_index_get(looptris[bm_tris[i]][0]),
                                  BM_elem_index_get(looptris[bm_tris[i]][1]),
                                  BM_elem_index_get(looptris[bm_tris[i]][2]));
      }
    }
  });

  GPU_indexbuf_build_in_place_ex(&builder, 0, mr.face_sorted->visible_tris_num, false, &ibo);
}

static void extract_tris_finish(const MeshRenderData &mr,
                                MeshBatchCache &cache,
                                gpu::IndexBuf &ibo)
{
  /* Create ibo sub-ranges. Always do this to avoid error when the standard surface batch
   * is created before the surfaces-per-material. */
  if (mr.use_final_mesh && cache.tris_per_mat) {
    int mat_start = 0;
    for (int i = 0; i < mr.materials_num; i++) {
      /* These IBOs have not been queried yet but we create them just in case they are needed
       * later since they are not tracked by mesh_buffer_cache_create_requested(). */
      if (cache.tris_per_mat[i] == nullptr) {
        cache.tris_per_mat[i] = GPU_indexbuf_calloc();
      }
      const int mat_tri_len = mr.face_sorted->tris_num_by_material[i];
      /* Multiply by 3 because these are triangle indices. */
      const int start = mat_start * 3;
      const int len = mat_tri_len * 3;
      GPU_indexbuf_create_subrange_in_place(cache.tris_per_mat[i], &ibo, start, len);
      mat_start += mat_tri_len;
    }
  }
}

static void extract_tris_init(const MeshRenderData &mr,
                              MeshBatchCache &cache,
                              void *ibo_v,
                              void * /*tls_data*/)
{
  gpu::IndexBuf &ibo = *static_cast<gpu::IndexBuf *>(ibo_v);

  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_tris_mesh(mr, ibo);
  }
  else {
    extract_tris_bmesh(mr, ibo);
  }

  extract_tris_finish(mr, cache, ibo);
}

static void extract_tris_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                     const MeshRenderData & /*mr*/,
                                     MeshBatchCache &cache,
                                     void *buffer,
                                     void * /*data*/)
{
  gpu::IndexBuf *ibo = static_cast<gpu::IndexBuf *>(buffer);
  /* Initialize the index buffer, it was already allocated, it will be filled on the device. */
  GPU_indexbuf_init_build_on_device(ibo, subdiv_cache.num_subdiv_triangles * 3);

  if (cache.tris_per_mat) {
    for (int i = 0; i < cache.mat_len; i++) {
      if (cache.tris_per_mat[i] == nullptr) {
        cache.tris_per_mat[i] = GPU_indexbuf_calloc();
      }

      /* Multiply by 6 since we have 2 triangles per quad. */
      const int start = subdiv_cache.mat_start[i] * 6;
      const int len = (subdiv_cache.mat_end[i] - subdiv_cache.mat_start[i]) * 6;
      GPU_indexbuf_create_subrange_in_place(cache.tris_per_mat[i], ibo, start, len);
    }
  }

  draw_subdiv_build_tris_buffer(subdiv_cache, ibo, cache.mat_len);
}

constexpr MeshExtract create_extractor_tris()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_tris_init;
  extractor.init_subdiv = extract_tris_init_subdiv;
  extractor.data_type = MR_DATA_CORNER_TRI | MR_DATA_POLYS_SORTED;
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.tris);
  return extractor;
}

/** \} */

const MeshExtract extract_tris = create_extractor_tris();

}  // namespace blender::draw
