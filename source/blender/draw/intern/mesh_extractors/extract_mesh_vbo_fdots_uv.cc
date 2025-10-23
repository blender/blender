/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BKE_attribute.hh"

#include "extract_mesh.hh"

namespace blender::draw {

static void extract_face_dots_uv_mesh(const MeshRenderData &mr, MutableSpan<float2> vbo_data)
{
  const Mesh &mesh = *mr.mesh;
  const StringRef name = mesh.active_uv_map_name();
  const bke::AttributeAccessor attributes = mesh.attributes();
  if (mr.use_subsurf_fdots) {
    const BitSpan facedot_tags = mesh.runtime->subsurf_face_dot_tags;
    const OffsetIndices faces = mr.faces;
    const Span<int> corner_verts = mr.corner_verts;
    const VArraySpan uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Corner);
    threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
      for (const int face_index : range) {
        const IndexRange face = faces[face_index];
        const auto corner = std::find_if(face.begin(), face.end(), [&](const int corner) {
          return facedot_tags[corner_verts[corner]].test();
        });
        if (corner == face.end()) {
          vbo_data[face_index] = float2(0);
        }
        else {
          vbo_data[face_index] = uv_map[*corner];
        }
      }
    });
  }
  else {
    /* Use the attribute API to average the attribute on the face domain. */
    const VArray uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Face);
    uv_map.materialize(vbo_data);
  }
}

static void extract_face_dots_uv_bm(const MeshRenderData &mr, MutableSpan<float2> vbo_data)
{
  const BMesh &bm = *mr.bm;
  const int offset = CustomData_get_offset(&bm.ldata, CD_PROP_FLOAT2);

  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
      vbo_data[face_index] = float2(0);
      for ([[maybe_unused]] const int i : IndexRange(face.len)) {
        vbo_data[face_index] += *BM_ELEM_CD_GET_FLOAT2_P(loop, offset);
        loop = loop->next;
      }
      vbo_data[face_index] /= face.len;
    }
  });
}

gpu::VertBufPtr extract_face_dots_uv(const MeshRenderData &mr)
{
  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "u", gpu::VertAttrType::SFLOAT_32_32);
    GPU_vertformat_alias_add(&format, "au");
    GPU_vertformat_alias_add(&format, "pos");
    return format;
  }();
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.faces_num);
  MutableSpan<float2> vbo_data = vbo->data<float2>();

  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_face_dots_uv_mesh(mr, vbo_data);
  }
  else {
    extract_face_dots_uv_bm(mr, vbo_data);
  }
  return vbo;
}

}  // namespace blender::draw
