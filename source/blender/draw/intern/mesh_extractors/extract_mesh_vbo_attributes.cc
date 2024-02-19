/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"

#include "attribute_convert.hh"
#include "draw_attributes.hh"
#include "draw_subdivision.hh"
#include "extract_mesh.hh"

#include "GPU_vertex_buffer.h"

namespace blender::draw {

/* ---------------------------------------------------------------------- */
/** \name Extract Attributes
 * \{ */

static void init_vbo_for_attribute(const MeshRenderData &mr,
                                   GPUVertBuf *vbo,
                                   const DRW_AttributeRequest &request,
                                   bool build_on_device,
                                   uint32_t len)
{
  char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
  GPU_vertformat_safe_attr_name(request.attribute_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
  /* Attributes use auto-name. */
  SNPRINTF(attr_name, "a%s", attr_safe_name);

  GPUVertFormat format = init_format_for_attribute(request.cd_type, attr_name);
  GPU_vertformat_deinterleave(&format);

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

template<typename T>
static void extract_data_mesh_mapped_corner(const Span<T> attribute,
                                            const Span<int> indices,
                                            GPUVertBuf &vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  MutableSpan data(static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo)), indices.size());

  if constexpr (std::is_same_v<T, VBOType>) {
    array_utils::gather(attribute, indices, data);
  }
  else {
    threading::parallel_for(indices.index_range(), 8192, [&](const IndexRange range) {
      for (const int i : range) {
        data[i] = Converter::convert(attribute[indices[i]]);
      }
    });
  }
}

template<typename T>
static void extract_data_mesh_face(const OffsetIndices<int> faces,
                                   const Span<T> attribute,
                                   GPUVertBuf &vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  MutableSpan data(static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo)), faces.total_size());

  threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      data.slice(faces[i]).fill(Converter::convert(attribute[i]));
    }
  });
}

template<typename T>
static void extract_data_bmesh_vert(const BMesh &bm, const int cd_offset, GPUVertBuf &vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo));

  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      const T *src = static_cast<const T *>(POINTER_OFFSET(loop->v->head.data, cd_offset));
      *data = Converter::convert(*src);
      loop = loop->next;
      data++;
    }
  }
}

template<typename T>
static void extract_data_bmesh_edge(const BMesh &bm, const int cd_offset, GPUVertBuf &vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo));

  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      const T &src = *static_cast<const T *>(POINTER_OFFSET(loop->e->head.data, cd_offset));
      *data = Converter::convert(src);
      loop = loop->next;
      data++;
    }
  }
}

template<typename T>
static void extract_data_bmesh_face(const BMesh &bm, const int cd_offset, GPUVertBuf &vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo));

  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    const T &src = *static_cast<const T *>(POINTER_OFFSET(face->head.data, cd_offset));
    std::fill_n(data, face->len, Converter::convert(src));
    data += face->len;
  }
}

template<typename T>
static void extract_data_bmesh_loop(const BMesh &bm, const int cd_offset, GPUVertBuf &vbo)
{
  using Converter = AttributeConverter<T>;
  using VBOType = typename Converter::VBOType;
  VBOType *data = static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo));

  const BMFace *face;
  BMIter f_iter;
  BM_ITER_MESH (face, &f_iter, &const_cast<BMesh &>(bm), BM_FACES_OF_MESH) {
    const BMLoop *loop = BM_FACE_FIRST_LOOP(face);
    for ([[maybe_unused]] const int i : IndexRange(face->len)) {
      const T &src = *static_cast<const T *>(POINTER_OFFSET(loop->head.data, cd_offset));
      *data = Converter::convert(src);
      loop = loop->next;
      data++;
    }
  }
}

static const CustomData *get_custom_data_for_domain(const BMesh &bm, bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return &bm.vdata;
    case bke::AttrDomain::Corner:
      return &bm.ldata;
    case bke::AttrDomain::Face:
      return &bm.pdata;
    case bke::AttrDomain::Edge:
      return &bm.edata;
    default:
      return nullptr;
  }
}

