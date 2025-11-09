/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_math_vector.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

struct UVStretchAngle {
  float angle;
  int16_t uv_angles[2];
};
BLI_STATIC_ASSERT_ALIGN(UVStretchAngle, 4)

struct MeshExtract_StretchAngle_Data {
  UVStretchAngle *vbo_data;
  const float2 *uv;
  float auv[2][2], last_auv[2];
  float av[2][3], last_av[3];
  int cd_ofs;
};

static void compute_normalize_edge_vectors(float auv[2][2],
                                           float av[2][3],
                                           const float uv[2],
                                           const float uv_prev[2],
                                           const float co[3],
                                           const float co_prev[3])
{
  /* Move previous edge. */
  copy_v2_v2(auv[0], auv[1]);
  copy_v3_v3(av[0], av[1]);
  /* 2d edge */
  sub_v2_v2v2(auv[1], uv_prev, uv);
  normalize_v2(auv[1]);
  /* 3d edge */
  sub_v3_v3v3(av[1], co_prev, co);
  normalize_v3(av[1]);
}

static short v2_to_short_angle(const float v[2])
{
  return atan2f(v[1], v[0]) * float(M_1_PI) * SHRT_MAX;
}

static void edituv_get_edituv_stretch_angle(float auv[2][2],
                                            const float av[2][3],
                                            UVStretchAngle *r_stretch)
{
  /* Send UVs to the shader and let it compute the aspect corrected angle. */
  r_stretch->uv_angles[0] = v2_to_short_angle(auv[0]);
  r_stretch->uv_angles[1] = v2_to_short_angle(auv[1]);
  /* Compute 3D angle here. */
  r_stretch->angle = angle_normalized_v3v3(av[0], av[1]) * float(M_1_PI);

#if 0 /* here for reference, this is done in shader now. */
  float uvang = angle_normalized_v2v2(auv0, auv1);
  float ang = angle_normalized_v3v3(av0, av1);
  float stretch = fabsf(uvang - ang) / float(M_PI);
  return 1.0f - pow2f(1.0f - stretch);
#endif
}

static void extract_uv_stretch_angle_bm(const MeshRenderData &mr,
                                        MutableSpan<UVStretchAngle> vbo_data)
{
  const BMesh &bm = *mr.bm;
  const StringRef active_name = mr.mesh->active_uv_map_name();
  const int uv_offset = CustomData_get_offset_named(&bm.ldata, CD_PROP_FLOAT2, active_name);

  float auv[2][2], last_auv[2];
  float av[2][3], last_av[3];

  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(face);
    do {
      const int l_index = BM_elem_index_get(l_iter);

      const float (*luv)[2], (*luv_next)[2];
      BMLoop *l_next = l_iter->next;
      if (l_iter == BM_FACE_FIRST_LOOP(face)) {
        /* First loop in face. */
        BMLoop *l_tmp = l_iter->prev;
        BMLoop *l_next_tmp = l_iter;
        luv = BM_ELEM_CD_GET_FLOAT2_P(l_tmp, uv_offset);
        luv_next = BM_ELEM_CD_GET_FLOAT2_P(l_next_tmp, uv_offset);
        compute_normalize_edge_vectors(auv,
                                       av,
                                       *luv,
                                       *luv_next,
                                       bm_vert_co_get(mr, l_tmp->v),
                                       bm_vert_co_get(mr, l_next_tmp->v));
        /* Save last edge. */
        copy_v2_v2(last_auv, auv[1]);
        copy_v3_v3(last_av, av[1]);
      }
      if (l_next == BM_FACE_FIRST_LOOP(face)) {
        /* Move previous edge. */
        copy_v2_v2(auv[0], auv[1]);
        copy_v3_v3(av[0], av[1]);
        /* Copy already calculated last edge. */
        copy_v2_v2(auv[1], last_auv);
        copy_v3_v3(av[1], last_av);
      }
      else {
        luv = BM_ELEM_CD_GET_FLOAT2_P(l_iter, uv_offset);
        luv_next = BM_ELEM_CD_GET_FLOAT2_P(l_next, uv_offset);
        compute_normalize_edge_vectors(auv,
                                       av,
                                       *luv,
                                       *luv_next,
                                       bm_vert_co_get(mr, l_iter->v),
                                       bm_vert_co_get(mr, l_next->v));
      }
      edituv_get_edituv_stretch_angle(auv, av, &vbo_data[l_index]);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

static void extract_uv_stretch_angle_mesh(const MeshRenderData &mr,
                                          MutableSpan<UVStretchAngle> vbo_data)
{
  const Span<float3> positions = mr.vert_positions;
  const OffsetIndices faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  const Mesh &mesh = *mr.mesh;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const StringRef name = mesh.active_uv_map_name();
  const VArraySpan uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Corner);

  float auv[2][2], last_auv[2];
  float av[2][3], last_av[3];

  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];
    const int corner_end = face.start() + face.size();
    for (int corner = face.start(); corner < corner_end; corner += 1) {
      int l_next = corner + 1;
      if (corner == face.start()) {
        /* First loop in face. */
        const int corner_last = corner_end - 1;
        const int l_next_tmp = face.start();
        compute_normalize_edge_vectors(auv,
                                       av,
                                       uv_map[corner_last],
                                       uv_map[l_next_tmp],
                                       positions[corner_verts[corner_last]],
                                       positions[corner_verts[l_next_tmp]]);
        /* Save last edge. */
        copy_v2_v2(last_auv, auv[1]);
        copy_v3_v3(last_av, av[1]);
      }
      if (l_next == corner_end) {
        l_next = face.start();
        /* Move previous edge. */
        copy_v2_v2(auv[0], auv[1]);
        copy_v3_v3(av[0], av[1]);
        /* Copy already calculated last edge. */
        copy_v2_v2(auv[1], last_auv);
        copy_v3_v3(av[1], last_av);
      }
      else {
        compute_normalize_edge_vectors(auv,
                                       av,
                                       uv_map[corner],
                                       uv_map[l_next],
                                       positions[corner_verts[corner]],
                                       positions[corner_verts[l_next]]);
      }
      edituv_get_edituv_stretch_angle(auv, av, &vbo_data[corner]);
    }
  }
}

