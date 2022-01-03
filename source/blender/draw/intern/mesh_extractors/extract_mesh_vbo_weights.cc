/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"

#include "draw_subdivision.h"
#include "extract_mesh.h"

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
                                 struct MeshBatchCache *cache,
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
    data->dvert = (const MDeformVert *)CustomData_get_layer(&mr->me->vdata, CD_MDEFORMVERT);
    data->cd_ofs = -1;
  }
}

static void extract_weights_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                         const BMFace *f,
                                         const int UNUSED(f_index),
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

static void extract_weights_iter_poly_mesh(const MeshRenderData *mr,
                                           const MPoly *mp,
                                           const int UNUSED(mp_index),
                                           void *_data)
{
  MeshExtract_Weight_Data *data = static_cast<MeshExtract_Weight_Data *>(_data);
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    if (data->dvert != nullptr) {
      const MDeformVert *dvert = &data->dvert[ml->v];
      data->vbo_data[ml_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
    else {
      const MDeformVert *dvert = nullptr;
      data->vbo_data[ml_index] = evaluate_vertex_weight(dvert, data->wstate);
    }
  }
}

static void extract_weights_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                        const MeshRenderData *UNUSED(mr),
                                        struct MeshBatchCache *cache,
                                        void *buffer,
                                        void *UNUSED(data))
{
  Mesh *coarse_mesh = subdiv_cache->mesh;
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  GPU_vertbuf_init_build_on_device(vbo, &format, subdiv_cache->num_subdiv_loops);

  GPUVertBuf *coarse_weights = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format(coarse_weights, &format);
  GPU_vertbuf_data_alloc(coarse_weights, coarse_mesh->totloop);
  float *coarse_weights_data = static_cast<float *>(GPU_vertbuf_get_data(coarse_weights));

  const DRW_MeshWeightState *wstate = &cache->weight_state;
  const MDeformVert *dverts = static_cast<const MDeformVert *>(
      CustomData_get_layer(&coarse_mesh->vdata, CD_MDEFORMVERT));

  for (int i = 0; i < coarse_mesh->totpoly; i++) {
    const MPoly *mpoly = &coarse_mesh->mpoly[i];

    for (int loop_index = mpoly->loopstart; loop_index < mpoly->loopstart + mpoly->totloop;
         loop_index++) {
      const MLoop *ml = &coarse_mesh->mloop[loop_index];

      if (dverts != nullptr) {
        const MDeformVert *dvert = &dverts[ml->v];
        coarse_weights_data[loop_index] = evaluate_vertex_weight(dvert, wstate);
      }
      else {
        coarse_weights_data[loop_index] = evaluate_vertex_weight(nullptr, wstate);
      }
    }
  }

  draw_subdiv_interp_custom_data(subdiv_cache, coarse_weights, vbo, 1, 0);

  GPU_vertbuf_discard(coarse_weights);
}

constexpr MeshExtract create_extractor_weights()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_weights_init;
  extractor.init_subdiv = extract_weights_init_subdiv;
  extractor.iter_poly_bm = extract_weights_iter_poly_bm;
  extractor.iter_poly_mesh = extract_weights_iter_poly_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_Weight_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.weights);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_weights = blender::draw::create_extractor_weights();
}
