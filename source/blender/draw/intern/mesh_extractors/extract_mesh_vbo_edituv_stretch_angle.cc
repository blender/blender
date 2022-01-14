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
/** \name Extract Edit UV angle stretch
 * \{ */

struct UVStretchAngle {
  int16_t angle;
  int16_t uv_angles[2];
};

struct MeshExtract_StretchAngle_Data {
  UVStretchAngle *vbo_data;
  MLoopUV *luv;
  float auv[2][2], last_auv[2];
  float av[2][3], last_av[3];
  int cd_ofs;
};

static void compute_normalize_edge_vectors(float auv[2][2],
                                           float av[2][3],
                                           const float uv[2],
                                           const float uv_prev[2],
                                           const float co[3],
                                           const float co_prev[3])
{
  /* Move previous edge. */
  copy_v2_v2(auv[0], auv[1]);
  copy_v3_v3(av[0], av[1]);
  /* 2d edge */
  sub_v2_v2v2(auv[1], uv_prev, uv);
  normalize_v2(auv[1]);
  /* 3d edge */
  sub_v3_v3v3(av[1], co_prev, co);
  normalize_v3(av[1]);
}

static short v2_to_short_angle(const float v[2])
{
  return atan2f(v[1], v[0]) * (float)M_1_PI * SHRT_MAX;
}

static void edituv_get_edituv_stretch_angle(float auv[2][2],
                                            const float av[2][3],
                                            UVStretchAngle *r_stretch)
{
  /* Send UV's to the shader and let it compute the aspect corrected angle. */
  r_stretch->uv_angles[0] = v2_to_short_angle(auv[0]);
  r_stretch->uv_angles[1] = v2_to_short_angle(auv[1]);
  /* Compute 3D angle here. */
  r_stretch->angle = angle_normalized_v3v3(av[0], av[1]) * (float)M_1_PI * SHRT_MAX;

#if 0 /* here for reference, this is done in shader now. */
  float uvang = angle_normalized_v2v2(auv0, auv1);
  float ang = angle_normalized_v3v3(av0, av1);
  float stretch = fabsf(uvang - ang) / (float)M_PI;
  return 1.0f - pow2f(1.0f - stretch);
#endif
}

static void extract_edituv_stretch_angle_init(const MeshRenderData *mr,
                                              struct MeshBatchCache *UNUSED(cache),
                                              void *buf,
                                              void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* Waning: adjust #UVStretchAngle struct accordingly. */
    GPU_vertformat_attr_add(&format, "angle", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_attr_add(&format, "uv_angles", GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  MeshExtract_StretchAngle_Data *data = static_cast<MeshExtract_StretchAngle_Data *>(tls_data);
  data->vbo_data = (UVStretchAngle *)GPU_vertbuf_get_data(vbo);

  /* Special iterator needed to save about half of the computing cost. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  }
  else {
    BLI_assert(ELEM(mr->extract_type, MR_EXTRACT_MAPPED, MR_EXTRACT_MESH));
    data->luv = (MLoopUV *)CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
  }
}

static void extract_edituv_stretch_angle_iter_poly_bm(const MeshRenderData *mr,
                                                      const BMFace *f,
                                                      const int UNUSED(f_index),
                                                      void *_data)
{
  MeshExtract_StretchAngle_Data *data = static_cast<MeshExtract_StretchAngle_Data *>(_data);
  float(*auv)[2] = data->auv, *last_auv = data->last_auv;
  float(*av)[3] = data->av, *last_av = data->last_av;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);

    const MLoopUV *luv, *luv_next;
    BMLoop *l_next = l_iter->next;
    if (l_iter == BM_FACE_FIRST_LOOP(f)) {
      /* First loop in face. */
      BMLoop *l_tmp = l_iter->prev;
      BMLoop *l_next_tmp = l_iter;
      luv = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_tmp, data->cd_ofs);
      luv_next = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_next_tmp, data->cd_ofs);
      compute_normalize_edge_vectors(auv,
                                     av,
                                     luv->uv,
                                     luv_next->uv,
                                     bm_vert_co_get(mr, l_tmp->v),
                                     bm_vert_co_get(mr, l_next_tmp->v));
      /* Save last edge. */
      copy_v2_v2(last_auv, auv[1]);
      copy_v3_v3(last_av, av[1]);
    }
    if (l_next == BM_FACE_FIRST_LOOP(f)) {
      /* Move previous edge. */
      copy_v2_v2(auv[0], auv[1]);
      copy_v3_v3(av[0], av[1]);
      /* Copy already calculated last edge. */
      copy_v2_v2(auv[1], last_auv);
      copy_v3_v3(av[1], last_av);
    }
    else {
      luv = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, data->cd_ofs);
      luv_next = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_next, data->cd_ofs);
      compute_normalize_edge_vectors(auv,
                                     av,
                                     luv->uv,
                                     luv_next->uv,
                                     bm_vert_co_get(mr, l_iter->v),
                                     bm_vert_co_get(mr, l_next->v));
    }
    edituv_get_edituv_stretch_angle(auv, av, &data->vbo_data[l_index]);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_edituv_stretch_angle_iter_poly_mesh(const MeshRenderData *mr,
                                                        const MPoly *mp,
                                                        const int UNUSED(mp_index),
                                                        void *_data)
{
  MeshExtract_StretchAngle_Data *data = static_cast<MeshExtract_StretchAngle_Data *>(_data);

  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    float(*auv)[2] = data->auv, *last_auv = data->last_auv;
    float(*av)[3] = data->av, *last_av = data->last_av;
    int l_next = ml_index + 1;
    const MVert *v, *v_next;
    if (ml_index == mp->loopstart) {
      /* First loop in face. */
      const int ml_index_last = ml_index_end - 1;
      const int l_next_tmp = mp->loopstart;
      v = &mr->mvert[mr->mloop[ml_index_last].v];
      v_next = &mr->mvert[mr->mloop[l_next_tmp].v];
      compute_normalize_edge_vectors(
          auv, av, data->luv[ml_index_last].uv, data->luv[l_next_tmp].uv, v->co, v_next->co);
      /* Save last edge. */
      copy_v2_v2(last_auv, auv[1]);
      copy_v3_v3(last_av, av[1]);
    }
    if (l_next == ml_index_end) {
      l_next = mp->loopstart;
      /* Move previous edge. */
      copy_v2_v2(auv[0], auv[1]);
      copy_v3_v3(av[0], av[1]);
      /* Copy already calculated last edge. */
      copy_v2_v2(auv[1], last_auv);
      copy_v3_v3(av[1], last_av);
    }
    else {
      v = &mr->mvert[mr->mloop[ml_index].v];
      v_next = &mr->mvert[mr->mloop[l_next].v];
      compute_normalize_edge_vectors(
          auv, av, data->luv[ml_index].uv, data->luv[l_next].uv, v->co, v_next->co);
    }
    edituv_get_edituv_stretch_angle(auv, av, &data->vbo_data[ml_index]);
  }
}

