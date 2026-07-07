/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "GPU_attribute_convert.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

template<typename GPUType>
static void extract_vert_normals(const Span<int> corner_verts,
                                 const Span<float3> vert_normals,
                                 MutableSpan<GPUType> normals)
{
  Array<GPUType> vert_normals_converted(vert_normals.size());
  gpu::convert_normals(vert_normals, vert_normals_converted.as_mutable_span());
  array_utils::gather(vert_normals_converted.as_span(), corner_verts, normals);
}

template<typename GPUType>
static void extract_face_normals(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const OffsetIndices faces = mr.faces;
  const Span<float3> face_normals = mr.face_normals;
  threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
    for (const int face : range) {
      normals.slice(faces[face]).fill(gpu::convert_normal<GPUType>(face_normals[face]));
    }
  });
}

template<typename GPUType>
static void extract_normals_mesh(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const auto get_vert_normals = [&]() {
    return mr.use_simplify_normals ? mr.mesh->vert_normals_true() : mr.mesh->vert_normals();
  };
  if (mr.normals_domain == bke::MeshNormalDomain::Face) {
    extract_face_normals(mr, normals);
  }
  else if (mr.normals_domain == bke::MeshNormalDomain::Point) {
    extract_vert_normals(mr.corner_verts, get_vert_normals(), normals);
  }
  else if (!mr.corner_normals.is_empty()) {
    gpu::convert_normals(mr.corner_normals, normals);
  }
  else if (mr.sharp_faces.is_empty()) {
    extract_vert_normals(mr.corner_verts, get_vert_normals(), normals);
  }
  else {
    const OffsetIndices faces = mr.faces;
    const Span<int> corner_verts = mr.corner_verts;
    const Span<bool> sharp_faces = mr.sharp_faces;
    const Span<float3> vert_normals = get_vert_normals();
    const Span<float3> face_normals = mr.face_normals;
    threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
      for (const int face : range) {
        if (sharp_faces[face]) {
          normals.slice(faces[face]).fill(gpu::convert_normal<GPUType>(face_normals[face]));
        }
        else {
          for (const int corner : faces[face]) {
            normals[corner] = gpu::convert_normal<GPUType>(vert_normals[corner_verts[corner]]);
          }
        }
      }
    });
  }
}

template<typename GPUType>
static void extract_vert_normals_bm(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const BMesh &bm = *mr.bm;
  if (mr.bm_free_normal_offset_vert != -1) {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
        const IndexRange face_range(BM_elem_index_get(loop), face.len);
        for (const int corner : face_range) {
          normals[corner] = gpu::convert_normal<GPUType>(
              BM_ELEM_CD_GET_FLOAT_P(loop->v, mr.bm_free_normal_offset_vert));
          loop = loop->next;
        }
      }
    });
  }
  else if (!mr.bm_vert_normals.is_empty()) {
    Array<GPUType> vert_normals_converted(mr.bm_vert_normals.size());
    gpu::convert_normals(mr.bm_vert_normals, vert_normals_converted.as_mutable_span());
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
        const IndexRange face_range(BM_elem_index_get(loop), face.len);
        for (const int corner : face_range) {
          normals[corner] = vert_normals_converted[BM_elem_index_get(loop->v)];
          loop = loop->next;
        }
      }
    });
  }
  else {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
        const IndexRange face_range(BM_elem_index_get(loop), face.len);
        for (const int corner : face_range) {
          normals[corner] = gpu::convert_normal<GPUType>(loop->v->no);
          loop = loop->next;
        }
      }
    });
  }
}

template<typename GPUType>
static void extract_face_normals_bm(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const BMesh &bm = *mr.bm;
  if (mr.bm_free_normal_offset_face != -1) {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
        normals.slice(face_range)
            .fill(gpu::convert_normal<GPUType>(
                BM_ELEM_CD_GET_FLOAT_P(&face, mr.bm_free_normal_offset_face)));
      }
    });
  }
  else if (!mr.bm_face_normals.is_empty()) {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
        normals.slice(face_range)
            .fill(gpu::convert_normal<GPUType>(mr.bm_face_normals[face_index]));
      }
    });
  }
  else {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
        normals.slice(face_range).fill(gpu::convert_normal<GPUType>(face.no));
      }
    });
  }
}

