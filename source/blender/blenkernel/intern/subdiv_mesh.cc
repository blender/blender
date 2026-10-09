/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_listbase_iterator.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_key.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_deform.hh"
#include "BKE_subdiv_eval.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_mesh_layout.hh"

namespace blender::bke::subdiv {

/* -------------------------------------------------------------------- */
/** \name Subdivision Context
 * \{ */

struct SubdivMeshContext {
  const ToMeshSettings *settings;
  const Mesh *coarse_mesh;
  Span<float3> coarse_positions;
  Span<int2> coarse_edges;
  OffsetIndices<int> coarse_faces;
  Span<int> coarse_corner_verts;

  Subdiv *subdiv;
  Mesh *subdiv_mesh;
  MutableSpan<float3> subdiv_positions;
  MutableSpan<int2> subdiv_edges;
  MutableSpan<int> subdiv_corner_verts;
  MutableSpan<int> subdiv_corner_edges;

  /* Generic attributes on each domain. */
  Vector<GVArraySpan> coarse_vert_attrs;
  Vector<GVArraySpan> coarse_edge_attrs;
  Vector<GVArraySpan> coarse_face_attrs;
  Vector<GVArraySpan> coarse_corner_attrs;

  /* Spans referencing the attribute arrays above, to avoid leaking #GVArraySpan elsewhere. */
  Vector<GSpan> coarse_vert_attr_spans;
  Vector<GSpan> coarse_edge_attr_spans;
  Vector<GSpan> coarse_face_attr_spans;
  Vector<GSpan> coarse_corner_attr_spans;

  /* Attribute writers for generic attributes on the result. */
  Vector<GSpanAttributeWriter> subdiv_vert_attrs;
  Vector<GSpanAttributeWriter> subdiv_edge_attrs;
  Vector<GSpanAttributeWriter> subdiv_face_attrs;
  Vector<GSpanAttributeWriter> subdiv_corner_attrs;

  /* Spans referencing the writers above, for simpler usage. */
  Vector<GMutableSpan> subdiv_vert_attr_spans;
  Vector<GMutableSpan> subdiv_edge_attr_spans;
  Vector<GMutableSpan> subdiv_face_attr_spans;
  Vector<GMutableSpan> subdiv_corner_attr_spans;

  /* Original index layers on base and result meshes. */
  Span<int> coarse_vert_origindex;
  Span<int> coarse_edge_origindex;
  Span<int> coarse_face_origindex;
  MutableSpan<int> subdiv_vert_origindex;
  MutableSpan<int> subdiv_edge_origindex;
  MutableSpan<int> subdiv_face_origindex;

  /* UV maps on the result mesh. Base mesh data was prepared in #eval_begin_from_mesh. */
  Vector<bke::SpanAttributeWriter<float2>> uv_maps;

  /* CD_ORCO/CD_CLOTH_ORCO interpolation. Base mesh data was prepared in #eval_begin_from_mesh. */
  float (*orco)[3];
  float (*cloth_orco)[3];

  /* Base and result mesh vertex group data. */
  Span<MDeformVert> coarse_dverts;
  MutableSpan<MDeformVert> subdiv_dverts;

  /* Base and result mesh data for interpolating custom normals. */
  Span<float3> coarse_CD_NORMAL;
  MutableSpan<float3> subdiv_CD_NORMAL;

  /* Base and result mesh data for interpolating particle system data. */
  Span<float2> coarse_CD_ORIGSPACE_MLOOP;
  MutableSpan<float2> subdiv_CD_ORIGSPACE_MLOOP;

  bool have_displacement;

  /* Write optimal display edge tags into a boolean array rather than the final bit vector
   * to avoid race conditions when setting bits. */
  Array<bool> subdiv_display_edges;

  /* Write face dot tags into a boolean array rather than the final bit vector to avoid race
   * conditions when setting bits. */
  Array<bool> subdiv_face_dot_tags;

