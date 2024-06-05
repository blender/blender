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

static void index_mask_to_ibo(const IndexMask &mask, gpu::IndexBuf &ibo)
{
  const int max_index = mask.min_array_size();
  GPUIndexBufBuilder builder;
  GPU_indexbuf_init(&builder, GPU_PRIM_POINTS, mask.size(), max_index);
  MutableSpan<uint> data = GPU_indexbuf_get_data(&builder);
  mask.to_indices<int>(data.cast<int>());
  GPU_indexbuf_build_in_place_ex(&builder, 0, max_index, false, &ibo);
}

static void extract_face_dots_mesh(const MeshRenderData &mr, gpu::IndexBuf &face_dots)
{
  IndexMaskMemory memory;
  const IndexMask visible_faces = calc_face_visibility_mesh(mr, memory);
  index_mask_to_ibo(visible_faces, face_dots);
}

static void extract_face_dots_bm(const MeshRenderData &mr, gpu::IndexBuf &face_dots)
{
  BMesh &bm = *mr.bm;
  IndexMaskMemory memory;
  const IndexMask visible_faces = IndexMask::from_predicate(
      IndexRange(bm.totface), GrainSize(4096), memory, [&](const int i) {
        return !BM_elem_flag_test_bool(BM_face_at_index(&bm, i), BM_ELEM_HIDDEN);
      });
  index_mask_to_ibo(visible_faces, face_dots);
}

void extract_face_dots(const MeshRenderData &mr, gpu::IndexBuf &face_dots)
{
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_face_dots_mesh(mr, face_dots);
  }
  else {
    extract_face_dots_bm(mr, face_dots);
  }
}

}  // namespace blender::draw
