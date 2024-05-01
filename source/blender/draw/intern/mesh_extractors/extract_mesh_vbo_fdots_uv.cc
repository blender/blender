/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_bitmap.h"

#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots UV
 * \{ */

struct MeshExtract_FdotUV_Data {
  float (*vbo_data)[2];
  const float (*uv_data)[2];
  int cd_ofs;
};

static void extract_fdots_uv_init(const MeshRenderData &mr,
                                  MeshBatchCache & /*cache*/,
                                  void *buf,
                                  void *tls_data)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "au");
    GPU_vertformat_alias_add(&format, "pos");
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.faces_num);

  if (!mr.use_subsurf_fdots) {
    /* Clear so we can accumulate on it. */
    memset(GPU_vertbuf_get_data(vbo), 0x0, mr.faces_num * GPU_vertbuf_get_format(vbo)->stride);
  }

  MeshExtract_FdotUV_Data *data = static_cast<MeshExtract_FdotUV_Data *>(tls_data);
  data->vbo_data = (float(*)[2])GPU_vertbuf_get_data(vbo);

  if (mr.extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr.bm->ldata, CD_PROP_FLOAT2);
  }
  else {
    data->uv_data = (const float(*)[2])CustomData_get_layer(&mr.mesh->corner_data, CD_PROP_FLOAT2);
  }
}

static void extract_fdots_uv_iter_face_bm(const MeshRenderData & /*mr*/,
                                          const BMFace *f,
                                          const int /*f_index*/,
                                          void *_data)
{
  MeshExtract_FdotUV_Data *data = static_cast<MeshExtract_FdotUV_Data *>(_data);
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    float w = 1.0f / float(f->len);
    const float *luv = BM_ELEM_CD_GET_FLOAT_P(l_iter, data->cd_ofs);
    madd_v2_v2fl(data->vbo_data[BM_elem_index_get(f)], luv, w);
  } while ((l_iter = l_iter->next) != l_first);
}

static void extract_fdots_uv_iter_face_mesh(const MeshRenderData &mr,
                                            const int face_index,
                                            void *_data)
{
  MeshExtract_FdotUV_Data *data = static_cast<MeshExtract_FdotUV_Data *>(_data);
  const BitSpan facedot_tags = mr.mesh->runtime->subsurf_face_dot_tags;

  for (const int corner : mr.faces[face_index]) {
    const int vert = mr.corner_verts[corner];
    if (mr.use_subsurf_fdots) {
      if (facedot_tags[vert]) {
        copy_v2_v2(data->vbo_data[face_index], data->uv_data[corner]);
      }
    }
    else {
      float w = 1.0f / float(mr.faces[face_index].size());
      madd_v2_v2fl(data->vbo_data[face_index], data->uv_data[corner], w);
    }
  }
}

constexpr MeshExtract create_extractor_fdots_uv()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_uv_init;
  extractor.iter_face_bm = extract_fdots_uv_iter_face_bm;
  extractor.iter_face_mesh = extract_fdots_uv_iter_face_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_FdotUV_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdots_uv);
  return extractor;
}

/** \} */

const MeshExtract extract_fdots_uv = create_extractor_fdots_uv();

}  // namespace blender::draw
