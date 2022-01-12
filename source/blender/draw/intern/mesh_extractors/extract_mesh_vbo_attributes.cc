/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include <functional>

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_float4.hh"
#include "BLI_string.h"

#include "BKE_attribute.h"

#include "draw_subdivision.h"
#include "extract_mesh.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Attributes
 * \{ */

static CustomData *get_custom_data_for_domain(const MeshRenderData *mr, AttributeDomain domain)
{
  switch (domain) {
    default: {
      return nullptr;
    }
    case ATTR_DOMAIN_POINT: {
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
    }
    case ATTR_DOMAIN_CORNER: {
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
    }
    case ATTR_DOMAIN_FACE: {
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->pdata : &mr->me->pdata;
    }
    case ATTR_DOMAIN_EDGE: {
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->edata : &mr->me->edata;
    }
  }
}

/* Utility to convert from the type used in the attributes to the types for the VBO.
 * This is mostly used to promote integers and booleans to floats, as other types (float, float2,
 * etc.) directly map to available GPU types. Booleans are still converted as attributes are vec4
 * in the shader.
 */
template<typename AttributeType, typename VBOType> struct attribute_type_converter {
  static VBOType convert_value(AttributeType value)
  {
    if constexpr (std::is_same_v<AttributeType, VBOType>) {
      return value;
    }

    /* This should only concern bools which are converted to floats. */
    return static_cast<VBOType>(value);
  }
};

/* Similar to the one in #extract_mesh_vcol_vbo.cc */
struct gpuMeshCol {
  ushort r, g, b, a;
};

template<> struct attribute_type_converter<MPropCol, gpuMeshCol> {
  static gpuMeshCol convert_value(MPropCol value)
  {
    gpuMeshCol result;
    result.r = unit_float_to_ushort_clamp(value.color[0]);
    result.g = unit_float_to_ushort_clamp(value.color[1]);
    result.b = unit_float_to_ushort_clamp(value.color[2]);
    result.a = unit_float_to_ushort_clamp(value.color[3]);
    return result;
  }
};

/* Return the number of component for the attribute's value type, or 0 if is it unsupported. */
static uint gpu_component_size_for_attribute_type(CustomDataType type)
{
  switch (type) {
    case CD_PROP_BOOL:
    case CD_PROP_INT32:
    case CD_PROP_FLOAT: {
      /* TODO(kevindietrich) : should be 1 when scalar attributes conversion is handled by us. See
       * comment #extract_attr_init. */
      return 3;
    }
    case CD_PROP_FLOAT2: {
      return 2;
    }
    case CD_PROP_FLOAT3: {
      return 3;
    }
    case CD_PROP_COLOR: {
      return 4;
    }
    default: {
      return 0;
    }
  }
}

static GPUVertFetchMode get_fetch_mode_for_type(CustomDataType type)
{
  switch (type) {
    case CD_PROP_INT32: {
      return GPU_FETCH_INT_TO_FLOAT;
    }
    case CD_PROP_COLOR: {
      return GPU_FETCH_INT_TO_FLOAT_UNIT;
    }
    default: {
      return GPU_FETCH_FLOAT;
    }
  }
}

static GPUVertCompType get_comp_type_for_type(CustomDataType type)
{
  switch (type) {
    case CD_PROP_INT32: {
      return GPU_COMP_I32;
    }
    case CD_PROP_COLOR: {
      return GPU_COMP_U16;
    }
    default: {
      return GPU_COMP_F32;
    }
  }
}

static void init_vbo_for_attribute(const MeshRenderData *mr,
                                   GPUVertBuf *vbo,
                                   const DRW_AttributeRequest &request,
                                   bool build_on_device,
                                   uint32_t len)
{
  GPUVertCompType comp_type = get_comp_type_for_type(request.cd_type);
  GPUVertFetchMode fetch_mode = get_fetch_mode_for_type(request.cd_type);
  const uint comp_size = gpu_component_size_for_attribute_type(request.cd_type);
  /* We should not be here if the attribute type is not supported. */
  BLI_assert(comp_size != 0);

  const CustomData *custom_data = get_custom_data_for_domain(mr, request.domain);
  char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  const char *layer_name = CustomData_get_layer_name(
      custom_data, request.cd_type, request.layer_index);
  GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);

  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);
  GPU_vertformat_attr_add(&format, attr_name, comp_type, comp_size, fetch_mode);

  /* Ensure Sculpt Vertex Colors are properly aliased. */
  if (request.cd_type == CD_PROP_COLOR && request.domain == ATTR_DOMAIN_POINT) {
    CustomData *cd_vdata = get_custom_data_for_domain(mr, ATTR_DOMAIN_POINT);
    if (request.layer_index == CustomData_get_render_layer(cd_vdata, CD_PROP_COLOR)) {
      GPU_vertformat_alias_add(&format, "c");
    }
    if (request.layer_index == CustomData_get_active_layer(cd_vdata, CD_PROP_COLOR)) {
      GPU_vertformat_alias_add(&format, "ac");
    }
  }

  if (build_on_device) {
    GPU_vertbuf_init_build_on_device(vbo, &format, len);
  }
  else {
    GPU_vertbuf_init_with_format(vbo, &format);
    GPU_vertbuf_data_alloc(vbo, len);
  }
}

