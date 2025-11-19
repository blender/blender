/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_array.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
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
#include "BKE_subdiv_foreach.hh"
#include "BKE_subdiv_mesh.hh"

#include "MEM_guardedalloc.h"

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
  MutableSpan<int> subdiv_face_offsets;
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

  /* Base and result mesh vertex group data. */
  Span<MVertSkin> coarse_CD_MVERT_SKIN;
  MutableSpan<MVertSkin> subdiv_CD_MVERT_SKIN;

  /* Base and result mesh data for interpolating custom normals. */
  Span<float3> coarse_CD_NORMAL;
  MutableSpan<float3> subdiv_CD_NORMAL;

  /* Base and result mesh data for interpolating particle system data. */
  Span<float2> coarse_CD_ORIGSPACE_MLOOP;
  MutableSpan<float2> subdiv_CD_ORIGSPACE_MLOOP;

  /* Per-subdivided vertex counter of averaged values. */
  int *accumulated_counters;
  bool have_displacement;

  /* Write optimal display edge tags into a boolean array rather than the final bit vector
   * to avoid race conditions when setting bits. */
  Array<bool> subdiv_display_edges;

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

  /* Copy active & default UV map names. */
  if (const char *name = CustomData_get_active_layer_name(&coarse_mesh.corner_data,
                                                          CD_PROP_FLOAT2))
  {
    const int i = CustomData_get_named_layer(&subdiv_mesh->corner_data, CD_PROP_FLOAT2, name);
    if (i >= 0) {
      CustomData_set_layer_active(&subdiv_mesh->corner_data, CD_PROP_FLOAT2, i);
    }
  }
  if (const char *name = CustomData_get_render_layer_name(&coarse_mesh.corner_data,
                                                          CD_PROP_FLOAT2))
  {
    const int i = CustomData_get_named_layer(&subdiv_mesh->corner_data, CD_PROP_FLOAT2, name);
    if (i >= 0) {
      CustomData_set_layer_render(&subdiv_mesh->corner_data, CD_PROP_FLOAT2, i);
    }
  }
}

