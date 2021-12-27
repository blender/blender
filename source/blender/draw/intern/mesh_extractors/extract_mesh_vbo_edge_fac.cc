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

#include "GPU_capabilities.h"

#include "draw_subdivision.h"
#include "extract_mesh.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edge Factor
 * Defines how much an edge is visible.
 * \{ */

struct MeshExtract_EdgeFac_Data {
  uchar *vbo_data;
  bool use_edge_render;
  /* Number of loop per edge. */
  uchar *edge_loop_count;
};

static float loop_edge_factor_get(const float f_no[3],
                                  const float v_co[3],
                                  const float v_no[3],
                                  const float v_next_co[3])
{
  float enor[3], evec[3];
  sub_v3_v3v3(evec, v_next_co, v_co);
  cross_v3_v3v3(enor, v_no, evec);
  normalize_v3(enor);
  float d = fabsf(dot_v3v3(enor, f_no));
  /* Re-scale to the slider range. */
  d *= (1.0f / 0.065f);
  CLAMP(d, 0.0f, 1.0f);
  return d;
}

static void extract_edge_fac_init(const MeshRenderData *mr,
                                  struct MeshBatchCache *UNUSED(cache),
                                  void *buf,
                                  void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(tls_data);

  if (mr->extract_type == MR_EXTRACT_MESH) {
    data->edge_loop_count = static_cast<uchar *>(
        MEM_callocN(sizeof(uint32_t) * mr->edge_len, __func__));

    /* HACK(fclem) Detecting the need for edge render.
     * We could have a flag in the mesh instead or check the modifier stack. */
    const MEdge *med = mr->medge;
    for (int e_index = 0; e_index < mr->edge_len; e_index++, med++) {
      if ((med->flag & ME_EDGERENDER) == 0) {
        data->use_edge_render = true;
        break;
      }
    }
  }
  else {
    /* HACK to bypass non-manifold check in mesh_edge_fac_finish(). */
    data->use_edge_render = true;
  }

  data->vbo_data = static_cast<uchar *>(GPU_vertbuf_get_data(vbo));
}

static void extract_edge_fac_iter_poly_bm(const MeshRenderData *mr,
                                          const BMFace *f,
                                          const int UNUSED(f_index),
                                          void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    if (BM_edge_is_manifold(l_iter->e)) {
      float ratio = loop_edge_factor_get(bm_face_no_get(mr, f),
                                         bm_vert_co_get(mr, l_iter->v),
                                         bm_vert_no_get(mr, l_iter->v),
                                         bm_vert_co_get(mr, l_iter->next->v));
      data->vbo_data[l_index] = ratio * 253 + 1;
    }
    else {
      data->vbo_data[l_index] = 255;
    }
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edge_fac_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int mp_index,
                                            void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);

  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];

    if (data->use_edge_render) {
      const MEdge *med = &mr->medge[ml->e];
      data->vbo_data[ml_index] = (med->flag & ME_EDGERENDER) ? 255 : 0;
    }
    else {

      /* Count loop per edge to detect non-manifold. */
      if (data->edge_loop_count[ml->e] < 3) {
        data->edge_loop_count[ml->e]++;
      }
      if (data->edge_loop_count[ml->e] == 2) {
        /* Manifold */
        const int ml_index_last = mp->totloop + mp->loopstart - 1;
        const int ml_index_other = (ml_index == ml_index_last) ? mp->loopstart : (ml_index + 1);
        const MLoop *ml_next = &mr->mloop[ml_index_other];
        const MVert *v1 = &mr->mvert[ml->v];
        const MVert *v2 = &mr->mvert[ml_next->v];
        float vnor_f[3];
        normal_short_to_float_v3(vnor_f, v1->no);
        float ratio = loop_edge_factor_get(mr->poly_normals[mp_index], v1->co, vnor_f, v2->co);
        data->vbo_data[ml_index] = ratio * 253 + 1;
      }
      else {
        /* Non-manifold */
        data->vbo_data[ml_index] = 255;
      }
    }
  }
}

static void extract_edge_fac_iter_ledge_bm(const MeshRenderData *mr,
                                           const BMEdge *UNUSED(eed),
                                           const int ledge_index,
                                           void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);
  data->vbo_data[mr->loop_len + (ledge_index * 2) + 0] = 255;
  data->vbo_data[mr->loop_len + (ledge_index * 2) + 1] = 255;
}

static void extract_edge_fac_iter_ledge_mesh(const MeshRenderData *mr,
                                             const MEdge *UNUSED(med),
                                             const int ledge_index,
                                             void *_data)
{
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);

  data->vbo_data[mr->loop_len + ledge_index * 2 + 0] = 255;
  data->vbo_data[mr->loop_len + ledge_index * 2 + 1] = 255;
}

