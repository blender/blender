/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Loop Normal
 * \{ */

template<typename GPUType> inline GPUType convert_normal(const float3 &src);

template<> inline GPUPackedNormal convert_normal(const float3 &src)
{
  return GPU_normal_convert_i10_v3(src);
}

template<> inline short4 convert_normal(const float3 &src)
{
  short4 dst;
  normal_float_to_short_v3(dst, src);
  return dst;
}

template<typename GPUType>
static void convert_normals_impl(const Span<float3> src, MutableSpan<GPUType> dst)
{
  threading::parallel_for(src.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      dst[i] = convert_normal<GPUType>(src[i]);
    }
  });
}

template<> void convert_normals(const Span<float3> src, MutableSpan<GPUPackedNormal> normals)
{
  convert_normals_impl(src, normals);
}
template<> void convert_normals(const Span<float3> src, MutableSpan<short4> normals)
{
  convert_normals_impl(src, normals);
}

template<typename GPUType>
static void extract_vert_normals(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  Array<GPUType> vert_normals_converted(mr.vert_normals.size());
  convert_normals(mr.vert_normals, vert_normals_converted.as_mutable_span());
  array_utils::gather(vert_normals_converted.as_span(), mr.corner_verts, normals);
}

template<typename GPUType>
static void extract_face_normals(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const OffsetIndices faces = mr.faces;
  const Span<float3> face_normals = mr.face_normals;
  threading::parallel_for(faces.index_range(), 4096, [&](const IndexRange range) {
    for (const int face : range) {
      normals.slice(faces[face]).fill(convert_normal<GPUType>(face_normals[face]));
    }
  });
}

template<typename GPUType>
static void extract_normals_mesh(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  if (mr.normals_domain == bke::MeshNormalDomain::Face) {
    extract_face_normals(mr, normals);
  }
  else if (mr.normals_domain == bke::MeshNormalDomain::Point) {
    extract_vert_normals(mr, normals);
  }
  else if (!mr.corner_normals.is_empty()) {
    convert_normals(mr.corner_normals, normals);
  }
  else if (mr.sharp_faces.is_empty()) {
    extract_vert_normals(mr, normals);
  }
  else {
    const OffsetIndices faces = mr.faces;
    const Span<int> corner_verts = mr.corner_verts;
    const Span<bool> sharp_faces = mr.sharp_faces;
    const Span<float3> vert_normals = mr.vert_normals;
    const Span<float3> face_normals = mr.face_normals;
    threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
      for (const int face : range) {
        if (sharp_faces[face]) {
          normals.slice(faces[face]).fill(convert_normal<GPUType>(face_normals[face]));
        }
        else {
          for (const int corner : faces[face]) {
            normals[corner] = convert_normal<GPUType>(vert_normals[corner_verts[corner]]);
          }
        }
      }
    });
  }
}

template<typename GPUType>
static void extract_paint_overlay_flags(const MeshRenderData &mr, MutableSpan<GPUType> normals)
{
  const bool use_face_select = (mr.mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  Span<bool> selection;
  if (mr.mesh->editflag & ME_EDIT_PAINT_FACE_SEL) {
    selection = mr.select_poly;
  }
  else if (mr.mesh->editflag & ME_EDIT_PAINT_VERT_SEL) {
    selection = mr.select_vert;
  }
  if (selection.is_empty() && mr.hide_poly.is_empty() && (!mr.edit_bmesh || !mr.v_origindex)) {
    return;
  }
  const OffsetIndices faces = mr.faces;
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    if (!selection.is_empty()) {
      if (use_face_select) {
        for (const int face : range) {
          if (selection[face]) {
            for (const int corner : faces[face]) {
              normals[corner].w = 1;
            }
          }
        }
      }
      else {
        const Span<int> corner_verts = mr.corner_verts;
        for (const int face : range) {
          for (const int corner : faces[face]) {
            if (selection[corner_verts[corner]]) {
              normals[corner].w = 1;
            }
          }
        }
      }
    }
    if (!mr.hide_poly.is_empty()) {
      const Span<bool> hide_poly = mr.hide_poly;
      for (const int face : range) {
        if (hide_poly[face]) {
          for (const int corner : faces[face]) {
            normals[corner].w = -1;
          }
        }
      }
    }
    if (mr.edit_bmesh && mr.v_origindex) {
      const Span<int> corner_verts = mr.corner_verts;
      const Span<int> orig_indices(mr.v_origindex, mr.verts_num);
      for (const int face : range) {
        for (const int corner : faces[face]) {
          if (orig_indices[corner_verts[corner]] == ORIGINDEX_NONE) {
            normals[corner].w = -1;
          }
        }
      }
    }
  });
}

