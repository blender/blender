/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_array_utils.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_utf8.h"

#include "BKE_attribute.hh"

#include "draw_subdivision.hh"
#include "extract_mesh.hh"

namespace blender::draw {

/* Initialize the vertex format to be used for UVs. Return true if any UV layer is
 * found, false otherwise. */
static bool mesh_extract_uv_format_init(GPUVertFormat *format,
                                        const MeshBatchCache &cache,
                                        const CustomData *cd_ldata,
                                        const MeshExtractType extract_type,
                                        uint32_t &r_uv_layers)
{
  GPU_vertformat_deinterleave(format);

  uint32_t uv_layers = cache.cd_used.uv;
  /* HACK to fix #68857 */
  if (extract_type == MeshExtractType::BMesh && cache.cd_used.edit_uv == 1) {
    int layer = CustomData_get_active_layer(cd_ldata, CD_PROP_FLOAT2);
    if (layer != -1 && !CustomData_layer_is_anonymous(cd_ldata, CD_PROP_FLOAT2, layer)) {
      uv_layers |= (1 << layer);
    }
  }

  r_uv_layers = 0;

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_PROP_FLOAT2, i);

      /* not all UV layers are guaranteed to exist, since the list of available UV
       * layers is generated for the evaluated mesh, which is needed to show modifier
       * results in editmode, but the actual mesh might be the base mesh.
       */
      if (layer_name) {
        r_uv_layers |= (1 << i);
        GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
        /* UV layer name. */
        SNPRINTF_UTF8(attr_name, "a%s", attr_safe_name);
        GPU_vertformat_attr_add(format, attr_name, blender::gpu::VertAttrType::SFLOAT_32_32);
        /* Active render layer name. */
        if (i == CustomData_get_render_layer(cd_ldata, CD_PROP_FLOAT2)) {
          GPU_vertformat_alias_add(format, "a");
        }
        /* Active display layer name. */
        if (i == CustomData_get_active_layer(cd_ldata, CD_PROP_FLOAT2)) {
          GPU_vertformat_alias_add(format, "au");
          /* Alias to `pos` for edit uvs. */
          GPU_vertformat_alias_add(format, "pos");
        }
        /* Stencil mask uv layer name. */
        if (i == CustomData_get_stencil_layer(cd_ldata, CD_PROP_FLOAT2)) {
          GPU_vertformat_alias_add(format, "mu");
        }
      }
    }
  }

  if (format->attr_len == 0) {
    GPU_vertformat_attr_add(format, "dummy", blender::gpu::VertAttrType::SFLOAT_32);
    return false;
  }

  return true;
}

gpu::VertBufPtr extract_uv_maps(const MeshRenderData &mr, const MeshBatchCache &cache)
{
  GPUVertFormat format = {0};

  const CustomData *cd_ldata = (mr.extract_type == MeshExtractType::BMesh) ? &mr.bm->ldata :
                                                                             &mr.mesh->corner_data;
  int v_len = mr.corners_num;
  uint32_t uv_layers = cache.cd_used.uv;
  if (!mesh_extract_uv_format_init(&format, cache, cd_ldata, mr.extract_type, uv_layers)) {
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, v_len);

  Vector<int> uv_indices;
  for (const int i : IndexRange(MAX_MTFACE)) {
    if (uv_layers & (1 << i)) {
      uv_indices.append(i);
    }
  }

  MutableSpan<float2> uv_data = vbo->data<float2>();
  threading::memory_bandwidth_bound_task(uv_data.size_in_bytes() * 2, [&]() {
    if (mr.extract_type == MeshExtractType::BMesh) {
      const BMesh &bm = *mr.bm;
      for (const int i : uv_indices.index_range()) {
        MutableSpan<float2> data = uv_data.slice(i * bm.totloop, bm.totloop);
        const int offset = CustomData_get_n_offset(cd_ldata, CD_PROP_FLOAT2, uv_indices[i]);
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
      for (const int i : uv_indices.index_range()) {
        const StringRef name = CustomData_get_layer_name(cd_ldata, CD_PROP_FLOAT2, uv_indices[i]);
        const VArray uv_map = *attributes.lookup_or_default<float2>(
            name, bke::AttrDomain::Corner, float2(0));
        array_utils::copy(uv_map, uv_data.slice(i * mr.corners_num, mr.corners_num));
      }
    }
  });
  return vbo;
}

gpu::VertBufPtr extract_uv_maps_subdiv(const DRWSubdivCache &subdiv_cache,
                                       const MeshBatchCache &cache)
{
  const Mesh *coarse_mesh = subdiv_cache.mesh;
  GPUVertFormat format = {0};

  uint v_len = subdiv_cache.num_subdiv_loops;
  uint uv_layers;
  if (!mesh_extract_uv_format_init(
          &format, cache, &coarse_mesh->corner_data, MeshExtractType::Mesh, uv_layers))
  {
    /* TODO(kevindietrich): handle this more gracefully. */
    v_len = 1;
  }

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_on_device(format, v_len));

  if (uv_layers == 0) {
    return vbo;
  }

  /* Index of the UV layer in the compact buffer. Used UV layers are stored in a single buffer. */
  int pack_layer_index = 0;
  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      const int offset = int(subdiv_cache.num_subdiv_loops) * pack_layer_index++;
      draw_subdiv_extract_uvs(subdiv_cache, vbo.get(), i, offset);
    }
  }
  return vbo;
}

}  // namespace blender::draw