template<typename GPUType>
static void extract_normals_bm(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const BMesh &bm = *mr.bm;
  if (mr.normals_domain == bke::MeshNormalDomain::Face) {
    extract_face_normals_bm(mr, normals);
  }
  else if (mr.normals_domain == bke::MeshNormalDomain::Point) {
    extract_vert_normals_bm(mr, normals);
  }
  else if (mr.bm_free_normal_offset_corner != -1) {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
        const IndexRange face_range(BM_elem_index_get(loop), face.len);
        for (const int corner : face_range) {
          normals[corner] = gpu::convert_normal<GPUType>(
              BM_ELEM_CD_GET_FLOAT_P(loop, mr.bm_free_normal_offset_corner));
          loop = loop->next;
        }
      }
    });
  }
  else if (!mr.bm_loop_normals.is_empty()) {
    gpu::convert_normals(mr.bm_loop_normals, normals);
  }
  else {
    threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
      for (const int face_index : range) {
        const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
        const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
        const IndexRange face_range(BM_elem_index_get(loop), face.len);

        if (!BM_elem_flag_test(&face, BM_ELEM_SMOOTH)) {
          if (!mr.bm_face_normals.is_empty()) {
            normals.slice(face_range)
                .fill(gpu::convert_normal<GPUType>(mr.bm_face_normals[face_index]));
          }
          else {
            normals.slice(face_range).fill(gpu::convert_normal<GPUType>(face.no));
          }
        }
        else {
          if (!mr.bm_vert_normals.is_empty()) {
            for (const int corner : face_range) {
              normals[corner] = gpu::convert_normal<GPUType>(
                  mr.bm_vert_normals[BM_elem_index_get(loop->v)]);
              loop = loop->next;
            }
          }
          else {
            for (const int corner : face_range) {
              normals[corner] = gpu::convert_normal<GPUType>(loop->v->no);
              loop = loop->next;
            }
          }
        }
      }
    });
  }
}

gpu::VertBufPtr extract_normals(const MeshRenderData &mr, const bool use_hq)
{
  const int size = mr.corners_num + mr.loose_indices_num;
  if (use_hq) {
    static const GPUVertFormat format = []() {
      GPUVertFormat format{};
      GPU_vertformat_attr_add(&format, "nor", gpu::VertAttrType::SNORM_16_16_16_16);
      GPU_vertformat_alias_add(&format, "lnor");
      GPU_vertformat_alias_add(&format, "vnor");
      return format;
    }();
    gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
    GPU_vertbuf_data_alloc(*vbo, size);
    MutableSpan vbo_data = vbo->data<short4>();
    MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
    MutableSpan loose_data = vbo_data.take_back(mr.loose_indices_num);

    if (mr.extract_type == MeshExtractType::Mesh) {
      extract_normals_mesh(mr, corners_data);
    }
    else {
      extract_normals_bm(mr, corners_data);
    }

    loose_data.fill(short4(0));
    return vbo;
  }
  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "nor", gpu::VertAttrType::SNORM_10_10_10_2);
    GPU_vertformat_alias_add(&format, "lnor");
    GPU_vertformat_alias_add(&format, "vnor");
    return format;
  }();
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, size);
  MutableSpan vbo_data = vbo->data<gpu::PackedNormal>();
  MutableSpan corners_data = vbo_data.take_front(mr.corners_num);
  MutableSpan loose_data = vbo_data.take_back(mr.loose_indices_num);

  if (mr.extract_type == MeshExtractType::Mesh) {
    extract_normals_mesh(mr, corners_data);
  }
  else {
    extract_normals_bm(mr, corners_data);
  }

  loose_data.fill(gpu::PackedNormal{});
  return vbo;
}

