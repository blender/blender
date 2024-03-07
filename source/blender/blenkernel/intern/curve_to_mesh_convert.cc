/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"

#include "BKE_curve_to_mesh.hh"

namespace blender::bke {

static int segments_num_no_duplicate_edge(const int points_num, const bool cyclic)
{
  if (points_num <= 2) {
    return curves::segments_num(points_num, false);
  }
  return curves::segments_num(points_num, cyclic);
}

static void fill_mesh_topology(const int vert_offset,
                               const int edge_offset,
                               const int face_offset,
                               const int loop_offset,
                               const int main_point_num,
                               const int profile_point_num,
                               const bool main_cyclic,
                               const bool profile_cyclic,
                               const bool fill_caps,
                               MutableSpan<int2> edges,
                               MutableSpan<int> corner_verts,
                               MutableSpan<int> corner_edges,
                               MutableSpan<int> face_offsets)
{
  const int main_segment_num = segments_num_no_duplicate_edge(main_point_num, main_cyclic);
  const int profile_segment_num = curves::segments_num(profile_point_num, profile_cyclic);

  if (profile_point_num == 1) {
    for (const int i : IndexRange(main_point_num - 1)) {
      int2 &edge = edges[edge_offset + i];
      edge[0] = vert_offset + i;
      edge[1] = vert_offset + i + 1;
    }

    if (main_cyclic && main_segment_num > 2) {
      int2 &edge = edges[edge_offset + main_segment_num - 1];
      edge[0] = vert_offset + main_point_num - 1;
      edge[1] = vert_offset;
    }
    return;
  }

  /* Add the edges running along the length of the curve, starting at each profile vertex. */
  const int main_edges_start = edge_offset;
  for (const int i_profile : IndexRange(profile_point_num)) {
    const int profile_edge_offset = main_edges_start + i_profile * main_segment_num;
    for (const int i_ring : IndexRange(main_segment_num)) {
      const int i_next_ring = (i_ring == main_point_num - 1) ? 0 : i_ring + 1;

      const int ring_vert_offset = vert_offset + profile_point_num * i_ring;
      const int next_ring_vert_offset = vert_offset + profile_point_num * i_next_ring;

      int2 &edge = edges[profile_edge_offset + i_ring];
      edge[0] = ring_vert_offset + i_profile;
      edge[1] = next_ring_vert_offset + i_profile;
    }
  }

  /* Add the edges running along each profile ring. */
  const int profile_edges_start = main_edges_start + profile_point_num * main_segment_num;
  for (const int i_ring : IndexRange(main_point_num)) {
    const int ring_vert_offset = vert_offset + profile_point_num * i_ring;

    const int ring_edge_offset = profile_edges_start + i_ring * profile_segment_num;
    for (const int i_profile : IndexRange(profile_segment_num)) {
      const int i_next_profile = (i_profile == profile_point_num - 1) ? 0 : i_profile + 1;

      int2 &edge = edges[ring_edge_offset + i_profile];
      edge[0] = ring_vert_offset + i_profile;
      edge[1] = ring_vert_offset + i_next_profile;
    }
  }

  /* Calculate face and corner indices. */
  for (const int i_ring : IndexRange(main_segment_num)) {
    const int i_next_ring = (i_ring == main_point_num - 1) ? 0 : i_ring + 1;

    const int ring_vert_offset = vert_offset + profile_point_num * i_ring;
    const int next_ring_vert_offset = vert_offset + profile_point_num * i_next_ring;

    const int ring_edge_start = profile_edges_start + profile_segment_num * i_ring;
    const int next_ring_edge_offset = profile_edges_start + profile_segment_num * i_next_ring;

    const int ring_face_offset = face_offset + i_ring * profile_segment_num;
    const int ring_loop_offset = loop_offset + i_ring * profile_segment_num * 4;

    for (const int i_profile : IndexRange(profile_segment_num)) {
      const int ring_segment_loop_offset = ring_loop_offset + i_profile * 4;
      const int i_next_profile = (i_profile == profile_point_num - 1) ? 0 : i_profile + 1;

      const int main_edge_start = main_edges_start + main_segment_num * i_profile;
      const int next_main_edge_start = main_edges_start + main_segment_num * i_next_profile;

      face_offsets[ring_face_offset + i_profile] = ring_segment_loop_offset;

      corner_verts[ring_segment_loop_offset] = ring_vert_offset + i_profile;
      corner_edges[ring_segment_loop_offset] = ring_edge_start + i_profile;

      corner_verts[ring_segment_loop_offset + 1] = ring_vert_offset + i_next_profile;
      corner_edges[ring_segment_loop_offset + 1] = next_main_edge_start + i_ring;

      corner_verts[ring_segment_loop_offset + 2] = next_ring_vert_offset + i_next_profile;
      corner_edges[ring_segment_loop_offset + 2] = next_ring_edge_offset + i_profile;

      corner_verts[ring_segment_loop_offset + 3] = next_ring_vert_offset + i_profile;
      corner_edges[ring_segment_loop_offset + 3] = main_edge_start + i_ring;
    }
  }

  const bool has_caps = fill_caps && !main_cyclic && profile_cyclic && profile_point_num > 2;
  if (has_caps) {
    const int face_num = main_segment_num * profile_segment_num;
    const int cap_loop_offset = loop_offset + face_num * 4;
    const int cap_face_offset = face_offset + face_num;

    face_offsets[cap_face_offset] = cap_loop_offset;
    face_offsets[cap_face_offset + 1] = cap_loop_offset + profile_segment_num;

    const int last_ring_index = main_point_num - 1;
    const int last_ring_vert_offset = vert_offset + profile_point_num * last_ring_index;
    const int last_ring_edge_offset = profile_edges_start + profile_segment_num * last_ring_index;

    for (const int i : IndexRange(profile_segment_num)) {
      const int i_inv = profile_segment_num - i - 1;
      corner_verts[cap_loop_offset + i] = vert_offset + i_inv;
      corner_edges[cap_loop_offset + i] = profile_edges_start + ((i == (profile_segment_num - 1)) ?
                                                                     (profile_segment_num - 1) :
                                                                     (i_inv - 1));
      corner_verts[cap_loop_offset + profile_segment_num + i] = last_ring_vert_offset + i;
      corner_edges[cap_loop_offset + profile_segment_num + i] = last_ring_edge_offset + i;
    }
  }
}

/** Set the sharp status for edges that correspond to control points with vector handles. */
static void mark_bezier_vector_edges_sharp(const int profile_point_num,
                                           const int main_segment_num,
                                           const Span<int> control_point_offsets,
                                           const Span<int8_t> handle_types_left,
                                           const Span<int8_t> handle_types_right,
                                           MutableSpan<bool> sharp_edges)
{
  const int main_edges_start = 0;
  if (curves::bezier::point_is_sharp(handle_types_left, handle_types_right, 0)) {
    sharp_edges.slice(main_edges_start, main_segment_num).fill(true);
  }

  for (const int i : IndexRange(profile_point_num).drop_front(1)) {
    if (curves::bezier::point_is_sharp(handle_types_left, handle_types_right, i)) {
      const int offset = main_edges_start + main_segment_num * control_point_offsets[i];
      sharp_edges.slice(offset, main_segment_num).fill(true);
    }
  }
}

static void fill_mesh_positions(const int main_point_num,
                                const int profile_point_num,
                                const Span<float3> main_positions,
                                const Span<float3> profile_positions,
                                const Span<float3> tangents,
                                const Span<float3> normals,
                                const Span<float> radii,
                                MutableSpan<float3> mesh_positions)
{
  if (profile_point_num == 1) {
    for (const int i_ring : IndexRange(main_point_num)) {
      float4x4 point_matrix = math::from_orthonormal_axes<float4x4>(
          main_positions[i_ring], normals[i_ring], tangents[i_ring]);
      if (!radii.is_empty()) {
        point_matrix = math::scale(point_matrix, float3(radii[i_ring]));
      }
      mesh_positions[i_ring] = math::transform_point(point_matrix, profile_positions.first());
    }
  }
  else {
    for (const int i_ring : IndexRange(main_point_num)) {
      float4x4 point_matrix = math::from_orthonormal_axes<float4x4>(
          main_positions[i_ring], normals[i_ring], tangents[i_ring]);
      if (!radii.is_empty()) {
        point_matrix = math::scale(point_matrix, float3(radii[i_ring]));
      }

      const int ring_vert_start = i_ring * profile_point_num;
      for (const int i_profile : IndexRange(profile_point_num)) {
        mesh_positions[ring_vert_start + i_profile] = math::transform_point(
            point_matrix, profile_positions[i_profile]);
      }
    }
  }
}

struct CurvesInfo {
  const CurvesGeometry &main;
  const CurvesGeometry &profile;

