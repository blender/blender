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
/** \name Custom data interpolation
 *
 * Data for the inside of base mesh faces is interpolated bilinearly within ptex faces. N-gons have
 * a ptex face for every corner, with the corner itself, the midpoint of the next edge, the face
 * center, and the midpoint of the previous edge as its corners (at the (0, 0), (1, 0), (1, 1) and
 * (0, 1) ptex face coordinates).
 *
 * To reduce overhead, the elements are processed in chunks, with a single type dispatch for every
 * attribute in each chunk.
 * \{ */

/** A range of subdivided elements inside of a base mesh face. */
struct ElementRange {
  /** Indices of the elements' ptex face coordinates and weights in the arrays for a chunk. */
  IndexRange coords;
  /** The subdivided elements, aligned with #coords. */
  IndexRange elements;
};

/**
 * Subdivided elements inside of base mesh faces, grouped by face so that values used by all of a
 * face's elements are only computed once. A face can have multiple ranges of elements, like the
 * vertices inside of each of its edges.
 */
struct FaceElements {
  Vector<int> faces;
  /** Groups #ranges by face. */
  Vector<int> offsets = {0};
  Vector<ElementRange> ranges;

  /** Add a group for a face with the ranges added since the last group. */
  void add_face(const int face)
  {
    if (this->ranges.size() > this->offsets.last()) {
      this->faces.append(face);
      this->offsets.append(this->ranges.size());
    }
  }

  OffsetIndices<int> ranges_by_face() const
  {
    return this->offsets.as_span();
  }
};

static Array<float4> quad_weights_from_coords(const Span<PtexCoord> coords)
{
  Array<float4> weights(coords.size());
  for (const int i : coords.index_range()) {
    weights[i] = quad_weights_from_uv(coords[i].uv.x, coords[i].uv.y);
  }
  return weights;
}

/** Mixing for generic attributes. */
template<typename T> struct AttributeMixer {
  static T center(const Span<T> src, const Span<int> indices, const Span<float> weights)
  {
    return attribute_math::mix_indices(src, indices, weights);
  }
  static T mid(const T &a, const T &b)
  {
    if constexpr (std::is_same_v<T, bool>) {
      return a || b;
    }
    else {
      return attribute_math::mix2(0.5f, a, b);
    }
  }
  static T quad(const float4 &w, const T &a, const T &b, const T &c, const T &d)
  {
    if constexpr (std::is_same_v<T, bool>) {
      return (a && w.x > 0.0f) || (b && w.y > 0.0f) || (c && w.z > 0.0f) || (d && w.w > 0.0f);
    }
    else {
      return attribute_math::mix4(w, a, b, c, d);
    }
  }
};

/** Mixing for custom normals, keeping them normalized. */
struct NormalMixer {
  static float3 center(const Span<float3> src, const Span<int> indices, const Span<float> weights)
  {
    float3 result(0);
    for (const int i : indices.index_range()) {
      result += src[indices[i]] * weights[i];
    }
    return math::normalize(result);
  }
  static float3 mid(const float3 &a, const float3 &b)
  {
    return math::normalize(math::interpolate(a, b, 0.5f));
  }
  static float3 quad(
      const float4 &w, const float3 &a, const float3 &b, const float3 &c, const float3 &d)
  {
    float3 result(0);
    result += a * w.x;
    result += b * w.y;
    result += c * w.z;
    result += d * w.w;
    return math::normalize(result);
  }
};

/**
 * Interpolate the values of subdivided elements inside of base mesh faces.
 * \param src_index: Gives the index in #src of the value for a base mesh face corner.
 */
