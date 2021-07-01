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

#include "draw_cache_extract_mesh_private.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots UV
 * \{ */

struct MeshExtract_FdotUV_Data {
  float (*vbo_data)[2];
  MLoopUV *uv_data;
  int cd_ofs;
};

static void extract_fdots_uv_init(const MeshRenderData *mr,
                                  struct MeshBatchCache *UNUSED(cache),
                                  void *buf,
                                  void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "au");
    GPU_vertformat_alias_add(&format, "pos");
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  if (!mr->use_subsurf_fdots) {
    /* Clear so we can accumulate on it. */
    memset(GPU_vertbuf_get_data(vbo), 0x0, mr->poly_len * GPU_vertbuf_get_format(vbo)->stride);
  }

  MeshExtract_FdotUV_Data *data = static_cast<MeshExtract_FdotUV_Data *>(tls_data);
  data->vbo_data = (float(*)[2])GPU_vertbuf_get_data(vbo);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  }
  else {
    data->uv_data = (MLoopUV *)CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
  }
}

static void extract_fdots_uv_iter_poly_bm(const MeshRenderData *UNUSED(mr),
                                          const BMFace *f,
                                          const int UNUSED(f_index),
                                          void *_data)
{
  MeshExtract_FdotUV_Data *data = static_cast<MeshExtract_FdotUV_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    float w = 1.0f / (float)f->len;
    const MLoopUV *luv = (const MLoopUV *)BM_ELEM_CD_GET_VOID_P(l_iter, data->cd_ofs);
    madd_v2_v2fl(data->vbo_data[BM_elem_index_get(f)], luv->uv, w);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_fdots_uv_iter_poly_mesh(const MeshRenderData *mr,
                                            const MPoly *mp,
                                            const int mp_index,
                                            void *_data)
{
  MeshExtract_FdotUV_Data *data = static_cast<MeshExtract_FdotUV_Data *>(_data);
  const MLoop *mloop = mr->mloop;
  const int ml_index_end = mp->loopstart + mp->totloop;
  for (int ml_index = mp->loopstart; ml_index < ml_index_end; ml_index += 1) {
    const MLoop *ml = &mloop[ml_index];
    if (mr->use_subsurf_fdots) {
      const MVert *mv = &mr->mvert[ml->v];
      if (mv->flag & ME_VERT_FACEDOT) {
        copy_v2_v2(data->vbo_data[mp_index], data->uv_data[ml_index].uv);
      }
    }
    else {
      float w = 1.0f / (float)mp->totloop;
      madd_v2_v2fl(data->vbo_data[mp_index], data->uv_data[ml_index].uv, w);
    }
  }
}

constexpr MeshExtract create_extractor_fdots_uv()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_uv_init;
  extractor.iter_poly_bm = extract_fdots_uv_iter_poly_bm;
  extractor.iter_poly_mesh = extract_fdots_uv_iter_poly_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_FdotUV_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferCache, vbo.fdots_uv);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
const MeshExtract extract_fdots_uv = blender::draw::create_extractor_fdots_uv();
}