static GPUVertFormat *get_edituv_stretch_angle_format_subdiv()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* Waning: adjust #UVStretchAngle struct accordingly. */
    GPU_vertformat_attr_add(&format, "angle", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "uv_angles", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  }
  return &format;
}

static void extract_edituv_stretch_angle_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                                     const MeshRenderData *mr,
                                                     struct MeshBatchCache *cache,
                                                     void *buffer,
                                                     void *UNUSED(tls_data))
{
  GPUVertBuf *refined_vbo = static_cast<GPUVertBuf *>(buffer);

  GPU_vertbuf_init_build_on_device(
      refined_vbo, get_edituv_stretch_angle_format_subdiv(), subdiv_cache->num_subdiv_loops);

  GPUVertBuf *pos_nor = cache->final.buff.vbo.pos_nor;
  GPUVertBuf *uvs = cache->final.buff.vbo.uv;

  /* UVs are stored contiguously so we need to compute the offset in the UVs buffer for the active
   * UV layer. */
  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_MESH) ? &mr->me->ldata : &mr->bm->ldata;

  uint32_t uv_layers = cache->cd_used.uv;
  /* HACK to fix T68857 */
  if (mr->extract_type == MR_EXTRACT_BMESH && cache->cd_used.edit_uv == 1) {
    int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
    if (layer != -1) {
      uv_layers |= (1 << layer);
    }
  }

  int uvs_offset = 0;
  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPUV)) {
        break;
      }

      uvs_offset += 1;
    }
  }

  /* The data is at `offset * num loops`, and we have 2 values per index. */
  uvs_offset *= subdiv_cache->num_subdiv_loops * 2;

  draw_subdiv_build_edituv_stretch_angle_buffer(
      subdiv_cache, pos_nor, uvs, uvs_offset, refined_vbo);
}

constexpr MeshExtract create_extractor_edituv_edituv_stretch_angle()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_edituv_stretch_angle_init;
  extractor.iter_poly_bm = extract_edituv_stretch_angle_iter_poly_bm;
  extractor.iter_poly_mesh = extract_edituv_stretch_angle_iter_poly_mesh;
  extractor.init_subdiv = extract_edituv_stretch_angle_init_subdiv;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_StretchAngle_Data);
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.edituv_stretch_angle);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_edituv_stretch_angle =
    blender::draw::create_extractor_edituv_edituv_stretch_angle();
}
