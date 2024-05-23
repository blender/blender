/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static void extract_positions_mesh(const MeshRenderData &mr, MutableSpan<float3> vbo_data)
{
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  threading::memory_bandwidth_bound_task(
      mr.vert_positions.size_in_bytes() + mr.corner_verts.size_in_bytes() +
          vbo_data.size_in_bytes() + mr.loose_edges.size(),
      [&]() {
        array_utils::gather(mr.vert_positions, mr.corner_verts, corners_data);
        extract_mesh_loose_edge_data(mr.vert_positions, mr.edges, mr.loose_edges, loose_edge_data);
        array_utils::gather(mr.vert_positions, mr.loose_verts, loose_vert_data);
      });
}

static void extract_positions_bm(const MeshRenderData &mr, MutableSpan<float3> vbo_data)
{
  const BMesh &bm = *mr.bm;
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_edge_data = vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2);
  MutableSpan loose_vert_data = vbo_data.take_back(mr.loose_verts.size());

  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        const int index = BM_elem_index_get(loop);
        corners_data[index] = bm_vert_co_get(mr, loop->v);
        loop = loop->next;
      }
    }
  });

  const Span<int> loose_edges = mr.loose_edges;
  threading::parallel_for(loose_edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMEdge &edge = *BM_edge_at_index(&const_cast<BMesh &>(bm), loose_edges[i]);
      loose_edge_data[i * 2 + 0] = bm_vert_co_get(mr, edge.v1);
      loose_edge_data[i * 2 + 1] = bm_vert_co_get(mr, edge.v2);
    }
  });

  const Span<int> loose_verts = mr.loose_verts;
  threading::parallel_for(loose_verts.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      const BMVert &vert = *BM_vert_at_index(&const_cast<BMesh &>(bm), loose_verts[i]);
      loose_vert_data[i] = bm_vert_co_get(mr, &vert);
    }
  });
}

void extract_positions(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  GPU_vertbuf_init_with_format(&vbo, &format);
  GPU_vertbuf_data_alloc(&vbo, mr.corners_num + mr.loose_indices_num);

  MutableSpan vbo_data(static_cast<float3 *>(GPU_vertbuf_get_data(&vbo)),
                       GPU_vertbuf_get_vertex_len(&vbo));
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_positions_mesh(mr, vbo_data);
  }
  else {
    extract_positions_bm(mr, vbo_data);
  }
}

static GPUVertFormat *get_normals_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  return &format;
}

static GPUVertFormat *get_custom_normals_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  return &format;
}

static void extract_vertex_flags(const MeshRenderData &mr, char *flags)
{
  for (int i = 0; i < mr.verts_num; i++) {
    char *flag = &flags[i];
    const bool vert_hidden = !mr.hide_vert.is_empty() && mr.hide_vert[i];
    /* Flag for paint mode overlay. */
    if (vert_hidden || ((mr.v_origindex) && (mr.v_origindex[i] == ORIGINDEX_NONE))) {
      *flag = -1;
    }
    else if (!mr.select_vert.is_empty() && mr.select_vert[i]) {
      *flag = 1;
    }
    else {
      *flag = 0;
    }
  }
}

static void extract_loose_positions_subdiv(const DRWSubdivCache &subdiv_cache,
                                           const MeshRenderData &mr,
                                           gpu::VertBuf &vbo)
{
  const Span<int> loose_verts = mr.loose_verts;
  const int loose_edges_num = mr.loose_edges.size();
  if (loose_verts.is_empty() && loose_edges_num == 0) {
    return;
  }

  /* TODO(@kevindietrich): replace this when compressed normals are supported. */
  struct SubdivPosNorLoop {
    float pos[3];
    float nor[3];
    float flag;
  };

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(&vbo);

  const int resolution = subdiv_cache.resolution;
  const Span<float3> cached_positions = subdiv_cache.loose_edge_positions;
  const int verts_per_edge = subdiv_verts_per_coarse_edge(subdiv_cache);
  const int edges_per_edge = subdiv_edges_per_coarse_edge(subdiv_cache);

  const int loose_geom_start = subdiv_cache.num_subdiv_loops;

  SubdivPosNorLoop edge_data[2];
  memset(edge_data, 0, sizeof(SubdivPosNorLoop) * 2);
  for (const int i : IndexRange(loose_edges_num)) {
    const int edge_offset = loose_geom_start + i * verts_per_edge;
    const Span<float3> positions = cached_positions.slice(i * resolution, resolution);
    for (const int edge : IndexRange(edges_per_edge)) {
      copy_v3_v3(edge_data[0].pos, positions[edge + 0]);
      copy_v3_v3(edge_data[1].pos, positions[edge + 1]);
      GPU_vertbuf_update_sub(&vbo,
                             (edge_offset + edge * 2) * sizeof(SubdivPosNorLoop),
                             sizeof(SubdivPosNorLoop) * 2,
                             &edge_data);
    }
  }

  const int loose_verts_start = loose_geom_start + loose_edges_num * verts_per_edge;
  const Span<float3> positions = mr.vert_positions;

  SubdivPosNorLoop vert_data;
  memset(&vert_data, 0, sizeof(SubdivPosNorLoop));
  for (const int i : loose_verts.index_range()) {
    copy_v3_v3(vert_data.pos, positions[loose_verts[i]]);
    GPU_vertbuf_update_sub(&vbo,
                           (loose_verts_start + i) * sizeof(SubdivPosNorLoop),
                           sizeof(SubdivPosNorLoop),
                           &vert_data);
  }
}

