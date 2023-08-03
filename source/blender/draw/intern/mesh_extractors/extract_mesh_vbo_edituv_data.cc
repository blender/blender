/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_mesh.hh"

#include "extract_mesh.hh"

#include "draw_cache_impl.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Data / Flags
 * \{ */

struct MeshExtract_EditUVData_Data {
  EditLoopData *vbo_data;
  BMUVOffsets offsets;
};

static void extract_edituv_data_init_common(const MeshRenderData &mr,
                                            GPUVertBuf *vbo,
                                            MeshExtract_EditUVData_Data *data,
                                            uint loop_len)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING: Adjust #EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, loop_len);

  data->vbo_data = (EditLoopData *)GPU_vertbuf_get_data(vbo);
  data->offsets = BM_uv_map_get_offsets(mr.bm);
}

static void extract_edituv_data_init(const MeshRenderData &mr,
                                     MeshBatchCache & /*cache*/,
                                     void *buf,
                                     void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  MeshExtract_EditUVData_Data *data = static_cast<MeshExtract_EditUVData_Data *>(tls_data);
  extract_edituv_data_init_common(mr, vbo, data, mr.loop_len);
}

static void extract_edituv_data_iter_face_bm(const MeshRenderData &mr,
                                             const BMFace *f,
                                             const int /*f_index*/,
                                             void *_data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    MeshExtract_EditUVData_Data *data = static_cast<MeshExtract_EditUVData_Data *>(_data);
    EditLoopData *eldata = &data->vbo_data[l_index];
    memset(eldata, 0x0, sizeof(*eldata));
    mesh_render_data_loop_flag(mr, l_iter, data->offsets, eldata);
    mesh_render_data_face_flag(mr, f, data->offsets, eldata);
    mesh_render_data_loop_edge_flag(mr, l_iter, data->offsets, eldata);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edituv_data_iter_face_mesh(const MeshRenderData &mr,
                                               const int face_index,
                                               void *_data)
{
  MeshExtract_EditUVData_Data *data = static_cast<MeshExtract_EditUVData_Data *>(_data);
  const IndexRange face = mr.faces[face_index];
  const int ml_index_end = face.start() + face.size();
  for (int ml_index = face.start(); ml_index < ml_index_end; ml_index += 1) {
    EditLoopData *eldata = &data->vbo_data[ml_index];
    memset(eldata, 0x0, sizeof(*eldata));
    BMFace *efa = bm_original_face_get(mr, face_index);
    if (efa) {
      BMVert *eve = bm_original_vert_get(mr, mr.corner_verts[ml_index]);
      BMEdge *eed = bm_original_edge_get(mr, mr.corner_edges[ml_index]);
      if (eed && eve) {
        /* Loop on an edge endpoint. */
        BMLoop *l = BM_face_edge_share_loop(efa, eed);
        mesh_render_data_loop_flag(mr, l, data->offsets, eldata);
        mesh_render_data_loop_edge_flag(mr, l, data->offsets, eldata);
      }
      else {
        if (eed == nullptr) {
          /* Find if the loop's vert is not part of an edit edge.
           * For this, we check if the previous loop was on an edge. */
          const int l_prev = bke::mesh::face_corner_prev(face, ml_index);
          eed = bm_original_edge_get(mr, mr.corner_edges[l_prev]);
        }
        if (eed) {
          /* Mapped points on an edge between two edit verts. */
          BMLoop *l = BM_face_edge_share_loop(efa, eed);
          mesh_render_data_loop_edge_flag(mr, l, data->offsets, eldata);
        }
      }
    }
  }
}

static void extract_edituv_data_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                            const MeshRenderData &mr,
                                            MeshBatchCache & /*cache*/,
                                            void *buf,
                                            void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  MeshExtract_EditUVData_Data *data = static_cast<MeshExtract_EditUVData_Data *>(tls_data);
  extract_edituv_data_init_common(mr, vbo, data, subdiv_cache.num_subdiv_loops);
}

static void extract_edituv_data_iter_subdiv_bm(const DRWSubdivCache &subdiv_cache,
                                               const MeshRenderData &mr,
                                               void *_data,
                                               uint subdiv_quad_index,
                                               const BMFace *coarse_quad)
{
  MeshExtract_EditUVData_Data *data = static_cast<MeshExtract_EditUVData_Data *>(_data);
  int *subdiv_loop_vert_index = (int *)GPU_vertbuf_get_data(subdiv_cache.verts_orig_index);
  int *subdiv_loop_edge_index = (int *)GPU_vertbuf_get_data(subdiv_cache.edges_orig_index);

  uint start_loop_idx = subdiv_quad_index * 4;
  uint end_loop_idx = (subdiv_quad_index + 1) * 4;
  for (uint i = start_loop_idx; i < end_loop_idx; i++) {
    const int vert_origindex = subdiv_loop_vert_index[i];
    int edge_origindex = subdiv_loop_edge_index[i];

    EditLoopData *edit_loop_data = &data->vbo_data[i];
    memset(edit_loop_data, 0, sizeof(EditLoopData));

    if (vert_origindex != -1 && edge_origindex != -1) {
      BMEdge *eed = BM_edge_at_index(mr.bm, edge_origindex);
      /* Loop on an edge endpoint. */
      BMLoop *l = BM_face_edge_share_loop(const_cast<BMFace *>(coarse_quad), eed);
      mesh_render_data_loop_flag(mr, l, data->offsets, edit_loop_data);
      mesh_render_data_loop_edge_flag(mr, l, data->offsets, edit_loop_data);
    }
    else {
      if (edge_origindex == -1) {
        /* Find if the loop's vert is not part of an edit edge.
         * For this, we check if the previous loop was on an edge. */
        const uint loop_index_last = (i == start_loop_idx) ? end_loop_idx - 1 : i - 1;
        edge_origindex = subdiv_loop_edge_index[loop_index_last];
      }
      if (edge_origindex != -1) {
        /* Mapped points on an edge between two edit verts. */
        BMEdge *eed = BM_edge_at_index(mr.bm, edge_origindex);
        BMLoop *l = BM_face_edge_share_loop(const_cast<BMFace *>(coarse_quad), eed);
        mesh_render_data_loop_edge_flag(mr, l, data->offsets, edit_loop_data);
      }
    }

    mesh_render_data_face_flag(mr, coarse_quad, data->offsets, edit_loop_data);
  }
}

static void extract_edituv_data_iter_subdiv_mesh(const DRWSubdivCache &subdiv_cache,
                                                 const MeshRenderData &mr,
                                                 void *_data,
                                                 uint subdiv_quad_index,
                                                 const int coarse_quad_index)
{
  BMFace *coarse_quad_bm = bm_original_face_get(mr, coarse_quad_index);
  extract_edituv_data_iter_subdiv_bm(subdiv_cache, mr, _data, subdiv_quad_index, coarse_quad_bm);
}

constexpr MeshExtract create_extractor_edituv_data()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_edituv_data_init;
  extractor.iter_face_bm = extract_edituv_data_iter_face_bm;
  extractor.iter_face_mesh = extract_edituv_data_iter_face_mesh;
  extractor.init_subdiv = extract_edituv_data_init_subdiv;
  extractor.iter_subdiv_bm = extract_edituv_data_iter_subdiv_bm;
  extractor.iter_subdiv_mesh = extract_edituv_data_iter_subdiv_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_EditUVData_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edituv_data);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_edituv_data = blender::draw::create_extractor_edituv_data();