template<typename AttributeType, typename VBOType>
static void fill_vertbuf_with_attribute(const MeshRenderData *mr,
                                        VBOType *vbo_data,
                                        const DRW_AttributeRequest &request)
{
  const CustomData *custom_data = get_custom_data_for_domain(mr, request.domain);
  BLI_assert(custom_data);
  const int layer_index = request.layer_index;

  const MPoly *mpoly = mr->mpoly;
  const MLoop *mloop = mr->mloop;

  const AttributeType *attr_data = static_cast<AttributeType *>(
      CustomData_get_layer_n(custom_data, request.cd_type, layer_index));

  using converter = attribute_type_converter<AttributeType, VBOType>;

  switch (request.domain) {
    default: {
      BLI_assert(false);
      break;
    }
    case ATTR_DOMAIN_POINT: {
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vbo_data++, mloop++) {
        *vbo_data = converter::convert_value(attr_data[mloop->v]);
      }
      break;
    }
    case ATTR_DOMAIN_CORNER: {
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vbo_data++) {
        *vbo_data = converter::convert_value(attr_data[ml_index]);
      }
      break;
    }
    case ATTR_DOMAIN_EDGE: {
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vbo_data++, mloop++) {
        *vbo_data = converter::convert_value(attr_data[mloop->e]);
      }
      break;
    }
    case ATTR_DOMAIN_FACE: {
      for (int mp_index = 0; mp_index < mr->poly_len; mp_index++) {
        const MPoly &poly = mpoly[mp_index];
        const VBOType value = converter::convert_value(attr_data[mp_index]);
        for (int l = 0; l < poly.totloop; l++) {
          *vbo_data++ = value;
        }
      }
      break;
    }
  }
}

template<typename AttributeType, typename VBOType>
static void fill_vertbuf_with_attribute_bm(const MeshRenderData *mr,
                                           VBOType *&vbo_data,
                                           const DRW_AttributeRequest &request)
{
  const CustomData *custom_data = get_custom_data_for_domain(mr, request.domain);
  BLI_assert(custom_data);
  const int layer_index = request.layer_index;

  int cd_ofs = CustomData_get_n_offset(custom_data, request.cd_type, layer_index);

  using converter = attribute_type_converter<AttributeType, VBOType>;

  BMIter f_iter;
  BMFace *efa;
  BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
    do {
      const AttributeType *attr_data = nullptr;
      if (request.domain == ATTR_DOMAIN_POINT) {
        attr_data = static_cast<const AttributeType *>(BM_ELEM_CD_GET_VOID_P(l_iter->v, cd_ofs));
      }
      else if (request.domain == ATTR_DOMAIN_CORNER) {
        attr_data = static_cast<const AttributeType *>(BM_ELEM_CD_GET_VOID_P(l_iter, cd_ofs));
      }
      else if (request.domain == ATTR_DOMAIN_FACE) {
        attr_data = static_cast<const AttributeType *>(BM_ELEM_CD_GET_VOID_P(efa, cd_ofs));
      }
      else if (request.domain == ATTR_DOMAIN_EDGE) {
        attr_data = static_cast<const AttributeType *>(BM_ELEM_CD_GET_VOID_P(l_iter->e, cd_ofs));
      }
      else {
        BLI_assert(false);
        continue;
      }
      *vbo_data = converter::convert_value(*attr_data);
      vbo_data++;
    } while ((l_iter = l_iter->next) != l_first);
  }
}

