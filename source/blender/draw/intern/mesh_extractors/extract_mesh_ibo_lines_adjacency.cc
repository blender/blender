/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_map.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "GPU_index_buffer.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Line Adjacency Indices
 * \{ */

#define NO_EDGE INT_MAX

struct MeshExtract_LineAdjacency_Data {
  Span<int> corner_tri_faces;
  GPUIndexBufBuilder elb;
  Map<OrderedEdge, int> *eh;
  bool is_manifold;
  /* Array to convert vert index to any loop index of this vert. */
  uint *vert_to_loop;
};

static void line_adjacency_data_init(MeshExtract_LineAdjacency_Data *data,
                                     uint vert_len,
                                     uint loop_len,
                                     uint tess_edge_len)
{
  data->vert_to_loop = static_cast<uint *>(MEM_callocN(sizeof(uint) * vert_len, __func__));

  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES_ADJ, tess_edge_len, loop_len);
  data->eh = new Map<OrderedEdge, int>();
  data->eh->reserve(tess_edge_len);
  data->is_manifold = true;
}

static void extract_lines_adjacency_init(const MeshRenderData &mr,
                                         MeshBatchCache & /*cache*/,
                                         void * /*buf*/,
                                         void *tls_data)
{
  /* Similar to poly_to_tri_count().
   * There is always (loop + triangle - 1) edges inside a face.
   * Accumulate for all faces and you get : */
  uint tess_edge_len = mr.corners_num + mr.corner_tris_num - mr.faces_num;

  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(tls_data);
  data->corner_tri_faces = mr.mesh->corner_tri_faces();
  line_adjacency_data_init(data, mr.verts_num, mr.corners_num, tess_edge_len);
}

BLI_INLINE void lines_adjacency_triangle(
    uint v1, uint v2, uint v3, uint l1, uint l2, uint l3, MeshExtract_LineAdjacency_Data *data)
{
  GPUIndexBufBuilder *elb = &data->elb;
  /* Iterate around the triangle's edges. */
  for (int e = 0; e < 3; e++) {
    SHIFT3(uint, v3, v2, v1);
    SHIFT3(uint, l3, l2, l1);

    bool inv_indices = (v2 > v3);
    data->eh->add_or_modify(
        {v2, v3},
        [&](int *value) {
          int new_value = int(l1) + 1; /* 0 cannot be signed so add one. */
          *value = inv_indices ? -new_value : new_value;
          /* Store loop indices for remaining non-manifold edges. */
          data->vert_to_loop[v2] = l2;
          data->vert_to_loop[v3] = l3;
        },
        [&](int *value) {
          int v_data = *value;
          if (v_data == NO_EDGE) {
            int new_value = int(l1) + 1; /* 0 cannot be signed so add one. */
            *value = inv_indices ? -new_value : new_value;
            /* Store loop indices for remaining non-manifold edges. */
            data->vert_to_loop[v2] = l2;
            data->vert_to_loop[v3] = l3;
          }
          else {
            /* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
            *value = NO_EDGE;
            bool inv_opposite = (v_data < 0);
            uint l_opposite = uint(abs(v_data)) - 1;
            /* TODO: Make this part thread-safe. */
            if (inv_opposite == inv_indices) {
              /* Don't share edge if triangles have non matching winding. */
              GPU_indexbuf_add_line_adj_verts(elb, l1, l2, l3, l1);
              GPU_indexbuf_add_line_adj_verts(elb, l_opposite, l2, l3, l_opposite);
              data->is_manifold = false;
            }
            else {
              GPU_indexbuf_add_line_adj_verts(elb, l1, l2, l3, l_opposite);
            }
          }
        });
  }
}

static void extract_lines_adjacency_iter_looptri_bm(const MeshRenderData & /*mr*/,
                                                    BMLoop **elt,
                                                    const int /*elt_index*/,
                                                    void *_data)
{
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);
  if (!BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN)) {
    lines_adjacency_triangle(BM_elem_index_get(elt[0]->v),
                             BM_elem_index_get(elt[1]->v),
                             BM_elem_index_get(elt[2]->v),
                             BM_elem_index_get(elt[0]),
                             BM_elem_index_get(elt[1]),
                             BM_elem_index_get(elt[2]),
                             data);
  }
}

static void extract_lines_adjacency_iter_corner_tri_mesh(const MeshRenderData &mr,
                                                         const int3 &tri,
                                                         const int elt_index,
                                                         void *_data)
{
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);
  const int face_i = data->corner_tri_faces[elt_index];
  const bool hidden = mr.use_hide && !mr.hide_poly.is_empty() && mr.hide_poly[face_i];
  if (hidden) {
    return;
  }
  lines_adjacency_triangle(mr.corner_verts[tri[0]],
                           mr.corner_verts[tri[1]],
                           mr.corner_verts[tri[2]],
                           tri[0],
                           tri[1],
                           tri[2],
                           data);
}