template<typename T, typename Mixer, typename SrcIndexFn>
static void interpolate_in_faces(const OffsetIndices<int> faces,
                                 const Span<int> face_ptex_offset,
                                 const SrcIndexFn &src_index,
                                 const FaceElements &groups,
                                 const Span<PtexCoord> coords,
                                 const Span<float4> weights,
                                 const Span<T> src,
                                 MutableSpan<T> dst)
{
  const OffsetIndices<int> ranges_by_face = groups.ranges_by_face();
  Array<int, 16> face_src_indices;
  Array<float, 16> center_weights;
  Array<T, 16> edge_mids;
  for (const int group : groups.faces.index_range()) {
    const int face_index = groups.faces[group];
    const IndexRange face = faces[face_index];
    const Span<ElementRange> ranges = groups.ranges.as_span().slice(ranges_by_face[group]);
    if (face.size() == 4) {
      const T &v0 = src[src_index(face[0])];
      const T &v1 = src[src_index(face[1])];
      const T &v2 = src[src_index(face[2])];
      const T &v3 = src[src_index(face[3])];
      for (const ElementRange &range : ranges) {
        for (const int i : range.coords.index_range()) {
          dst[range.elements[i]] = Mixer::quad(weights[range.coords[i]], v0, v1, v2, v3);
        }
      }
      continue;
    }
    const int corners_num = face.size();
    face_src_indices.reinitialize(corners_num);
    for (const int i : face.index_range()) {
      face_src_indices[i] = src_index(face[i]);
    }
    center_weights.reinitialize(corners_num);
    center_weights.fill(1.0f / float(corners_num));
    const T center = Mixer::center(src, face_src_indices, center_weights);
    edge_mids.reinitialize(corners_num);
    for (const int i : face.index_range()) {
      edge_mids[i] = Mixer::mid(src[face_src_indices[i]],
                                src[face_src_indices[(i + 1) % corners_num]]);
    }
    for (const ElementRange &range : ranges) {
      for (const int i : range.coords.index_range()) {
        const int coord = range.coords[i];
        const int corner = coords[coord].ptex_face_index - face_ptex_offset[face_index];
        const int prev_corner = (corner + corners_num - 1) % corners_num;
        dst[range.elements[i]] = Mixer::quad(weights[coord],
                                             src[face_src_indices[corner]],
                                             edge_mids[corner],
                                             center,
                                             edge_mids[prev_corner]);
      }
    }
  }
}

static void interpolate_dverts_in_faces(const SubdivMeshContext *ctx,
                                        const Span<int> face_ptex_offset,
                                        const FaceElements &groups,
                                        const Span<PtexCoord> coords,
                                        const Span<float4> weights)
{
  const Span<MDeformVert> src = ctx->coarse_dverts;
  MutableSpan<MDeformVert> dst = ctx->subdiv_dverts;
  const OffsetIndices<int> ranges_by_face = groups.ranges_by_face();
  MDeformWeightSet dw_buffer;
  Array<float, 16> center_weights;
  Array<MDeformVert, 16> edge_mids;
  for (const int group : groups.faces.index_range()) {
    const int face_index = groups.faces[group];
    const IndexRange face = ctx->coarse_faces[face_index];
    const Span<int> face_verts = ctx->coarse_corner_verts.slice(face);
    const Span<ElementRange> ranges = groups.ranges.as_span().slice(ranges_by_face[group]);
    if (face.size() == 4) {
      for (const ElementRange &range : ranges) {
        for (const int i : range.coords.index_range()) {
          dst[range.elements[i]] = mix_deform_verts(
              src, face_verts, Span(&weights[range.coords[i]].x, 4), dw_buffer);
        }
      }
      continue;
    }
    const int corners_num = face.size();
    center_weights.reinitialize(corners_num);
    center_weights.fill(1.0f / float(corners_num));
    MDeformVert center = mix_deform_verts(src, face_verts, center_weights, dw_buffer);
    edge_mids.reinitialize(corners_num);
    for (const int i : face.index_range()) {
      const std::array<int, 2> indices{face_verts[i], face_verts[(i + 1) % corners_num]};
      edge_mids[i] = mix_deform_verts(src, indices, {0.5f, 0.5f}, dw_buffer);
    }
    for (const ElementRange &range : ranges) {
      for (const int i : range.coords.index_range()) {
        const int coord = range.coords[i];
        const int corner = coords[coord].ptex_face_index - face_ptex_offset[face_index];
        const int prev_corner = (corner + corners_num - 1) % corners_num;
        const std::array<MDeformVert, 4> sources{
            src[face_verts[corner]], edge_mids[corner], center, edge_mids[prev_corner]};
        dst[range.elements[i]] = mix_deform_verts(
            sources, {0, 1, 2, 3}, Span(&weights[coord].x, 4), dw_buffer);
      }
    }
    BKE_defvert_array_free_elems(&center, 1);
    BKE_defvert_array_free_elems(edge_mids.data(), corners_num);
  }
}

