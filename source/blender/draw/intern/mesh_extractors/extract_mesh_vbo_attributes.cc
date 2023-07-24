/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include <functional>

#include "BLI_color.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_mesh.hh"

#include "draw_attributes.hh"
#include "draw_subdivision.h"
#include "extract_mesh.hh"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Attributes
 * \{ */

static CustomData *get_custom_data_for_domain(const MeshRenderData *mr, eAttrDomain domain)
{
  switch (domain) {
    case ATTR_DOMAIN_POINT:
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
    case ATTR_DOMAIN_CORNER:
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
    case ATTR_DOMAIN_FACE:
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->pdata : &mr->me->pdata;
    case ATTR_DOMAIN_EDGE:
      return (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->edata : &mr->me->edata;
    default:
      return nullptr;
  }
}

/* Utility to convert from the type used in the attributes to the types for the VBO.
 * This is mostly used to promote integers and booleans to floats, as other types (float, float2,
 * etc.) directly map to available GPU types. Booleans are still converted as attributes are vec4
 * in the shader.
 */
template<typename AttributeType, typename VBOType> struct AttributeTypeConverter {
  static VBOType convert_value(AttributeType value)
  {
    if constexpr (std::is_same_v<AttributeType, VBOType>) {
      return value;
    }

    /* This should only concern bools which are converted to floats. */
    return static_cast<VBOType>(value);
  }
};

struct gpuMeshCol {
  ushort r, g, b, a;
};

template<> struct AttributeTypeConverter<MPropCol, gpuMeshCol> {
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

template<> struct AttributeTypeConverter<ColorGeometry4b, gpuMeshCol> {
  static gpuMeshCol convert_value(ColorGeometry4b value)
  {
    gpuMeshCol result;
    result.r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[value.r]);
    result.g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[value.g]);
    result.b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[value.b]);
    result.a = unit_float_to_ushort_clamp(value.a * (1.0f / 255.0f));
    return result;
  }
};

/* Return the number of component for the attribute's value type, or 0 if is it unsupported. */
static uint gpu_component_size_for_attribute_type(eCustomDataType type)
{
  switch (type) {
    case CD_PROP_BOOL:
    case CD_PROP_INT8:
    case CD_PROP_INT32:
    case CD_PROP_FLOAT:
      /* TODO(@kevindietrich): should be 1 when scalar attributes conversion is handled by us. See
       * comment #extract_attr_init. */
      return 3;
    case CD_PROP_FLOAT2:
    case CD_PROP_INT32_2D:
      return 2;
    case CD_PROP_FLOAT3:
      return 3;
    case CD_PROP_COLOR:
    case CD_PROP_BYTE_COLOR:
    case CD_PROP_QUATERNION:
      return 4;
    default:
      return 0;
  }
}

static GPUVertFetchMode get_fetch_mode_for_type(eCustomDataType type)
{
  switch (type) {
    case CD_PROP_INT8:
    case CD_PROP_INT32:
    case CD_PROP_INT32_2D:
      return GPU_FETCH_INT_TO_FLOAT;
    case CD_PROP_BYTE_COLOR:
      return GPU_FETCH_INT_TO_FLOAT_UNIT;
    default:
      return GPU_FETCH_FLOAT;
  }
}

static GPUVertCompType get_comp_type_for_type(eCustomDataType type)
{
  switch (type) {
    case CD_PROP_INT8:
    case CD_PROP_INT32_2D:
    case CD_PROP_INT32:
      return GPU_COMP_I32;
    case CD_PROP_BYTE_COLOR:
      /* This should be u8,
       * but u16 is required to store the color in linear space without precision loss */
      return GPU_COMP_U16;
    default:
      return GPU_COMP_F32;
  }
}

