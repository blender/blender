/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "GPU_index_buffer.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static const GPUVertFormat &get_fdots_pos_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  return format;
}

static const GPUVertFormat &get_fdots_nor_format_subdiv()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }
  return format;
}

static void extract_face_dot_positions_mesh(const MeshRenderData &mr, MutableSpan<float3> vbo_data)
{
  const Span<float3> positions = mr.vert_positions;
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  if (mr.use_subsurf_fdots) {
    const BitSpan facedot_tags = mr.mesh->runtime->subsurf_face_dot_tags;
    threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
      for (const int face : range) {
        const Span<int> face_verts = corner_verts.slice(faces[face]);
        const int *vert = std::find_if(face_verts.begin(), face_verts.end(), [&](const int vert) {
          return facedot_tags[vert].test();
        });
        if (vert == face_verts.end()) {
          vbo_data[face] = float3(0);
        }
        else {
          vbo_data[face] = positions[*vert];
        }
      }
    });
  }
  else {
    threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
      for (const int face : range) {
        vbo_data[face] = bke::mesh::face_center_calc(positions, corner_verts.slice(faces[face]));
      }
    });
  }
}

static void extract_face_dot_positions_bm(const MeshRenderData &mr, MutableSpan<float3> vbo_data)
{
  const BMesh &bm = *mr.bm;
  threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
      if (mr.bm_vert_coords.is_empty()) {
        BM_face_calc_center_median(&face, vbo_data[face_index]);
      }
      else {
        BM_face_calc_center_median_vcos(&bm, &face, vbo_data[face_index], mr.bm_vert_coords);
      }
    }
  });
}

void extract_face_dots_position(const MeshRenderData &mr, gpu::VertBuf &vbo)
{
  GPU_vertbuf_init_with_format(vbo, get_fdots_pos_format());
  GPU_vertbuf_data_alloc(vbo, mr.corners_num + mr.loose_indices_num);

  MutableSpan vbo_data = vbo.data<float3>();
  if (mr.extract_type == MR_EXTRACT_MESH) {
    extract_face_dot_positions_mesh(mr, vbo_data);
  }
  else {
    extract_face_dot_positions_bm(mr, vbo_data);
  }
}

void extract_face_dots_subdiv(const DRWSubdivCache &subdiv_cache,
                              gpu::VertBuf &fdots_pos,
                              gpu::VertBuf *fdots_nor,
                              gpu::IndexBuf &fdots)
{
  /* We "extract" positions, normals, and indices at once. */
  /* The normals may not be requested. */
  if (fdots_nor) {
    GPU_vertbuf_init_build_on_device(
        *fdots_nor, get_fdots_nor_format_subdiv(), subdiv_cache.num_coarse_faces);
  }
  GPU_vertbuf_init_build_on_device(
      fdots_pos, get_fdots_pos_format(), subdiv_cache.num_coarse_faces);
  GPU_indexbuf_init_build_on_device(&fdots, subdiv_cache.num_coarse_faces);
  draw_subdiv_build_fdots_buffers(subdiv_cache, &fdots_pos, fdots_nor, &fdots);
}

}  // namespace blender::draw
