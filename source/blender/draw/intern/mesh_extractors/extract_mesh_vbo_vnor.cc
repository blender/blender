/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Vertex Normal
 * \{ */

static void extract_vnor_init(const MeshRenderData &mr,
                              MeshBatchCache & /*cache*/,
                              void *buf,
                              void *tls_data)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "vnor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.corners_num);

  if (mr.extract_type == MR_EXTRACT_MESH) {
    MutableSpan vbo_data(static_cast<GPUPackedNormal *>(GPU_vertbuf_get_data(vbo)),
                         mr.corners_num);
    extract_vert_normals(mr, vbo_data);
  }
  else {
    *static_cast<GPUPackedNormal **>(tls_data) = static_cast<GPUPackedNormal *>(
        GPU_vertbuf_get_data(vbo));
  }
}

static void extract_vnor_iter_face_bm(const MeshRenderData &mr,
                                      const BMFace *face,
                                      const int /*f_index*/,
                                      void *data_v)
{
  GPUPackedNormal *data = *static_cast<GPUPackedNormal **>(data_v);
  const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
  for ([[maybe_unused]] const int i : IndexRange(face->len)) {
    const int index = BM_elem_index_get(loop);
    data[index] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, loop->v));
    loop = loop->next;
  }
}

constexpr MeshExtract create_extractor_vnor()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_vnor_init;
  extractor.iter_face_bm = extract_vnor_iter_face_bm;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = sizeof(GPUPackedNormal *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.vnor);
  return extractor;
}

/** \} */

const MeshExtract extract_vnor = create_extractor_vnor();

}  // namespace blender::draw