template<typename AttributeType, typename VBOType = AttributeType>
static void extract_attr_generic(const MeshRenderData *mr,
                                 GPUVertBuf *vbo,
                                 const DRW_AttributeRequest &request)
{
  VBOType *vbo_data = static_cast<VBOType *>(GPU_vertbuf_get_data(vbo));

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    fill_vertbuf_with_attribute_bm<AttributeType>(mr, vbo_data, request);
  }
  else {
    fill_vertbuf_with_attribute<AttributeType>(mr, vbo_data, request);
  }
}

static void extract_attr_init(const MeshRenderData *mr,
                              struct MeshBatchCache *cache,
                              void *buf,
                              void *UNUSED(tls_data),
                              int index)
{
  const DRW_MeshAttributes *attrs_used = &cache->attr_used;
  const DRW_AttributeRequest &request = attrs_used->requests[index];

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);

  init_vbo_for_attribute(mr, vbo, request, false, static_cast<uint32_t>(mr->loop_len));

  /* TODO(kevindietrich) : float3 is used for scalar attributes as the implicit conversion done by
   * OpenGL to vec4 for a scalar `s` will produce a `vec4(s, 0, 0, 1)`. However, following the
   * Blender convention, it should be `vec4(s, s, s, 1)`. This could be resolved using a similar
   * texture as for volume attribute, so we can control the conversion ourselves. */
  switch (request.cd_type) {
    case CD_PROP_BOOL: {
      extract_attr_generic<bool, float3>(mr, vbo, request);
      break;
    }
    case CD_PROP_INT32: {
      extract_attr_generic<int32_t, float3>(mr, vbo, request);
      break;
    }
    case CD_PROP_FLOAT: {
      extract_attr_generic<float, float3>(mr, vbo, request);
      break;
    }
    case CD_PROP_FLOAT2: {
      extract_attr_generic<float2>(mr, vbo, request);
      break;
    }
    case CD_PROP_FLOAT3: {
      extract_attr_generic<float3>(mr, vbo, request);
      break;
    }
    case CD_PROP_COLOR: {
      extract_attr_generic<MPropCol, gpuMeshCol>(mr, vbo, request);
      break;
    }
    default: {
      BLI_assert(false);
    }
  }
}

static void extract_attr_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                     const MeshRenderData *mr,
                                     MeshBatchCache *cache,
                                     void *buffer,
                                     void *UNUSED(tls_data),
                                     int index)
{
  const DRW_MeshAttributes *attrs_used = &cache->attr_used;
  const DRW_AttributeRequest &request = attrs_used->requests[index];

  Mesh *coarse_mesh = subdiv_cache->mesh;

  const uint32_t dimensions = gpu_component_size_for_attribute_type(request.cd_type);

  /* Prepare VBO for coarse data. The compute shader only expects floats. */
  GPUVertBuf *src_data = GPU_vertbuf_calloc();
  static GPUVertFormat coarse_format = {0};
  GPU_vertformat_attr_add(&coarse_format, "data", GPU_COMP_F32, dimensions, GPU_FETCH_FLOAT);
  GPU_vertbuf_init_with_format_ex(src_data, &coarse_format, GPU_USAGE_STATIC);
  GPU_vertbuf_data_alloc(src_data, static_cast<uint32_t>(coarse_mesh->totloop));

  switch (request.cd_type) {
    case CD_PROP_BOOL: {
      extract_attr_generic<bool, float3>(mr, src_data, request);
      break;
    }
    case CD_PROP_INT32: {
      extract_attr_generic<int32_t, float3>(mr, src_data, request);
      break;
    }
    case CD_PROP_FLOAT: {
      extract_attr_generic<float, float3>(mr, src_data, request);
      break;
    }
    case CD_PROP_FLOAT2: {
      extract_attr_generic<float2>(mr, src_data, request);
      break;
    }
    case CD_PROP_FLOAT3: {
      extract_attr_generic<float3>(mr, src_data, request);
      break;
    }
    case CD_PROP_COLOR: {
      extract_attr_generic<MPropCol, gpuMeshCol>(mr, src_data, request);
      break;
    }
    default: {
      BLI_assert(false);
    }
  }

  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  init_vbo_for_attribute(mr, dst_buffer, request, true, subdiv_cache->num_subdiv_loops);

  /* Ensure data is uploaded properly. */
  GPU_vertbuf_tag_dirty(src_data);
  draw_subdiv_interp_custom_data(
      subdiv_cache, src_data, dst_buffer, static_cast<int>(dimensions), 0);

  GPU_vertbuf_discard(src_data);
}

