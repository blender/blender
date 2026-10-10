/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

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
  const Span<bool> hide_poly = mr.hide_poly;
  const Span<int> orig_indices = mr.edit_bmesh && mr.orig_index_vert ?
                                     Span(mr.orig_index_vert, mr.verts_num) :
                                     Span<int>();
  if (selection.is_empty() && hide_poly.is_empty() && orig_indices.is_empty()) {
    flags.fill(0);
    return;
  }
  /* Write every value once, the VBO data is only written to. Hidden faces and unmapped vertices
   * take precedence over the selection. */
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face : range) {
      if (!hide_poly.is_empty() && hide_poly[face]) {
        flags.slice(faces[face]).fill(-1);
        continue;
      }
      for (const int corner : faces[face]) {
        const int vert = corner_verts[corner];
        if (!orig_indices.is_empty() && orig_indices[vert] == ORIGINDEX_NONE) {
          flags[corner] = -1;
        }
        else if (selection.is_empty()) {
          flags[corner] = 0;
        }
        else {
          flags[corner] = selection[use_face_select ? face : vert] ? 1 : 0;
        }
      }
    }
  });
}

static void extract_edit_flags_bm(const MeshRenderData &mr, MutableSpan<int> flags)
{
  const BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
      flags.slice(face_range).fill(BM_elem_flag_test(&face, BM_ELEM_HIDDEN) ? -1 : 0);
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
