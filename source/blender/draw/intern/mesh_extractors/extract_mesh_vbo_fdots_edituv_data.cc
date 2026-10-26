/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

#include "draw_cache_impl.hh"

namespace blender::draw {

static void extract_face_dots_edituv_data_bmesh(const MeshRenderData &mr,
                                                MutableSpan<EditLoopData> vbo_data)
{
  const BMesh &bm = *mr.bm;
  const BMUVOffsets offsets = BM_uv_map_offsets_get(&bm);
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      EditLoopData value = {};
      mesh_render_data_face_flag(mr, &face, offsets, value);
      vbo_data[face_index] = value;
    }
  });
}

static void extract_face_dots_edituv_data_mapped_bmesh(const MeshRenderData &mr,
                                                       MutableSpan<EditLoopData> vbo_data)
{
  if (!mr.orig_index_face) {
    vbo_data.fill({});
    return;
  }
  const BMUVOffsets offsets = BM_uv_map_offsets_get(mr.bm);
  const Span<int> orig_index_face(mr.orig_index_face, mr.faces_num);
  threading::parallel_for(IndexRange(mr.faces_num), 4096, [&](const IndexRange range) {
    for (const int face : range) {
      EditLoopData value = {};
      if (orig_index_face[face] != ORIGINDEX_NONE) {
        const BMFace *orig_face = bm_original_face_get(mr, face);
        mesh_render_data_face_flag(mr, orig_face, offsets, value);
      }
      vbo_data[face] = value;
    }
  });
}

gpu::VertBufPtr extract_face_dots_edituv_data(const MeshRenderData &mr)
{
  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "data", gpu::VertAttrType::UINT_8_8_8_8);
    GPU_vertformat_alias_add(&format, "flag");
    return format;
  }();
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.faces_num);
  MutableSpan vbo_data = vbo->data<EditLoopData>();
  if (mr.extract_type == MeshExtractType::BMesh) {
    extract_face_dots_edituv_data_bmesh(mr, vbo_data);
  }
  else {
    extract_face_dots_edituv_data_mapped_bmesh(mr, vbo_data);
  }
  return vbo;
}

}  // namespace blender::draw