/** Interpolate the data of subdivided vertices inside of base mesh faces. */
static void interpolate_vert_data(const SubdivMeshContext *ctx,
                                  const Span<int> face_ptex_offset,
                                  const FaceElements &groups,
                                  const Span<PtexCoord> coords)
{
  const Array<float4> weights = quad_weights_from_coords(coords);
  const auto src_index = [&](const int corner) { return ctx->coarse_corner_verts[corner]; };
  for (const int attr : ctx->coarse_vert_attr_spans.index_range()) {
    const GSpan src = ctx->coarse_vert_attr_spans[attr];
    GMutableSpan dst = ctx->subdiv_vert_attr_spans[attr];
    attribute_math::to_static_type(src.type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        interpolate_in_faces<T, AttributeMixer<T>>(ctx->coarse_faces,
                                                   face_ptex_offset,
                                                   src_index,
                                                   groups,
                                                   coords,
                                                   weights,
                                                   src.typed<T>(),
                                                   dst.typed<T>());
      }
    });
  }
  if (!ctx->coarse_vert_origindex.is_empty()) {
    for (const ElementRange &range : groups.ranges) {
      ctx->subdiv_vert_origindex.slice(range.elements).fill(ORIGINDEX_NONE);
    }
  }
  if (!ctx->coarse_dverts.is_empty()) {
    interpolate_dverts_in_faces(ctx, face_ptex_offset, groups, coords, weights);
  }
}

