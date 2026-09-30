/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"
#include "BLI_math_vector_c.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static void extract_vert_positions_mesh(const MeshRenderData &mr,
                                        MutableSpan<float3> vert_data,
                                        MutableSpan<float3> loose_data)
{
  MutableSpan loose_edge_data = loose_data.take_front(mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = loose_data.take_back(mr.loose_verts.size());
  threading::memory_bandwidth_bound_task(
      mr.vert_positions.size_in_bytes() * 2 + loose_data.size_in_bytes(), [&]() {
        array_utils::copy(mr.vert_positions, vert_data);
        extract_mesh_loose_edge_data(mr.vert_positions, mr.edges, mr.loose_edges, loose_edge_data);
        array_utils::gather(mr.vert_positions, mr.loose_verts, loose_vert_data);
      });
}

static void extract_vert_positions_bm(const MeshRenderData &mr,
                                      MutableSpan<float3> vert_data,
                                      MutableSpan<float3> loose_data)
{
  const BMesh &bm = *mr.bm;
  MutableSpan loose_edge_data = loose_data.take_front(mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = loose_data.take_back(mr.loose_verts.size());

  threading::parallel_for(IndexRange(bm.totvert), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      vert_data[i] = bm_vert_co_get(mr, BM_vert_at_index(&const_cast<BMesh &>(bm), i));
    }
  });

  mr.loose_edges.foreach_index(
      [&](const int i, const int pos) {
        const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), i);
        loose_edge_data[pos * 2 + 0] = bm_vert_co_get(mr, edge.v1);
        loose_edge_data[pos * 2 + 1] = bm_vert_co_get(mr, edge.v2);
      },
      exec_mode::grain_size(4096));

  mr.loose_verts.foreach_index(
      [&](const int i, const int pos) {
        const BMVert &vert = *BM_vert_at_index(&const_cast<BMesh &>(bm), i);
        loose_vert_data[pos] = bm_vert_co_get(mr, &vert);
      },
      exec_mode::grain_size(2048));
}

gpu::VertBufPtr extract_positions(const MeshRenderData &mr, MeshBufferCache &cache)
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "pos", gpu::VertAttrType::SFLOAT_32_32_32);

  gpu::VertBufPtr vert_vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vert_vbo, mr.verts_num);
  Array<float3> loose_data(mr.loose_indices_num);
  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_vert_positions_mesh(mr, vert_vbo->data<float3>(), loose_data);
  }
  else {
    extract_vert_positions_bm(mr, vert_vbo->data<float3>(), loose_data);
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, mr.corners_num + mr.loose_indices_num));
  gather_vert_to_corner_gpu(mr, cache, *vert_vbo, *vbo);
  loose_data_upload(mr, loose_data.as_span(), *vbo);
  return vbo;
}

static void extract_loose_positions_subdiv(const DRWSubdivCache &subdiv_cache,
                                           const MeshRenderData &mr,
                                           gpu::VertBuf &vbo)
{
  const IndexMask &loose_verts = mr.loose_verts;
  const int loose_edges_num = mr.loose_edges.size();
  if (loose_verts.is_empty() && loose_edges_num == 0) {
    return;
  }

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(&vbo);

  const int resolution = subdiv_cache.resolution;
  const Span<float3> cached_positions = subdiv_cache.loose_edge_positions;
  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  const int edges_per_edge = subdiv_edges_per_coarse_edge(subdiv_cache);

  const int loose_geom_start = subdiv_cache.num_subdiv_loops;

  float3 edge_data[2];
  for (const int i : IndexRange(loose_edges_num)) {
    const int edge_offset = loose_geom_start + i * verts_per_edge;
    const Span<float3> positions = cached_positions.slice(i * resolution, resolution);
    for (const int edge : IndexRange(edges_per_edge)) {
      edge_data[0] = positions[edge + 0];
      edge_data[1] = positions[edge + 1];
      GPU_vertbuf_update_sub(
          &vbo, (edge_offset + edge * 2) * sizeof(float3), sizeof(float3) * 2, &edge_data);
    }
  }

  const int loose_verts_start = loose_geom_start + loose_edges_num * verts_per_edge;
  const Span<float3> positions = mr.vert_positions;
  loose_verts.foreach_index([&](const int i, const int pos) {
    GPU_vertbuf_update_sub(
        &vbo, (loose_verts_start + pos) * sizeof(float3), sizeof(float3), &positions[i]);
  });
}

gpu::VertBufPtr extract_positions_subdiv(const DRWSubdivCache &subdiv_cache,
                                         const MeshRenderData &mr,
                                         gpu::VertBufPtr *orco_vbo)
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute(
      "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, subdiv_full_vbo_size(mr, subdiv_cache)));

  if (subdiv_cache.num_subdiv_loops == 0) {
    extract_loose_positions_subdiv(subdiv_cache, mr, *vbo);
    return vbo;
  }

  if (orco_vbo) {
    /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
     * attributes. This is a substantial waste of video-ram and should be done another way.
     * Unfortunately, at the time of writing, I did not found any other "non disruptive"
     * alternative. */
    static const GPUVertFormat format = GPU_vertformat_from_attribute(
        "orco", gpu::VertAttrType::SFLOAT_32_32_32_32);
    *orco_vbo = gpu::VertBufPtr(
        GPU_vertbuf_create_on_device(format, subdiv_cache.num_subdiv_loops));
  }

  draw_subdiv_extract_pos(subdiv_cache, vbo.get(), orco_vbo ? orco_vbo->get() : nullptr);

  extract_loose_positions_subdiv(subdiv_cache, mr, *vbo);
  return vbo;
}

}  // namespace blender::draw
