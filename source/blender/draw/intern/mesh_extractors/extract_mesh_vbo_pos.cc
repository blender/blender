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

/* ---------------------------------------------------------------------- */
/** \name Extract Position and Vertex Normal
 * \{ */

static void extract_pos_init(const MeshRenderData &mr,
                             MeshBatchCache & /*cache*/,
                             void *buf,
                             void *tls_data)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.corners_num + mr.loose_indices_num);

  MutableSpan vbo_data(static_cast<float3 *>(GPU_vertbuf_get_data(vbo)),
                       GPU_vertbuf_get_vertex_len(vbo));
  if (mr.extract_type == MR_EXTRACT_MESH) {
    threading::memory_bandwidth_bound_task(
        mr.vert_positions.size_in_bytes() + mr.corner_verts.size_in_bytes() +
            vbo_data.size_in_bytes() + mr.loose_edges.size(),
        [&]() {
          array_utils::gather(
              mr.vert_positions, mr.corner_verts, vbo_data.take_front(mr.corner_verts.size()));
          extract_mesh_loose_edge_data(mr.vert_positions,
                                       mr.edges,
                                       mr.loose_edges,
                                       vbo_data.slice(mr.corners_num, mr.loose_edges.size() * 2));
          array_utils::gather(
              mr.vert_positions, mr.loose_verts, vbo_data.take_back(mr.loose_verts.size()));
        });
  }
  else {
    *static_cast<float3 **>(tls_data) = vbo_data.data();
  }
}

static void extract_pos_iter_face_bm(const MeshRenderData &mr,
                                     const BMFace *f,
                                     const int /*f_index*/,
                                     void *_data)
{
  float3 *data = *static_cast<float3 **>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    data[l_index] = bm_vert_co_get(mr, l_iter->v);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_pos_iter_loose_edge_bm(const MeshRenderData &mr,
                                           const BMEdge *eed,
                                           const int loose_edge_i,
                                           void *_data)
{
  float3 *data = *static_cast<float3 **>(_data);
  int index = mr.corners_num + loose_edge_i * 2;
  data[index + 0] = bm_vert_co_get(mr, eed->v1);
  data[index + 1] = bm_vert_co_get(mr, eed->v2);
}

static void extract_pos_iter_loose_vert_bm(const MeshRenderData &mr,
                                           const BMVert *eve,
                                           const int loose_vert_i,
                                           void *_data)
{
  float3 *data = *static_cast<float3 **>(_data);
  const int offset = mr.corners_num + (mr.loose_edges_num * 2);
  const int index = offset + loose_vert_i;
  data[index] = bm_vert_co_get(mr, eve);
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

static void extract_pos_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                    const MeshRenderData &mr,
                                    MeshBatchCache &cache,
                                    void *buffer,
                                    void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;

  /* Initialize the vertex buffer, it was already allocated. */
  GPU_vertbuf_init_build_on_device(
      vbo, draw_subdiv_get_pos_nor_format(), subdiv_cache.num_subdiv_loops + loose_geom.loop_len);

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

  gpu::VertBuf *orco_vbo = cache.final.buff.vbo.orco;

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

  draw_subdiv_extract_pos_nor(subdiv_cache, flags_buffer, vbo, orco_vbo);

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

    draw_subdiv_finalize_custom_normals(subdiv_cache, dst_custom_normals, vbo);

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
                                   vbo,
                                   subdiv_cache.subdiv_vertex_face_adjacency_offsets,
                                   subdiv_cache.subdiv_vertex_face_adjacency,
                                   subdiv_loop_subdiv_vert_index,
                                   vert_normals);

    draw_subdiv_finalize_normals(subdiv_cache, vert_normals, subdiv_loop_subdiv_vert_index, vbo);

    GPU_vertbuf_discard(vert_normals);
    GPU_vertbuf_discard(subdiv_loop_subdiv_vert_index);
  }

  GPU_vertbuf_discard(flags_buffer);
}

static void extract_pos_loose_geom_subdiv(const DRWSubdivCache &subdiv_cache,
                                          const MeshRenderData & /*mr*/,
                                          void *buffer,
                                          void * /*data*/)
{
  const DRWSubdivLooseGeom &loose_geom = subdiv_cache.loose_geom;
  if (loose_geom.loop_len == 0) {
    return;
  }

  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  uint offset = subdiv_cache.num_subdiv_loops;

  /* TODO(@kevindietrich): replace this when compressed normals are supported. */
  struct SubdivPosNorLoop {
    float pos[3];
    float nor[3];
    float flag;
  };

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(vbo);

  Span<DRWSubdivLooseEdge> loose_edges = draw_subdiv_cache_get_loose_edges(subdiv_cache);

  SubdivPosNorLoop edge_data[2];
  memset(edge_data, 0, sizeof(SubdivPosNorLoop) * 2);
  for (const DRWSubdivLooseEdge &loose_edge : loose_edges) {
    const DRWSubdivLooseVertex &v1 = loose_geom.verts[loose_edge.loose_subdiv_v1_index];
    const DRWSubdivLooseVertex &v2 = loose_geom.verts[loose_edge.loose_subdiv_v2_index];

    copy_v3_v3(edge_data[0].pos, v1.co);
    copy_v3_v3(edge_data[1].pos, v2.co);

    GPU_vertbuf_update_sub(
        vbo, offset * sizeof(SubdivPosNorLoop), sizeof(SubdivPosNorLoop) * 2, &edge_data);

    offset += 2;
  }

  SubdivPosNorLoop vert_data;
  memset(&vert_data, 0, sizeof(SubdivPosNorLoop));
  Span<DRWSubdivLooseVertex> loose_verts = draw_subdiv_cache_get_loose_verts(subdiv_cache);

  for (const DRWSubdivLooseVertex &loose_vert : loose_verts) {
    copy_v3_v3(vert_data.pos, loose_vert.co);

    GPU_vertbuf_update_sub(
        vbo, offset * sizeof(SubdivPosNorLoop), sizeof(SubdivPosNorLoop), &vert_data);

    offset += 1;
  }
}

constexpr MeshExtract create_extractor_pos()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_pos_init;
  extractor.iter_face_bm = extract_pos_iter_face_bm;
  extractor.iter_loose_edge_bm = extract_pos_iter_loose_edge_bm;
  extractor.iter_loose_vert_bm = extract_pos_iter_loose_vert_bm;
  extractor.init_subdiv = extract_pos_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_pos_loose_geom_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(float3 *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.pos);
  return extractor;
}

/** \} */

const MeshExtract extract_pos = create_extractor_pos();

}  // namespace blender::draw