void extract_positions_subdiv(const DRWSubdivCache &subdiv_cache,
                              const MeshRenderData &mr,
                              gpu::VertBuf &vbo,
                              gpu::VertBuf *orco_vbo)
{
  GPU_vertbuf_init_build_on_device(
      &vbo, draw_subdiv_get_pos_nor_format(), subdiv_full_vbo_size(mr, subdiv_cache));

  if (subdiv_cache.num_subdiv_loops == 0) {
    return;
  }

  gpu::VertBuf *flags_buffer = GPU_vertbuf_calloc();
  static GPUVertFormat flag_format = {0};
  if (flag_format.attr_len == 0) {
    GPU_vertformat_attr_add(&flag_format, "flag", GPU_COMP_I32, 1, GPU_FETCH_INT);
  }
  GPU_vertbuf_init_with_format(flags_buffer, &flag_format);
  GPU_vertbuf_data_alloc(flags_buffer, divide_ceil_u(mr.verts_num, 4));
  char *flags = static_cast<char *>(GPU_vertbuf_get_data(flags_buffer));
  extract_vertex_flags(mr, flags);
  GPU_vertbuf_tag_dirty(flags_buffer);

  if (orco_vbo) {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
       * attributes. This is a substantial waste of video-ram and should be done another way.
       * Unfortunately, at the time of writing, I did not found any other "non disruptive"
       * alternative. */
      GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    }
    GPU_vertbuf_init_build_on_device(orco_vbo, &format, subdiv_cache.num_subdiv_loops);
  }

  draw_subdiv_extract_pos_nor(subdiv_cache, flags_buffer, &vbo, orco_vbo);

  if (subdiv_cache.use_custom_loop_normals) {
    const Mesh *coarse_mesh = subdiv_cache.mesh;
    const Span<float3> corner_normals = coarse_mesh->corner_normals();

    gpu::VertBuf *src_custom_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_with_format(src_custom_normals, get_custom_normals_format());
    GPU_vertbuf_data_alloc(src_custom_normals, coarse_mesh->corners_num);

    memcpy(GPU_vertbuf_get_data(src_custom_normals),
           corner_normals.data(),
           corner_normals.size_in_bytes());

    gpu::VertBuf *dst_custom_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(
        dst_custom_normals, get_custom_normals_format(), subdiv_cache.num_subdiv_loops);

    draw_subdiv_interp_custom_data(
        subdiv_cache, src_custom_normals, dst_custom_normals, GPU_COMP_F32, 3, 0);

    draw_subdiv_finalize_custom_normals(subdiv_cache, dst_custom_normals, &vbo);

    GPU_vertbuf_discard(src_custom_normals);
    GPU_vertbuf_discard(dst_custom_normals);
  }
  else {
    /* We cannot evaluate vertex normals using the limit surface, so compute them manually. */
    gpu::VertBuf *subdiv_loop_subdiv_vert_index = draw_subdiv_build_origindex_buffer(
        subdiv_cache.subdiv_loop_subdiv_vert_index, subdiv_cache.num_subdiv_loops);

    gpu::VertBuf *vert_normals = GPU_vertbuf_calloc();
    GPU_vertbuf_init_build_on_device(
        vert_normals, get_normals_format(), subdiv_cache.num_subdiv_verts);

    draw_subdiv_accumulate_normals(subdiv_cache,
                                   &vbo,
                                   subdiv_cache.subdiv_vertex_face_adjacency_offsets,
                                   subdiv_cache.subdiv_vertex_face_adjacency,
                                   subdiv_loop_subdiv_vert_index,
                                   vert_normals);

    draw_subdiv_finalize_normals(subdiv_cache, vert_normals, subdiv_loop_subdiv_vert_index, &vbo);

    GPU_vertbuf_discard(vert_normals);
    GPU_vertbuf_discard(subdiv_loop_subdiv_vert_index);
  }

  GPU_vertbuf_discard(flags_buffer);

  extract_loose_positions_subdiv(subdiv_cache, mr, vbo);
}

}  // namespace blender::draw
