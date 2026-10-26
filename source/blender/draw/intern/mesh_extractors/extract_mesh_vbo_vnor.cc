/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "GPU_attribute_convert.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

static void extract_vert_normals_mesh(const MeshRenderData &mr,
                                      MutableSpan<int1010102_norm> vert_data,
                                      MutableSpan<int1010102_norm> loose_data)
{
  const Span<float3> vert_normals = mr.mesh->vert_normals();
  gpu::convert_normals(vert_normals, vert_data);

  /* Use the original normals rather than the converted values, since #vert_data is only written
   * to. */
  Array<float3> loose_normals(loose_data.size());
  MutableSpan loose_edge_normals = loose_normals.as_mutable_span().take_front(
      mr.loose_edges.size() * 2);
  MutableSpan loose_vert_normals = loose_normals.as_mutable_span().take_back(
      mr.loose_verts.size());
  extract_mesh_loose_edge_data(vert_normals, mr.edges, mr.loose_edges, loose_edge_normals);
  array_utils::gather(vert_normals, mr.loose_verts, loose_vert_normals);
  gpu::convert_normals(loose_normals.as_span(), loose_data);
}

static void extract_vert_normals_bmesh(const MeshRenderData &mr,
                                       MutableSpan<int1010102_norm> vert_data,
                                       MutableSpan<int1010102_norm> loose_data)
{
  const BMesh &bm = *mr.bm;
  MutableSpan loose_edge_data = loose_data.take_front(mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = loose_data.take_back(mr.loose_verts.size());

  threading::parallel_for(IndexRange(bm.totvert), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMVert *vert = BM_vert_at_index(&const_cast<BMesh &>(bm), i);
      vert_data[i] = gpu::convert_normal<int1010102_norm>(bm_vert_no_get(mr, vert));
    }
  });

  mr.loose_edges.foreach_index(
      [&](const int i, const int pos) {
        const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), i);
        loose_edge_data[pos * 2 + 0] = gpu::convert_normal<int1010102_norm>(
            bm_vert_no_get(mr, edge.v1));
        loose_edge_data[pos * 2 + 1] = gpu::convert_normal<int1010102_norm>(
            bm_vert_no_get(mr, edge.v2));
      },
      exec_mode::grain_size(2048));

  mr.loose_verts.foreach_index(
      [&](const int i, const int pos) {
        const BMVert *vert = BM_vert_at_index(&const_cast<BMesh &>(bm), i);
        loose_vert_data[pos] = gpu::convert_normal<int1010102_norm>(bm_vert_no_get(mr, vert));
      },
      exec_mode::grain_size(2048));
}

static const GPUVertFormat &get_vert_normals_format()
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "vnor", gpu::VertAttrType::SNORM_10_10_10_2);
  return format;
}

gpu::VertBufPtr extract_vert_normals(const MeshRenderData &mr, MeshBufferCache &cache)
{
  const GPUVertFormat &format = get_vert_normals_format();

  gpu::VertBufPtr vert_vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vert_vbo, mr.verts_num);
  Array<int1010102_norm> loose_data(mr.loose_indices_num);
  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_vert_normals_mesh(mr, vert_vbo->data<int1010102_norm>(), loose_data);
  }
  else {
    extract_vert_normals_bmesh(mr, vert_vbo->data<int1010102_norm>(), loose_data);
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, mr.corners_num + mr.loose_indices_num));
  gather_vert_to_corner_gpu(mr, cache, *vert_vbo, *vbo);
  loose_data_upload(mr, loose_data.as_span(), *vbo);
  return vbo;
}

gpu::VertBufPtr extract_vert_normals_subdiv(const MeshRenderData &mr,
                                            const DRWSubdivCache &subdiv_cache)
{
  const GPUVertFormat &format = get_vert_normals_format();

  const int vbo_size = subdiv_full_vbo_size(mr, subdiv_cache);

  gpu::VertBufPtr vnor = gpu::VertBufPtr(GPU_vertbuf_create_on_device(format, vbo_size));
  if (subdiv_cache.num_subdiv_loops == 0) {
    update_loose_normals(mr, subdiv_cache, *vnor);
    return vnor;
  }

  const Mesh *coarse_mesh = subdiv_cache.mesh;
  const uint corner_count = coarse_mesh->corner_normals().size();
  gpu::VertBufPtr src = gpu::VertBuf::from_span(coarse_mesh->corner_normals());
  gpu::VertBufPtr src_compressed = gpu::VertBuf::device_only<int1010102_norm>(corner_count);
  mesh_compress_normals(*src, *src_compressed);
  draw_subdiv_interp_corner_normals(subdiv_cache, *src_compressed, *vnor);

  update_loose_normals(mr, subdiv_cache, *vnor);
  return vnor;
}

}  // namespace blender::draw