/** Interpolate the data of subdivided face corners. */
static void interpolate_corner_data(const SubdivMeshContext *ctx,
                                    const Span<int> face_ptex_offset,
                                    const FaceElements &groups,
                                    const Span<PtexCoord> coords)
{
  const Array<float4> weights = quad_weights_from_coords(coords);
  const auto src_index = [&](const int corner) { return corner; };
  for (const int attr : ctx->coarse_corner_attr_spans.index_range()) {
    const GSpan src = ctx->coarse_corner_attr_spans[attr];
    GMutableSpan dst = ctx->subdiv_corner_attr_spans[attr];
    attribute_math::to_static_type(src.type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        interpolate_in_faces<T, AttributeMixer<T>>(ctx->coarse_faces,
                                                   face_ptex_offset,
                                                   src_index,
                                                   groups,
                                                   coords,
                                                   weights,
                                                   src.typed<T>(),
                                                   dst.typed<T>());
      }
    });
  }
  if (!ctx->coarse_CD_NORMAL.is_empty()) {
    interpolate_in_faces<float3, NormalMixer>(ctx->coarse_faces,
                                              face_ptex_offset,
                                              src_index,
                                              groups,
                                              coords,
                                              weights,
                                              ctx->coarse_CD_NORMAL,
                                              ctx->subdiv_CD_NORMAL);
  }
  if (!ctx->coarse_CD_ORIGSPACE_MLOOP.is_empty()) {
    interpolate_in_faces<float2, AttributeMixer<float2>>(ctx->coarse_faces,
                                                         face_ptex_offset,
                                                         src_index,
                                                         groups,
                                                         coords,
                                                         weights,
                                                         ctx->coarse_CD_ORIGSPACE_MLOOP,
                                                         ctx->subdiv_CD_ORIGSPACE_MLOOP);
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
    Array<PtexCoord> edge_coords(edge_verts_num);
    Array<PtexCoord> other_edge_coords;

    /* The vertices processed by this task, which are evaluated together afterwards. */
    Vector<int> verts;
    Vector<PtexCoord> coords;
    /* The vertices inside of base mesh edges, interpolated from the first face using them. */
    FaceElements edge_vert_groups;
    /* The coordinates of the vertices in every face that uses them, for averaging displacement.
     * Every vertex has a group of coordinates. */
    Vector<int> displacement_offsets = {0};
    Vector<PtexCoord> displacement_coords;

    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      for (const int corner : face) {
        /* Process every vertex and edge only from the first face corner using it. */
        const int vert = corner_verts[corner];
        const Span<int> vert_corners = vert_to_corner[vert];
        if (vert_corners.first() == corner) {
          /* Subdivided vertices corresponding to base mesh vertices have the same indices. */
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
        corner_edge_ptex_coords(corner, edge, edge_coords);
        const IndexRange edge_verts = layout.base_edge_verts(edge);
        edge_vert_groups.ranges.append({IndexRange(coords.size(), edge_verts_num), edge_verts});
        for (const int edge_vert : edge_verts) {
          verts.append(edge_vert);
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
      edge_vert_groups.add_face(face_index);
    }

    interpolate_vert_data(ctx, face_ptex_offset, edge_vert_groups, coords);

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

        FaceElements groups;
        for (const int face : range) {
          const IndexRange face_verts = inner_verts_by_face[face];
          groups.ranges.append({face_verts.shift(-chunk_verts.start()),
                                face_verts.shift(layout.inner_verts_start)});
          groups.add_face(face);
        }
        interpolate_vert_data(ctx, face_ptex_offset, groups, ptex_coords);

        for (const int coarse_face_index : range) {
          const IndexRange coarse_face = ctx->coarse_faces[coarse_face_index];
          for (const int vert : inner_verts_by_face[coarse_face_index]) {
            const float2 &uv = ptex_coords[vert - chunk_verts.start()].uv;
            if (subdiv_mesh_is_center_vert(coarse_face, uv.x, uv.y)) {
              ctx->subdiv_face_dot_tags[layout.inner_verts_start + vert] = true;
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

static void subdiv_mesh_corner_data(SubdivMeshContext *ctx, const MeshLayout &layout)
{
  const Span<int> face_ptex_offset = face_ptex_offset_get(ctx->subdiv);
  threading::parallel_for(
      ctx->coarse_faces.index_range(), layout.base_face_grain_size(), [&](const IndexRange range) {
        const IndexRange chunk_corners = layout.base_face_corners(range);
        Array<PtexCoord> ptex_coords(chunk_corners.size());
        fill_corner_ptex_coords(
            layout, *ctx->coarse_mesh, face_ptex_offset, range, ptex_coords, {});

        for (const int i : ctx->uv_maps.index_range()) {
          eval_face_varying(
              ctx->subdiv, i, ptex_coords, ctx->uv_maps[i].span.slice(chunk_corners));
        }

        FaceElements groups;
        for (const int face : range) {
          const IndexRange face_corners = layout.base_face_corners(face);
          groups.ranges.append({face_corners.shift(-chunk_corners.start()), face_corners});
          groups.add_face(face);
        }
        interpolate_corner_data(ctx, face_ptex_offset, groups, ptex_coords);
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

static void subdiv_mesh_copy_base_verts(SubdivMeshContext *ctx)
{
  /* Subdivided vertices corresponding to base mesh vertices have the same indices. */
  const int verts_num = ctx->coarse_mesh->verts_num;
  for (const int attr : ctx->coarse_vert_attr_spans.index_range()) {
    array_utils::copy(GVArray::from_span(ctx->coarse_vert_attr_spans[attr]),
                      ctx->subdiv_vert_attr_spans[attr].take_front(verts_num));
  }
  if (!ctx->coarse_vert_origindex.is_empty()) {
    array_utils::copy(ctx->coarse_vert_origindex,
                      ctx->subdiv_vert_origindex.take_front(verts_num));
  }
  if (!ctx->coarse_dverts.is_empty()) {
    gather_deform_verts(
        ctx->coarse_dverts, IndexMask(verts_num), ctx->subdiv_dverts.take_front(verts_num));
  }
  /* The positions are only copied for vertices that aren't used by faces, other vertices are
   * evaluated on the limit surface. */
  array_utils::copy(ctx->coarse_positions,
                    ctx->coarse_mesh->verts_no_face(),
                    ctx->subdiv_positions.take_front(verts_num));
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

/** Interpolate the data of the vertices inside of loose edges from the edges' vertices. */
static void interpolate_loose_edge_vert_data(const SubdivMeshContext *ctx,
                                             const MeshLayout &layout,
                                             const IndexMask &loose_edges)
{
  const Span<int2> edges = ctx->coarse_edges;
  const float inv_resolution_1 = 1.0f / float(layout.resolution - 1);
  for (const int attr : ctx->coarse_vert_attr_spans.index_range()) {
    const GSpan src_generic = ctx->coarse_vert_attr_spans[attr];
    GMutableSpan dst_generic = ctx->subdiv_vert_attr_spans[attr];
    attribute_math::to_static_type(src_generic.type(), [&]<typename T>() {
      if constexpr (!std::is_same_v<T, std::string>) {
        const Span<T> src = src_generic.typed<T>();
        MutableSpan<T> dst = dst_generic.typed<T>();
        loose_edges.foreach_index([&](const int edge) {
          const T &a = src[edges[edge][0]];
          const T &b = src[edges[edge][1]];
          const IndexRange edge_verts = layout.base_edge_verts(edge);
          for (const int i : edge_verts.index_range()) {
            const float u = (i + 1) * inv_resolution_1;
            if constexpr (std::is_same_v<T, bool>) {
              dst[edge_verts[i]] = (a && 1.0f - u > 0.0f) || (b && u > 0.0f);
            }
            else {
              dst[edge_verts[i]] = attribute_math::mix2(u, a, b);
            }
          }
        });
      }
    });
  }
  if (!ctx->coarse_vert_origindex.is_empty()) {
    loose_edges.foreach_index([&](const int edge) {
      ctx->subdiv_vert_origindex.slice(layout.base_edge_verts(edge)).fill(ORIGINDEX_NONE);
    });
  }
  if (!ctx->coarse_dverts.is_empty()) {
    MDeformWeightSet dvert_mix_buffer;
    loose_edges.foreach_index([&](const int edge) {
      const std::array<int, 2> verts{edges[edge][0], edges[edge][1]};
      const IndexRange edge_verts = layout.base_edge_verts(edge);
      for (const int i : edge_verts.index_range()) {
        const float u = (i + 1) * inv_resolution_1;
        ctx->subdiv_dverts[edge_verts[i]] = mix_deform_verts(
            ctx->coarse_dverts, verts, {1.0f - u, u}, dvert_mix_buffer);
      }
    });
  }
}

static void subdiv_mesh_vert_of_loose_edge(SubdivMeshContext *ctx,
                                           const int coarse_edge_index,
                                           const float u,
                                           const int subdiv_vert_index)
{
  const bool is_simple = ctx->subdiv->settings.is_simple;
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
  const IndexMask &loose_edges = ctx->coarse_mesh->loose_edges();
  threading::parallel_for(loose_edges.index_range(), 64, [&](const IndexRange range) {
    const IndexMask chunk = loose_edges.slice(range);
    /* The data of the end points has been copied from the base mesh vertices already. */
    interpolate_loose_edge_vert_data(ctx, layout, chunk);
    chunk.foreach_index([&](const int coarse_edge_index) {
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
    });
  });
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
  subdiv_mesh_copy_base_verts(&subdiv_context);
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

  bke::MutableAttributeAccessor attributes = result->attributes_for_write();
  for (const StringRef name : {"skin_modifier_root", "skin_modifier_loose"}) {
    bke::SpanAttributeWriter<bool> attr = attributes.lookup_for_write_span<bool>(name);
    if (attr && attr.domain == AttrDomain::Point) {
      attr.span.drop_front(coarse_mesh->verts_num).fill(false);
      attr.finish();
    }
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
