/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include <climits>

#include "BLI_string.h"

#include "GPU_attribute_convert.hh"

#include "BKE_attribute.hh"
#include "BKE_editmesh_tangent.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_tangent.hh"

#include "extract_mesh.hh"

#include "draw_subdivision.hh"

namespace blender::draw {

static Array<Array<float4>> extract_tan_init_common(const MeshRenderData &mr,
                                                    const MeshBatchCache &cache,
                                                    GPUVertFormat *format,
                                                    gpu::VertAttrType gpu_attr_type)
{
  GPU_vertformat_deinterleave(format);

  VectorSet<std::string> tan_layers = cache.cd_used.tan;
  bool use_orco_tan = cache.cd_used.tan_orco;

  const StringRef active_name = mr.mesh->active_uv_map_name();
  const StringRef default_name = mr.mesh->default_uv_map_name();

  /* FIXME(#91838): This is to avoid a crash when orco tangent was requested but there are valid
   * uv layers. It would be better to fix the root cause. */
  if (tan_layers.is_empty() && use_orco_tan && !default_name.is_empty()) {
    tan_layers.add(default_name);
    use_orco_tan = false;
  }

  if (use_orco_tan) {
    Array<float4> tangents;
    if (mr.extract_type == MeshExtractType::BMesh) {
      Array<float3> positions = BM_mesh_vert_coords_alloc(mr.bm);
      tangents = BKE_editmesh_orco_tangents_calc(
          mr.edit_bmesh, mr.bm_face_normals, mr.bm_loop_normals, positions);
    }
    else {
      Span<float3> orco;
      Array<float3> orco_allocated;
      if (const float3 *orco_ptr = static_cast<const float3 *>(
              CustomData_get_layer(&mr.mesh->vert_data, CD_ORCO)))
      {
        orco = Span(orco_ptr, mr.verts_num);
      }
      else {
        orco_allocated = mr.vert_positions;
        /* TODO: This is not thread-safe. Draw extraction should not modify the mesh. */
        BKE_mesh_orco_verts_transform(const_cast<Mesh *>(mr.mesh), orco_allocated, false);
        orco = orco_allocated;
      }
      tangents = bke::mesh::calc_orco_tangents(mr.vert_positions,
                                               mr.faces,
                                               mr.corner_verts,
                                               mr.mesh->corner_tris(),
                                               mr.mesh->corner_tri_faces(),
                                               mr.sharp_faces,
                                               mr.mesh->vert_normals(),
                                               mr.face_normals,
                                               mr.corner_normals,
                                               orco);
    }

    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
    GPU_vertformat_safe_attr_name("orco", attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
    SNPRINTF(attr_name, "t%s", attr_safe_name);
    GPU_vertformat_attr_add(format, attr_name, gpu_attr_type);
    GPU_vertformat_alias_add(format, "t");
    GPU_vertformat_alias_add(format, "at");

    return {std::move(tangents)};
  }

  Vector<StringRef> uv_names;
  for (const StringRef name : tan_layers.as_span().take_front(MAX_MTFACE)) {
    if (tan_layers.contains(name)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      GPU_vertformat_safe_attr_name(name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* Tangent layer name. */
      SNPRINTF(attr_name, "t%s", attr_safe_name);
      GPU_vertformat_attr_add(format, attr_name, gpu_attr_type);
      /* Active render layer name. */
      if (name == default_name) {
        GPU_vertformat_alias_add(format, "t");
      }
      /* Active display layer name. */
      if (name == active_name) {
        GPU_vertformat_alias_add(format, "at");
      }

      uv_names.append(name);
    }
  }

  if (uv_names.is_empty()) {
    GPU_vertformat_attr_add(format, "dummy", blender::gpu::VertAttrType::SFLOAT_32);
    return {};
  }

  Array<Array<float4>> results;
  if (mr.extract_type == MeshExtractType::BMesh) {
    results = BKE_editmesh_uv_tangents_calc(
        mr.edit_bmesh, mr.bm_face_normals, mr.bm_loop_normals, uv_names);
  }
  else {
    Array<VArraySpan<float2>> uv_maps(uv_names.size());
    Array<Span<float2>> uv_map_spans(uv_names.size());
    const bke::AttributeAccessor attributes = mr.mesh->attributes();
    for (const int i : uv_names.index_range()) {
      uv_maps[i] = *attributes.lookup<float2>(uv_names[i], bke::AttrDomain::Corner);
      uv_map_spans[i] = uv_maps[i];
    }
    results = bke::mesh::calc_uv_tangents(mr.vert_positions,
                                          mr.faces,
                                          mr.corner_verts,
                                          mr.mesh->corner_tris(),
                                          mr.mesh->corner_tri_faces(),
                                          mr.sharp_faces,
                                          mr.mesh->vert_normals(),
                                          mr.face_normals,
                                          mr.corner_normals,
                                          uv_map_spans);
  }

  if (format->attr_len == 0) {
    GPU_vertformat_attr_add(format, "dummy", blender::gpu::VertAttrType::SFLOAT_32);
  }

  return results;
}

gpu::VertBufPtr extract_tangents(const MeshRenderData &mr,
                                 const MeshBatchCache &cache,
                                 const bool use_hq)
{
  gpu::VertAttrType gpu_attr_type = use_hq ? gpu::VertAttrType::SNORM_16_16_16_16 :
                                             gpu::VertAttrType::SNORM_10_10_10_2;

  GPUVertFormat format = {0};
  const Array<Array<float4>> tangents = extract_tan_init_common(mr, cache, &format, gpu_attr_type);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num);

  if (use_hq) {
    MutableSpan tan_data = vbo->data<short4>();
    int vbo_index = 0;
    for (const int i : tangents.index_range()) {
      const Span<float4> layer_data = tangents[i];
      for (int corner = 0; corner < mr.corners_num; corner++) {
        tan_data[vbo_index] = gpu::convert_normal<short4>(float3(layer_data[corner]));
        tan_data[vbo_index].w = (layer_data[corner][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        vbo_index++;
      }
    }
    BLI_assert(vbo_index == tan_data.size());
  }
  else {
    MutableSpan tan_data = vbo->data<gpu::PackedNormal>();
    int vbo_index = 0;
    for (const int i : tangents.index_range()) {
      const Span<float4> layer_data = tangents[i];
      for (int corner = 0; corner < mr.corners_num; corner++) {
        tan_data[vbo_index] = gpu::convert_normal<gpu::PackedNormal>(float3(layer_data[corner]));
        tan_data[vbo_index].w = (layer_data[corner][3] > 0.0f) ? 1 : -2;
        vbo_index++;
      }
    }
    BLI_assert(vbo_index == tan_data.size());
  }

  return vbo;
}

static const GPUVertFormat &get_coarse_tan_format()
{
  static GPUVertFormat format = GPU_vertformat_from_attribute(
      "tan", gpu::VertAttrType::SFLOAT_32_32_32_32);
  return format;
}

gpu::VertBufPtr extract_tangents_subdiv(const MeshRenderData &mr,
                                        const DRWSubdivCache &subdiv_cache,
                                        const MeshBatchCache &cache)
{
  gpu::VertAttrType gpu_attr_type = gpu::VertAttrType::SFLOAT_32_32_32_32;
  GPUVertFormat format = {0};
  const Array<Array<float4>> tangents = extract_tan_init_common(mr, cache, &format, gpu_attr_type);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(
      GPU_vertbuf_create_on_device(format, subdiv_cache.num_subdiv_loops));

  gpu::VertBuf *coarse_vbo = GPU_vertbuf_calloc();
  /* Dynamic as we upload and interpolate layers one at a time. */
  GPU_vertbuf_init_with_format_ex(*coarse_vbo, get_coarse_tan_format(), GPU_USAGE_DYNAMIC);
  GPU_vertbuf_data_alloc(*coarse_vbo, mr.corners_num);

  /* Index of the tangent layer in the compact buffer. Used layers are stored in a single buffer.
   */
  int pack_layer_index = 0;
  for (const int i : tangents.index_range()) {
    float4 *tan_data = coarse_vbo->data<float4>().data();
    const Span<float4> values = tangents[i];
    for (int corner = 0; corner < mr.corners_num; corner++) {
      *tan_data = values[corner];
      (*tan_data)[3] = (values[corner][3] > 0.0f) ? 1.0f : -1.0f;
      tan_data++;
    }

    /* Ensure data is uploaded properly. */
    GPU_vertbuf_tag_dirty(coarse_vbo);
    /* Include stride in offset. */
    const int dst_offset = int(subdiv_cache.num_subdiv_loops) * 4 * pack_layer_index++;
    draw_subdiv_interp_custom_data(subdiv_cache, *coarse_vbo, *vbo, GPU_COMP_F32, 4, dst_offset);
  }

  GPU_vertbuf_discard(coarse_vbo);
  return vbo;
}

}  // namespace blender::draw