  /* Make sure these are spans because they are potentially accessed many times. */
  VArraySpan<bool> main_cyclic;
  VArraySpan<bool> profile_cyclic;
};
static CurvesInfo get_curves_info(const CurvesGeometry &main, const CurvesGeometry &profile)
{
  return {main, profile, main.cyclic(), profile.cyclic()};
}

static bool offsets_contain_single_point(const OffsetIndices<int> offsets)
{
  for (const int64_t i : offsets.index_range()) {
    if (offsets[i].size() == 1) {
      return true;
    }
  }
  return false;
}

struct ResultOffsets {
  /** The total number of curve combinations. */
  int total;

  /** Offsets into the result mesh for each combination. */
  Array<int> vert;
  Array<int> edge;
  Array<int> loop;
  Array<int> face;

  /* The indices of the main and profile curves that form each combination. */
  Array<int> main_indices;
  Array<int> profile_indices;

  /** Whether any curve in the profile or curve input has only a single evaluated point. */
  bool any_single_point_main;
  bool any_single_point_profile;
};
static ResultOffsets calculate_result_offsets(const CurvesInfo &info, const bool fill_caps)
{
  ResultOffsets result;
  result.total = info.main.curves_num() * info.profile.curves_num();

  const OffsetIndices<int> main_offsets = info.main.evaluated_points_by_curve();
  const OffsetIndices<int> profile_offsets = info.profile.evaluated_points_by_curve();

  threading::parallel_invoke(
      result.total > 1024,
      [&]() {
        result.vert.reinitialize(result.total + 1);
        result.edge.reinitialize(result.total + 1);
        result.loop.reinitialize(result.total + 1);
        result.face.reinitialize(result.total + 1);

        int mesh_index = 0;
        int vert_offset = 0;
        int edge_offset = 0;
        int loop_offset = 0;
        int face_offset = 0;
        for (const int i_main : main_offsets.index_range()) {
          const bool main_cyclic = info.main_cyclic[i_main];
          const int main_point_num = main_offsets[i_main].size();
          const int main_segment_num = segments_num_no_duplicate_edge(main_point_num, main_cyclic);
          for (const int i_profile : profile_offsets.index_range()) {
            result.vert[mesh_index] = vert_offset;
            result.edge[mesh_index] = edge_offset;
            result.loop[mesh_index] = loop_offset;
            result.face[mesh_index] = face_offset;

            const bool profile_cyclic = info.profile_cyclic[i_profile];
            const int profile_point_num = profile_offsets[i_profile].size();
            const int profile_segment_num = curves::segments_num(profile_point_num,
                                                                 profile_cyclic);

            const bool has_caps = fill_caps && !main_cyclic && profile_cyclic &&
                                  profile_point_num > 2;
            const int tube_face_num = main_segment_num * profile_segment_num;

            vert_offset += main_point_num * profile_point_num;

            /* Add the ring edges, with one ring for every curve vertex, and the edge loops
             * that run along the length of the curve, starting on the first profile. */
            edge_offset += main_point_num * profile_segment_num +
                           main_segment_num * profile_point_num;

            /* Add two cap N-gons for every ending. */
            face_offset += tube_face_num + (has_caps ? 2 : 0);

            /* All faces on the tube are quads, and all cap faces are N-gons with an edge for each
             * profile edge. */
            loop_offset += tube_face_num * 4 + (has_caps ? profile_segment_num * 2 : 0);

            mesh_index++;
          }
        }

        result.vert.last() = vert_offset;
        result.edge.last() = edge_offset;
        result.loop.last() = loop_offset;
        result.face.last() = face_offset;
      },
      [&]() {
        result.main_indices.reinitialize(result.total);
        result.profile_indices.reinitialize(result.total);

        int mesh_index = 0;
        for (const int i_main : main_offsets.index_range()) {
          for (const int i_profile : profile_offsets.index_range()) {
            result.main_indices[mesh_index] = i_main;
            result.profile_indices[mesh_index] = i_profile;
            mesh_index++;
          }
        }
      },
      [&]() { result.any_single_point_main = offsets_contain_single_point(main_offsets); },
      [&]() { result.any_single_point_profile = offsets_contain_single_point(profile_offsets); });

  return result;
}

static AttrDomain get_attribute_domain_for_mesh(const AttributeAccessor &mesh_attributes,
                                                const AttributeIDRef &attribute_id)
{
  /* Only use a different domain if it is builtin and must only exist on one domain. */
  if (!mesh_attributes.is_builtin(attribute_id)) {
    return AttrDomain::Point;
  }

  std::optional<AttributeMetaData> meta_data = mesh_attributes.lookup_meta_data(attribute_id);
  if (!meta_data) {
    return AttrDomain::Point;
  }

  return meta_data->domain;
}

static bool should_add_attribute_to_mesh(const AttributeAccessor &curve_attributes,
                                         const AttributeAccessor &mesh_attributes,
                                         const AttributeIDRef &id,
                                         const AttributeMetaData &meta_data,
                                         const AnonymousAttributePropagationInfo &propagation_info)
{

  /* The position attribute has special non-generic evaluation. */
  if (id.name() == "position") {
    return false;
  }
  /* Don't propagate built-in curves attributes that are not built-in on meshes. */
  if (curve_attributes.is_builtin(id) && !mesh_attributes.is_builtin(id)) {
    return false;
  }
  if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
    return false;
  }
  if (meta_data.data_type == CD_PROP_STRING) {
    return false;
  }
  return true;
}