  /* Lazily initialize a map from vertices to connected edges. */
  Array<int> vert_to_edge_offsets;
  Array<int> vert_to_edge_indices;
  GroupedSpan<int> vert_to_edge_map;
};

static void subdiv_mesh_ctx_cache_uv_layers(SubdivMeshContext *ctx)
{
  const Mesh &coarse_mesh = *ctx->coarse_mesh;
  Mesh *subdiv_mesh = ctx->subdiv_mesh;
  bke::MutableAttributeAccessor attributes = subdiv_mesh->attributes_for_write();
  for (const StringRef name : coarse_mesh.uv_map_names()) {
    SpanAttributeWriter uv_map = attributes.lookup_or_add_for_write_only_span<float2>(
        name, AttrDomain::Corner);
    if (!uv_map) {
      /* Attribute lookup can fail because of name collisions with vertex groups. */
      continue;
    }
    ctx->uv_maps.append(std::move(uv_map));
  }
}

static void subdiv_mesh_ctx_cache_custom_data_layers(SubdivMeshContext *ctx)
{
  const Mesh &coarse_mesh = *ctx->coarse_mesh;
  Mesh *subdiv_mesh = ctx->subdiv_mesh;
  ctx->subdiv_positions = subdiv_mesh->vert_positions_for_write();
  ctx->subdiv_edges = subdiv_mesh->edges_for_write();
  ctx->subdiv_corner_verts = subdiv_mesh->corner_verts_for_write();
  ctx->subdiv_corner_edges = subdiv_mesh->corner_edges_for_write();

  ctx->coarse_dverts = coarse_mesh.deform_verts();
  if (!ctx->coarse_dverts.is_empty()) {
    ctx->subdiv_dverts = subdiv_mesh->deform_verts_for_write();
  }
  if (const auto *src = static_cast<const float3 *>(
          CustomData_get_layer(&coarse_mesh.corner_data, CD_NORMAL)))
  {
    ctx->coarse_CD_NORMAL = {src, coarse_mesh.corners_num};
    ctx->subdiv_CD_NORMAL = {
        static_cast<float3 *>(CustomData_add_layer(
            &subdiv_mesh->corner_data, CD_NORMAL, CD_CONSTRUCT, subdiv_mesh->corners_num)),
        subdiv_mesh->corners_num};
  }
  if (const auto *src = static_cast<const float2 *>(
          CustomData_get_layer(&coarse_mesh.corner_data, CD_ORIGSPACE_MLOOP)))
  {
    ctx->coarse_CD_ORIGSPACE_MLOOP = {src, coarse_mesh.corners_num};
    ctx->subdiv_CD_ORIGSPACE_MLOOP = {
        static_cast<float2 *>(CustomData_add_layer(&subdiv_mesh->corner_data,
                                                   CD_ORIGSPACE_MLOOP,
                                                   CD_CONSTRUCT,
                                                   subdiv_mesh->corners_num)),
        subdiv_mesh->corners_num};
  }
  if (const auto *src = static_cast<const int *>(
          CustomData_get_layer(&coarse_mesh.vert_data, CD_ORIGINDEX)))
  {
    ctx->coarse_vert_origindex = {src, coarse_mesh.verts_num};
    ctx->subdiv_vert_origindex = {
        static_cast<int *>(CustomData_add_layer(
            &subdiv_mesh->vert_data, CD_ORIGINDEX, CD_CONSTRUCT, subdiv_mesh->verts_num)),
        subdiv_mesh->verts_num};
  }
  if (const auto *src = static_cast<const int *>(
          CustomData_get_layer(&coarse_mesh.edge_data, CD_ORIGINDEX)))
  {
    ctx->coarse_edge_origindex = {src, coarse_mesh.edges_num};
    ctx->subdiv_edge_origindex = {
        static_cast<int *>(CustomData_add_layer(
            &subdiv_mesh->edge_data, CD_ORIGINDEX, CD_CONSTRUCT, subdiv_mesh->edges_num)),
        subdiv_mesh->edges_num};
  }
  if (const auto *src = static_cast<const int *>(
          CustomData_get_layer(&coarse_mesh.face_data, CD_ORIGINDEX)))
  {
    ctx->coarse_face_origindex = {src, coarse_mesh.faces_num};
    ctx->subdiv_face_origindex = {
        static_cast<int *>(CustomData_add_layer(
            &subdiv_mesh->face_data, CD_ORIGINDEX, CD_CONSTRUCT, subdiv_mesh->faces_num)),
        subdiv_mesh->faces_num};
  }
  /* UV layers interpolation. */
  subdiv_mesh_ctx_cache_uv_layers(ctx);
  /* Orco interpolation. */
  if (CustomData_has_layer(&coarse_mesh.vert_data, CD_ORCO)) {
    ctx->orco = static_cast<float (*)[3]>(CustomData_add_layer(
        &subdiv_mesh->vert_data, CD_ORCO, CD_CONSTRUCT, subdiv_mesh->verts_num));
  }
  if (CustomData_has_layer(&coarse_mesh.vert_data, CD_CLOTH_ORCO)) {
    ctx->cloth_orco = static_cast<float (*)[3]>(CustomData_add_layer(
        &subdiv_mesh->vert_data, CD_CLOTH_ORCO, CD_CONSTRUCT, subdiv_mesh->verts_num));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop custom data copy helpers
 * \{ */

struct LoopsOfPtex {
  /* First loop of the ptex, starts at ptex (0, 0) and goes in u direction. */
  int first_loop;
  /* Last loop of the ptex, starts at ptex (0, 0) and goes in v direction. */
  int last_loop;
  /* For quad coarse faces only. */
  int second_loop;
  int third_loop;
};

static void loops_of_ptex_get(LoopsOfPtex *loops_of_ptex,
                              const IndexRange coarse_face,
                              const int ptex_of_face_index)
{
  const int first_ptex_loop_index = coarse_face.start() + ptex_of_face_index;
  /* Loop which look in the (opposite) V direction of the current
   * ptex face.
   *
   * TODO(sergey): Get rid of using module on every iteration. */
  const int last_ptex_loop_index = coarse_face.start() +
                                   (ptex_of_face_index + coarse_face.size() - 1) %
                                       coarse_face.size();
  loops_of_ptex->first_loop = first_ptex_loop_index;
  loops_of_ptex->last_loop = last_ptex_loop_index;
  if (coarse_face.size() == 4) {
    loops_of_ptex->second_loop = loops_of_ptex->first_loop + 1;
    loops_of_ptex->third_loop = loops_of_ptex->first_loop + 2;
  }
  else {
    loops_of_ptex->second_loop = -1;
    loops_of_ptex->third_loop = -1;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex custom data interpolation helpers
 * \{ */

static float3 mix_normals(const Span<float3> src,
                          const Span<int> src_indices,
                          const Span<float> weights)
{
  float3 result(0);
  for (const int i : src_indices.index_range()) {
    result += src[src_indices[i]] * weights[i];
  }
  return math::normalize(result);
}

static float3 mix_normals(const Span<float3> src,
                          const std::array<int, 4> &src_indices,
                          const float4 &weights)
{
  float3 result(0);
  for (const int i : IndexRange(src_indices.size())) {
    result += src[src_indices[i]] * weights[i];
  }
  return math::normalize(result);
}

static float3 mix_normals(const Span<float3> src,
                          const std::array<int, 2> &src_indices,
                          const float factor)
{
  return math::normalize(math::interpolate(src[src_indices[0]], src[src_indices[1]], factor));
}

static void mix_attrs(const Span<GSpan> src,
                      const std::array<int, 2> &src_indices,
                      const float factor,
                      const int dst_index,
                      const Span<GMutableSpan> dst)
{
  for (const int attr : src.index_range()) {
    attribute_math::to_static_type(src[attr].type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        const Span<T> src_attr = src[attr].typed<T>();
        MutableSpan<T> dst_attr = dst[attr].typed<T>();
        if constexpr (std::is_same_v<T, bool>) {
          dst_attr[dst_index] = bke::attribute_math::mix_indices(
              src_attr, src_indices, {1.0f - factor, factor});
        }
        else {
          dst_attr[dst_index] = attribute_math::mix2(
              factor, src_attr[src_indices[0]], src_attr[src_indices[1]]);
        }
      }
    });
  }
}

static void mix_attrs(const Span<GSpan> src,
                      const std::array<int, 4> &src_indices,
                      const float4 &weights,
                      const int dst_index,
                      const Span<GMutableSpan> dst)
{
  for (const int attr : src.index_range()) {
    attribute_math::to_static_type(src[attr].type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        const Span<T> src_attr = src[attr].typed<T>();
        MutableSpan<T> dst_attr = dst[attr].typed<T>();
        if constexpr (std::is_same_v<T, bool>) {
          dst_attr[dst_index] = bke::attribute_math::mix_indices(
              src_attr, src_indices, Span(&weights.x, 4));
        }
        else {
          dst_attr[dst_index] = attribute_math::mix4(weights,
                                                     src_attr[src_indices[0]],
                                                     src_attr[src_indices[1]],
                                                     src_attr[src_indices[2]],
                                                     src_attr[src_indices[3]]);
        }
      }
    });
  }
}

static void mix_attrs(const Span<GSpan> src,
                      const Span<int> src_indices,
                      const Span<float> weights,
                      const int dst_index,
                      const Span<GMutableSpan> dst)
{
  for (const int attr : src.index_range()) {
    attribute_math::to_static_type(src[attr].type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        const Span<T> src_attr = src[attr].typed<T>();
        MutableSpan<T> dst_attr = dst[attr].typed<T>();
        dst_attr[dst_index] = bke::attribute_math::mix_indices(src_attr, src_indices, weights);
      }
    });
  }
}

static void copy_attrs(const Span<GSpan> src,
                       const int src_index,
                       const int dst_index,
                       const Span<GMutableSpan> dst)
{
  for (const int attr : src.index_range()) {
    const CPPType &type = src[attr].type();
    type.copy_construct(src[attr][src_index], dst[attr][dst_index]);
  }
}

/* TODO(sergey): Somehow de-duplicate with loops storage, without too much
 * exception cases all over the code. */

struct VerticesForInterpolation {
  /* This field points to a vertex data which is to be used for interpolation. The idea is to avoid
   * unnecessary copies for regular faces, where we can simply use base vertices. */
  Span<GSpan> vert_data;
  Span<MDeformVert> dverts_data;
  /* Vertices data calculated for ptex corners. There are always 4 elements
   * in these arrays, aligned the following way:
   *
   *   index 0 -> uv (0, 0)
   *   index 1 -> uv (0, 1)
   *   index 2 -> uv (1, 1)
   *   index 3 -> uv (1, 0)
   *
   * Is allocated for non-regular faces (triangles and n-gons). */
  AlignedBuffer<1024, 64> buffer;
  LinearAllocator<> allocator;
  Array<GSpan> storage_spans;
  std::array<MDeformVert, 4> dverts_storage = {};
  /* Indices within vert_data to interpolate for. The indices are aligned
   * with uv coordinates in a similar way as indices in storage_spans. */
  std::array<int, 4> vert_indices;

  MDeformWeightSet dvert_mix_buffer;