static void extract_lnor_init(const MeshRenderData &mr,
                              MeshBatchCache & /*cache*/,
                              void *buf,
                              void *tls_data)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.corners_num);

  if (mr.extract_type == MR_EXTRACT_MESH) {
    MutableSpan vbo_data(static_cast<GPUPackedNormal *>(GPU_vertbuf_get_data(vbo)),
                         mr.corners_num);
    extract_normals_mesh(mr, vbo_data);
    extract_paint_overlay_flags(mr, vbo_data);
  }
  else {
    *static_cast<GPUPackedNormal **>(tls_data) = static_cast<GPUPackedNormal *>(
        GPU_vertbuf_get_data(vbo));
  }
}

static void extract_lnor_iter_face_bm(const MeshRenderData &mr,
                                      const BMFace *f,
                                      const int /*f_index*/,
                                      void *data_v)
{
  GPUPackedNormal *data = *(GPUPackedNormal **)data_v;
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (!mr.corner_normals.is_empty()) {
      data[l_index] = GPU_normal_convert_i10_v3(mr.corner_normals[l_index]);
    }
    else {
      if (mr.normals_domain == bke::MeshNormalDomain::Face ||
          !BM_elem_flag_test(f, BM_ELEM_SMOOTH))
      {
        data[l_index] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, f));
      }
      else {
        data[l_index] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, l_iter->v));
      }
    }
    data[l_index].w = BM_elem_flag_test(f, BM_ELEM_HIDDEN) ? -1 : 0;
  } while ((l_iter = l_iter->next) != l_first);
}

static GPUVertFormat *get_subdiv_lnor_format()
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "lnor");
    GPU_vertformat_alias_add(&format, "vnor");
  }
  return &format;
}

static void extract_lnor_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                     const MeshRenderData & /*mr*/,
                                     MeshBatchCache &cache,
                                     void *buffer,
                                     void * /*data*/)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buffer);
  gpu::VertBuf *pos_nor = cache.final.buff.vbo.pos;
  BLI_assert(pos_nor);
  GPU_vertbuf_init_build_on_device(vbo, get_subdiv_lnor_format(), subdiv_cache.num_subdiv_loops);
  draw_subdiv_build_lnor_buffer(subdiv_cache, pos_nor, vbo);
}

constexpr MeshExtract create_extractor_lnor()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lnor_init;
  extractor.init_subdiv = extract_lnor_init_subdiv;
  extractor.iter_face_bm = extract_lnor_iter_face_bm;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = sizeof(GPUPackedNormal *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.nor);
  return extractor;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract HQ Loop Normal
 * \{ */

static void extract_lnor_hq_init(const MeshRenderData &mr,
                                 MeshBatchCache & /*cache*/,
                                 void *buf,
                                 void *tls_data)
{
  gpu::VertBuf *vbo = static_cast<gpu::VertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.corners_num);

  if (mr.extract_type == MR_EXTRACT_MESH) {
    MutableSpan vbo_data(static_cast<short4 *>(GPU_vertbuf_get_data(vbo)), mr.corners_num);
    extract_normals_mesh(mr, vbo_data);
    extract_paint_overlay_flags(mr, vbo_data);
  }
  else {
    *(short4 **)tls_data = static_cast<short4 *>(GPU_vertbuf_get_data(vbo));
  }
}

static void extract_lnor_hq_iter_face_bm(const MeshRenderData &mr,
                                         const BMFace *f,
                                         const int /*f_index*/,
                                         void *data)
{
  BMLoop *l_iter, *l_first;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    const int l_index = BM_elem_index_get(l_iter);
    if (!mr.corner_normals.is_empty()) {
      normal_float_to_short_v3(&(*(short4 **)data)[l_index].x, mr.corner_normals[l_index]);
    }
    else {
      if (BM_elem_flag_test(f, BM_ELEM_SMOOTH)) {
        normal_float_to_short_v3(&(*(short4 **)data)[l_index].x, bm_vert_no_get(mr, l_iter->v));
      }
      else {
        normal_float_to_short_v3(&(*(short4 **)data)[l_index].x, bm_face_no_get(mr, f));
      }
    }
  } while ((l_iter = l_iter->next) != l_first);
}

constexpr MeshExtract create_extractor_lnor_hq()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_lnor_hq_init;
  extractor.init_subdiv = extract_lnor_init_subdiv;
  extractor.iter_face_bm = extract_lnor_hq_iter_face_bm;
  extractor.data_type = MR_DATA_LOOP_NOR;
  extractor.data_size = sizeof(short4 *);
  extractor.use_threading = true;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.nor);
  return extractor;
}

/** \} */

const MeshExtract extract_nor = create_extractor_lnor();
const MeshExtract extract_nor_hq = create_extractor_lnor_hq();

}  // namespace blender::draw