static GSpan evaluate_attribute(const GVArray &src,
                                const CurvesGeometry &curves,
                                Vector<std::byte> &buffer)
{
  if (curves.is_single_type(CURVE_TYPE_POLY) && src.is_span()) {
    return src.get_internal_span();
  }
  buffer.reinitialize(curves.evaluated_points_num() * src.type().size());
  GMutableSpan eval{src.type(), buffer.data(), curves.evaluated_points_num()};
  curves.interpolate_to_evaluated(src.get_internal_span(), eval);
  return eval;
}

/** Information at a specific combination of main and profile curves. */
struct CombinationInfo {
  int i_main;
  int i_profile;

  IndexRange main_points;
  IndexRange profile_points;

  bool main_cyclic;
  bool profile_cyclic;

  int main_segment_num;
  int profile_segment_num;

  IndexRange vert_range;
  IndexRange edge_range;
  IndexRange face_range;
  IndexRange loop_range;
};
template<typename Fn>
static void foreach_curve_combination(const CurvesInfo &info,
                                      const ResultOffsets &offsets,
                                      const Fn &fn)
{
  const OffsetIndices<int> main_offsets = info.main.evaluated_points_by_curve();
  const OffsetIndices<int> profile_offsets = info.profile.evaluated_points_by_curve();
  const OffsetIndices<int> vert_offsets(offsets.vert);
  const OffsetIndices<int> edge_offsets(offsets.edge);
  const OffsetIndices<int> face_offsets(offsets.face);
  const OffsetIndices<int> loop_offsets(offsets.loop);
  threading::parallel_for(IndexRange(offsets.total), 512, [&](IndexRange range) {
    for (const int i : range) {
      const int i_main = offsets.main_indices[i];
      const int i_profile = offsets.profile_indices[i];

      const IndexRange main_points = main_offsets[i_main];
      const IndexRange profile_points = profile_offsets[i_profile];

      const bool main_cyclic = info.main_cyclic[i_main];
      const bool profile_cyclic = info.profile_cyclic[i_profile];

      /* Pass all information in a struct to avoid repeating arguments in many lambdas.
       * The idea is that inlining `fn` will help avoid accessing unnecessary information,
       * though that may or may not happen in practice. */
      fn(CombinationInfo{i_main,
                         i_profile,
                         main_points,
                         profile_points,
                         main_cyclic,
                         profile_cyclic,
                         curves::segments_num(main_points.size(), main_cyclic),
                         curves::segments_num(profile_points.size(), profile_cyclic),
                         vert_offsets[i],
                         edge_offsets[i],
                         face_offsets[i],
                         loop_offsets[i]});
    }
  });
}