gpu::VertBufPtr extract_edituv_stretch_angle(const MeshRenderData &mr)
{
  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    /* Waning: adjust #UVStretchAngle struct accordingly. */
    GPU_vertformat_attr_add(&format, "angle", gpu::VertAttrType::SFLOAT_32);
    GPU_vertformat_attr_add(&format, "uv_angles", gpu::VertAttrType::SNORM_16_16);
    return format;
  }();

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num);
  MutableSpan vbo_data = vbo->data<UVStretchAngle>();

  if (mr.extract_type == MeshExtractType::BMesh) {
    extract_uv_stretch_angle_bm(mr, vbo_data);
  }
  else {
    extract_uv_stretch_angle_mesh(mr, vbo_data);
  }
  return vbo;
}

static const GPUVertFormat &get_edituv_stretch_angle_format_subdiv()
{
  static const GPUVertFormat format = []() {
    GPUVertFormat format{};
    /* Waning: adjust #UVStretchAngle struct accordingly. */
    GPU_vertformat_attr_add(&format, "angle", gpu::VertAttrType::SFLOAT_32);
    GPU_vertformat_attr_add(&format, "uv_angles", gpu::VertAttrType::SFLOAT_32_32);
    return format;
  }();
  return format;
}

gpu::VertBufPtr extract_edituv_stretch_angle_subdiv(const MeshRenderData &mr,
                                                    const DRWSubdivCache &subdiv_cache,
                                                    const MeshBatchCache &cache)
{
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_on_device(
      get_edituv_stretch_angle_format_subdiv(), subdiv_cache.num_subdiv_loops));

  gpu::VertBuf *pos = cache.final.buff.vbos.lookup(VBOType::Position).get();
  gpu::VertBuf *uvs = cache.final.buff.vbos.lookup(VBOType::UVs).get();

  /* It may happen that the data for the UV editor is requested before (as a separate draw update)
   * the data for the mesh when switching to the `UV Editing` workspace, and therefore the position
   * buffer might not be created yet. In this case, create a buffer it locally, the subdivision
   * data should already be evaluated if we are here. This can happen if the subsurf modifier is
   * only enabled in edit-mode. See #96338. */
  if (!pos) {
    pos = GPU_vertbuf_calloc();
    static const GPUVertFormat pos_format = GPU_vertformat_from_attribute(
        "pos", gpu::VertAttrType::SFLOAT_32_32_32);
    GPU_vertbuf_init_build_on_device(*pos, pos_format, subdiv_full_vbo_size(mr, subdiv_cache));
    draw_subdiv_extract_pos(subdiv_cache, pos, nullptr);
  }

  /* UVs are stored contiguously so we need to compute the offset in the UVs buffer for the active
   * UV layer. */

  VectorSet<std::string> uv_layers = cache.cd_used.uv;
  /* HACK to fix #68857 */
  if (mr.extract_type == MeshExtractType::BMesh && cache.cd_used.edit_uv == 1) {
    const StringRef active_name = mr.mesh->active_uv_map_name();
    if (!active_name.is_empty()) {
      uv_layers.add_as(active_name);
    }
  }

  int uvs_offset = uv_layers.index_of(mr.mesh->active_uv_map_name());

  /* The data is at `offset * num loops`, and we have 2 values per index. */
  uvs_offset *= subdiv_cache.num_subdiv_loops * 2;

  draw_subdiv_build_edituv_stretch_angle_buffer(subdiv_cache, pos, uvs, uvs_offset, vbo.get());
  return vbo;
}

}  // namespace blender::draw
