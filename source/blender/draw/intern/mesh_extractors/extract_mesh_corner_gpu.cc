/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_math_base_c.hh"

#include "GPU_capabilities.hh"
#include "GPU_compute.hh"
#include "GPU_shader.hh"
#include "GPU_state.hh"

#include "draw_defines.hh"
#include "draw_shader.hh"

#include "extract_mesh.hh"

namespace blender::draw {

template<typename GetIndex>
static gpu::VertBufPtr bm_corner_indices_create(const BMesh &bm, const GetIndex get_index)
{
  gpu::VertBufPtr buf = gpu::VertBuf::from_size<int>(bm.totloop);
  MutableSpan<int> data = buf->data<int>();
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        data[BM_elem_index_get(loop)] = get_index(*loop);
        loop = loop->next;
      }
    }
  });
  return buf;
}

static gpu::VertBuf &corner_verts_ensure(const MeshRenderData &mr, MeshBufferCache &cache)
{
  if (!cache.corner_verts) {
    if (mr.extract_type == MeshExtractType::Mesh) {
      cache.corner_verts = gpu::VertBuf::from_span(mr.corner_verts);
    }
    else {
      cache.corner_verts = bm_corner_indices_create(
          *mr.bm, [](const BMLoop &loop) { return BM_elem_index_get(loop.v); });
    }
  }
  return *cache.corner_verts;
}

static gpu::VertBuf &corner_edges_ensure(const MeshRenderData &mr, MeshBufferCache &cache)
{
  if (!cache.corner_edges) {
    if (mr.extract_type == MeshExtractType::Mesh) {
      cache.corner_edges = gpu::VertBuf::from_span(mr.corner_edges);
    }
    else {
      cache.corner_edges = bm_corner_indices_create(
          *mr.bm, [](const BMLoop &loop) { return BM_elem_index_get(loop.e); });
    }
  }
  return *cache.corner_edges;
}

static gpu::VertBuf &face_offsets_ensure(const MeshRenderData &mr, MeshBufferCache &cache)
{
  if (cache.face_offsets) {
    return *cache.face_offsets;
  }
  if (mr.extract_type == MeshExtractType::Mesh) {
    cache.face_offsets = gpu::VertBuf::from_span(mr.faces.data().cast<uint>());
    return *cache.face_offsets;
  }
  const BMesh &bm = *mr.bm;
  cache.face_offsets = gpu::VertBuf::from_size<int>(bm.totface + 1);
  MutableSpan<int> data = cache.face_offsets->data<int>();
  threading::parallel_for(IndexRange(bm.totface), 4096, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      data[face_index] = BM_elem_index_get(BM_FACE_FIRST_LOOP(&face));
    }
  });
  data[bm.totface] = bm.totloop;
  return *cache.face_offsets;
}

static void dispatch_batched(gpu::Shader *shader, const int size)
{
  const int64_t max_batch_size = int64_t(GPU_max_work_group_count(0)) *
                                 DRW_MESH_TO_CORNER_GROUP_SIZE;
  int start = 0;
  while (start < size) {
    const int batch_size = std::min(int64_t(size - start), max_batch_size);
    GPU_shader_uniform_1i(shader, "start", start);
    GPU_shader_uniform_1i(shader, "count", batch_size);
    GPU_compute_dispatch(shader, divide_ceil_u(batch_size, DRW_MESH_TO_CORNER_GROUP_SIZE), 1, 1);
    start += batch_size;
  }
}

static int element_size(const gpu::VertBuf &src, const gpu::VertBuf &dst)
{
  const int size = GPU_vertbuf_get_format(&dst)->stride;
  BLI_assert(GPU_vertbuf_get_format(&src)->stride == size);
  UNUSED_VARS_NDEBUG(src);
  return size;
}

static void gather_to_corner_gpu(const MeshRenderData &mr,
                                 gpu::VertBuf &src,
                                 gpu::VertBuf &indices,
                                 gpu::VertBuf &dst)
{
  BLI_assert(GPU_vertbuf_get_vertex_len(&dst) >= mr.corners_num);
  gpu::Shader *shader = DRW_shader_mesh_gather_get(element_size(src, dst));
  GPU_shader_bind(shader);
  GPU_vertbuf_bind_as_ssbo(&src, 0);
  GPU_vertbuf_bind_as_ssbo(&indices, 1);
  GPU_vertbuf_bind_as_ssbo(&dst, 2);
  dispatch_batched(shader, mr.corners_num);
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_SHADER_STORAGE);
  GPU_shader_unbind();
}

void gather_vert_to_corner_gpu(const MeshRenderData &mr,
                               MeshBufferCache &cache,
                               gpu::VertBuf &src,
                               gpu::VertBuf &dst)
{
  if (mr.corners_num == 0) {
    return;
  }
  BLI_assert(GPU_vertbuf_get_vertex_len(&src) == mr.verts_num);
  gather_to_corner_gpu(mr, src, corner_verts_ensure(mr, cache), dst);
}

void gather_edge_to_corner_gpu(const MeshRenderData &mr,
                               MeshBufferCache &cache,
                               gpu::VertBuf &src,
                               gpu::VertBuf &dst)
{
  if (mr.corners_num == 0) {
    return;
  }
  BLI_assert(GPU_vertbuf_get_vertex_len(&src) == mr.edges_num);
  gather_to_corner_gpu(mr, src, corner_edges_ensure(mr, cache), dst);
}

void scatter_face_to_corner_gpu(const MeshRenderData &mr,
                                MeshBufferCache &cache,
                                gpu::VertBuf &src,
                                gpu::VertBuf &dst)
{
  if (mr.corners_num == 0) {
    return;
  }
  BLI_assert(GPU_vertbuf_get_vertex_len(&src) == mr.faces_num);
  BLI_assert(GPU_vertbuf_get_vertex_len(&dst) >= mr.corners_num);
  gpu::Shader *shader = DRW_shader_mesh_scatter_faces_get(element_size(src, dst));
  GPU_shader_bind(shader);
  GPU_vertbuf_bind_as_ssbo(&src, 0);
  GPU_vertbuf_bind_as_ssbo(&face_offsets_ensure(mr, cache), 1);
  GPU_vertbuf_bind_as_ssbo(&dst, 2);
  dispatch_batched(shader, mr.faces_num);
  GPU_memory_barrier(GPU_BARRIER_VERTEX_ATTRIB_ARRAY | GPU_BARRIER_SHADER_STORAGE);
  GPU_shader_unbind();
}

}  // namespace blender::draw