static void build_mesh_positions(const CurvesInfo &curves_info,
                                 const ResultOffsets &offsets,
                                 Vector<std::byte> &eval_buffer,
                                 Mesh &mesh)
{
  BLI_assert(!mesh.attributes().contains("position"));
  const Span<float3> profile_positions = curves_info.profile.evaluated_positions();
  const bool ignore_profile_position = profile_positions.size() == 1 &&
                                       math::is_equal(profile_positions.first(), float3(0.0f));
  if (ignore_profile_position) {
    if (mesh.verts_num == curves_info.main.points_num()) {
      const GAttributeReader src = curves_info.main.attributes().lookup("position");
      if (src.sharing_info && src.varray.is_span()) {
        const AttributeInitShared init(src.varray.get_internal_span().data(), *src.sharing_info);
        if (mesh.attributes_for_write().add<float3>("position", AttrDomain::Point, init)) {
          return;
        }
      }
    }
  }
  const Span<float3> main_positions = curves_info.main.evaluated_positions();
  mesh.attributes_for_write().add<float3>("position", AttrDomain::Point, AttributeInitConstruct());
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  if (ignore_profile_position) {
    array_utils::copy(main_positions, positions);
    return;
  }
  const Span<float3> tangents = curves_info.main.evaluated_tangents();
  const Span<float3> normals = curves_info.main.evaluated_normals();
  Span<float> radii_eval;
  if (const GVArray radii = *curves_info.main.attributes().lookup("radius", AttrDomain::Point)) {
    radii_eval = evaluate_attribute(radii, curves_info.main, eval_buffer).typed<float>();
  }
  foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
    fill_mesh_positions(info.main_points.size(),
                        info.profile_points.size(),
                        main_positions.slice(info.main_points),
                        profile_positions.slice(info.profile_points),
                        tangents.slice(info.main_points),
                        normals.slice(info.main_points),
                        radii_eval.is_empty() ? radii_eval : radii_eval.slice(info.main_points),
                        positions.slice(info.vert_range));
  });
}

