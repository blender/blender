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

gpu::VertBufPtr extract_skin_roots(const MeshRenderData &mr)
{
  /* Exclusively for edit mode. */
  BLI_assert(mr.bm);

  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "size", gpu::VertAttrType::SFLOAT_32);
    GPU_vertformat_attr_add(&format, "local_pos", gpu::VertAttrType::SFLOAT_32_32_32);
    return format;
  }();

  Vector<SkinRootData> skin_roots;
  const int radius_offset = CustomData_get_offset_named(
      &mr.bm->vdata, CD_PROP_FLOAT2, "skin_modifier_radius");
  const int root_offset = CustomData_get_offset_named(
      &mr.bm->vdata, CD_PROP_BOOL, "skin_modifier_root");
  if (radius_offset == -1 || root_offset == -1) {
    return {};
  }

  BMIter iter;
  BMVert *vert;
  BM_ITER_MESH (vert, &iter, mr.bm, BM_VERTS_OF_MESH) {
    if (BM_ELEM_CD_GET_BOOL(vert, root_offset)) {
      const float2 *radius = static_cast<const float2 *>(
          BM_ELEM_CD_GET_VOID_P(vert, radius_offset));
      skin_roots.append({((*radius)[0] + (*radius)[1]) * 0.5f, bm_vert_co_get(mr, vert)});
    }
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, skin_roots.size());
  vbo->data<SkinRootData>().copy_from(skin_roots);
  return vbo;
}

}  // namespace blender::draw
