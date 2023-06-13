/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Skin Modifier Roots
 * \{ */

struct SkinRootData {
  float size;
  float local_pos[3];
};

static void extract_skin_roots_init(const MeshRenderData *mr,
                                    MeshBatchCache * /*cache*/,
                                    void *buf,
                                    void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  /* Exclusively for edit mode. */
  BLI_assert(mr->bm);

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "local_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->bm->totvert);

  SkinRootData *vbo_data = (SkinRootData *)GPU_vertbuf_get_data(vbo);

  int root_len = 0;
  int cd_ofs = CustomData_get_offset(&mr->bm->vdata, CD_MVERT_SKIN);

  BMIter iter;
  BMVert *eve;
  BM_ITER_MESH (eve, &iter, mr->bm, BM_VERTS_OF_MESH) {
    const MVertSkin *vs = (const MVertSkin *)BM_ELEM_CD_GET_VOID_P(eve, cd_ofs);
    if (vs->flag & MVERT_SKIN_ROOT) {
      vbo_data->size = (vs->radius[0] + vs->radius[1]) * 0.5f;
      copy_v3_v3(vbo_data->local_pos, bm_vert_co_get(mr, eve));
      vbo_data++;
      root_len++;
    }
  }

  /* It's really unlikely that all verts will be roots. Resize to avoid losing VRAM. */
  GPU_vertbuf_data_len_set(vbo, root_len);
}

constexpr MeshExtract create_extractor_skin_roots()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_skin_roots_init;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.skin_roots);
  return extractor;
}

/** \} */

}  // namespace blender::draw

const MeshExtract extract_skin_roots = blender::draw::create_extractor_skin_roots();