template<typename T>
static void copy_main_point_data_to_mesh_verts(const Span<T> src,
                                               const int profile_point_num,
                                               MutableSpan<T> dst)
{
  for (const int i_ring : src.index_range()) {
    const int ring_vert_start = i_ring * profile_point_num;
    dst.slice(ring_vert_start, profile_point_num).fill(src[i_ring]);
  }
}

template<typename T>
static void copy_main_point_data_to_mesh_edges(const Span<T> src,
                                               const int profile_point_num,
                                               const int main_segment_num,
                                               const int profile_segment_num,
                                               MutableSpan<T> dst)
{
  const int edges_start = profile_point_num * main_segment_num;
  for (const int i_ring : src.index_range()) {
    const int ring_edge_start = edges_start + profile_segment_num * i_ring;
    dst.slice(ring_edge_start, profile_segment_num).fill(src[i_ring]);
  }
}

template<typename T>
static void copy_main_point_data_to_mesh_faces(const Span<T> src,
                                               const int main_segment_num,
                                               const int profile_segment_num,
                                               MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(main_segment_num)) {
    const int ring_face_start = profile_segment_num * i_ring;
    dst.slice(ring_face_start, profile_segment_num).fill(src[i_ring]);
  }
}

static bool try_sharing_point_data(const CurvesGeometry &main,
                                   const AttributeIDRef &id,
                                   const GAttributeReader &src,
                                   MutableAttributeAccessor mesh_attributes)
{
  if (mesh_attributes.domain_size(AttrDomain::Point) != main.points_num()) {
    return false;
  }
  if (!src.sharing_info || !src.varray.is_span()) {
    return false;
  }
  return mesh_attributes.add(
      id,
      AttrDomain::Point,
      bke::cpp_type_to_custom_data_type(src.varray.type()),
      AttributeInitShared(src.varray.get_internal_span().data(), *src.sharing_info));
}

static bool try_direct_evaluate_point_data(const CurvesGeometry &main,
                                           const GAttributeReader &src,
                                           GMutableSpan dst)
{
  if (dst.size() != main.evaluated_points_num()) {
    return false;
  }
  if (!src.varray.is_span()) {
    return false;
  }
  main.interpolate_to_evaluated(src.varray.get_internal_span(), dst);
  return true;
}