static void extract_lines_adjacency_finish(const MeshRenderData & /*mr*/,
                                           MeshBatchCache &cache,
                                           void *buf,
                                           void *_data)
{
  gpu::IndexBuf *ibo = static_cast<gpu::IndexBuf *>(buf);
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);
  /* Create edges for remaining non manifold edges. */
  for (const auto item : data->eh->items()) {
    int v_data = item.value;
    if (v_data == NO_EDGE) {
      continue;
    }

    int v2 = item.key.v_low;
    int v3 = item.key.v_high;

    int l1 = uint(abs(v_data)) - 1;
    if (v_data < 0) { /* `inv_opposite`. */
      std::swap(v2, v3);
    }
    int l2 = data->vert_to_loop[v2];
    int l3 = data->vert_to_loop[v3];
    GPU_indexbuf_add_line_adj_verts(&data->elb, l1, l2, l3, l1);
    data->is_manifold = false;
  }
  delete data->eh;

  cache.is_manifold = data->is_manifold;

  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data->vert_to_loop);
}

static void extract_lines_adjacency_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                                const MeshRenderData & /*mr*/,
                                                MeshBatchCache & /*cache*/,
                                                void * /*buf*/,
                                                void *_data)
{
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);

  /* For each face there is (loop + triangle - 1) edges. Since we only have quads, and a quad
   * is split into 2 triangles, we have (loop + 2 - 1) = (loop + 1) edges for each quad, or in
   * total: (number_of_loops + number_of_quads). */
  const uint tess_len = subdiv_cache.num_subdiv_loops + subdiv_cache.num_subdiv_quads;
  line_adjacency_data_init(
      data, subdiv_cache.num_subdiv_verts, subdiv_cache.num_subdiv_loops, tess_len);
}

static void extract_lines_adjacency_iter_subdiv(const DRWSubdivCache &subdiv_cache,
                                                const MeshRenderData & /*mr*/,
                                                void *_data,
                                                uint subdiv_quad_index)
{
  MeshExtract_LineAdjacency_Data *data = static_cast<MeshExtract_LineAdjacency_Data *>(_data);

  const uint loop_index = subdiv_quad_index * 4;
  const uint l0 = loop_index + 0;
  const uint l1 = loop_index + 1;
  const uint l2 = loop_index + 2;
  const uint l3 = loop_index + 3;

  const uint v0 = subdiv_cache.subdiv_loop_subdiv_vert_index[l0];
  const uint v1 = subdiv_cache.subdiv_loop_subdiv_vert_index[l1];
  const uint v2 = subdiv_cache.subdiv_loop_subdiv_vert_index[l2];
  const uint v3 = subdiv_cache.subdiv_loop_subdiv_vert_index[l3];

  lines_adjacency_triangle(v0, v1, v2, l0, l1, l2, data);
  lines_adjacency_triangle(v0, v2, v3, l0, l2, l3, data);
}

static void extract_lines_adjacency_iter_subdiv_bm(const DRWSubdivCache &subdiv_cache,
                                                   const MeshRenderData &mr,
                                                   void *_data,
                                                   uint subdiv_quad_index,
                                                   const BMFace * /*coarse_quad*/)
{
  extract_lines_adjacency_iter_subdiv(subdiv_cache, mr, _data, subdiv_quad_index);
}

static void extract_lines_adjacency_iter_subdiv_mesh(const DRWSubdivCache &subdiv_cache,
                                                     const MeshRenderData &mr,
                                                     void *_data,
                                                     uint subdiv_quad_index,
                                                     const int /*coarse_quad_index*/)
{
  extract_lines_adjacency_iter_subdiv(subdiv_cache, mr, _data, subdiv_quad_index);
}

static void extract_lines_adjacency_finish_subdiv(const DRWSubdivCache & /*subdiv_cache*/,
                                                  const MeshRenderData &mr,
                                                  MeshBatchCache &cache,
                                                  void *buf,
                                                  void *_data)
{
  extract_lines_adjacency_finish(mr, cache, buf, _data);
}

#undef NO_EDGE

constexpr MeshExtract create_extractor_lines_adjacency()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lines_adjacency_init;
  extractor.iter_looptri_bm = extract_lines_adjacency_iter_looptri_bm;
  extractor.iter_corner_tri_mesh = extract_lines_adjacency_iter_corner_tri_mesh;
  extractor.finish = extract_lines_adjacency_finish;
  extractor.init_subdiv = extract_lines_adjacency_init_subdiv;
  extractor.iter_subdiv_bm = extract_lines_adjacency_iter_subdiv_bm;
  extractor.iter_subdiv_mesh = extract_lines_adjacency_iter_subdiv_mesh;
  extractor.finish_subdiv = extract_lines_adjacency_finish_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_LineAdjacency_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, ibo.lines_adjacency);
  return extractor;
}

/** \} */

const MeshExtract extract_lines_adjacency = create_extractor_lines_adjacency();

}  // namespace blender::draw
