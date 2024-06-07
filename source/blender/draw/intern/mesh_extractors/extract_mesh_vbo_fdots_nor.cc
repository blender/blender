/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "extract_mesh.hh"

namespace blender::draw {

#define NOR_AND_FLAG_DEFAULT 0
#define NOR_AND_FLAG_SELECT 1
#define NOR_AND_FLAG_ACTIVE -1
#define NOR_AND_FLAG_HIDDEN -2

template<typename GPUType>
static void extract_face_dot_normals_mesh(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  convert_normals(mr.face_normals, normals);
  const GPUType invalid_normal = convert_normal<GPUType>(float3(0));
  threading::parallel_for(IndexRange(mr.faces_num), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const BMFace *face = bm_original_face_get(mr, i);
      if (!face || BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
        normals[i] = invalid_normal;
        normals[i].w = NOR_AND_FLAG_HIDDEN;
      }
      else if (BM_elem_flag_test(face, BM_ELEM_SELECT)) {
        normals[i].w = (face == mr.efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT;
      }
    }
  });
}

template<typename GPUType>
void extract_face_dot_normals_bm(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const GPUType invalid_normal = convert_normal<GPUType>(float3(0));
  threading::parallel_for(IndexRange(mr.faces_num), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      BMFace *face = BM_face_at_index(mr.bm, i);
      if (BM_elem_flag_test(face, BM_ELEM_HIDDEN)) {
        normals[i] = invalid_normal;
        normals[i].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        normals[i] = convert_normal<GPUType>(bm_face_no_get(mr, face));
        normals[i].w = (BM_elem_flag_test(face, BM_ELEM_SELECT) ?
                            ((face == mr.efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                            NOR_AND_FLAG_DEFAULT);
      }
    }
  });
}

void extract_face_dot_normals(const MeshRenderData &mr, const bool use_hq, gpu::VertBuf &vbo)
{
  if (use_hq) {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
    GPU_vertbuf_init_with_format(vbo, format);
    GPU_vertbuf_data_alloc(vbo, mr.faces_num);
    MutableSpan vbo_data(static_cast<short4 *>(GPU_vertbuf_get_data(vbo)), mr.faces_num);

    if (mr.extract_type == MR_EXTRACT_MESH) {
      extract_face_dot_normals_mesh(mr, vbo_data);
    }
    else {
      extract_face_dot_normals_bm(mr, vbo_data);
    }
  }
  else {
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    }
    GPU_vertbuf_init_with_format(vbo, format);
    GPU_vertbuf_data_alloc(vbo, mr.faces_num);
    MutableSpan vbo_data(static_cast<GPUPackedNormal *>(GPU_vertbuf_get_data(vbo)), mr.faces_num);

    if (mr.extract_type == MR_EXTRACT_MESH) {
      extract_face_dot_normals_mesh(mr, vbo_data);
    }
    else {
      extract_face_dot_normals_bm(mr, vbo_data);
    }
  }
}

}  // namespace blender::draw