/* Wrappers around extract_attr_init so we can pass the index of the attribute that we want to
 * extract. The overall API does not allow us to pass this in a convenient way. */
#define EXTRACT_INIT_WRAPPER(index) \
  static void extract_attr_init##index( \
      const MeshRenderData *mr, struct MeshBatchCache *cache, void *buf, void *tls_data) \
  { \
    extract_attr_init(mr, cache, buf, tls_data, index); \
  } \
  static void extract_attr_init_subdiv##index(const DRWSubdivCache *subdiv_cache, \
                                              const MeshRenderData *mr, \
                                              struct MeshBatchCache *cache, \
                                              void *buf, \
                                              void *tls_data) \
  { \
    extract_attr_init_subdiv(subdiv_cache, mr, cache, buf, tls_data, index); \
  }

EXTRACT_INIT_WRAPPER(0)
EXTRACT_INIT_WRAPPER(1)
EXTRACT_INIT_WRAPPER(2)
EXTRACT_INIT_WRAPPER(3)
EXTRACT_INIT_WRAPPER(4)
EXTRACT_INIT_WRAPPER(5)
EXTRACT_INIT_WRAPPER(6)
EXTRACT_INIT_WRAPPER(7)
EXTRACT_INIT_WRAPPER(8)
EXTRACT_INIT_WRAPPER(9)
EXTRACT_INIT_WRAPPER(10)
EXTRACT_INIT_WRAPPER(11)
EXTRACT_INIT_WRAPPER(12)
EXTRACT_INIT_WRAPPER(13)
EXTRACT_INIT_WRAPPER(14)

template<int index>
constexpr MeshExtract create_extractor_attr(ExtractInitFn fn, ExtractInitSubdivFn subdiv_fn)
{
  MeshExtract extractor = {nullptr};
  extractor.init = fn;
  extractor.init_subdiv = subdiv_fn;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.attr[index]);
  return extractor;
}

/** \} */

}  // namespace blender::draw

extern "C" {
#define CREATE_EXTRACTOR_ATTR(index) \
  blender::draw::create_extractor_attr<index>(blender::draw::extract_attr_init##index, \
                                              blender::draw::extract_attr_init_subdiv##index)

const MeshExtract extract_attr[GPU_MAX_ATTR] = {
    CREATE_EXTRACTOR_ATTR(0),
    CREATE_EXTRACTOR_ATTR(1),
    CREATE_EXTRACTOR_ATTR(2),
    CREATE_EXTRACTOR_ATTR(3),
    CREATE_EXTRACTOR_ATTR(4),
    CREATE_EXTRACTOR_ATTR(5),
    CREATE_EXTRACTOR_ATTR(6),
    CREATE_EXTRACTOR_ATTR(7),
    CREATE_EXTRACTOR_ATTR(8),
    CREATE_EXTRACTOR_ATTR(9),
    CREATE_EXTRACTOR_ATTR(10),
    CREATE_EXTRACTOR_ATTR(11),
    CREATE_EXTRACTOR_ATTR(12),
    CREATE_EXTRACTOR_ATTR(13),
    CREATE_EXTRACTOR_ATTR(14),
};
}