static void extract_attribute(const MeshRenderData &mr,
                              const DRW_AttributeRequest &request,
                              GPUVertBuf &vbo)
{
  if (mr.extract_type == MR_EXTRACT_BMESH) {
    const CustomData &custom_data = *get_custom_data_for_domain(*mr.bm, request.domain);
    const char *name = request.attribute_name;
    const int cd_offset = CustomData_get_offset_named(&custom_data, request.cd_type, name);

    bke::attribute_math::convert_to_static_type(request.cd_type, [&](auto dummy) {
      using T = decltype(dummy);
      switch (request.domain) {
        case bke::AttrDomain::Point:
          extract_data_bmesh_vert<T>(*mr.bm, cd_offset, vbo);
          break;
        case bke::AttrDomain::Edge:
          extract_data_bmesh_edge<T>(*mr.bm, cd_offset, vbo);
          break;
        case bke::AttrDomain::Face:
          extract_data_bmesh_face<T>(*mr.bm, cd_offset, vbo);
          break;
        case bke::AttrDomain::Corner:
          extract_data_bmesh_loop<T>(*mr.bm, cd_offset, vbo);
          break;
        default:
          BLI_assert_unreachable();
      }
    });
  }
  else {
    const bke::AttributeAccessor attributes = mr.mesh->attributes();
    const StringRef name = request.attribute_name;
    const eCustomDataType data_type = request.cd_type;
    const GVArraySpan attribute = *attributes.lookup_or_default(name, request.domain, data_type);

    bke::attribute_math::convert_to_static_type(request.cd_type, [&](auto dummy) {
      using T = decltype(dummy);
      switch (request.domain) {
        case bke::AttrDomain::Point:
          extract_data_mesh_mapped_corner(attribute.typed<T>(), mr.corner_verts, vbo);
          break;
        case bke::AttrDomain::Edge:
          extract_data_mesh_mapped_corner(attribute.typed<T>(), mr.corner_edges, vbo);
          break;
        case bke::AttrDomain::Face:
          extract_data_mesh_face(mr.faces, attribute.typed<T>(), vbo);
          break;
        case bke::AttrDomain::Corner:
          vertbuf_data_extract_direct(attribute.typed<T>(), vbo);
          break;
        default:
          BLI_assert_unreachable();
      }
    });
  }
}

static void extract_attr_init(
    const MeshRenderData &mr, MeshBatchCache &cache, void *buf, void * /*tls_data*/, int index)
{
  const DRW_AttributeRequest &request = cache.attr_used.requests[index];
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  init_vbo_for_attribute(mr, vbo, request, false, uint32_t(mr.loop_len));
  extract_attribute(mr, request, *vbo);
}

static void extract_attr_init_subdiv(const DRWSubdivCache &subdiv_cache,
                                     const MeshRenderData &mr,
                                     MeshBatchCache &cache,
                                     void *buffer,
                                     void * /*tls_data*/,
                                     int index)
{
  const DRW_Attributes *attrs_used = &cache.attr_used;
  const DRW_AttributeRequest &request = attrs_used->requests[index];

  Mesh *coarse_mesh = subdiv_cache.mesh;

  /* Prepare VBO for coarse data. The compute shader only expects floats. */
  GPUVertBuf *src_data = GPU_vertbuf_calloc();
  GPUVertFormat coarse_format = draw::init_format_for_attribute(request.cd_type, "data");
  GPU_vertbuf_init_with_format_ex(src_data, &coarse_format, GPU_USAGE_STATIC);
  GPU_vertbuf_data_alloc(src_data, uint32_t(coarse_mesh->corners_num));

  extract_attribute(mr, request, *src_data);

  GPUVertBuf *dst_buffer = static_cast<GPUVertBuf *>(buffer);
  init_vbo_for_attribute(mr, dst_buffer, request, true, subdiv_cache.num_subdiv_loops);

  /* Ensure data is uploaded properly. */
  GPU_vertbuf_tag_dirty(src_data);
  bke::attribute_math::convert_to_static_type(request.cd_type, [&](auto dummy) {
    using T = decltype(dummy);
    using Converter = AttributeConverter<T>;
    draw_subdiv_interp_custom_data(subdiv_cache,
                                   src_data,
                                   dst_buffer,
                                   Converter::gpu_component_type,
                                   Converter::gpu_component_len,
                                   0);
  });

  GPU_vertbuf_discard(src_data);
}

/* Wrappers around extract_attr_init so we can pass the index of the attribute that we want to
 * extract. The overall API does not allow us to pass this in a convenient way. */
#define EXTRACT_INIT_WRAPPER(index) \
  static void extract_attr_init##index( \
      const MeshRenderData &mr, MeshBatchCache &cache, void *buf, void *tls_data) \
  { \
    extract_attr_init(mr, cache, buf, tls_data, index); \
  } \
  static void extract_attr_init_subdiv##index(const DRWSubdivCache &subdiv_cache, \
                                              const MeshRenderData &mr, \
                                              MeshBatchCache &cache, \
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

static void extract_mesh_attr_viewer_init(const MeshRenderData &mr,
                                          MeshBatchCache & /*cache*/,
                                          void *buf,
                                          void * /*tls_data*/)
{
  GPUVertBuf *vbo = static_cast<GPUVertBuf *>(buf);
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "attribute_value", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr.loop_len);
  MutableSpan<ColorGeometry4f> attr{static_cast<ColorGeometry4f *>(GPU_vertbuf_get_data(vbo)),
                                    mr.loop_len};

  const StringRefNull attr_name = ".viewer";
  const bke::AttributeAccessor attributes = mr.mesh->attributes();
  const bke::AttributeReader attribute = attributes.lookup_or_default<ColorGeometry4f>(
      attr_name, bke::AttrDomain::Corner, {1.0f, 0.0f, 1.0f, 1.0f});
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

#define CREATE_EXTRACTOR_ATTR(index) \
  create_extractor_attr<index>(extract_attr_init##index, extract_attr_init_subdiv##index)

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

const MeshExtract extract_attr_viewer = create_extractor_attr_viewer();

}  // namespace blender::draw
