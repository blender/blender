/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_utf8.h"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

/* Initialize the vertex format to be used for UVs. Return true if any UV layer is
 * found, false otherwise. */
static VectorSet<StringRef> mesh_extract_uv_format_init(GPUVertFormat *format,
                                                        const MeshBatchCache &cache,
                                                        const CustomData *cd_ldata,
                                                        const StringRef active_name,
                                                        const StringRef default_name,
                                                        const MeshExtractType extract_type)
{
  GPU_vertformat_deinterleave(format);

  VectorSet<StringRef> uv_layers;
  for (const StringRef name : cache.cd_used.uv.as_span().take_front(MAX_MTFACE)) {
    uv_layers.add_new(name);
  }

  /* HACK to fix #68857 */
  if (extract_type == MeshExtractType::BMesh && cache.cd_used.edit_uv == 1) {
    if (!bke::attribute_name_is_anonymous(default_name)) {
      uv_layers.add(default_name);
    }
  }

  const StringRef stencil_name = [&]() -> StringRef {
    const int stencil_index = CustomData_get_stencil_layer_index(cd_ldata, CD_PROP_FLOAT2);
    if (stencil_index == -1) {
      return "";
    }
    return cd_ldata->layers[stencil_index].name;
  }();

  for (const StringRef name : uv_layers) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
    GPU_vertformat_safe_attr_name(name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
    SNPRINTF_UTF8(attr_name, "a%s", attr_safe_name);
    GPU_vertformat_attr_add(format, attr_name, blender::gpu::VertAttrType::SFLOAT_32_32);
    if (name == default_name) {
      GPU_vertformat_alias_add(format, "a");
    }
    if (name == active_name) {
      GPU_vertformat_alias_add(format, "au");
      /* Alias to `pos` for edit uvs. */
      GPU_vertformat_alias_add(format, "pos");
    }
    if (name == stencil_name) {
      GPU_vertformat_alias_add(format, "mu");
    }
  }

  if (format->attr_len == 0) {
    GPU_vertformat_attr_add(format, "dummy", blender::gpu::VertAttrType::SFLOAT_32_32);
  }

  return uv_layers;
}

gpu::VertBufPtr extract_uv_maps(const MeshRenderData &mr, const MeshBatchCache &cache)
{
  GPUVertFormat format = {0};

  int v_len = mr.corners_num;
  const VectorSet<StringRef> uv_layers = mesh_extract_uv_format_init(
      &format,
      cache,
      (mr.extract_type == MeshExtractType::BMesh) ? &mr.bm->ldata : &mr.mesh->corner_data,
      mr.mesh->active_uv_map_name(),
      mr.mesh->default_uv_map_name(),
      mr.extract_type);
  if (uv_layers.is_empty()) {
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, v_len);

  MutableSpan<float2> uv_data = vbo->data<float2>();
  threading::memory_bandwidth_bound_task(uv_data.size_in_bytes() * 2, [&]() {
    if (mr.extract_type == MeshExtractType::BMesh) {
      const BMesh &bm = *mr.bm;
      for (const int i : uv_layers.index_range()) {
        MutableSpan<float2> data = uv_data.slice(i * bm.totloop, bm.totloop);
        const int offset = CustomData_get_offset_named(&bm.ldata, CD_PROP_FLOAT2, uv_layers[i]);
        threading::parallel_for(IndexRange(bm.totface), 2048, [&](const IndexRange range) {
          for (const int face_index : range) {
            const BMFace &face = *BM_face_at_index(&const_cast<BMesh &>(bm), face_index);
            const BMLoop *loop = BM_FACE_FIRST_LOOP(&face);
            for ([[maybe_unused]] const int i : IndexRange(face.len)) {
              const int index = BM_elem_index_get(loop);
              data[index] = BM_ELEM_CD_GET_FLOAT_P(loop, offset);
              loop = loop->next;
            }
          }
        });
      }
    }
    else {
      const bke::AttributeAccessor attributes = mr.mesh->attributes();
      for (const int i : uv_layers.index_range()) {
        const VArray uv_map = *attributes.lookup_or_default<float2>(
            uv_layers[i], bke::AttrDomain::Corner, float2(0));
        array_utils::copy(uv_map, uv_data.slice(i * mr.corners_num, mr.corners_num));
      }
    }
  });
  return vbo;
}

static VectorSet<StringRef> all_uv_map_attributes(const Mesh &mesh)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  VectorSet<StringRef> result;
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain == bke::AttrDomain::Corner && iter.data_type == bke::AttrType::Float2) {
      result.add(iter.name);
    }
  });
  return result;
}

gpu::VertBufPtr extract_uv_maps_subdiv(const DRWSubdivCache &subdiv_cache,
                                       const MeshBatchCache &cache)
{
  const Mesh *coarse_mesh = subdiv_cache.mesh;
  GPUVertFormat format = {0};

  const VectorSet<StringRef> uv_layers = mesh_extract_uv_format_init(
      &format,
      cache,
      &coarse_mesh->corner_data,
      coarse_mesh->active_uv_map_name(),
      coarse_mesh->default_uv_map_name(),
      MeshExtractType::Mesh);

  uint v_len = subdiv_cache.num_subdiv_loops;
  if (uv_layers.is_empty()) {
    /* TODO(kevindietrich): handle this more gracefully. */
    v_len = 1;
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_on_device(format, v_len));

  if (uv_layers.is_empty()) {
    return vbo;
  }

  const VectorSet<StringRef> all_uv_maps = all_uv_map_attributes(*coarse_mesh);

  /* Index of the UV layer in the compact buffer. Used UV layers are stored in a single buffer. */
  for (const int pack_layer_index : uv_layers.index_range()) {
    const StringRef name = uv_layers[pack_layer_index];
    const int i = all_uv_maps.index_of(name);
    const int offset = int(subdiv_cache.num_subdiv_loops) * pack_layer_index;
    draw_subdiv_extract_uvs(subdiv_cache, vbo.get(), i, offset);
  }
  return vbo;
}

}  // namespace blender::draw
