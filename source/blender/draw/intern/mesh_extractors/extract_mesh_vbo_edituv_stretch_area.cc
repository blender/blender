/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_math_geom.h"
#include "BLI_math_vector_types.hh"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

BLI_INLINE float area_ratio_get(float area, float uvarea)
{
  if (area >= FLT_EPSILON && uvarea >= FLT_EPSILON) {
    return uvarea / area;
  }
  return 0.0f;
}

BLI_INLINE float area_ratio_to_stretch(float ratio, float tot_ratio)
{
  ratio *= tot_ratio;
  return (ratio > 1.0f) ? (1.0f / ratio) : ratio;
}

struct AreaInfo {
  float tot_area = 0.0f;
  float tot_uv_area = 0.0f;
};
static AreaInfo compute_area_ratio(const MeshRenderData &mr, MutableSpan<float> r_area_ratio)
{
  if (mr.extract_type == MeshExtractType::BMesh) {
    const BMesh &bm = *mr.bm;
    const StringRef active_name = mr.mesh->active_uv_map_name();
    const int uv_offset = CustomData_get_offset_named(&bm.ldata, CD_PROP_FLOAT2, active_name);
    return threading::parallel_reduce(
        IndexRange(bm.totface),
        1024,
        AreaInfo{},
        [&](const IndexRange range, AreaInfo info) {
          for (const int face_index : range) {
            const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
            const float area = BM_face_calc_area(&face);
            const float uvarea = BM_face_calc_area_uv(&face, uv_offset);
            info.tot_area += area;
            info.tot_uv_area += uvarea;
            r_area_ratio[face_index] = area_ratio_get(area, uvarea);
          }
          return info;
        },
        [](const AreaInfo &a, const AreaInfo &b) {
          return AreaInfo{a.tot_area + b.tot_area, a.tot_uv_area + b.tot_uv_area};
        });
  }

  const Span<float3> positions = mr.vert_positions;
  const OffsetIndices<int> faces = mr.faces;
  const Span<int> corner_verts = mr.corner_verts;
  const Mesh &mesh = *mr.mesh;
  const bke::AttributeAccessor attributes = mesh.attributes();
  const StringRef name = mesh.active_uv_map_name();
  const VArraySpan uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Corner);

  return threading::parallel_reduce(
      faces.index_range(),
      1024,
      AreaInfo{},
      [&](const IndexRange range, AreaInfo info) {
        for (const int face_index : range) {
          const IndexRange face = faces[face_index];
          const float area = bke::mesh::face_area_calc(positions, corner_verts.slice(face));
          float uvarea = area_poly_v2(reinterpret_cast<const float (*)[2]>(&uv_map[face.start()]),
                                      face.size());
          info.tot_area += area;
          info.tot_uv_area += uvarea;
          r_area_ratio[face_index] = area_ratio_get(area, uvarea);
        }
        return info;
      },
      [](const AreaInfo &a, const AreaInfo &b) {
        return AreaInfo{a.tot_area + b.tot_area, a.tot_uv_area + b.tot_uv_area};
      });
}

gpu::VertBufPtr extract_edituv_stretch_area(const MeshRenderData &mr,
                                            float &tot_area,
                                            float &tot_uv_area)
{
  Array<float> area_ratio(mr.faces_num);
  const AreaInfo info = compute_area_ratio(mr, area_ratio);
  tot_area = info.tot_area;
  tot_uv_area = info.tot_uv_area;

  static const GPUVertFormat format = GPU_vertformat_from_attribute("ratio",
                                                                    gpu::VertAttrType::SFLOAT_32);
  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num);
  MutableSpan<float> vbo_data = vbo->data<float>();

  const int64_t bytes = area_ratio.as_span().size_in_bytes() + vbo_data.size_in_bytes();
  threading::memory_bandwidth_bound_task(bytes, [&]() {
    if (mr.extract_type == MeshExtractType::BMesh) {
      const BMesh &bm = *mr.bm;
      threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
        for (const int face_index : range) {
          const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
          const IndexRange face_range(BM_elem_index_get(BM_FACE_FIRST_LOOP(&face)), face.len);
          vbo_data.slice(face_range).fill(area_ratio[face_index]);
        }
      });
    }
    else {
      BLI_assert(mr.extract_type == MeshExtractType::Mesh);
      const OffsetIndices<int> faces = mr.faces;
      threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
        for (const int face : range) {
          vbo_data.slice(faces[face]).fill(area_ratio[face]);
        }
      });
    }
  });
  return vbo;
}

gpu::VertBufPtr extract_edituv_stretch_area_subdiv(const MeshRenderData &mr,
                                                   const DRWSubdivCache &subdiv_cache,
                                                   float &tot_area,
                                                   float &tot_uv_area)
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute("ratio",
                                                                    gpu::VertAttrType::SFLOAT_32);
  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, subdiv_cache.num_subdiv_loops));

  gpu::VertBuf *coarse_vbo = GPU_vertbuf_calloc();
  GPU_vertbuf_init_with_format(*coarse_vbo, format);
  GPU_vertbuf_data_alloc(*coarse_vbo, mr.faces_num);
  MutableSpan coarse_vbo_data = coarse_vbo->data<float>();
  const AreaInfo info = compute_area_ratio(mr, coarse_vbo_data);
  tot_area = info.tot_area;
  tot_uv_area = info.tot_uv_area;

  draw_subdiv_build_edituv_stretch_area_buffer(subdiv_cache, coarse_vbo, vbo.get());

  GPU_vertbuf_discard(coarse_vbo);
  return vbo;
}

}  // namespace blender::draw