static void subdiv_mesh_ctx_cache_custom_data_layers(SubdivMeshContext *ctx)
{
  const Mesh &coarse_mesh = *ctx->coarse_mesh;
  Mesh *subdiv_mesh = ctx->subdiv_mesh;
  ctx->subdiv_positions = subdiv_mesh->vert_positions_for_write();
  ctx->subdiv_edges = subdiv_mesh->edges_for_write();
  ctx->subdiv_face_offsets = subdiv_mesh->face_offsets_for_write();
  ctx->subdiv_corner_verts = subdiv_mesh->corner_verts_for_write();
  ctx->subdiv_corner_edges = subdiv_mesh->corner_edges_for_write();

  ctx->coarse_dverts = coarse_mesh.deform_verts();
  if (!ctx->coarse_dverts.is_empty()) {
    ctx->subdiv_dverts = subdiv_mesh->deform_verts_for_write();
  }
  if (const auto *src = static_cast<const MVertSkin *>(
          CustomData_get_layer(&coarse_mesh.vert_data, CD_MVERT_SKIN)))
  {
    ctx->coarse_CD_MVERT_SKIN = {src, coarse_mesh.verts_num};
    ctx->subdiv_CD_MVERT_SKIN = {
        static_cast<MVertSkin *>(CustomData_add_layer(
            &subdiv_mesh->vert_data, CD_MVERT_SKIN, CD_CONSTRUCT, subdiv_mesh->verts_num)),
        subdiv_mesh->verts_num};
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

static void subdiv_mesh_prepare_accumulator(SubdivMeshContext *ctx, int num_vertices)
{
  if (!ctx->have_displacement) {
    return;
  }
  /* #subdiv_accumulate_vert_displacement requires zero initialization of positions so the
   * displacements can be accumulated into the array from a per-vertex-per-corner/edge callback. */
  ctx->subdiv_positions.fill(float3(0));
  ctx->accumulated_counters = MEM_calloc_arrayN<int>(num_vertices, __func__);
}

static void subdiv_mesh_context_free(SubdivMeshContext *ctx)
{
  MEM_SAFE_FREE(ctx->accumulated_counters);
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

static MVertSkin mix_CD_MVERT_SKIN(const Span<MVertSkin> src,
                                   const Span<int> src_indices,
                                   const Span<float> weights)
{
  float3 radius(0);
  for (const int i : src_indices.index_range()) {
    radius += float3(src[src_indices[i]].radius) * weights[i];
  }
  MVertSkin result{};
  copy_v3_v3(result.radius, radius);
  return result;
}

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

static bool mix_bools(const Span<bool> src, const Span<int> indices, const Span<float> weights)
{
  for (const int i : indices.index_range()) {
    if (weights[i] == 0.0f) {
      continue;
    }
    if (src[indices[i]]) {
      return true;
    }
  }
  return false;
}

static void mix_attrs(const Span<GSpan> src,
                      const std::array<int, 2> &src_indices,
                      const float factor,
                      const int dst_index,
                      const Span<GMutableSpan> dst)
{
  for (const int attr : src.index_range()) {
    attribute_math::convert_to_static_type(src[attr].type(), [&](auto dummy) {
      using T = decltype(dummy);
      const Span<T> src_attr = src[attr].typed<T>();
      MutableSpan<T> dst_attr = dst[attr].typed<T>();
      if constexpr (std::is_same_v<T, bool>) {
        dst_attr[dst_index] = mix_bools(src_attr, src_indices, {1.0f - factor, factor});
      }
      else {
        dst_attr[dst_index] = attribute_math::mix2(
            factor, src_attr[src_indices[0]], src_attr[src_indices[1]]);
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
    attribute_math::convert_to_static_type(src[attr].type(), [&](auto dummy) {
      using T = decltype(dummy);
      const Span<T> src_attr = src[attr].typed<T>();
      MutableSpan<T> dst_attr = dst[attr].typed<T>();
      if constexpr (std::is_same_v<T, bool>) {
        dst_attr[dst_index] = mix_bools(src_attr, src_indices, {&weights.x, 4});
      }
      else {
        dst_attr[dst_index] = attribute_math::mix4(weights,
                                                   src_attr[src_indices[0]],
                                                   src_attr[src_indices[1]],
                                                   src_attr[src_indices[2]],
                                                   src_attr[src_indices[3]]);
      }
    });
  }
}

template<typename T>
static T mix_attr(const Span<T> src, const Span<int> src_indices, const Span<float> weights)
{
  T dst;
  attribute_math::DefaultPropagationMixer<T> mixer({&dst, 1});
  for (const int i : src_indices.index_range()) {
    mixer.mix_in(0, src[src_indices[i]], weights[i]);
  }
  mixer.finalize();
  return dst;
}

static void mix_attrs(const Span<GSpan> src,
                      const Span<int> src_indices,
                      const Span<float> weights,
                      const int dst_index,
                      const Span<GMutableSpan> dst)
{
  for (const int attr : src.index_range()) {
    attribute_math::convert_to_static_type(src[attr].type(), [&](auto dummy) {
      using T = decltype(dummy);
      const Span<T> src_attr = src[attr].typed<T>();
      MutableSpan<T> dst_attr = dst[attr].typed<T>();
      dst_attr[dst_index] = mix_attr(src_attr, src_indices, weights);
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
  Span<MVertSkin> CD_MVERT_SKIN_data;
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
  std::array<MVertSkin, 4> CD_MVERT_SKIN_storage = {};
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
    vert_interpolation->CD_MVERT_SKIN_data = ctx->coarse_CD_MVERT_SKIN;
    vert_interpolation->vert_indices[0] = ctx->coarse_corner_verts[coarse_face.start() + 0];
    vert_interpolation->vert_indices[1] = ctx->coarse_corner_verts[coarse_face.start() + 1];
    vert_interpolation->vert_indices[2] = ctx->coarse_corner_verts[coarse_face.start() + 2];
    vert_interpolation->vert_indices[3] = ctx->coarse_corner_verts[coarse_face.start() + 3];
  }
  else {
    vert_interpolation->vert_data = vert_interpolation->storage_spans;
    vert_interpolation->dverts_data = vert_interpolation->dverts_storage;
    vert_interpolation->CD_MVERT_SKIN_data = vert_interpolation->CD_MVERT_SKIN_storage;
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
    if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
      vert_interpolation->CD_MVERT_SKIN_storage[2] = mix_CD_MVERT_SKIN(
          ctx->coarse_CD_MVERT_SKIN, indices, weights);
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
    if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
      vert_interpolation->CD_MVERT_SKIN_storage[0] = ctx->coarse_CD_MVERT_SKIN[vert];
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
    if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
      vert_interpolation->CD_MVERT_SKIN_storage[1] = mix_CD_MVERT_SKIN(
          ctx->coarse_CD_MVERT_SKIN, first_indices, {0.5f, 0.5f});
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
    if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
      vert_interpolation->CD_MVERT_SKIN_storage[3] = mix_CD_MVERT_SKIN(
          ctx->coarse_CD_MVERT_SKIN, last_indices, {0.5f, 0.5f});
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
      loop_interpolation->CD_ORIGSPACE_MLOOP_storage[2] = mix_attr(
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
/** \name TLS
 * \{ */

struct SubdivMeshTLS {
  VerticesForInterpolation *vert_interpolation = nullptr;
  int vert_interpolation_coarse_face_index = -1;
  int vert_interpolation_coarse_corner = -1;

  LoopsForInterpolation *loop_interpolation = nullptr;
  int loop_interpolation_coarse_face_index = -1;
  int loop_interpolation_coarse_corner = -1;
};

static void subdiv_mesh_tls_free(void *tls_v)
{
  SubdivMeshTLS *tls = static_cast<SubdivMeshTLS *>(tls_v);
  delete tls->vert_interpolation;
  delete tls->loop_interpolation;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation helper functions
 * \{ */

static void subdiv_vert_orco_evaluate(const SubdivMeshContext *ctx,
                                      const int ptex_face_index,
                                      const float u,
                                      const float v,
                                      const int subdiv_vert_index)
{
  if (ctx->orco || ctx->cloth_orco) {
    float vert_data[6];
    eval_vert_data(ctx->subdiv, ptex_face_index, u, v, vert_data);

    if (ctx->orco) {
      copy_v3_v3(ctx->orco[subdiv_vert_index], vert_data);
      if (ctx->cloth_orco) {
        copy_v3_v3(ctx->cloth_orco[subdiv_vert_index], vert_data + 3);
      }
    }
    else if (ctx->cloth_orco) {
      copy_v3_v3(ctx->cloth_orco[subdiv_vert_index], vert_data);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Accumulation helpers
 * \{ */

static void subdiv_accumulate_vert_displacement(SubdivMeshContext *ctx,
                                                const int ptex_face_index,
                                                const float u,
                                                const float v,
                                                const int subdiv_vert_index)
{
  /* Accumulate displacement. */
  Subdiv *subdiv = ctx->subdiv;
  float3 dummy_P;
  float3 dPdu;
  float3 dPdv;
  float3 D;
  eval_limit_point_and_derivatives(subdiv, ptex_face_index, u, v, dummy_P, dPdu, dPdv);

  eval_displacement(subdiv, ptex_face_index, u, v, dPdu, dPdv, D);
  ctx->subdiv_positions[subdiv_vert_index] += D;

  if (ctx->accumulated_counters) {
    ++ctx->accumulated_counters[subdiv_vert_index];
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static bool subdiv_mesh_topology_info(const ForeachContext *foreach_context,
                                      const int num_vertices,
                                      const int num_edges,
                                      const int num_loops,
                                      const int num_faces,
                                      const int * /*subdiv_face_offset*/)
{
  /* Multi-resolution grid data will be applied or become invalid after subdivision,
   * so don't try to preserve it and use memory. Crease values should also not be interpolated. */
  CustomData_MeshMasks mask = CD_MASK_EVERYTHING;
  mask.lmask &= ~CD_MASK_MULTIRES_GRIDS;

  SubdivMeshContext *subdiv_context = static_cast<SubdivMeshContext *>(foreach_context->user_data);

  const Mesh &coarse_mesh = *subdiv_context->coarse_mesh;
  subdiv_context->subdiv_mesh = BKE_mesh_new_nomain(num_vertices, num_edges, num_faces, num_loops);
  Mesh &subdiv_mesh = *subdiv_context->subdiv_mesh;
  BKE_mesh_copy_parameters_for_eval(subdiv_context->subdiv_mesh, &coarse_mesh);

  if (num_faces != 0) {
    subdiv_mesh.face_offsets_for_write().last() = num_loops;
  }

  /* Create corner data for interpolation without topology attributes. */
  MutableAttributeAccessor attributes = subdiv_mesh.attributes_for_write();
  coarse_mesh.attributes().foreach_attribute([&](const AttributeIter &iter) {
    if (iter.data_type == AttrType::String) {
      return;
    }
    if (iter.domain == AttrDomain::Point) {
      if (ELEM(iter.name, "position")) {
        return;
      }
      subdiv_context->coarse_vert_attrs.append(*iter.get());
      subdiv_context->coarse_vert_attr_spans.append(subdiv_context->coarse_vert_attrs.last());
      subdiv_context->subdiv_vert_attrs.append(
          attributes.lookup_or_add_for_write_only_span(iter.name, iter.domain, iter.data_type));
      subdiv_context->subdiv_vert_attr_spans.append(subdiv_context->subdiv_vert_attrs.last().span);
    }
    else if (iter.domain == AttrDomain::Edge) {
      if (ELEM(iter.name, ".edge_verts")) {
        return;
      }
      subdiv_context->coarse_edge_attrs.append(*iter.get());
      subdiv_context->coarse_edge_attr_spans.append(subdiv_context->coarse_edge_attrs.last());
      subdiv_context->subdiv_edge_attrs.append(
          attributes.lookup_or_add_for_write_only_span(iter.name, iter.domain, iter.data_type));
      subdiv_context->subdiv_edge_attr_spans.append(subdiv_context->subdiv_edge_attrs.last().span);
    }
    else if (iter.domain == AttrDomain::Face) {
      subdiv_context->coarse_face_attrs.append(*iter.get());
      subdiv_context->coarse_face_attr_spans.append(subdiv_context->coarse_face_attrs.last());
      subdiv_context->subdiv_face_attrs.append(
          attributes.lookup_or_add_for_write_only_span(iter.name, iter.domain, iter.data_type));
      subdiv_context->subdiv_face_attr_spans.append(subdiv_context->subdiv_face_attrs.last().span);
    }
    else if (iter.domain == AttrDomain::Corner) {
      if (ELEM(iter.name, ".corner_vert", ".corner_edge")) {
        return;
      }
      if (iter.data_type == AttrType::Float2) {
        return;
      }
      subdiv_context->coarse_corner_attrs.append(*iter.get());
      subdiv_context->coarse_corner_attr_spans.append(subdiv_context->coarse_corner_attrs.last());
      subdiv_context->subdiv_corner_attrs.append(
          attributes.lookup_or_add_for_write_only_span(iter.name, iter.domain, iter.data_type));
      subdiv_context->subdiv_corner_attr_spans.append(
          subdiv_context->subdiv_corner_attrs.last().span);
    }
  });

  subdiv_mesh_ctx_cache_custom_data_layers(subdiv_context);
  subdiv_mesh_prepare_accumulator(subdiv_context, num_vertices);
  subdiv_mesh.runtime->subsurf_face_dot_tags.clear();
  subdiv_mesh.runtime->subsurf_face_dot_tags.resize(num_vertices);
  if (subdiv_context->settings->use_optimal_display) {
    subdiv_context->subdiv_display_edges = Array<bool>(num_edges, false);
  }
  return true;
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
  if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
    ctx->subdiv_CD_MVERT_SKIN[subdiv_vert_index] = ctx->coarse_CD_MVERT_SKIN[coarse_vert_index];
  }
}

static float4 quad_weights_from_uv(const float u, const float v)
{
  return {(1.0f - u) * (1.0f - v), u * (1.0f - v), u * v, (1.0f - u) * v};
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
  if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
    ctx->subdiv_CD_MVERT_SKIN[subdiv_vert_index] = mix_CD_MVERT_SKIN(
        vert_interpolation->CD_MVERT_SKIN_data,
        vert_interpolation->vert_indices,
        Span(&weights.x, 4));
  }
}

static void evaluate_vert_and_apply_displacement_copy(const SubdivMeshContext *ctx,
                                                      const int ptex_face_index,
                                                      const float u,
                                                      const float v,
                                                      const int coarse_vert_index,
                                                      const int subdiv_vert_index)
{
  float3 &subdiv_position = ctx->subdiv_positions[subdiv_vert_index];
  /* Displacement is accumulated in subdiv vertex position.
   * Needs to be backed up before copying data from original vertex. */
  float D[3] = {0.0f, 0.0f, 0.0f};
  if (ctx->have_displacement) {
    const float inv_num_accumulated = 1.0f / ctx->accumulated_counters[subdiv_vert_index];
    copy_v3_v3(D, subdiv_position);
    mul_v3_fl(D, inv_num_accumulated);
  }
  /* Copy custom data and evaluate position. */
  subdiv_vert_data_copy(ctx, coarse_vert_index, subdiv_vert_index);
  subdiv_position = eval_limit_point(ctx->subdiv, ptex_face_index, u, v);
  /* Apply displacement. */
  subdiv_position += D;
  /* Evaluate undeformed texture coordinate. */
  subdiv_vert_orco_evaluate(ctx, ptex_face_index, u, v, subdiv_vert_index);
  /* Remove face-dot flag. This can happen if there is more than one subsurf modifier. */
  ctx->subdiv_mesh->runtime->subsurf_face_dot_tags[subdiv_vert_index].reset();
}

static void evaluate_vert_and_apply_displacement_interpolate(
    const SubdivMeshContext *ctx,
    const int ptex_face_index,
    const float u,
    const float v,
    VerticesForInterpolation *vert_interpolation,
    const int subdiv_vert_index)
{
  float3 &subdiv_position = ctx->subdiv_positions[subdiv_vert_index];
  /* Displacement is accumulated in subdiv vertex position.
   * Needs to be backed up before copying data from original vertex. */
  float D[3] = {0.0f, 0.0f, 0.0f};
  if (ctx->have_displacement) {
    const float inv_num_accumulated = 1.0f / ctx->accumulated_counters[subdiv_vert_index];
    copy_v3_v3(D, subdiv_position);
    mul_v3_fl(D, inv_num_accumulated);
  }
  /* Interpolate custom data and evaluate position. */
  subdiv_vert_data_interpolate(ctx, subdiv_vert_index, vert_interpolation, u, v);
  subdiv_position = eval_limit_point(ctx->subdiv, ptex_face_index, u, v);
  /* Apply displacement. */
  add_v3_v3(subdiv_position, D);
  /* Evaluate undeformed texture coordinate. */
  subdiv_vert_orco_evaluate(ctx, ptex_face_index, u, v, subdiv_vert_index);
}

static void subdiv_mesh_vert_displacement_every_corner_or_edge(
    const ForeachContext *foreach_context,
    void * /*tls*/,
    const int ptex_face_index,
    const float u,
    const float v,
    const int subdiv_vert_index)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  subdiv_accumulate_vert_displacement(ctx, ptex_face_index, u, v, subdiv_vert_index);
}

static void subdiv_mesh_vert_displacement_every_corner(const ForeachContext *foreach_context,
                                                       void *tls,
                                                       const int ptex_face_index,
                                                       const float u,
                                                       const float v,
                                                       const int /*coarse_vert_index*/,
                                                       const int /*coarse_face_index*/,
                                                       const int /*coarse_corner*/,
                                                       const int subdiv_vert_index)
{
  subdiv_mesh_vert_displacement_every_corner_or_edge(
      foreach_context, tls, ptex_face_index, u, v, subdiv_vert_index);
}

static void subdiv_mesh_vert_displacement_every_edge(const ForeachContext *foreach_context,
                                                     void *tls,
                                                     const int ptex_face_index,
                                                     const float u,
                                                     const float v,
                                                     const int /*coarse_edge_index*/,
                                                     const int /*coarse_face_index*/,
                                                     const int /*coarse_corner*/,
                                                     const int subdiv_vert_index)
{
  subdiv_mesh_vert_displacement_every_corner_or_edge(
      foreach_context, tls, ptex_face_index, u, v, subdiv_vert_index);
}

static void subdiv_mesh_vert_corner(const ForeachContext *foreach_context,
                                    void * /*tls*/,
                                    const int ptex_face_index,
                                    const float u,
                                    const float v,
                                    const int coarse_vert_index,
                                    const int /*coarse_face_index*/,
                                    const int /*coarse_corner*/,
                                    const int subdiv_vert_index)
{
  BLI_assert(coarse_vert_index != ORIGINDEX_NONE);
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  evaluate_vert_and_apply_displacement_copy(
      ctx, ptex_face_index, u, v, coarse_vert_index, subdiv_vert_index);
}

static void subdiv_mesh_ensure_vert_interpolation(SubdivMeshContext *ctx,
                                                  SubdivMeshTLS *tls,
                                                  const int coarse_face_index,
                                                  const int coarse_corner)
{
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  if (!tls->vert_interpolation) {
    tls->vert_interpolation = new VerticesForInterpolation(*ctx);
  }
  if (tls->vert_interpolation_coarse_face_index != coarse_face_index) {
    vert_interpolation_from_face(ctx, tls->vert_interpolation, coarse_face);
    vert_interpolation_from_corner(ctx, tls->vert_interpolation, coarse_face, coarse_corner);
  }
  else if (tls->vert_interpolation_coarse_corner != coarse_corner) {
    vert_interpolation_from_corner(ctx, tls->vert_interpolation, coarse_face, coarse_corner);
  }
  tls->vert_interpolation_coarse_face_index = coarse_face_index;
  tls->vert_interpolation_coarse_corner = coarse_corner;
}

static void subdiv_mesh_vert_edge(const ForeachContext *foreach_context,
                                  void *tls_v,
                                  const int ptex_face_index,
                                  const float u,
                                  const float v,
                                  const int /*coarse_edge_index*/,
                                  const int coarse_face_index,
                                  const int coarse_corner,
                                  const int subdiv_vert_index)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  SubdivMeshTLS *tls = static_cast<SubdivMeshTLS *>(tls_v);
  subdiv_mesh_ensure_vert_interpolation(ctx, tls, coarse_face_index, coarse_corner);
  evaluate_vert_and_apply_displacement_interpolate(
      ctx, ptex_face_index, u, v, tls->vert_interpolation, subdiv_vert_index);
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

static void subdiv_mesh_tag_center_vert(const IndexRange coarse_face,
                                        const int subdiv_vert_index,
                                        const float u,
                                        const float v,
                                        Mesh *subdiv_mesh)
{
  if (subdiv_mesh_is_center_vert(coarse_face, u, v)) {
    subdiv_mesh->runtime->subsurf_face_dot_tags[subdiv_vert_index].set();
  }
}

static void subdiv_mesh_vert_inner(const ForeachContext *foreach_context,
                                   void *tls_v,
                                   const int ptex_face_index,
                                   const float u,
                                   const float v,
                                   const int coarse_face_index,
                                   const int coarse_corner,
                                   const int subdiv_vert_index)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  SubdivMeshTLS *tls = static_cast<SubdivMeshTLS *>(tls_v);
  Subdiv *subdiv = ctx->subdiv;
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  Mesh *subdiv_mesh = ctx->subdiv_mesh;
  subdiv_mesh_ensure_vert_interpolation(ctx, tls, coarse_face_index, coarse_corner);
  subdiv_vert_data_interpolate(ctx, subdiv_vert_index, tls->vert_interpolation, u, v);
  ctx->subdiv_positions[subdiv_vert_index] = eval_final_point(subdiv, ptex_face_index, u, v);
  subdiv_mesh_tag_center_vert(coarse_face, subdiv_vert_index, u, v, subdiv_mesh);
  subdiv_vert_orco_evaluate(ctx, ptex_face_index, u, v, subdiv_vert_index);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge subdivision process
 * \{ */

static void subdiv_copy_edge_data(SubdivMeshContext *ctx,
                                  const int subdiv_edge_index,
                                  const int coarse_edge_index)
{
  if (coarse_edge_index == ORIGINDEX_NONE) {
    if (!ctx->coarse_edge_origindex.is_empty()) {
      ctx->subdiv_edge_origindex[subdiv_edge_index] = ORIGINDEX_NONE;
    }
    for (const int attr : ctx->coarse_edge_attrs.index_range()) {
      const CPPType &type = ctx->coarse_edge_attrs[attr].type();
      type.copy_construct(type.default_value(),
                          ctx->subdiv_edge_attr_spans[attr][subdiv_edge_index]);
    }
    return;
  }
  copy_attrs(ctx->coarse_edge_attr_spans,
             coarse_edge_index,
             subdiv_edge_index,
             ctx->subdiv_edge_attr_spans);
  if (!ctx->coarse_edge_origindex.is_empty()) {
    ctx->subdiv_edge_origindex[subdiv_edge_index] = ctx->coarse_edge_origindex[coarse_edge_index];
  }
  if (ctx->settings->use_optimal_display) {
    ctx->subdiv_display_edges[subdiv_edge_index] = true;
  }
}

static void subdiv_mesh_edge(const ForeachContext *foreach_context,
                             void * /*tls*/,
                             const int coarse_edge_index,
                             const int subdiv_edge_index,
                             const bool /*is_loose*/,
                             const int subdiv_v1,
                             const int subdiv_v2)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  subdiv_copy_edge_data(ctx, subdiv_edge_index, coarse_edge_index);
  ctx->subdiv_edges[subdiv_edge_index][0] = subdiv_v1;
  ctx->subdiv_edges[subdiv_edge_index][1] = subdiv_v2;
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

static void subdiv_eval_uv_layer(SubdivMeshContext *ctx,
                                 const int corner_index,
                                 const int ptex_face_index,
                                 const float u,
                                 const float v)
{
  Subdiv *subdiv = ctx->subdiv;
  for (const int i : ctx->uv_maps.index_range()) {
    eval_face_varying(subdiv, i, ptex_face_index, u, v, ctx->uv_maps[i].span[corner_index]);
  }
}

static void subdiv_mesh_ensure_loop_interpolation(SubdivMeshContext *ctx,
                                                  SubdivMeshTLS *tls,
                                                  const int coarse_face_index,
                                                  const int coarse_corner)
{
  const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
  if (!tls->loop_interpolation) {
    tls->loop_interpolation = new LoopsForInterpolation(*ctx);
  }
  if (tls->loop_interpolation_coarse_face_index != coarse_face_index) {
    loop_interpolation_from_face(ctx, tls->loop_interpolation, coarse_face);
    loop_interpolation_from_corner(ctx, tls->loop_interpolation, coarse_face, coarse_corner);
  }
  else if (tls->loop_interpolation_coarse_corner != coarse_corner) {
    loop_interpolation_from_corner(ctx, tls->loop_interpolation, coarse_face, coarse_corner);
  }
  tls->loop_interpolation_coarse_face_index = coarse_face_index;
  tls->loop_interpolation_coarse_corner = coarse_corner;
}

static void subdiv_mesh_loop(const ForeachContext *foreach_context,
                             void *tls_v,
                             const int ptex_face_index,
                             const float u,
                             const float v,
                             const int /*coarse_loop_index*/,
                             const int coarse_face_index,
                             const int coarse_corner,
                             const int subdiv_loop_index,
                             const int subdiv_vert_index,
                             const int subdiv_edge_index)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  SubdivMeshTLS *tls = static_cast<SubdivMeshTLS *>(tls_v);
  subdiv_mesh_ensure_loop_interpolation(ctx, tls, coarse_face_index, coarse_corner);
  subdiv_interpolate_corner_data(ctx, subdiv_loop_index, tls->loop_interpolation, u, v);
  subdiv_eval_uv_layer(ctx, subdiv_loop_index, ptex_face_index, u, v);
  ctx->subdiv_corner_verts[subdiv_loop_index] = subdiv_vert_index;
  ctx->subdiv_corner_edges[subdiv_loop_index] = subdiv_edge_index;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Polygons subdivision process
 * \{ */

static void subdiv_mesh_face(const ForeachContext *foreach_context,
                             void * /*tls*/,
                             const int coarse_face_index,
                             const int subdiv_face_index,
                             const int start_loop_index,
                             const int /*num_loops*/)
{
  BLI_assert(coarse_face_index != ORIGINDEX_NONE);
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  copy_attrs(ctx->coarse_face_attr_spans,
             coarse_face_index,
             subdiv_face_index,
             ctx->subdiv_face_attr_spans);
  if (!ctx->coarse_face_origindex.is_empty()) {
    ctx->subdiv_face_origindex[subdiv_face_index] = ctx->coarse_face_origindex[coarse_face_index];
  }
  ctx->subdiv_face_offsets[subdiv_face_index] = start_loop_index;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Loose elements subdivision process
 * \{ */

static void subdiv_mesh_vert_loose(const ForeachContext *foreach_context,
                                   void * /*tls*/,
                                   const int coarse_vert_index,
                                   const int subdiv_vert_index)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  subdiv_vert_data_copy(ctx, coarse_vert_index, subdiv_vert_index);
  ctx->subdiv_positions[subdiv_vert_index] = ctx->coarse_positions[coarse_vert_index];
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
  if (!ctx->coarse_CD_MVERT_SKIN.is_empty()) {
    ctx->subdiv_CD_MVERT_SKIN[subdiv_vert_index] = mix_CD_MVERT_SKIN(
        ctx->coarse_CD_MVERT_SKIN, coarse_vert_indices, {1.0f - u, u});
  }
}

static void subdiv_mesh_vert_of_loose_edge(const ForeachContext *foreach_context,
                                           void * /*tls*/,
                                           const int coarse_edge_index,
                                           const float u,
                                           const int subdiv_vert_index)
{
  SubdivMeshContext *ctx = static_cast<SubdivMeshContext *>(foreach_context->user_data);
  const int2 &coarse_edge = ctx->coarse_edges[coarse_edge_index];
  const bool is_simple = ctx->subdiv->settings.is_simple;

  /* Interpolate custom data when not an end point.
   * This data has already been copied from the original vertex by #subdiv_mesh_vert_loose. */
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Initialization
 * \{ */

static void setup_foreach_callbacks(const SubdivMeshContext *subdiv_context,
                                    ForeachContext *foreach_context)
{
  *foreach_context = {};
  /* General information. */
  foreach_context->topology_info = subdiv_mesh_topology_info;
  /* Every boundary geometry. Used for displacement averaging. */
  if (subdiv_context->have_displacement) {
    foreach_context->vert_every_corner = subdiv_mesh_vert_displacement_every_corner;
    foreach_context->vert_every_edge = subdiv_mesh_vert_displacement_every_edge;
  }
  foreach_context->vert_corner = subdiv_mesh_vert_corner;
  foreach_context->vert_edge = subdiv_mesh_vert_edge;
  foreach_context->vert_inner = subdiv_mesh_vert_inner;
  foreach_context->edge = subdiv_mesh_edge;
  foreach_context->loop = subdiv_mesh_loop;
  foreach_context->poly = subdiv_mesh_face;
  foreach_context->vert_loose = subdiv_mesh_vert_loose;
  foreach_context->vert_of_loose_edge = subdiv_mesh_vert_of_loose_edge;
  foreach_context->user_data_tls_free = subdiv_mesh_tls_free;
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
  if (coarse_mesh->loose_edges().count > 0) {
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
  ForeachContext foreach_context;
  setup_foreach_callbacks(&subdiv_context, &foreach_context);
  SubdivMeshTLS tls{};
  foreach_context.user_data = &subdiv_context;
  foreach_context.user_data_tls_size = sizeof(SubdivMeshTLS);
  foreach_context.user_data_tls = &tls;
  foreach_subdiv_geometry(subdiv, &foreach_context, settings, coarse_mesh);
  stats_end(&subdiv->stats, SUBDIV_STATS_SUBDIV_TO_MESH_GEOMETRY);
  Mesh *result = subdiv_context.subdiv_mesh;

  /* NOTE: Using normals from the limit surface gives different results than Blender's vertex
   * normal calculation. Since vertex normals are supposed to be a consistent cache, don't bother
   * calculating them here. The work may have been pointless anyway if the mesh is deformed or
   * changed afterwards. */

  /* Move the optimal display edge array to the final bit vector. */
  if (!subdiv_context.subdiv_display_edges.is_empty()) {
    result->runtime->subsurf_optimal_display_edges = BitVector<>(
        subdiv_context.subdiv_display_edges);
  }

  if (coarse_mesh->verts_no_face().count == 0) {
    result->tag_loose_verts_none();
  }
  if (coarse_mesh->loose_edges().count == 0) {
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
  subdiv_mesh_context_free(&subdiv_context);
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