static const GPUVertFormat &get_normals_format()
{
  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    GPU_vertformat_attr_add(&format, "nor", gpu::VertAttrType::SFLOAT_32_32_32);
    GPU_vertformat_alias_add(&format, "lnor");
    GPU_vertformat_alias_add(&format, "vnor");
    return format;
  }();
  return format;
}

static void update_loose_normals(const MeshRenderData &mr,
                                 const DRWSubdivCache &subdiv_cache,
                                 gpu::VertBuf &lnor)
{
  const int vbo_size = subdiv_full_vbo_size(mr, subdiv_cache);
  const int loose_geom_start = subdiv_cache.num_subdiv_loops;

  /* Push VBO content to the GPU and bind the VBO so that #GPU_vertbuf_update_sub can work. */
  GPU_vertbuf_use(&lnor);

  /* Default to zeroed attribute. The overlay shader should expect this and render engines should
   * never draw loose geometry. */
  const float3 default_normal(0.0f, 0.0f, 0.0f);
  for (const int i : IndexRange::from_begin_end(loose_geom_start, vbo_size)) {
    /* TODO(fclem): This has HORRENDOUS performance. Prefer clearing the buffer on device with
     * something like glClearBufferSubData. */
    GPU_vertbuf_update_sub(&lnor, i * sizeof(float3), sizeof(float3), &default_normal);
  }
}

gpu::VertBufPtr extract_normals_subdiv(const MeshRenderData &mr,
                                       const DRWSubdivCache &subdiv_cache,
                                       gpu::VertBuf &pos)
{
  const int vbo_size = subdiv_full_vbo_size(mr, subdiv_cache);

  gpu::VertBufPtr lnor = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(get_normals_format(), vbo_size));
  if (subdiv_cache.num_subdiv_loops == 0) {
    update_loose_normals(mr, subdiv_cache, *lnor);
    return lnor;
  }

  if (subdiv_cache.use_custom_loop_normals) {
    const Mesh *coarse_mesh = subdiv_cache.mesh;
    static GPUVertFormat src_normals_format = GPU_vertformat_from_attribute(
        "vnor", gpu::VertAttrType::SFLOAT_32_32_32);
    gpu::VertBufPtr src = gpu::VertBufPtr(GPU_vertbuf_create_with_format(src_normals_format));
    GPU_vertbuf_data_alloc(*src, coarse_mesh->corners_num);
    src->data<float3>().copy_from(coarse_mesh->corner_normals());
    draw_subdiv_interp_corner_normals(subdiv_cache, *src, *lnor);

    update_loose_normals(mr, subdiv_cache, *lnor);
    return lnor;
  }

  gpu::VertBufPtr subdiv_corner_verts = gpu::VertBufPtr(draw_subdiv_build_origindex_buffer(
      subdiv_cache.subdiv_loop_subdiv_vert_index, subdiv_cache.num_subdiv_loops));

  /* Calculate vertex normals (stored here per subdivided vertex rather than per subdivided face
   * corner). The values are used for smooth shaded faces later. */
  static GPUVertFormat vert_normals_format = GPU_vertformat_from_attribute(
      "vnor", gpu::VertAttrType::SFLOAT_32_32_32);
  gpu::VertBufPtr vert_normals = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(vert_normals_format, subdiv_cache.num_subdiv_verts));
  draw_subdiv_accumulate_normals(subdiv_cache,
                                 &pos,
                                 subdiv_cache.subdiv_vert_face_adjacency_offsets,
                                 subdiv_cache.subdiv_vert_face_adjacency,
                                 subdiv_corner_verts.get(),
                                 vert_normals.get());

  /* Compute final normals for face corners, either using the vertex normal corresponding to the
   * corner, or by calculating the face normal.
   *
   * TODO: Avoid using face normals or vertex normals if possible, using `mr.normals_domain`. */
  draw_subdiv_build_lnor_buffer(
      subdiv_cache, &pos, vert_normals.get(), subdiv_corner_verts.get(), lnor.get());

  update_loose_normals(mr, subdiv_cache, *lnor);

  return lnor;
}

}  // namespace blender::draw
