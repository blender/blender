/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static void extract_paint_overlay_flags(const MeshRenderData &mr, MutableSpan<int> flags)
{
  const bool use_face_select = (mr.mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  Span<bool> selection;
  if (mr.mesh->editflag & ME_EDIT_PAINT_FACE_SEL) {
    selection = mr.select_poly;
  }
  else if (mr.mesh->editflag & ME_EDIT_PAINT_VERT_SEL) {
    selection = mr.select_vert;
  }
  if (selection.is_empty() && mr.hide_poly.is_empty() && (!mr.edit_bmesh || !mr.orig_index_vert)) {
    flags.fill(0);
    return;
  }
  const OffsetIndices faces = mr.faces;
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    if (selection.is_empty()) {
      flags.fill(0);
    }
    else {
      if (use_face_select) {
        for (const int face : range) {
          flags.slice(faces[face]).fill(selection[face] ? 1 : 0);
        }
      }
      else {
        const Span<int> corner_verts = mr.corner_verts;
        for (const int face : range) {
          for (const int corner : faces[face]) {
            flags[corner] = selection[corner_verts[corner]] ? 1 : 0;
          }
        }
      }
    }
    if (!mr.hide_poly.is_empty()) {
      const Span<bool> hide_poly = mr.hide_poly;
      for (const int face : range) {
        if (hide_poly[face]) {
          flags.slice(faces[face]).fill(-1);
        }
      }
    }
    if (mr.edit_bmesh && mr.orig_index_vert) {
      const Span<int> corner_verts = mr.corner_verts;
      const Span<int> orig_indices(mr.orig_index_vert, mr.verts_num);
      for (const int face : range) {
        for (const int corner : faces[face]) {
          if (orig_indices[corner_verts[corner]] == ORIGINDEX_NONE) {
            flags[corner] = -1;
          }
        }
      }
    }
  });
}

static void extract_edit_flags_bm(const MeshRenderData &mr, MutableSpan<int> flags)
{
  /* TODO: Return early if there are no hidden faces. */
  const BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      if (BM_elem_flag_test(&face, BM_ELEM_HIDDEN)) {
        const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
        flags.slice(face_range).fill(-1);
      }
    }
  });
}

static const GPUVertFormat &get_paint_overlay_flag_format()
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute("paint_overlay_flag",
                                                                    gpu::VertAttrType::SINT_32);
  return format;
}

gpu::VertBufPtr extract_paint_overlay_flags(const MeshRenderData &mr)
{
  const int size = mr.corners_num + mr.loose_indices_num;
  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_with_format(get_paint_overlay_flag_format()));
  GPU_vertbuf_data_alloc(*vbo, size);
  MutableSpan vbo_data = vbo->data<int>();
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_data = vbo_data.take_back(mr.loose_indices_num);

  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_paint_overlay_flags(mr, corners_data);
  }
  else {
    extract_edit_flags_bm(mr, corners_data);
  }

  loose_data.fill(0);
  return vbo;
}

static void update_loose_flags(const MeshRenderData &mr,
                               const DRWSubdivCache &subdiv_cache,
                               gpu::VertBuf &flags)
{
  const int vbo_size = subdiv_full_vbo_size(mr, subdiv_cache);
  const int loose_geom_start = subdiv_cache.num_subdiv_loops;

  /* Push VBO content to the GPU and bind the VBO so that #GPU_vertbuf_update_sub can work. */
  GPU_vertbuf_use(&flags);

  /* Default to zeroed attribute. The overlay shader should expect this and render engines should
   * never draw loose geometry. */
  const int default_value = 0;
  for (const int i : IndexRange::from_begin_end(loose_geom_start, vbo_size)) {
    /* TODO(fclem): This has HORRENDOUS performance. Prefer clearing the buffer on device with
     * something like glClearBufferSubData. */
    GPU_vertbuf_update_sub(&flags, i * sizeof(int), sizeof(int), &default_value);
  }
}

gpu::VertBufPtr extract_paint_overlay_flags_subdiv(const MeshRenderData &mr,
                                                   const DRWSubdivCache &subdiv_cache)
{
  gpu::VertBufPtr flags = gpu::VertBufPtr(GPU_vertbuf_create_on_device(
      get_paint_overlay_flag_format(), subdiv_full_vbo_size(mr, subdiv_cache)));

  draw_subdiv_build_paint_overlay_flag_buffer(subdiv_cache, *flags);

  update_loose_flags(mr, subdiv_cache, *flags);
  return flags;
}

}  // namespace blender::draw
