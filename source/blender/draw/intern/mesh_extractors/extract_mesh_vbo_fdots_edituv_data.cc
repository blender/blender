/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

#include "draw_cache_impl.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Face-dots  Edit UV flag
 * \{ */

struct MeshExtract_EditUVFdotData_Data {
  EditLoopData *vbo_data;
  BMUVOffsets offsets;
};

static void extract_fdots_edituv_data_init(const MeshRenderData &mr,
                                           MeshBatchCache & /*cache*/,
                                           void *buf,
                                           void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "flag", GPU_COMP_U8, 4, GPU_FETCH_INT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.face_len);

  MeshExtract_EditUVFdotData_Data *data = static_cast<MeshExtract_EditUVFdotData_Data *>(tls_data);
  data->vbo_data = (EditLoopData *)GPU_vertbuf_get_data(vbo);
  data->offsets = BM_uv_map_get_offsets(mr.bm);
}

static void extract_fdots_edituv_data_iter_face_bm(const MeshRenderData &mr,
                                                   const BMFace *f,
                                                   const int /*f_index*/,
                                                   void *_data)
{
  MeshExtract_EditUVFdotData_Data *data = static_cast<MeshExtract_EditUVFdotData_Data *>(_data);
  EditLoopData *eldata = &data->vbo_data[BM_elem_index_get(f)];
  memset(eldata, 0x0, sizeof(*eldata));
  mesh_render_data_face_flag(mr, f, data->offsets, eldata);
}

static void extract_fdots_edituv_data_iter_face_mesh(const MeshRenderData &mr,
                                                     const int face_index,
                                                     void *_data)
{
  MeshExtract_EditUVFdotData_Data *data = static_cast<MeshExtract_EditUVFdotData_Data *>(_data);
  EditLoopData *eldata = &data->vbo_data[face_index];
  memset(eldata, 0x0, sizeof(*eldata));
  BMFace *efa = bm_original_face_get(mr, face_index);
  if (efa) {
    mesh_render_data_face_flag(mr, efa, data->offsets, eldata);
  }
}

constexpr MeshExtract create_extractor_fdots_edituv_data()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_fdots_edituv_data_init;
  extractor.iter_face_bm = extract_fdots_edituv_data_iter_face_bm;
  extractor.iter_face_mesh = extract_fdots_edituv_data_iter_face_mesh;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = sizeof(MeshExtract_EditUVFdotData_Data);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.fdots_edituv_data);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_fdots_edituv_data = blender::draw::create_extractor_fdots_edituv_data();