static void extract_edge_fac_finish(const MeshRenderData *mr,
                                    struct MeshBatchCache *UNUSED(cache),
                                    void *buf,
                                    void *_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  MeshExtract_EdgeFac_Data *data = static_cast<MeshExtract_EdgeFac_Data *>(_data);

  if (GPU_crappy_amd_driver()) {
    /* Some AMD drivers strangely crash with VBO's with a one byte format.
     * To workaround we reinitialize the VBO with another format and convert
     * all bytes to floats. */
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    /* We keep the data reference in data->vbo_data. */
    data->vbo_data = static_cast<uchar *>(GPU_vertbuf_steal_data(vbo));
    GPU_vertbuf_clear(vbo);

    int buf_len = mr->loop_len + mr->loop_loose_len;
    GPU_vertbuf_init_with_format(vbo, &format);
    GPU_vertbuf_data_alloc(vbo, buf_len);

    float *fdata = (float *)GPU_vertbuf_get_data(vbo);
    for (int ml_index = 0; ml_index < buf_len; ml_index++, fdata++) {
      *fdata = data->vbo_data[ml_index] / 255.0f;
    }
    /* Free old byte data. */
    MEM_freeN(data->vbo_data);
  }
  MEM_SAFE_FREE(data->edge_loop_count);
}

/* Different function than the one used for the non-subdivision case, as we directly take care of
 * the buggy AMD driver case. */
static GPUVertFormat *get_subdiv_edge_fac_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    if (GPU_crappy_amd_driver()) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    else {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
  }
  return &format;
}

static void extract_edge_fac_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                         const MeshRenderData *mr,
                                         struct MeshBatchCache *cache,
                                         void *buffer,
                                         void *UNUSED(data))
{
  GPUVertBuf *edge_idx = cache->final.buff.vbo.edge_idx;
  GPUVertBuf *pos_nor = cache->final.buff.vbo.pos_nor;
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  GPU_vertbuf_init_build_on_device(
      vbo, get_subdiv_edge_fac_format(), subdiv_cache->num_subdiv_loops + mr->loop_loose_len);

  /* Create a temporary buffer for the edge original indices if it was not requested. */
  const bool has_edge_idx = edge_idx != nullptr;
  GPUVertBuf *loop_edge_idx = nullptr;
  if (has_edge_idx) {
    loop_edge_idx = edge_idx;
  }
  else {
    loop_edge_idx = GPU_vertbuf_calloc();
    draw_subdiv_init_origindex_buffer(
        loop_edge_idx,
        static_cast<int *>(GPU_vertbuf_get_data(subdiv_cache->edges_orig_index)),
        subdiv_cache->num_subdiv_loops,
        0);
  }

  draw_subdiv_build_edge_fac_buffer(subdiv_cache, pos_nor, loop_edge_idx, vbo);

  if (!has_edge_idx) {
    GPU_vertbuf_discard(loop_edge_idx);
  }
}

static void extract_edge_fac_loose_geom_subdiv(const DRWSubdivCache *subdiv_cache,
                                               const MeshRenderData *UNUSED(mr),
                                               const MeshExtractLooseGeom *loose_geom,
                                               void *buffer,
                                               void *UNUSED(data))
{
  if (loose_geom->edge_len == 0) {
    return;
  }

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);

  /* Make sure buffer is active for sending loose data. */
  GPU_vertbuf_use(vbo);

  uint offset = subdiv_cache->num_subdiv_loops;
  for (int i = 0; i < loose_geom->edge_len; i++) {
    if (GPU_crappy_amd_driver()) {
      float loose_edge_fac[2] = {1.0f, 1.0f};
      GPU_vertbuf_update_sub(vbo, offset * sizeof(float), sizeof(loose_edge_fac), loose_edge_fac);
    }
    else {
      char loose_edge_fac[2] = {255, 255};
      GPU_vertbuf_update_sub(vbo, offset * sizeof(char), sizeof(loose_edge_fac), loose_edge_fac);
    }

    offset += 2;
  }
}

constexpr MeshExtract create_extractor_edge_fac()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_edge_fac_init;
  extractor.iter_poly_bm = extract_edge_fac_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edge_fac_iter_poly_mesh;
  extractor.iter_ledge_bm = extract_edge_fac_iter_ledge_bm;
  extractor.iter_ledge_mesh = extract_edge_fac_iter_ledge_mesh;
  extractor.init_subdiv = extract_edge_fac_init_subdiv;
  extractor.iter_loose_geom_subdiv = extract_edge_fac_loose_geom_subdiv;
  extractor.finish = extract_edge_fac_finish;
  extractor.data_type = MR_DATA_POLY_NOR;
  extractor.data_size = sizeof(MeshExtract_EdgeFac_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edge_fac);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_edge_fac = blender::draw::create_extractor_edge_fac();
}
