/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

#include "draw_cache_impl.hh"

namespace blender::draw {

void extract_face_dots_edituv_data(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }
  GPU_vertbuf_init_with_format(vbo, format);
  GPU_vertbuf_data_alloc(vbo, mr.faces_num);
  MutableSpan vbo_data = vbo.data<EditLoopData>();
  const BMesh &bm = *mr.bm;
  const BMUVOffsets offsets = BM_uv_map_get_offsets(&bm);
  if (mr.extract_type == MeshExtractType::BMesh) {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        vbo_data[face_index] = {};
        mesh_render_data_face_flag(mr, &face, offsets, vbo_data[face_index]);
      }
    });
  }
  else {
    if (mr.orig_index_face) {
      const Span<int> orig_index_face(mr.orig_index_face, mr.faces_num);
      threading::parallel_for(IndexRange(mr.faces_num), 4096, [&](const IndexRange range) {
        for (const int face : range) {
          vbo_data[face] = {};
          if (orig_index_face[face] == ORIGINDEX_NONE) {
            continue;
          }
          const BMFace *orig_face = bm_original_face_get(mr, face);
          mesh_render_data_face_flag(mr, orig_face, offsets, vbo_data[face]);
        }
      });
    }
    else {
      vbo_data.fill({});
    }
  }
}

}  // namespace blender::draw
