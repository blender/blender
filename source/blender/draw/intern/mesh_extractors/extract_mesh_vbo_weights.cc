/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_mesh.hh"

#include "draw_subdivision.h"
#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Vertex Weight
 * \{ */

struct MeshExtract_Weight_Data {
  float *vbo_data;
  const DRW_MeshWeightState *wstate;
  const MDeformVert *dvert; /* For #Mesh. */
  int cd_ofs;               /* For #BMesh. */
};

static float evaluate_vertex_weight(const MDeformVert *dvert, const DRW_MeshWeightState *wstate)
{
  /* Error state. */
  if ((wstate->defgroup_active < 0) && (wstate->defgroup_len > 0)) {
    return -2.0f;
  }
  if (dvert == nullptr) {
    return (wstate->alert_mode != OB_DRAW_GROUPUSER_NONE) ? -1.0f : 0.0f;
  }

  float input = 0.0f;
  if (wstate->flags & DRW_MESH_WEIGHT_STATE_MULTIPAINT) {
    /* Multi-Paint feature */
    bool is_normalized = (wstate->flags & (DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE |
                                           DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE));
    input = BKE_defvert_multipaint_collective_weight(dvert,
                                                     wstate->defgroup_len,
                                                     wstate->defgroup_sel,
                                                     wstate->defgroup_sel_count,
                                                     is_normalized);
    /* make it black if the selected groups have no weight on a vertex */
    if (input == 0.0f) {
      return -1.0f;
    }
  }
  else {
    /* default, non tricky behavior */
    input = BKE_defvert_find_weight(dvert, wstate->defgroup_active);

    if (input == 0.0f) {
      switch (wstate->alert_mode) {
        case OB_DRAW_GROUPUSER_ACTIVE:
          return -1.0f;
          break;
        case OB_DRAW_GROUPUSER_ALL:
          if (BKE_defvert_is_weight_zero(dvert, wstate->defgroup_len)) {
            return -1.0f;
          }
          break;
      }
    }
  }

  /* Lock-Relative: display the fraction of current weight vs total unlocked weight. */
  if (wstate->flags & DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE) {
    input = BKE_defvert_lock_relative_weight(
        input, dvert, wstate->defgroup_len, wstate->defgroup_locked, wstate->defgroup_unlocked);
  }

  CLAMP(input, 0.0f, 1.0f);
  return input;
}

static void extract_weights_init(const MeshRenderData *mr,
                                 MeshBatchCache *cache,
                                 void *buf,
                                 void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_Weight_Data *data = static_cast<MeshExtract_Weight_Data *>(tls_data);
  data->vbo_data = (float *)GPU_vertbuf_get_data(vbo);
  data->wstate = &cache->weight_state;

  if (data->wstate->defgroup_active == -1) {
    /* Nothing to show. */
    data->dvert = nullptr;
    data->cd_ofs = -1;
  }
  else if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->dvert = nullptr;
    data->cd_ofs = CustomData_get_offset(&mr->bm->vdata, CD_MDEFORMVERT);
  }
  else {
    data->dvert = mr->me->deform_verts().data();
    data->cd_ofs = -1;
  }
}

static void extract_weights_iter_face_bm(const MeshRenderData * /*mr*/,
                                         const BMFace *f,
                                         const int /*f_index*/,
                                         void *_data)
{
  MeshExtract_Weight_Data *data = static_cast<MeshExtract_Weight_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (data->cd_ofs != -1) {
      const MDeformVert *dvert = (const MDeformVert *)BM_ELEM_CD_GET_VOID_P(l_iter->v,
                                                                            data->cd_ofs);
      data->vbo_data[l_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
    else {
      data->vbo_data[l_index] = evaluate_vertex_weight(nullptr, data->wstate);
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_weights_iter_face_mesh(const MeshRenderData *mr,
                                           const int face_index,
                                           void *_data)
{
  MeshExtract_Weight_Data *data = static_cast<MeshExtract_Weight_Data *>(_data);
  for (const int ml_index : mr->faces[face_index]) {
    const int vert = mr->corner_verts[ml_index];
    if (data->dvert != nullptr) {
      const MDeformVert *dvert = &data->dvert[vert];
      data->vbo_data[ml_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
    else {
      const MDeformVert *dvert = nullptr;
      data->vbo_data[ml_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
  }
}

static void extract_weights_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                        const MeshRenderData *mr,
                                        MeshBatchCache *cache,
                                        void *buffer,
                                        void *_data)
{
  Mesh *coarse_mesh = subdiv_cache->mesh;
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  GPU_vertbuf_init_build_on_device(vbo, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *coarse_weights = GPU_vertbuf_calloc();
  extract_weights_init(mr, cache, coarse_weights, _data);

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    const OffsetIndices coarse_faces = coarse_mesh->faces();
    for (const int i : coarse_faces.index_range()) {
      extract_weights_iter_face_mesh(mr, i, _data);
    }
  }
  else {
    BMIter f_iter;
    BMFace *efa;
    int face_index = 0;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, face_index) {
      extract_weights_iter_face_bm(mr, efa, face_index, _data);
    }
  }

  draw_subdiv_interp_custom_data(subdiv_cache, coarse_weights, vbo, GPU_COMP_F32, 1, 0);

  GPU_vertbuf_discard(coarse_weights);
}

constexpr MeshExtract create_extractor_weights()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_weights_init;
  extractor.init_subdiv = extract_weights_init_subdiv;
  extractor.iter_face_bm = extract_weights_iter_face_bm;
  extractor.iter_face_mesh = extract_weights_iter_face_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_Weight_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.weights);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_weights = blender::draw::create_extractor_weights();