  VerticesForInterpolation(const SubdivMeshContext &ctx)
      : storage_spans(ctx.coarse_vert_attr_spans.size())
  {
    this->allocator.provide_buffer(this->buffer);
    for (const int i : ctx.coarse_vert_attr_spans.index_range()) {
      const CPPType &type = ctx.coarse_vert_attr_spans[i].type();
      void *data = this->allocator.allocate_array(type, 4);
      this->storage_spans[i] = {type, data, 4};
    }
  }
  ~VerticesForInterpolation()
  {
    BKE_defvert_array_free_elems(this->dverts_storage.data(), this->dverts_storage.size());
  }
};

static void vert_interpolation_from_face(const SubdivMeshContext *ctx,
                                         VerticesForInterpolation *vert_interpolation,
                                         const IndexRange coarse_face)
{
  if (coarse_face.size() == 4) {
    vert_interpolation->vert_data = ctx->coarse_vert_attr_spans;
    vert_interpolation->dverts_data = ctx->coarse_dverts;
    vert_interpolation->vert_indices[0] = ctx->coarse_corner_verts[coarse_face.start() + 0];
    vert_interpolation->vert_indices[1] = ctx->coarse_corner_verts[coarse_face.start() + 1];
    vert_interpolation->vert_indices[2] = ctx->coarse_corner_verts[coarse_face.start() + 2];
    vert_interpolation->vert_indices[3] = ctx->coarse_corner_verts[coarse_face.start() + 3];
  }
  else {
    vert_interpolation->vert_data = vert_interpolation->storage_spans;
    vert_interpolation->dverts_data = vert_interpolation->dverts_storage;
    vert_interpolation->vert_indices[0] = 0;
    vert_interpolation->vert_indices[1] = 1;
    vert_interpolation->vert_indices[2] = 2;
    vert_interpolation->vert_indices[3] = 3;
    /* Interpolate center of face right away, it stays unchanged for all
     * ptex faces. */
    const float weight = 1.0f / float(coarse_face.size());
    Array<float, 32> weights(coarse_face.size());
    Span<int> indices = ctx->coarse_corner_verts.slice(coarse_face);
    for (int i = 0; i < coarse_face.size(); i++) {
      weights[i] = weight;
    }
    mix_attrs(ctx->coarse_vert_attr_spans,
              indices,
              weights.as_span(),
              2,
              vert_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_dverts.is_empty()) {
      BKE_defvert_array_free_elems(&vert_interpolation->dverts_storage[2], 1);
      vert_interpolation->dverts_storage[2] = mix_deform_verts(
          ctx->coarse_dverts, indices, weights, vert_interpolation->dvert_mix_buffer);
    }
  }
}

static void vert_interpolation_from_corner(const SubdivMeshContext *ctx,
                                           VerticesForInterpolation *vert_interpolation,
                                           const IndexRange coarse_face,
                                           const int corner)
{
  if (coarse_face.size() == 4) {
    /* Nothing to do, all indices and data is already assigned. */
  }
  else {
    LoopsOfPtex loops_of_ptex;
    loops_of_ptex_get(&loops_of_ptex, coarse_face, corner);
    /* PTEX face corner corresponds to a face loop with same index. */
    const int vert = ctx->coarse_corner_verts[coarse_face.start() + corner];
    copy_attrs(ctx->coarse_vert_attr_spans,
               vert,
               0,
               vert_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_dverts.is_empty()) {
      BKE_defvert_array_free_elems(vert_interpolation->dverts_storage.data(), 1);
      BKE_defvert_array_copy(
          vert_interpolation->dverts_storage.data(), &ctx->coarse_dverts[vert], 1);
    }
    /* Interpolate remaining ptex face corners, which hits loops
     * middle points.
     *
     * TODO(sergey): Re-use one of interpolation results from previous
     * iteration. */
    const int first_loop_index = loops_of_ptex.first_loop;
    const int last_loop_index = loops_of_ptex.last_loop;
    const std::array<int, 2> first_indices{
        ctx->coarse_corner_verts[first_loop_index],
        ctx->coarse_corner_verts[coarse_face.start() +
                                 (first_loop_index - coarse_face.start() + 1) %
                                     coarse_face.size()]};
    const std::array<int, 2> last_indices{ctx->coarse_corner_verts[first_loop_index],
                                          ctx->coarse_corner_verts[last_loop_index]};
    mix_attrs(ctx->coarse_vert_attr_spans,
              first_indices,
              0.5f,
              1,
              vert_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_dverts.is_empty()) {
      BKE_defvert_array_free_elems(&vert_interpolation->dverts_storage[1], 1);
      vert_interpolation->dverts_storage[1] = mix_deform_verts(
          ctx->coarse_dverts, first_indices, {0.5f, 0.5f}, vert_interpolation->dvert_mix_buffer);
    }
    mix_attrs(ctx->coarse_vert_attr_spans,
              last_indices,
              0.5f,
              3,
              vert_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_dverts.is_empty()) {
      BKE_defvert_array_free_elems(&vert_interpolation->dverts_storage[3], 1);
      vert_interpolation->dverts_storage[3] = mix_deform_verts(
          ctx->coarse_dverts, last_indices, {0.5f, 0.5f}, vert_interpolation->dvert_mix_buffer);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loop custom data interpolation helpers
 * \{ */

struct LoopsForInterpolation {
  /* This field points to a loop data which is to be used for interpolation. The idea is to avoid
   * unnecessary copies for regular faces, where we can simply interpolate base corners. */
  Span<GSpan> corner_data;
  Span<float3> CD_NORMAL_data;
  Span<float2> CD_ORIGSPACE_MLOOP_data;
  /* Loops data calculated for ptex corners. There are always 4 elements
   * in these arrays, aligned the following way:
   *
   *   index 0 -> uv (0, 0)
   *   index 1 -> uv (0, 1)
   *   index 2 -> uv (1, 1)
   *   index 3 -> uv (1, 0)
   *
   * Is allocated for non-regular faces (triangles and n-gons). */
  AlignedBuffer<1024, 64> buffer;
  LinearAllocator<> allocator;
  Array<GSpan> storage_spans;
  std::array<float3, 4> CD_NORMAL_storage;
  std::array<float2, 4> CD_ORIGSPACE_MLOOP_storage;

  /* Indices within corner_data to interpolate for. The indices are aligned with
   * uv coordinates in a similar way as indices in storage_spans. */
  std::array<int, 4> loop_indices;

  LoopsForInterpolation(const SubdivMeshContext &ctx)
      : storage_spans(ctx.coarse_corner_attr_spans.size())
  {
    this->allocator.provide_buffer(this->buffer);
    for (const int i : ctx.coarse_corner_attr_spans.index_range()) {
      const CPPType &type = ctx.coarse_corner_attr_spans[i].type();
      void *data = this->allocator.allocate_array(type, 4);
      this->storage_spans[i] = {type, data, 4};
    }
  }
};

static void loop_interpolation_from_face(const SubdivMeshContext *ctx,
                                         LoopsForInterpolation *loop_interpolation,
                                         const IndexRange coarse_face)
{
  if (coarse_face.size() == 4) {
    loop_interpolation->corner_data = ctx->coarse_corner_attr_spans;
    loop_interpolation->CD_NORMAL_data = ctx->coarse_CD_NORMAL;
    loop_interpolation->CD_ORIGSPACE_MLOOP_data = ctx->coarse_CD_ORIGSPACE_MLOOP;
    loop_interpolation->loop_indices[0] = coarse_face.start() + 0;
    loop_interpolation->loop_indices[1] = coarse_face.start() + 1;
    loop_interpolation->loop_indices[2] = coarse_face.start() + 2;
    loop_interpolation->loop_indices[3] = coarse_face.start() + 3;
  }
  else {
    loop_interpolation->corner_data = loop_interpolation->storage_spans;
    loop_interpolation->CD_NORMAL_data = loop_interpolation->CD_NORMAL_storage;
    loop_interpolation->CD_ORIGSPACE_MLOOP_data = loop_interpolation->CD_ORIGSPACE_MLOOP_storage;
    loop_interpolation->loop_indices[0] = 0;
    loop_interpolation->loop_indices[1] = 1;
    loop_interpolation->loop_indices[2] = 2;
    loop_interpolation->loop_indices[3] = 3;
    /* Interpolate center of face right away, it stays unchanged for all
     * ptex faces. */
    const float weight = 1.0f / float(coarse_face.size());
    Array<float, 32> weights(coarse_face.size());
    Array<int, 32> indices(coarse_face.size());
    for (int i = 0; i < coarse_face.size(); i++) {
      weights[i] = weight;
      indices[i] = coarse_face.start() + i;
    }
    mix_attrs(ctx->coarse_corner_attr_spans,
              indices,
              weights.as_span(),
              2,
              loop_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_CD_NORMAL.is_empty()) {
      loop_interpolation->CD_NORMAL_storage[2] = mix_normals(
          ctx->coarse_CD_NORMAL, indices, weights.as_span());
    }
    if (!ctx->coarse_CD_ORIGSPACE_MLOOP.is_empty()) {
      loop_interpolation->CD_ORIGSPACE_MLOOP_storage[2] = bke::attribute_math::mix_indices(
          ctx->coarse_CD_ORIGSPACE_MLOOP, indices, weights.as_span());
    }
  }
}

static void loop_interpolation_from_corner(const SubdivMeshContext *ctx,
                                           LoopsForInterpolation *loop_interpolation,
                                           const IndexRange coarse_face,
                                           const int corner)
{
  if (coarse_face.size() == 4) {
    /* Nothing to do, all indices and data is already assigned. */
  }
  else {
    LoopsOfPtex loops_of_ptex;
    loops_of_ptex_get(&loops_of_ptex, coarse_face, corner);
    /* PTEX face corner corresponds to a face loop with same index. */
    copy_attrs(ctx->coarse_corner_attr_spans,
               coarse_face.start() + corner,
               0,
               loop_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_CD_NORMAL.is_empty()) {
      loop_interpolation->CD_NORMAL_storage[0] =
          ctx->coarse_CD_NORMAL[coarse_face.start() + corner];
    }
    if (!ctx->coarse_CD_ORIGSPACE_MLOOP.is_empty()) {
      loop_interpolation->CD_ORIGSPACE_MLOOP_storage[0] =
          ctx->coarse_CD_ORIGSPACE_MLOOP[coarse_face.start() + corner];
    }
    /* Interpolate remaining ptex face corners, which hits loops
     * middle points.
     *
     * TODO(sergey): Re-use one of interpolation results from previous
     * iteration. */
    const int base_loop_index = coarse_face.start();
    const int first_loop_index = loops_of_ptex.first_loop;
    const int second_loop_index = base_loop_index +
                                  (first_loop_index - base_loop_index + 1) % coarse_face.size();
    const std::array<int, 2> first_indices{first_loop_index, second_loop_index};
    const std::array<int, 2> last_indices{loops_of_ptex.last_loop, loops_of_ptex.first_loop};
    mix_attrs(ctx->coarse_corner_attr_spans,
              first_indices,
              0.5f,
              1,
              loop_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_CD_NORMAL.is_empty()) {
      loop_interpolation->CD_NORMAL_storage[1] = mix_normals(
          ctx->coarse_CD_NORMAL, first_indices, 0.5f);
    }
    if (!ctx->coarse_CD_ORIGSPACE_MLOOP.is_empty()) {
      loop_interpolation->CD_ORIGSPACE_MLOOP_storage[1] = attribute_math::mix2(
          0.5f,
          ctx->coarse_CD_ORIGSPACE_MLOOP[first_indices[0]],
          ctx->coarse_CD_ORIGSPACE_MLOOP[first_indices[1]]);
    }
    mix_attrs(ctx->coarse_corner_attr_spans,
              last_indices,
              0.5f,
              3,
              loop_interpolation->storage_spans.as_span().cast<GMutableSpan>());
    if (!ctx->coarse_CD_NORMAL.is_empty()) {
      loop_interpolation->CD_NORMAL_storage[3] = mix_normals(
          ctx->coarse_CD_NORMAL, last_indices, 0.5f);
    }
    if (!ctx->coarse_CD_ORIGSPACE_MLOOP.is_empty()) {
      loop_interpolation->CD_ORIGSPACE_MLOOP_storage[3] = attribute_math::mix2(
          0.5f,
          ctx->coarse_CD_ORIGSPACE_MLOOP[last_indices[0]],
          ctx->coarse_CD_ORIGSPACE_MLOOP[last_indices[1]]);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation helper functions
 * \{ */

/**
 * Evaluate undeformed texture coordinates.
 * \param subdiv_verts: The vertex for each coordinate, either #Span<int> or #IndexRange.
 */
template<typename IndicesT>
static void subdiv_verts_orco_evaluate(const SubdivMeshContext *ctx,
                                       const Span<PtexCoord> coords,
                                       const IndicesT &subdiv_verts)
{
  if (!ctx->orco && !ctx->cloth_orco) {
    return;
  }
  const int vert_data_size = (ctx->orco ? 3 : 0) + (ctx->cloth_orco ? 3 : 0);
  Array<float> vert_data(coords.size() * vert_data_size);
  eval_vert_data(ctx->subdiv, coords, vert_data);
  for (const int i : coords.index_range()) {
    const float *data = &vert_data[i * vert_data_size];
    const int subdiv_vert_index = subdiv_verts[i];
    if (ctx->orco) {
      copy_v3_v3(ctx->orco[subdiv_vert_index], data);
      if (ctx->cloth_orco) {
        copy_v3_v3(ctx->cloth_orco[subdiv_vert_index], data + 3);
      }
    }
    else if (ctx->cloth_orco) {
      copy_v3_v3(ctx->cloth_orco[subdiv_vert_index], data);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static void create_attrs_and_retrieve_interp_spans(const AttributeAccessor src_attrs,
                                                   const bke::AttrDomain domain,
                                                   const Set<StringRef> &skip_names,
                                                   MutableAttributeAccessor dst_attrs,
                                                   Vector<GVArraySpan> &coarse_varray_spans,
                                                   Vector<GSpan> &coarse_spans,
                                                   Vector<GSpanAttributeWriter> &dst_writers,
                                                   Vector<GMutableSpan> &dst_spans)

{
  src_attrs.foreach_attribute([&](const AttributeIter &iter) {
    if (iter.domain != domain) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (skip_names.contains(iter.name)) {
      return;
    }
    GVArray src = *iter.get();
    {
      const CommonVArrayInfo info = src.common_info();
      if (info.type == CommonVArrayInfo::Type::Single) {
        const GPointer value(src.type(), info.data);
        if (dst_attrs.add(iter.name, domain, iter.data_type, bke::AttributeInitValue(value))) {
          return;
        }
      }
    }
    GSpanAttributeWriter dst = dst_attrs.lookup_or_add_for_write_only_span(
        iter.name, domain, iter.data_type);
    if (!dst) {
      /* Attribute creation can fail with a vertex group name conflict. */
      return;
    }
    coarse_varray_spans.append(std::move(src));
    coarse_spans.append(coarse_varray_spans.last());
    dst_writers.append(std::move(dst));
    dst_spans.append(dst_writers.last().span);
  });
}

static void subdiv_mesh_create(SubdivMeshContext *subdiv_context, const MeshLayout &layout)
{
  /* Multi-resolution grid data will be applied or become invalid after subdivision,
   * so don't try to preserve it and use memory. Crease values should also not be interpolated. */
  CustomData_MeshMasks mask = CD_MASK_EVERYTHING;
  mask.lmask &= ~CD_MASK_MULTIRES_GRIDS;

  const Mesh &coarse_mesh = *subdiv_context->coarse_mesh;
  subdiv_context->subdiv_mesh = BKE_mesh_new_nomain(
      layout.verts_num, layout.edges_num, layout.faces_num, layout.corners_num);
  Mesh &subdiv_mesh = *subdiv_context->subdiv_mesh;
  BKE_mesh_copy_parameters_for_eval(subdiv_context->subdiv_mesh, &coarse_mesh);

  if (layout.faces_num != 0) {
    offset_indices::fill_constant_group_size(4, 0, subdiv_mesh.face_offsets_for_write());
  }

  /* Create corner data for interpolation without topology attributes. */
  const AttributeAccessor coarse_attrs = coarse_mesh.attributes();
  MutableAttributeAccessor attributes = subdiv_mesh.attributes_for_write();

  Set<StringRef> vert_skip_names{{"position"}};
  for (const bDeformGroup &group : coarse_mesh.vertex_group_names) {
    vert_skip_names.add(group.name);
  }
  create_attrs_and_retrieve_interp_spans(coarse_attrs,
                                         AttrDomain::Point,
                                         vert_skip_names,
                                         attributes,
                                         subdiv_context->coarse_vert_attrs,
                                         subdiv_context->coarse_vert_attr_spans,
                                         subdiv_context->subdiv_vert_attrs,
                                         subdiv_context->subdiv_vert_attr_spans);

  create_attrs_and_retrieve_interp_spans(coarse_attrs,
                                         AttrDomain::Edge,
                                         {".edge_verts"},
                                         attributes,
                                         subdiv_context->coarse_edge_attrs,
                                         subdiv_context->coarse_edge_attr_spans,
                                         subdiv_context->subdiv_edge_attrs,
                                         subdiv_context->subdiv_edge_attr_spans);

  create_attrs_and_retrieve_interp_spans(coarse_attrs,
                                         AttrDomain::Face,
                                         {},
                                         attributes,
                                         subdiv_context->coarse_face_attrs,
                                         subdiv_context->coarse_face_attr_spans,
                                         subdiv_context->subdiv_face_attrs,
                                         subdiv_context->subdiv_face_attr_spans);

  /* Rely on #CD_NORMAL to propagate normals to subdivision surfaces.
   * These are converted into "custom_normals" afterwards, otherwise these normals
   * would interpolated without being normalized, see: #152277. */
  Set<StringRef> corner_skip_names{".corner_vert", ".corner_edge", "custom_normal"};
  /* UV map names are interpolated separately. */
  for (const StringRef name : coarse_mesh.uv_map_names()) {
    corner_skip_names.add_new(name);
  }
  create_attrs_and_retrieve_interp_spans(coarse_attrs,
                                         AttrDomain::Corner,
                                         corner_skip_names,
                                         attributes,
                                         subdiv_context->coarse_corner_attrs,
                                         subdiv_context->coarse_corner_attr_spans,
                                         subdiv_context->subdiv_corner_attrs,
                                         subdiv_context->subdiv_corner_attr_spans);

  subdiv_mesh_ctx_cache_custom_data_layers(subdiv_context);
  subdiv_context->subdiv_face_dot_tags = Array<bool>(layout.verts_num, false);
  if (subdiv_context->settings->use_optimal_display) {
    subdiv_context->subdiv_display_edges = Array<bool>(layout.edges_num, false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertex subdivision process
 * \{ */

static void subdiv_vert_data_copy(const SubdivMeshContext *ctx,
                                  const int coarse_vert_index,
                                  const int subdiv_vert_index)
{
  copy_attrs(ctx->coarse_vert_attr_spans,
             coarse_vert_index,
             subdiv_vert_index,
             ctx->subdiv_vert_attr_spans);
  if (!ctx->coarse_vert_origindex.is_empty()) {
    ctx->subdiv_vert_origindex[subdiv_vert_index] = ctx->coarse_vert_origindex[coarse_vert_index];
  }
  if (!ctx->coarse_dverts.is_empty()) {
    BKE_defvert_array_copy(
        &ctx->subdiv_dverts[subdiv_vert_index], &ctx->coarse_dverts[coarse_vert_index], 1);
  }
}

static void subdiv_vert_data_interpolate(const SubdivMeshContext *ctx,
                                         const int subdiv_vert_index,
                                         VerticesForInterpolation *vert_interpolation,
                                         const float u,
                                         const float v)
{
  const float4 weights = quad_weights_from_uv(u, v);
  mix_attrs(vert_interpolation->vert_data,
            vert_interpolation->vert_indices,
            weights,
            subdiv_vert_index,
            ctx->subdiv_vert_attr_spans);
  if (!ctx->coarse_vert_origindex.is_empty()) {
    ctx->subdiv_vert_origindex[subdiv_vert_index] = ORIGINDEX_NONE;
  }
  if (!ctx->coarse_dverts.is_empty()) {
    ctx->subdiv_dverts[subdiv_vert_index] = mix_deform_verts(vert_interpolation->dverts_data,
                                                             vert_interpolation->vert_indices,
                                                             Span(&weights.x, 4),
                                                             vert_interpolation->dvert_mix_buffer);
  }
}

/**
 * Evaluate subdivided vertices corresponding to base mesh vertices and inside of base mesh
 * edges used by faces. These vertices are shared by multiple base faces and are evaluated from
 * the first face that uses them, and the displacement is averaged from all faces.
 */
static void subdiv_mesh_boundary_verts(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  const Mesh &coarse_mesh = *ctx->coarse_mesh;
  const OffsetIndices<int> faces = ctx->coarse_faces;
  const Span<int2> edges = ctx->coarse_edges;
  const Span<int> corner_verts = ctx->coarse_corner_verts;
  const Span<int> corner_edges = coarse_mesh.corner_edges();
  const Span<int> corner_to_face = coarse_mesh.corner_to_face_map();
  const GroupedSpan<int> vert_to_corner = coarse_mesh.vert_to_corner_map();
  const GroupedSpan<int> edge_to_corner = coarse_mesh.edge_to_corner_map();
  const Span<int> face_ptex_offset = face_ptex_offset_get(ctx->subdiv);
  const int edge_verts_num = layout.resolution - 2;

  const auto corner_vert_ptex_coord = [&](const int corner) {
    const int face = corner_to_face[corner];
    return face_corner_ptex_coord(
        faces[face], face_ptex_offset[face], corner - faces[face].start());
  };
  const auto corner_edge_ptex_coords = [&](const int corner,
                                           const int edge,
                                           MutableSpan<PtexCoord> coords) {
    const int face = corner_to_face[corner];
    const bool flip = edges[edge][1] == corner_verts[corner];
    fill_face_corner_edge_ptex_coords(
        layout, faces[face], face_ptex_offset[face], corner - faces[face].start(), flip, coords);
  };

  threading::parallel_for(faces.index_range(), 256, [&](const IndexRange range) {
    VerticesForInterpolation vert_interpolation(*ctx);
    Array<PtexCoord> edge_coords(edge_verts_num);
    Array<PtexCoord> other_edge_coords;

    /* The vertices processed by this task, which are evaluated together afterwards. */
    Vector<int> verts;
    Vector<PtexCoord> coords;
    /* The coordinates of the vertices in every face that uses them, for averaging displacement.
     * Every vertex has a group of coordinates. */
    Vector<int> displacement_offsets = {0};
    Vector<PtexCoord> displacement_coords;

    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      bool interpolation_initialized = false;
      int interpolation_corner = -1;
      for (const int corner : face) {
        /* Process every vertex and edge only from the first face corner using it. */
        const int vert = corner_verts[corner];
        const Span<int> vert_corners = vert_to_corner[vert];
        if (vert_corners.first() == corner) {
          /* Subdivided vertices corresponding to base mesh vertices have the same indices. */
          subdiv_vert_data_copy(ctx, vert, vert);
          verts.append(vert);
          coords.append(
              face_corner_ptex_coord(face, face_ptex_offset[face_index], corner - face.start()));
          if (ctx->have_displacement) {
            for (const int other_corner : vert_corners) {
              displacement_coords.append(corner_vert_ptex_coord(other_corner));
            }
            displacement_offsets.append(displacement_coords.size());
          }
        }

        const int edge = corner_edges[corner];
        const Span<int> edge_corners = edge_to_corner[edge];
        if (edge_corners.first() != corner) {
          continue;
        }
        if (!interpolation_initialized) {
          vert_interpolation_from_face(ctx, &vert_interpolation, face);
          interpolation_initialized = true;
        }
        corner_edge_ptex_coords(corner, edge, edge_coords);
        const IndexRange edge_verts = layout.base_edge_verts(edge);
        for (const int i : edge_verts.index_range()) {
          const PtexCoord &coord = edge_coords[i];
          /* Quads have a single ptex face, other faces have a ptex face for every corner. */
          const int coarse_corner = face.size() == 4 ?
                                        0 :
                                        coord.ptex_face_index - face_ptex_offset[face_index];
          if (coarse_corner != interpolation_corner) {
            vert_interpolation_from_corner(ctx, &vert_interpolation, face, coarse_corner);
            interpolation_corner = coarse_corner;
          }
          subdiv_vert_data_interpolate(
              ctx, edge_verts[i], &vert_interpolation, coord.uv.x, coord.uv.y);
          verts.append(edge_verts[i]);
        }
        coords.extend(edge_coords);
        if (ctx->have_displacement) {
          other_edge_coords.reinitialize(edge_corners.size() * edge_verts_num);
          for (const int i : edge_corners.index_range()) {
            corner_edge_ptex_coords(
                edge_corners[i],
                edge,
                other_edge_coords.as_mutable_span().slice(i * edge_verts_num, edge_verts_num));
          }
          for (const int vert_i : IndexRange(edge_verts_num)) {
            for (const int i : edge_corners.index_range()) {
              displacement_coords.append(other_edge_coords[i * edge_verts_num + vert_i]);
            }
            displacement_offsets.append(displacement_coords.size());
          }
        }
      }
    }

    Array<float3> positions(coords.size());
    eval_limit_points(ctx->subdiv, coords, positions);
    if (ctx->have_displacement) {
      /* Average the displacement from all faces using each vertex. */
      Array<float3> displacement(coords.size());
      eval_displacement_averaged(
          ctx->subdiv, displacement_coords, displacement_offsets.as_span(), displacement);
      for (const int i : positions.index_range()) {
        positions[i] += displacement[i];
      }
    }
    array_utils::scatter(
        positions.as_span(), verts.as_span(), ctx->subdiv_positions, exec_mode::serial);
    subdiv_verts_orco_evaluate(ctx, coords.as_span(), verts.as_span());
  });
}

static bool subdiv_mesh_is_center_vert(const IndexRange coarse_face, const float u, const float v)
{
  if (coarse_face.size() == 4) {
    if (u == 0.5f && v == 0.5f) {
      return true;
    }
  }
  else {
    if (u == 1.0f && v == 1.0f) {
      return true;
    }
  }
  return false;
}

static void subdiv_mesh_inner_verts(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  Subdiv *subdiv = ctx->subdiv;
  const Span<int> face_ptex_offset = face_ptex_offset_get(subdiv);
  const OffsetIndices<int> inner_verts_by_face = layout.inner_verts_by_face();
  threading::parallel_for(
      ctx->coarse_faces.index_range(), layout.base_face_grain_size(), [&](const IndexRange range) {
        const IndexRange chunk_verts = inner_verts_by_face[range];
        Array<PtexCoord> ptex_coords(chunk_verts.size());
        fill_inner_vert_ptex_coords(
            layout, *ctx->coarse_mesh, face_ptex_offset, range, ptex_coords);
        const IndexRange subdiv_verts = chunk_verts.shift(layout.inner_verts_start);
        eval_final_points(subdiv, ptex_coords, ctx->subdiv_positions.slice(subdiv_verts));
        subdiv_verts_orco_evaluate(ctx, ptex_coords.as_span(), subdiv_verts);

        VerticesForInterpolation vert_interpolation(*ctx);
        for (const int coarse_face_index : range) {
          const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
          vert_interpolation_from_face(ctx, &vert_interpolation, coarse_face);
          int prev_coarse_corner = -1;
          for (const int vert : inner_verts_by_face[coarse_face_index]) {
            const PtexCoord &coord = ptex_coords[vert - chunk_verts.start()];
            const float u = coord.uv.x;
            const float v = coord.uv.y;
            /* Quads have a single ptex face, other faces have a ptex face for every corner. */
            const int coarse_corner = coarse_face.size() == 4 ?
                                          0 :
                                          coord.ptex_face_index -
                                              face_ptex_offset[coarse_face_index];
            if (coarse_corner != prev_coarse_corner) {
              vert_interpolation_from_corner(ctx, &vert_interpolation, coarse_face, coarse_corner);
              prev_coarse_corner = coarse_corner;
            }
            const int subdiv_vert_index = layout.inner_verts_start + vert;
            subdiv_vert_data_interpolate(ctx, subdiv_vert_index, &vert_interpolation, u, v);
            if (subdiv_mesh_is_center_vert(coarse_face, u, v)) {
              ctx->subdiv_face_dot_tags[subdiv_vert_index] = true;
            }
          }
        }
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge subdivision process
 * \{ */

/**
 * Copy each source value to a group of `group_size` destination values.
 */
static void copy_to_constant_size_groups(const GSpan src, const int group_size, GMutableSpan dst)
{
  const CPPType &type = src.type();
  threading::parallel_for(IndexRange(src.size()), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      type.fill_construct_n(src[i], dst[i * group_size], group_size);
    }
  });
}

static void subdiv_mesh_edges(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  const IndexRange base_edge_edges(layout.inner_edges_start);
  const IndexRange inner_edges = IndexRange::from_begin_end(layout.inner_edges_start,
                                                            layout.edges_num);
  const int edges_per_base_edge = layout.resolution - 1;

  fill_base_edge_edges(layout,
                       ctx->coarse_edges,
                       ctx->coarse_edges.index_range(),
                       ctx->subdiv_edges.slice(base_edge_edges));
  fill_inner_edges(layout, *ctx->coarse_mesh, ctx->subdiv_edges.slice(inner_edges));

  /* Edges along base mesh edges copy the base edge's data, edges inside of base mesh faces don't
   * correspond to a base mesh edge and get default values. */
  for (const int attr : ctx->coarse_edge_attrs.index_range()) {
    const GSpan src = ctx->coarse_edge_attr_spans[attr];
    GMutableSpan dst = ctx->subdiv_edge_attr_spans[attr];
    const CPPType &type = src.type();
    copy_to_constant_size_groups(src, edges_per_base_edge, dst.slice(base_edge_edges));
    const GMutableSpan dst_inner = dst.slice(inner_edges);
    type.fill_construct_n(type.default_value(), dst_inner.data(), dst_inner.size());
  }
  if (!ctx->coarse_edge_origindex.is_empty()) {
    copy_to_constant_size_groups(GSpan(ctx->coarse_edge_origindex),
                                 edges_per_base_edge,
                                 ctx->subdiv_edge_origindex.slice(base_edge_edges));
    ctx->subdiv_edge_origindex.slice(inner_edges).fill(ORIGINDEX_NONE);
  }
  if (ctx->settings->use_optimal_display) {
    ctx->subdiv_display_edges.as_mutable_span().slice(base_edge_edges).fill(true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loops creation/interpolation
 * \{ */

static void subdiv_interpolate_corner_data(const SubdivMeshContext *ctx,
                                           const int subdiv_loop_index,
                                           const LoopsForInterpolation *loop_interpolation,
                                           const float u,
                                           const float v)
{
  const float4 weights = quad_weights_from_uv(u, v);
  mix_attrs(loop_interpolation->corner_data,
            loop_interpolation->loop_indices,
            weights,
            subdiv_loop_index,
            ctx->subdiv_corner_attr_spans);
  if (!ctx->coarse_CD_NORMAL.is_empty()) {
    ctx->subdiv_CD_NORMAL[subdiv_loop_index] = mix_normals(
        loop_interpolation->CD_NORMAL_data, loop_interpolation->loop_indices, weights);
  }
  if (!ctx->coarse_CD_ORIGSPACE_MLOOP.is_empty()) {
    ctx->subdiv_CD_ORIGSPACE_MLOOP[subdiv_loop_index] = attribute_math::mix4(
        weights,
        loop_interpolation->CD_ORIGSPACE_MLOOP_data[loop_interpolation->loop_indices[0]],
        loop_interpolation->CD_ORIGSPACE_MLOOP_data[loop_interpolation->loop_indices[1]],
        loop_interpolation->CD_ORIGSPACE_MLOOP_data[loop_interpolation->loop_indices[2]],
        loop_interpolation->CD_ORIGSPACE_MLOOP_data[loop_interpolation->loop_indices[3]]);
  }
}

static void subdiv_mesh_corner_data(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  const Span<int> face_ptex_offset = face_ptex_offset_get(ctx->subdiv);
  const OffsetIndices<int> faces_by_base_face = layout.faces_by_base_face();
  threading::parallel_for(
      ctx->coarse_faces.index_range(), layout.base_face_grain_size(), [&](const IndexRange range) {
        const IndexRange chunk_faces = faces_by_base_face[range];
        const IndexRange chunk_corners = layout.base_face_corners(range);
        Array<PtexCoord> ptex_coords(chunk_corners.size());
        Array<int> base_corners(chunk_faces.size());
        fill_corner_ptex_coords(
            layout, *ctx->coarse_mesh, face_ptex_offset, range, ptex_coords, base_corners);

        for (const int i : ctx->uv_maps.index_range()) {
          eval_face_varying(
              ctx->subdiv, i, ptex_coords, ctx->uv_maps[i].span.slice(chunk_corners));
        }

        LoopsForInterpolation loop_interpolation(*ctx);
        for (const int coarse_face_index : range) {
          const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
          loop_interpolation_from_face(ctx, &loop_interpolation, coarse_face);
          int prev_coarse_corner = -1;
          for (const int face : faces_by_base_face[coarse_face_index]) {
            const int coarse_corner = base_corners[face - chunk_faces.start()];
            if (coarse_corner != prev_coarse_corner) {
              loop_interpolation_from_corner(ctx, &loop_interpolation, coarse_face, coarse_corner);
              prev_coarse_corner = coarse_corner;
            }
            for (const int corner : IndexRange(face * 4, 4)) {
              const PtexCoord &coord = ptex_coords[corner - chunk_corners.start()];
              const float u = coord.uv.x;
              const float v = coord.uv.y;
              subdiv_interpolate_corner_data(ctx, corner, &loop_interpolation, u, v);
            }
          }
        }
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Polygons subdivision process
 * \{ */

static void subdiv_mesh_faces(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  const OffsetIndices<int> subdiv_faces_by_base_face = layout.faces_by_base_face();
  threading::memory_bandwidth_bound_task(int64_t(ctx->subdiv_mesh->faces_num) * 4, [&]() {
    for (const int i : ctx->coarse_face_attrs.index_range()) {
      array_utils::gather_to_groups(subdiv_faces_by_base_face,
                                    ctx->coarse_faces.index_range(),
                                    ctx->coarse_face_attr_spans[i],
                                    ctx->subdiv_face_attr_spans[i]);
    }
    if (!ctx->coarse_face_origindex.is_empty()) {
      array_utils::gather_to_groups(subdiv_faces_by_base_face,
                                    ctx->coarse_faces.index_range(),
                                    ctx->coarse_face_origindex,
                                    ctx->subdiv_face_origindex);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loose elements subdivision process
 * \{ */

static void subdiv_mesh_verts_no_face(SubdivMeshContext *ctx)
{
  /* Subdivided vertices corresponding to base mesh vertices have the same indices. */
  ctx->coarse_mesh->verts_no_face().foreach_index(
      [&](const int vert) {
        subdiv_vert_data_copy(ctx, vert, vert);
        ctx->subdiv_positions[vert] = ctx->coarse_positions[vert];
      },
      exec_mode::grain_size(1024));
}

/* Get neighbor edges of the given one.
 * - neighbors[0] is an edge adjacent to edge->v1.
 * - neighbors[1] is an edge adjacent to edge->v2. */
static std::array<std::optional<int2>, 2> find_edge_neighbors(
    const Span<int2> coarse_edges, const GroupedSpan<int> vert_to_edge_map, const int edge_index)
{
  /* Vertices which has more than one neighbor are considered infinitely
   * sharp. This is also how topology factory treats vertices of a surface
   * which are adjacent to a loose edge. */
  const auto neighbor_edge_if_single = [&](const int vert) -> std::optional<int2> {
    const Span<int> neighbors = vert_to_edge_map[vert];
    if (neighbors.size() != 2) {
      return std::nullopt;
    }
    return neighbors[0] == edge_index ? coarse_edges[neighbors[1]] : coarse_edges[neighbors[0]];
  };
  const int2 edge = coarse_edges[edge_index];
  return {neighbor_edge_if_single(edge[0]), neighbor_edge_if_single(edge[1])};
}

static std::array<float3, 4> find_loose_edge_interpolation_positions(
    const Span<float3> coarse_positions,
    const int2 &coarse_edge,
    const std::array<std::optional<int2>, 2> &neighbors)
{
  std::array<float3, 4> result;
  /* Middle points corresponds to the edge. */
  result[1] = coarse_positions[coarse_edge[0]];
  result[2] = coarse_positions[coarse_edge[1]];
  /* Start point, duplicate from edge start if no neighbor. */
  if (const std::optional<int2> &other = neighbors[0]) {
    result[0] = coarse_positions[mesh::edge_other_vert(*other, coarse_edge[0])];
  }
  else {
    result[0] = result[1] * 2.0f - result[2];
  }
  /* End point, duplicate from edge end if no neighbor. */
  if (const std::optional<int2> &other = neighbors[1]) {
    result[3] = coarse_positions[mesh::edge_other_vert(*other, coarse_edge[1])];
  }
  else {
    result[3] = result[2] * 2.0f - result[1];
  }
  return result;
}

float3 mesh_interpolate_position_on_edge(const Span<float3> coarse_positions,
                                         const Span<int2> coarse_edges,
                                         const GroupedSpan<int> vert_to_edge_map,
                                         const int coarse_edge_index,
                                         const bool is_simple,
                                         const float u)
{
  const int2 edge = coarse_edges[coarse_edge_index];
  if (is_simple) {
    return math::interpolate(coarse_positions[edge[0]], coarse_positions[edge[1]], u);
  }
  /* Find neighbors of the coarse edge. */
  const std::array<std::optional<int2>, 2> neighbors = find_edge_neighbors(
      coarse_edges, vert_to_edge_map, coarse_edge_index);
  const std::array<float3, 4> points = find_loose_edge_interpolation_positions(
      coarse_positions, edge, neighbors);
  float4 weights;
  key_curve_position_weights(u, weights, KEY_BSPLINE);
  return bke::attribute_math::mix4(weights, points[0], points[1], points[2], points[3]);
}

static void subdiv_mesh_vert_of_loose_edge_interpolate(SubdivMeshContext *ctx,
                                                       const int2 &coarse_edge,
                                                       const float u,
                                                       const int subdiv_vert_index)
{
  /* This is never used for end-points (which are copied from the original). */
  BLI_assert(u > 0.0f);
  BLI_assert(u < 1.0f);
  const std::array<int, 2> coarse_vert_indices{coarse_edge[0], coarse_edge[1]};
  mix_attrs(ctx->coarse_vert_attr_spans,
            coarse_vert_indices,
            u,
            subdiv_vert_index,
            ctx->subdiv_vert_attr_spans);
  if (!ctx->coarse_vert_origindex.is_empty()) {
    ctx->subdiv_vert_origindex[subdiv_vert_index] = ORIGINDEX_NONE;
  }
  if (!ctx->coarse_dverts.is_empty()) {
    MDeformWeightSet dvert_mix_buffer;
    ctx->subdiv_dverts[subdiv_vert_index] = mix_deform_verts(
        ctx->coarse_dverts, coarse_vert_indices, {1.0f - u, u}, dvert_mix_buffer);
  }
}

static void subdiv_mesh_vert_of_loose_edge(SubdivMeshContext *ctx,
                                           const int coarse_edge_index,
                                           const float u,
                                           const int subdiv_vert_index)
{
  const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_index];
  const bool is_simple = ctx->subdiv->settings.is_simple;

  /* Interpolate custom data when not an end point.
   * This data has already been copied from the original vertex by #subdiv_mesh_verts_no_face. */
  if (!ELEM(u, 0.0, 1.0)) {
    subdiv_mesh_vert_of_loose_edge_interpolate(ctx, coarse_edge, u, subdiv_vert_index);
  }
  /* Interpolate coordinate. */
  ctx->subdiv_positions[subdiv_vert_index] = mesh_interpolate_position_on_edge(
      ctx->coarse_positions,
      ctx->coarse_edges,
      ctx->vert_to_edge_map,
      coarse_edge_index,
      is_simple,
      u);
}

static void subdiv_mesh_loose_edges(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  const int resolution = layout.resolution;
  const float inv_resolution_1 = 1.0f / float(resolution - 1);
  ctx->coarse_mesh->loose_edges().foreach_index(
      [&](const int coarse_edge_index) {
        const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_index];
        const IndexRange edge_verts = layout.base_edge_verts(coarse_edge_index);
        for (const int i : IndexRange(resolution)) {
          const float u = i * inv_resolution_1;
          int subdiv_vert_index;
          if (i == 0) {
            subdiv_vert_index = coarse_edge[0];
          }
          else if (i == resolution - 1) {
            subdiv_vert_index = coarse_edge[1];
          }
          else {
            subdiv_vert_index = edge_verts[i - 1];
          }
          subdiv_mesh_vert_of_loose_edge(ctx, coarse_edge_index, u, subdiv_vert_index);
        }
      },
      exec_mode::grain_size(64));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public entry point
 * \{ */

Mesh *subdiv_to_mesh(Subdiv *subdiv, const ToMeshSettings *settings, const Mesh *coarse_mesh)
{

  stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
  /* Make sure evaluator is up to date with possible new topology, and that
   * it is refined for the new positions of coarse vertices. */
  if (!eval_begin_from_mesh(subdiv, coarse_mesh, SUBDIV_EVALUATOR_TYPE_CPU)) {
    /* This could happen in two situations:
     * - OpenSubdiv is disabled.
     * - Something totally bad happened, and OpenSubdiv rejected our topology.
     * In either way, we can't safely continue. */
    if (coarse_mesh->faces_num) {
      stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
      return nullptr;
    }
  }
  /* Initialize subdivision mesh creation context. */
  SubdivMeshContext subdiv_context{};
  subdiv_context.settings = settings;

  subdiv_context.coarse_mesh = coarse_mesh;
  subdiv_context.coarse_positions = coarse_mesh->vert_positions();
  subdiv_context.coarse_edges = coarse_mesh->edges();
  subdiv_context.coarse_faces = coarse_mesh->faces();
  subdiv_context.coarse_corner_verts = coarse_mesh->corner_verts();
  if (!coarse_mesh->loose_edges().is_empty()) {
    subdiv_context.vert_to_edge_map = mesh::build_vert_to_edge_map(
        subdiv_context.coarse_edges,
        coarse_mesh->verts_num,
        subdiv_context.vert_to_edge_offsets,
        subdiv_context.vert_to_edge_indices);
  }

  subdiv_context.subdiv = subdiv;
  subdiv_context.have_displacement = (subdiv->displacement_evaluator != nullptr);
  /* Multi-threaded traversal/evaluation. */
  stats_begin(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
  const MeshLayout layout = build_mesh_layout(*coarse_mesh, settings->resolution);
  subdiv_mesh_create(&subdiv_context, layout);
  subdiv_mesh_verts_no_face(&subdiv_context);
  subdiv_mesh_boundary_verts(&subdiv_context, layout);
  subdiv_mesh_edges(&subdiv_context, layout);
  fill_corner_verts_and_edges(layout,
                              *coarse_mesh,
                              subdiv_context.coarse_faces.index_range(),
                              subdiv_context.subdiv_corner_verts,
                              subdiv_context.subdiv_corner_edges);
  subdiv_mesh_inner_verts(&subdiv_context, layout);
  subdiv_mesh_corner_data(&subdiv_context, layout);
  subdiv_mesh_faces(&subdiv_context, layout);
  subdiv_mesh_loose_edges(&subdiv_context, layout);
  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
  Mesh *result = subdiv_context.subdiv_mesh;

  /* NOTE: Using normals from the limit surface gives different results than Blender's vertex
   * normal calculation. Since vertex normals are supposed to be a consistent cache, don't bother
   * calculating them here. The work may have been pointless anyway if the mesh is deformed or
   * changed afterwards. */

  result->runtime->subsurf_face_dot_tags = BitVector<>(subdiv_context.subdiv_face_dot_tags);

  /* Move the optimal display edge array to the final bit vector. */
  if (!subdiv_context.subdiv_display_edges.is_empty()) {
    result->runtime->subsurf_optimal_display_edges = BitVector<>(
        subdiv_context.subdiv_display_edges);
  }

  if (coarse_mesh->verts_no_face().is_empty()) {
    result->tag_loose_verts_none();
  }
  if (coarse_mesh->loose_edges().is_empty()) {
    result->tag_loose_edges_none();
  }
  result->tag_overlapping_none();

  if (subdiv->settings.is_simple) {
    /* In simple subdivision, min and max positions are not changed, avoid recomputing bounds. */
    result->runtime->bounds_cache = coarse_mesh->runtime->bounds_cache;
  }

  for (bke::SpanAttributeWriter<float2> &attr : subdiv_context.uv_maps) {
    attr.finish();
  }
  for (bke::GSpanAttributeWriter &attr : subdiv_context.subdiv_vert_attrs) {
    attr.finish();
  }
  for (bke::GSpanAttributeWriter &attr : subdiv_context.subdiv_edge_attrs) {
    attr.finish();
  }
  for (bke::GSpanAttributeWriter &attr : subdiv_context.subdiv_face_attrs) {
    attr.finish();
  }
  for (bke::GSpanAttributeWriter &attr : subdiv_context.subdiv_corner_attrs) {
    attr.finish();
  }

  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Limit surface
 * \{ */

void calculate_limit_positions(Mesh *mesh, MutableSpan<float3> limit_positions)
{
  BLI_assert(mesh->verts_num == limit_positions.size());

  limit_positions.copy_from(mesh->vert_positions());

  Settings settings{};
  settings.is_simple = false;
  settings.is_adaptive = true;
  settings.level = 1;
  settings.use_creases = true;

  /* Default subdivision surface modifier settings:
   * - UV Smooth:Keep Corners.
   * - BoundarySmooth: All. */
  settings.vtx_boundary_interpolation = SUBDIV_VTX_BOUNDARY_EDGE_ONLY;
  settings.fvar_linear_interpolation = SUBDIV_FVAR_LINEAR_INTERPOLATION_CORNERS_AND_JUNCTIONS;

  Subdiv *subdiv = update_from_mesh(nullptr, &settings, mesh);
  if (subdiv) {
    deform_coarse_vertices(subdiv, mesh, limit_positions);
    free(subdiv);
  }
}

/** \} */

}  // namespace blender::bke::subdiv