static void init_vbo_for_attribute(const MeshRenderData &mr,
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

  char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(request.attribute_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  SNPRINTF(attr_name, "a%s", attr_safe_name);

  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);
  GPU_vertformat_attr_add(&format, attr_name, comp_type, comp_size, fetch_mode);

  if (mr.active_color_name && STREQ(request.attribute_name, mr.active_color_name)) {
    GPU_vertformat_alias_add(&format, "ac");
  }
  if (mr.default_color_name && STREQ(request.attribute_name, mr.default_color_name)) {
    GPU_vertformat_alias_add(&format, "c");
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

  const Span<int> corner_verts = mr->corner_verts;
  const Span<int> corner_edges = mr->corner_edges;

  const AttributeType *attr_data = static_cast<const AttributeType *>(
      CustomData_get_layer_n(custom_data, request.cd_type, layer_index));

  using Converter = AttributeTypeConverter<AttributeType, VBOType>;

  switch (request.domain) {
    case ATTR_DOMAIN_POINT:
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vbo_data++) {
        *vbo_data = Converter::convert_value(attr_data[corner_verts[ml_index]]);
      }
      break;
    case ATTR_DOMAIN_CORNER:
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vbo_data++) {
        *vbo_data = Converter::convert_value(attr_data[ml_index]);
      }
      break;
    case ATTR_DOMAIN_EDGE:
      for (int ml_index = 0; ml_index < mr->loop_len; ml_index++, vbo_data++) {
        *vbo_data = Converter::convert_value(attr_data[corner_edges[ml_index]]);
      }
      break;
    case ATTR_DOMAIN_FACE:
      for (int face_index = 0; face_index < mr->face_len; face_index++) {
        const IndexRange face = mr->faces[face_index];
        const VBOType value = Converter::convert_value(attr_data[face_index]);
        for (int l = 0; l < face.size(); l++) {
          *vbo_data++ = value;
        }
      }
      break;
    default:
      BLI_assert_unreachable();
      break;
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

  const int cd_ofs = CustomData_get_n_offset(custom_data, request.cd_type, layer_index);

  using Converter = AttributeTypeConverter<AttributeType, VBOType>;

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
        BLI_assert_unreachable();
        continue;
      }
      *vbo_data = Converter::convert_value(*attr_data);
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

static void extract_attr(const MeshRenderData *mr,
                         GPUVertBuf *vbo,
                         const DRW_AttributeRequest &request)
{
  /* TODO(@kevindietrich): float3 is used for scalar attributes as the implicit conversion done by
   * OpenGL to vec4 for a scalar `s` will produce a `vec4(s, 0, 0, 1)`. However, following the
   * Blender convention, it should be `vec4(s, s, s, 1)`. This could be resolved using a similar
   * texture as for volume attribute, so we can control the conversion ourselves. */
  switch (request.cd_type) {
    case CD_PROP_BOOL:
      extract_attr_generic<bool, float3>(mr, vbo, request);
      break;
    case CD_PROP_INT8:
      extract_attr_generic<int8_t, int3>(mr, vbo, request);
      break;
    case CD_PROP_INT32:
      extract_attr_generic<int32_t, int3>(mr, vbo, request);
      break;
    case CD_PROP_INT32_2D:
      extract_attr_generic<int2>(mr, vbo, request);
      break;
    case CD_PROP_FLOAT:
      extract_attr_generic<float, float3>(mr, vbo, request);
      break;
    case CD_PROP_FLOAT2:
      extract_attr_generic<float2>(mr, vbo, request);
      break;
    case CD_PROP_FLOAT3:
      extract_attr_generic<float3>(mr, vbo, request);
      break;
    case CD_PROP_QUATERNION:
    case CD_PROP_COLOR:
      extract_attr_generic<float4>(mr, vbo, request);
      break;
    case CD_PROP_BYTE_COLOR:
      extract_attr_generic<ColorGeometry4b, gpuMeshCol>(mr, vbo, request);
      break;
    default:
      BLI_assert_unreachable();
  }
}

static void extract_attr_init(
    const MeshRenderData *mr, MeshBatchCache *cache, void *buf, void * /*tls_data*/, int index)
{
  const DRW_Attributes *attrs_used = &cache->attr_used;
  const DRW_AttributeRequest &request = attrs_used->requests[index];

  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);

  init_vbo_for_attribute(*mr, vbo, request, false, uint32_t(mr->loop_len));

  extract_attr(mr, vbo, request);
}

