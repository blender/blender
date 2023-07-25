/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_math_vector_types.hh"

#include "MEM_guardedalloc.h"

#include "BKE_mesh.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV area stretch
 * \{ */

static void extract_edituv_stretch_area_init(const MeshRenderData *mr,
                                             MeshBatchCache * /*cache*/,
                                             void *buf,
                                             void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "ratio", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);
}

BLI_INLINE float area_ratio_get(float area, float uvarea)
{
  if (area >= FLT_EPSILON && uvarea >= FLT_EPSILON) {
    return uvarea / area;
  }
  return 0.0f;
}

BLI_INLINE float area_ratio_to_stretch(float ratio, float tot_ratio)
{
  ratio *= tot_ratio;
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
    int uv_ofs = CustomData_get_offset(cd_ldata, CD_PROP_FLOAT2);

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
    BLI_assert(mr->extract_type == MR_EXTRACT_MESH);
    const float2 *uv_data = (const float2 *)CustomData_get_layer(&mr->me->loop_data,
                                                                 CD_PROP_FLOAT2);
    for (int face_index = 0; face_index < mr->face_len; face_index++) {
      const IndexRange face = mr->faces[face_index];
      const float area = bke::mesh::face_area_calc(mr->vert_positions,
                                                   mr->corner_verts.slice(face));
      float uvarea = area_poly_v2(reinterpret_cast<const float(*)[2]>(&uv_data[face.start()]),
                                  face.size());
      tot_area += area;
      tot_uv_area += uvarea;
      r_area_ratio[face_index] = area_ratio_get(area, uvarea);
    }
  }

  r_tot_area = tot_area;
  r_tot_uv_area = tot_uv_area;
}

static void extract_edituv_stretch_area_finish(const MeshRenderData *mr,
                                               MeshBatchCache *cache,
                                               void *buf,
                                               void * /*data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  float *area_ratio = static_cast<float *>(MEM_mallocN(sizeof(float) * mr->face_len, __func__));
  compute_area_ratio(mr, area_ratio, cache->tot_area, cache->tot_uv_area);

  /* Copy face data for each loop. */
  float *loop_stretch = (float *)GPU_vertbuf_get_data(vbo);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMFace *efa;
    BMIter f_iter;
    int f, l_index = 0;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      for (int i = 0; i < efa->len; i++, l_index++) {
        loop_stretch[l_index] = area_ratio[f];
      }
    }
  }
  else {
    BLI_assert(mr->extract_type == MR_EXTRACT_MESH);
    for (int face_index = 0; face_index < mr->face_len; face_index++) {
      for (const int l_index : mr->faces[face_index]) {
        loop_stretch[l_index] = area_ratio[face_index];
      }
    }
  }

  MEM_freeN(area_ratio);
}

static void extract_edituv_stretch_area_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                                    const MeshRenderData *mr,
                                                    MeshBatchCache *cache,
                                                    void *buffer,
                                                    void * /*data*/)
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

const MeshExtract extract_edituv_stretch_area =
    blender::draw::create_extractor_edituv_stretch_area();