static void copy_main_point_domain_attribute_to_mesh(const CurvesInfo &curves_info,
                                                     const AttributeIDRef &id,
                                                     const ResultOffsets &offsets,
                                                     const AttrDomain dst_domain,
                                                     const GAttributeReader &src_attribute,
                                                     Vector<std::byte> &eval_buffer,
                                                     MutableAttributeAccessor mesh_attributes)
{
  if (dst_domain == AttrDomain::Point) {
    if (try_sharing_point_data(curves_info.main, id, src_attribute, mesh_attributes)) {
      return;
    }
  }
  GSpanAttributeWriter dst_attribute = mesh_attributes.lookup_or_add_for_write_only_span(
      id, dst_domain, bke::cpp_type_to_custom_data_type(src_attribute.varray.type()));
  if (!dst_attribute) {
    return;
  }
  if (dst_domain == AttrDomain::Point) {
    if (try_direct_evaluate_point_data(curves_info.main, src_attribute, dst_attribute.span)) {
      dst_attribute.finish();
      return;
    }
  }
  const GSpan src_all = evaluate_attribute(*src_attribute, curves_info.main, eval_buffer);
  attribute_math::convert_to_static_type(src_attribute.varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const Span<T> src = src_all.typed<T>();
    MutableSpan<T> dst = dst_attribute.span.typed<T>();
    switch (dst_domain) {
      case AttrDomain::Point:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_main_point_data_to_mesh_verts(
              src.slice(info.main_points), info.profile_points.size(), dst.slice(info.vert_range));
        });
        break;
      case AttrDomain::Edge:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_main_point_data_to_mesh_edges(src.slice(info.main_points),
                                             info.profile_points.size(),
                                             info.main_segment_num,
                                             info.profile_segment_num,
                                             dst.slice(info.edge_range));
        });
        break;
      case AttrDomain::Face:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_main_point_data_to_mesh_faces(src.slice(info.main_points),
                                             info.main_segment_num,
                                             info.profile_segment_num,
                                             dst.slice(info.face_range));
        });
        break;
      case AttrDomain::Corner:
        /* Unsupported for now, since there are no builtin attributes to convert into. */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });
  dst_attribute.finish();
}

template<typename T>
static void copy_profile_point_data_to_mesh_verts(const Span<T> src,
                                                  const int main_point_num,
                                                  MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(main_point_num)) {
    const int profile_vert_start = i_ring * src.size();
    for (const int i_profile : src.index_range()) {
      dst[profile_vert_start + i_profile] = src[i_profile];
    }
  }
}

template<typename T>
static void copy_profile_point_data_to_mesh_edges(const Span<T> src,
                                                  const int main_segment_num,
                                                  MutableSpan<T> dst)
{
  for (const int i_profile : src.index_range()) {
    const int profile_edge_offset = i_profile * main_segment_num;
    dst.slice(profile_edge_offset, main_segment_num).fill(src[i_profile]);
  }
}

template<typename T>
static void copy_profile_point_data_to_mesh_faces(const Span<T> src,
                                                  const int main_segment_num,
                                                  const int profile_segment_num,
                                                  MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(main_segment_num)) {
    const int profile_face_start = i_ring * profile_segment_num;
    for (const int i_profile : IndexRange(profile_segment_num)) {
      dst[profile_face_start + i_profile] = src[i_profile];
    }
  }
}

static void copy_profile_point_domain_attribute_to_mesh(const CurvesInfo &curves_info,
                                                        const ResultOffsets &offsets,
                                                        const AttrDomain dst_domain,
                                                        const GSpan src_all,
                                                        GMutableSpan dst_all)
{
  attribute_math::convert_to_static_type(src_all.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const Span<T> src = src_all.typed<T>();
    MutableSpan<T> dst = dst_all.typed<T>();
    switch (dst_domain) {
      case AttrDomain::Point:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_profile_point_data_to_mesh_verts(
              src.slice(info.profile_points), info.main_points.size(), dst.slice(info.vert_range));
        });
        break;
      case AttrDomain::Edge:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_profile_point_data_to_mesh_edges(
              src.slice(info.profile_points), info.main_segment_num, dst.slice(info.edge_range));
        });
        break;
      case AttrDomain::Face:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_profile_point_data_to_mesh_faces(src.slice(info.profile_points),
                                                info.main_segment_num,
                                                info.profile_segment_num,
                                                dst.slice(info.face_range));
        });
        break;
      case AttrDomain::Corner:
        /* Unsupported for now, since there are no builtin attributes to convert into. */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });
}

template<typename T>
static void copy_indices_to_offset_ranges(const VArray<T> &src,
                                          const Span<int> curve_indices,
                                          const OffsetIndices<int> mesh_offsets,
                                          MutableSpan<T> dst)
{
  /* This unnecessarily instantiates the "is single" case (which should be handled elsewhere if
   * it's ever used for attributes), but the alternative is duplicating the function for spans and
   * other virtual arrays. */
  devirtualize_varray(src, [&](const auto src) {
    threading::parallel_for(curve_indices.index_range(), 512, [&](IndexRange range) {
      for (const int i : range) {
        dst.slice(mesh_offsets[i]).fill(src[curve_indices[i]]);
      }
    });
  });
}