static void extract_attr_init_subdiv(const DRWSubdivCache *subdiv_cache,
                                     const MeshRenderData *mr,
                                     MeshBatchCache *cache,
                                     void *buffer,
                                     void * /*tls_data*/,
                                     int index)
{
  const DRW_Attributes *attrs_used = &cache->attr_used;
  const DRW_AttributeRequest &request = attrs_used->requests[index];

  Mesh *coarse_mesh = subdiv_cache->mesh;

  GPUVertCompType comp_type = get_comp_type_for_type(request.cd_type);
  GPUVertFetchMode fetch_mode = get_fetch_mode_for_type(request.cd_type);
  const uint32_t dimensions = gpu_component_size_for_attribute_type(request.cd_type);

  /* Prepare VBO for coarse data. The compute shader only expects floats. */
  GPUVertBuf *src_data = GPU_vertbuf_calloc();
  GPUVertFormat coarse_format = {0};
  GPU_vertformat_attr_add(&coarse_format, "data", comp_type, dimensions, fetch_mode);
  GPU_vertbuf_init_with_format_ex(src_data, &coarse_format, GPU_USAGE_STATIC);
  GPU_vertbuf_data_alloc(src_data, uint32_t(coarse_mesh->totloop));

  extract_attr(mr, src_data, request);

  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  init_vbo_for_attribute(*mr, dst_buffer, request, true, subdiv_cache->num_subdiv_loops);

  /* Ensure data is uploaded properly. */
  GPU_vertbuf_tag_dirty(src_data);
  draw_subdiv_interp_custom_data(
      subdiv_cache, src_data, dst_buffer, comp_type, int(dimensions), 0);

  GPU_vertbuf_discard(src_data);
}

/* Wrappers around extract_attr_init so we can pass the index of the attribute that we want to
 * extract. The overall API does not allow us to pass this in a convenient way. */
#define EXTRACT_INIT_WRAPPER(index) \
  static void extract_attr_init##index( \
      const MeshRenderData *mr, MeshBatchCache *cache, void *buf, void *tls_data) \
  { \
    extract_attr_init(mr, cache, buf, tls_data, index); \
  } \
  static void extract_attr_init_subdiv##index(const DRWSubdivCache *subdiv_cache, \
                                              const MeshRenderData *mr, \
                                              MeshBatchCache *cache, \
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

template<int Index>
constexpr MeshExtract create_extractor_attr(ExtractInitFn fn, ExtractInitSubdivFn subdiv_fn)
{
  MeshExtract extractor = {nullptr};
  extractor.init = fn;
  extractor.init_subdiv = subdiv_fn;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.attr[Index]);
  return extractor;
}

static void extract_mesh_attr_viewer_init(const MeshRenderData *mr,
                                          MeshBatchCache * /*cache*/,
                                          void *buf,
                                          void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "attribute_value", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);
  MutableSpan<ColorGeometry4f> attr{static_cast<ColorGeometry4f *>(GPU_vertbuf_get_data(vbo)),
                                    mr->loop_len};

  const StringRefNull attr_name = ".viewer";
  const bke::AttributeAccessor attributes = mr->me->attributes();
  const bke::AttributeReader attribute = attributes.lookup_or_default<ColorGeometry4f>(
      attr_name, ATTR_DOMAIN_CORNER, {1.0f, 0.0f, 1.0f, 1.0f});
  attribute.varray.materialize(attr);
}

constexpr MeshExtract create_extractor_attr_viewer()
{
  MeshExtract extractor = {nullptr};
  extractor.init = extract_mesh_attr_viewer_init;
  extractor.data_type = MR_DATA_NONE;
  extractor.data_size = 0;
  extractor.use_threading = false;
  extractor.mesh_buffer_offset = offsetof(MeshBufferList, vbo.attr_viewer);
  return extractor;
}

/** \} */

}  // namespace blender::draw

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

const MeshExtract extract_attr_viewer = blender::draw::create_extractor_attr_viewer();
