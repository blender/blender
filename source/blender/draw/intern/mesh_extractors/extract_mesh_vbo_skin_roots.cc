/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "DNA_meshdata_types.h"

#include "extract_mesh.hh"

namespace blender::draw {

struct SkinRootData {
  float size;
  float3 local_pos;
};

void extract_skin_roots(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  /* Exclusively for edit mode. */
  BLI_assert(mr.bm);

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "local_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }

  Vector<SkinRootData> skin_roots;
  const int offset = CustomData_get_offset(&mr.bm->vdata, CD_MVERT_SKIN);
  BMIter iter;
  BMVert *vert;
  BM_ITER_MESH (vert, &iter, mr.bm, BM_VERTS_OF_MESH) {
    const MVertSkin *vs = (const MVertSkin *)BM_ELEM_CD_GET_VOID_P(vert, offset);
    if (vs->flag & MVERT_SKIN_ROOT) {
      skin_roots.append({(vs->radius[0] + vs->radius[1]) * 0.5f, bm_vert_co_get(mr, vert)});
    }
  }

  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, skin_roots.size());
  vbo.data<SkinRootData>().copy_from(skin_roots);
}

}  // namespace blender::draw