static void copy_curve_domain_attribute_to_mesh(const ResultOffsets &mesh_offsets,
                                                const Span<int> curve_indices,
                                                const AttrDomain dst_domain,
                                                const GVArray &src,
                                                GMutableSpan dst)
{
  Span<int> offsets;
  switch (dst_domain) {
    case AttrDomain::Point:
      offsets = mesh_offsets.vert;
      break;
    case AttrDomain::Edge:
      offsets = mesh_offsets.edge;
      break;
    case AttrDomain::Face:
      offsets = mesh_offsets.face;
      break;
    case AttrDomain::Corner:
      offsets = mesh_offsets.loop;
      break;
    default:
      BLI_assert_unreachable();
      return;
  }
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    copy_indices_to_offset_ranges(src.typed<T>(), curve_indices, offsets, dst.typed<T>());
  });
}

static void write_sharp_bezier_edges(const CurvesInfo &curves_info,
                                     const ResultOffsets &offsets,
                                     MutableAttributeAccessor mesh_attributes,
                                     SpanAttributeWriter<bool> &sharp_edges)
{
  const CurvesGeometry &profile = curves_info.profile;
  if (!profile.has_curve_with_type(CURVE_TYPE_BEZIER)) {
    return;
  }
  const VArraySpan<int8_t> handle_types_left{profile.handle_types_left()};
  const VArraySpan<int8_t> handle_types_right{profile.handle_types_right()};
  if (!handle_types_left.contains(BEZIER_HANDLE_VECTOR) &&
      !handle_types_right.contains(BEZIER_HANDLE_VECTOR))
  {
    return;
  }

  sharp_edges = mesh_attributes.lookup_or_add_for_write_span<bool>("sharp_edge", AttrDomain::Edge);

  const OffsetIndices profile_points_by_curve = profile.points_by_curve();
  const VArray<int8_t> types = profile.curve_types();
  foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
    if (types[info.i_profile] == CURVE_TYPE_BEZIER) {
      const IndexRange points = profile_points_by_curve[info.i_profile];
      mark_bezier_vector_edges_sharp(points.size(),
                                     info.main_segment_num,
                                     profile.bezier_evaluated_offsets_for_curve(info.i_profile),
                                     handle_types_left.slice(points),
                                     handle_types_right.slice(points),
                                     sharp_edges.span.slice(info.edge_range));
    }
  });
}

