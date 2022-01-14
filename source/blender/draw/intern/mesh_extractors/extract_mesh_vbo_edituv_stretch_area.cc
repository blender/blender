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

#include "BKE_mesh.h"

#include "extract_mesh.h"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV area stretch
 * \{ */

static void extract_edituv_stretch_area_init(const MeshRenderData *mr,
                                             struct MeshBatchCache *UNUSED(cache),
                                             void *buf,
                                             void *UNUSED(tls_data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "ratio", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);
}

BLI_INLINE float area_ratio_get(float area, float uvarea)
{
  if (area >= FLT_EPSILON && uvarea >= FLT_EPSILON) {
    /* Tag inversion by using the sign. */
    return (area > uvarea) ? (uvarea / area) : -(area / uvarea);
  }
  return 0.0f;
}

BLI_INLINE float area_ratio_to_stretch(float ratio, float tot_ratio, float inv_tot_ratio)
{
  ratio *= (ratio > 0.0f) ? tot_ratio : -inv_tot_ratio;
  return (ratio > 1.0f) ? (1.0f / ratio) : ratio;
}

static void compute_area_ratio(const MeshRenderData *mr,
                               float *r_area_ratio,
                               float &r_tot_area,
                               float &r_tot_uv_area)
{
  float tot_area = 0.0f, tot_uv_area = 0.0f;

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    CustomData *cd_ldata = &mr->bm->ldata;
    int uv_ofs = CustomData_get_offset(cd_ldata, CD_MLOOPUV);

    BMFace *efa;
    BMIter f_iter;
    int f;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      float area = BM_face_calc_area(efa);
      float uvarea = BM_face_calc_area_uv(efa, uv_ofs);
      tot_area += area;
      tot_uv_area += uvarea;
      r_area_ratio[f] = area_ratio_get(area, uvarea);
    }
  }
  else {
    BLI_assert(ELEM(mr->extract_type, MR_EXTRACT_MAPPED, MR_EXTRACT_MESH));
    const MLoopUV *uv_data = (const MLoopUV *)CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      float area = BKE_mesh_calc_poly_area(mp, &mr->mloop[mp->loopstart], mr->mvert);
      float uvarea = BKE_mesh_calc_poly_uv_area(mp, uv_data);
      tot_area += area;
      tot_uv_area += uvarea;
      r_area_ratio[mp_index] = area_ratio_get(area, uvarea);
    }
  }

  r_tot_area = tot_area;
  r_tot_uv_area = tot_uv_area;
}

static void extract_edituv_stretch_area_finish(const MeshRenderData *mr,
                                               struct MeshBatchCache *cache,
                                               void *buf,
                                               void *UNUSED(data))
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  float *area_ratio = static_cast<float *>(MEM_mallocN(sizeof(float) * mr->poly_len, __func__));
  compute_area_ratio(mr, area_ratio, cache->tot_area, cache->tot_uv_area);

  /* Convert in place to avoid an extra allocation */
  uint16_t *poly_stretch = (uint16_t *)area_ratio;
  for (int mp_index = 0; mp_index < mr->poly_len; mp_index++) {
    poly_stretch[mp_index] = area_ratio[mp_index] * SHRT_MAX;
  }

  /* Copy face data for each loop. */
  uint16_t *loop_stretch = (uint16_t *)GPU_vertbuf_get_data(vbo);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMFace *efa;
    BMIter f_iter;
    int f, l_index = 0;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      for (int i = 0; i < efa->len; i++, l_index++) {
        loop_stretch[l_index] = poly_stretch[f];
      }
    }
  }
  else {
    BLI_assert(ELEM(mr->extract_type, MR_EXTRACT_MAPPED, MR_EXTRACT_MESH));
    const MPoly *mp = mr->mpoly;
    for (int mp_index = 0, l_index = 0; mp_index < mr->poly_len; mp_index++, mp++) {
      for (int i = 0; i < mp->totloop; i++, l_index++) {
        loop_stretch[l_index] = poly_stretch[mp_index];
      }
    }
  }

  MEM_freeN(area_ratio);
}

static void extract_edituv_stretch_area_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                                    const MeshRenderData *mr,
                                                    struct MeshBatchCache *cache,
                                                    void *buffer,
                                                    void *UNUSED(data))
{

  /* Initialize final buffer. */
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buffer);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "ratio", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_build_on_device(vbo, &format, subdiv_cache->num_subdiv_loops);

  /* Initialize coarse data buffer. */

  GPUVertBuf *coarse_data = GPU_vertbuf_calloc();

  /* We use the same format as we just copy data around. */
  GPU_vertbuf_init_with_format(coarse_data, &format);
  GPU_vertbuf_data_alloc(coarse_data, mr->loop_len);

  compute_area_ratio(mr,
                     static_cast<float *>(GPU_vertbuf_get_data(coarse_data)),
                     cache->tot_area,
                     cache->tot_uv_area);

  draw_subdiv_build_edituv_stretch_area_buffer(subdiv_cache, coarse_data, vbo);

  GPU_vertbuf_discard(coarse_data);
}

constexpr MeshExtract create_extractor_edituv_stretch_area()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_edituv_stretch_area_init;
  extractor.finish = extract_edituv_stretch_area_finish;
  extractor.init_subdiv = extract_edituv_stretch_area_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edituv_stretch_area);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_edituv_stretch_area =
    blender::draw::create_extractor_edituv_stretch_area();
}
