/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "GPU_index_buffer.hh"

#include "extract_mesh.hh"

namespace blender::draw {

static IndexMask calc_face_visibility_mesh(const MeshRenderData &mr, IndexMaskMemory &memory)
{
  IndexMask visible(mr.faces_num);
  if (!mr.hide_poly.is_empty()) {
    visible = IndexMask::from_bools_inverse(visible, mr.hide_poly, memory);
  }
  if (mr.use_subsurf_fdots) {
    const OffsetIndices faces = mr.faces;
    const Span<int> corner_verts = mr.corner_verts;
    const BitSpan facedot_tags = mr.mesh->runtime->subsurf_face_dot_tags;
    visible = IndexMask::from_predicate(visible, GrainSize(4096), memory, [&](const int i) {
      const Span<int> face_verts = corner_verts.slice(faces[i]);
      return std::any_of(face_verts.begin(), face_verts.end(), [&](const int vert) {
        return facedot_tags[vert];
      });
    });
  }
  return visible;
}

static gpu::IndexBufPtr index_mask_to_ibo(const IndexMask &mask)
{
  const int max_index = mask.min_array_size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, mask.size(), max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);
  mask.to_indices<int>(data.cast<int>());
  return gpu::IndexBufPtr(GPU_indexbuf_build_ex(&builder, 0, max_index, false));
}

static gpu::IndexBufPtr extract_face_dots_mesh(const MeshRenderData &mr)
{
  IndexMaskMemory memory;
  const IndexMask visible_faces = calc_face_visibility_mesh(mr, memory);
  return index_mask_to_ibo(visible_faces);
}

static gpu::IndexBufPtr extract_face_dots_bm(const MeshRenderData &mr)
{
  BMesh &bm = *mr.bm;
  IndexMaskMemory memory;
  const IndexMask visible_faces = IndexMask::from_predicate(
      IndexRange(bm.totface), GrainSize(4096), memory, [&](const int i) {
        return !BM_elem_flag_test_bool(BM_face_at_index(&bm, i), BM_ELEM_HIDDEN);
      });
  return index_mask_to_ibo(visible_faces);
}

gpu::IndexBufPtr extract_face_dots(const MeshRenderData &mr)
{
  if (mr.extract_type == MeshExtractType::Mesh) {
    return extract_face_dots_mesh(mr);
  }
  return extract_face_dots_bm(mr);
}

}  // namespace blender::draw