Mesh *curve_to_mesh_sweep(const CurvesGeometry &main,
                          const CurvesGeometry &profile,
                          const bool fill_caps,
                          const AnonymousAttributePropagationInfo &propagation_info)
{
  const CurvesInfo curves_info = get_curves_info(main, profile);

  const ResultOffsets offsets = calculate_result_offsets(curves_info, fill_caps);
  if (offsets.vert.last() == 0) {
    return nullptr;
  }

  /* Add the position attribute later so it can be shared in some cases. */
  Mesh *mesh = BKE_mesh_new_nomain(
      0, offsets.edge.last(), offsets.face.last(), offsets.loop.last());
  CustomData_free_layer_named(&mesh->vert_data, "position", 0);
  mesh->verts_num = offsets.vert.last();

  MutableSpan<int2> edges = mesh->edges_for_write();
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  MutableSpan<int> corner_edges = mesh->corner_edges_for_write();
  MutableAttributeAccessor mesh_attributes = mesh->attributes_for_write();

  foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
    fill_mesh_topology(info.vert_range.start(),
                       info.edge_range.start(),
                       info.face_range.start(),
                       info.loop_range.start(),
                       info.main_points.size(),
                       info.profile_points.size(),
                       info.main_cyclic,
                       info.profile_cyclic,
                       fill_caps,
                       edges,
                       corner_verts,
                       corner_edges,
                       face_offsets);
  });

  if (fill_caps) {
    /* TODO: This is used to keep the tests passing after refactoring mesh shade smooth flags. It
     * can be removed if the tests are updated and the final shading results will be the same. */
    SpanAttributeWriter<bool> sharp_faces = mesh_attributes.lookup_or_add_for_write_span<bool>(
        "sharp_face", AttrDomain::Face);
    foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
      const bool has_caps = fill_caps && !info.main_cyclic && info.profile_cyclic;
      if (has_caps) {
        const int face_num = info.main_segment_num * info.profile_segment_num;
        const int cap_face_offset = info.face_range.start() + face_num;
        sharp_faces.span[cap_face_offset] = true;
        sharp_faces.span[cap_face_offset + 1] = true;
      }
    });
    sharp_faces.finish();
  }

  Vector<std::byte> eval_buffer;

  build_mesh_positions(curves_info, offsets, eval_buffer, *mesh);

  mesh->tag_overlapping_none();
  if (!offsets.any_single_point_main) {
    /* If there are no single point curves, every combination will have at least loose edges. */
    mesh->tag_loose_verts_none();
    if (!offsets.any_single_point_profile) {
      /* If there are no single point profiles, every combination will have faces. */
      mesh->tag_loose_edges_none();
    }
  }

  SpanAttributeWriter<bool> sharp_edges;
  write_sharp_bezier_edges(curves_info, offsets, mesh_attributes, sharp_edges);
  if (fill_caps) {
    if (!sharp_edges) {
      sharp_edges = mesh_attributes.lookup_or_add_for_write_span<bool>("sharp_edge",
                                                                       AttrDomain::Edge);
    }
    foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
      if (info.main_cyclic || !info.profile_cyclic) {
        return;
      }
      const int main_edges_start = info.edge_range.start();
      const int last_ring_index = info.main_points.size() - 1;
      const int profile_edges_start = main_edges_start +
                                      info.profile_points.size() * info.main_segment_num;
      const int last_ring_edge_offset = profile_edges_start +
                                        info.profile_segment_num * last_ring_index;

      sharp_edges.span.slice(profile_edges_start, info.profile_segment_num).fill(true);
      sharp_edges.span.slice(last_ring_edge_offset, info.profile_segment_num).fill(true);
    });
  }
  sharp_edges.finish();

  const AttributeAccessor main_attributes = main.attributes();
  main_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (!should_add_attribute_to_mesh(
            main_attributes, mesh_attributes, id, meta_data, propagation_info))
    {
      return true;
    }

    const AttrDomain src_domain = meta_data.domain;
    const eCustomDataType type = meta_data.data_type;
    const GAttributeReader src = main_attributes.lookup(id, src_domain, type);
    const AttrDomain dst_domain = get_attribute_domain_for_mesh(mesh_attributes, id);

    if (src_domain == AttrDomain::Point) {
      copy_main_point_domain_attribute_to_mesh(
          curves_info, id, offsets, dst_domain, src, eval_buffer, mesh_attributes);
    }
    else if (src_domain == AttrDomain::Curve) {
      GSpanAttributeWriter dst = mesh_attributes.lookup_or_add_for_write_only_span(
          id, dst_domain, type);
      if (dst) {
        copy_curve_domain_attribute_to_mesh(
            offsets, offsets.main_indices, dst_domain, *src, dst.span);
      }
      dst.finish();
    }

    return true;
  });

  const AttributeAccessor profile_attributes = profile.attributes();
  profile_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (main_attributes.contains(id)) {
      return true;
    }
    if (!should_add_attribute_to_mesh(
            profile_attributes, mesh_attributes, id, meta_data, propagation_info))
    {
      return true;
    }
    const AttrDomain src_domain = meta_data.domain;
    const eCustomDataType type = meta_data.data_type;
    const GVArray src = *profile_attributes.lookup(id, src_domain, type);

    const AttrDomain dst_domain = get_attribute_domain_for_mesh(mesh_attributes, id);
    GSpanAttributeWriter dst = mesh_attributes.lookup_or_add_for_write_only_span(
        id, dst_domain, type);
    if (!dst) {
      return true;
    }

    if (src_domain == AttrDomain::Point) {
      copy_profile_point_domain_attribute_to_mesh(curves_info,
                                                  offsets,
                                                  dst_domain,
                                                  evaluate_attribute(src, profile, eval_buffer),
                                                  dst.span);
    }
    else if (src_domain == AttrDomain::Curve) {
      copy_curve_domain_attribute_to_mesh(
          offsets, offsets.profile_indices, dst_domain, src, dst.span);
    }

    dst.finish();
    return true;
  });

  return mesh;
}

static CurvesGeometry get_curve_single_vert()
{
  CurvesGeometry curves(1, 1);
  curves.offsets_for_write().last() = 1;
  curves.positions_for_write().fill(float3(0.0f));
  curves.fill_curve_types(CURVE_TYPE_POLY);

  return curves;
}

Mesh *curve_to_wire_mesh(const CurvesGeometry &curve,
                         const AnonymousAttributePropagationInfo &propagation_info)
{
  static const CurvesGeometry vert_curve = get_curve_single_vert();
  return curve_to_mesh_sweep(curve, vert_curve, false, propagation_info);
}

}  // namespace blender::bke
