/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_math_matrix.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_material.h"
#include "BKE_mesh.h"

#include "BKE_curve_to_mesh.hh"

namespace blender::bke {

static void fill_mesh_topology(const int vert_offset,
                               const int edge_offset,
                               const int poly_offset,
                               const int loop_offset,
                               const int main_point_num,
                               const int profile_point_num,
                               const bool main_cyclic,
                               const bool profile_cyclic,
                               const bool fill_caps,
                               MutableSpan<MEdge> edges,
                               MutableSpan<MLoop> loops,
                               MutableSpan<MPoly> polys)
{
  const int main_segment_num = curves::segments_num(main_point_num, main_cyclic);
  const int profile_segment_num = curves::segments_num(profile_point_num, profile_cyclic);

  if (profile_point_num == 1) {
    for (const int i : IndexRange(main_point_num - 1)) {
      MEdge &edge = edges[edge_offset + i];
      edge.v1 = vert_offset + i;
      edge.v2 = vert_offset + i + 1;
    }

    if (main_cyclic && main_segment_num > 1) {
      MEdge &edge = edges[edge_offset + main_segment_num - 1];
      edge.v1 = vert_offset + main_point_num - 1;
      edge.v2 = vert_offset;
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

      MEdge &edge = edges[profile_edge_offset + i_ring];
      edge.v1 = ring_vert_offset + i_profile;
      edge.v2 = next_ring_vert_offset + i_profile;
    }
  }

  /* Add the edges running along each profile ring. */
  const int profile_edges_start = main_edges_start + profile_point_num * main_segment_num;
  for (const int i_ring : IndexRange(main_point_num)) {
    const int ring_vert_offset = vert_offset + profile_point_num * i_ring;

    const int ring_edge_offset = profile_edges_start + i_ring * profile_segment_num;
    for (const int i_profile : IndexRange(profile_segment_num)) {
      const int i_next_profile = (i_profile == profile_point_num - 1) ? 0 : i_profile + 1;

      MEdge &edge = edges[ring_edge_offset + i_profile];
      edge.v1 = ring_vert_offset + i_profile;
      edge.v2 = ring_vert_offset + i_next_profile;
    }
  }

  /* Calculate poly and corner indices. */
  for (const int i_ring : IndexRange(main_segment_num)) {
    const int i_next_ring = (i_ring == main_point_num - 1) ? 0 : i_ring + 1;

    const int ring_vert_offset = vert_offset + profile_point_num * i_ring;
    const int next_ring_vert_offset = vert_offset + profile_point_num * i_next_ring;

    const int ring_edge_start = profile_edges_start + profile_segment_num * i_ring;
    const int next_ring_edge_offset = profile_edges_start + profile_segment_num * i_next_ring;

    const int ring_poly_offset = poly_offset + i_ring * profile_segment_num;
    const int ring_loop_offset = loop_offset + i_ring * profile_segment_num * 4;

    for (const int i_profile : IndexRange(profile_segment_num)) {
      const int ring_segment_loop_offset = ring_loop_offset + i_profile * 4;
      const int i_next_profile = (i_profile == profile_point_num - 1) ? 0 : i_profile + 1;

      const int main_edge_start = main_edges_start + main_segment_num * i_profile;
      const int next_main_edge_start = main_edges_start + main_segment_num * i_next_profile;

      MPoly &poly = polys[ring_poly_offset + i_profile];
      poly.loopstart = ring_segment_loop_offset;
      poly.totloop = 4;
      poly.flag = ME_SMOOTH;

      MLoop &loop_a = loops[ring_segment_loop_offset];
      loop_a.v = ring_vert_offset + i_profile;
      loop_a.e = ring_edge_start + i_profile;
      MLoop &loop_b = loops[ring_segment_loop_offset + 1];
      loop_b.v = ring_vert_offset + i_next_profile;
      loop_b.e = next_main_edge_start + i_ring;
      MLoop &loop_c = loops[ring_segment_loop_offset + 2];
      loop_c.v = next_ring_vert_offset + i_next_profile;
      loop_c.e = next_ring_edge_offset + i_profile;
      MLoop &loop_d = loops[ring_segment_loop_offset + 3];
      loop_d.v = next_ring_vert_offset + i_profile;
      loop_d.e = main_edge_start + i_ring;
    }
  }

  const bool has_caps = fill_caps && !main_cyclic && profile_cyclic && profile_point_num > 2;
  if (has_caps) {
    const int poly_num = main_segment_num * profile_segment_num;
    const int cap_loop_offset = loop_offset + poly_num * 4;
    const int cap_poly_offset = poly_offset + poly_num;

    MPoly &poly_start = polys[cap_poly_offset];
    poly_start.loopstart = cap_loop_offset;
    poly_start.totloop = profile_segment_num;
    MPoly &poly_end = polys[cap_poly_offset + 1];
    poly_end.loopstart = cap_loop_offset + profile_segment_num;
    poly_end.totloop = profile_segment_num;

    const int last_ring_index = main_point_num - 1;
    const int last_ring_vert_offset = vert_offset + profile_point_num * last_ring_index;
    const int last_ring_edge_offset = profile_edges_start + profile_segment_num * last_ring_index;

    for (const int i : IndexRange(profile_segment_num)) {
      const int i_inv = profile_segment_num - i - 1;
      MLoop &loop_start = loops[cap_loop_offset + i];
      loop_start.v = vert_offset + i_inv;
      loop_start.e = profile_edges_start +
                     ((i == (profile_segment_num - 1)) ? (profile_segment_num - 1) : (i_inv - 1));
      MLoop &loop_end = loops[cap_loop_offset + profile_segment_num + i];
      loop_end.v = last_ring_vert_offset + i;
      loop_end.e = last_ring_edge_offset + i;
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

struct ResultOffsets {
  /** The total number of curve combinations. */
  int total;

  /** Offsets into the result mesh for each combination. */
  Array<int> vert;
  Array<int> edge;
  Array<int> loop;
  Array<int> poly;

  /* The indices of the main and profile curves that form each combination. */
  Array<int> main_indices;
  Array<int> profile_indices;
};
static ResultOffsets calculate_result_offsets(const CurvesInfo &info, const bool fill_caps)
{
  ResultOffsets result;
  result.total = info.main.curves_num() * info.profile.curves_num();
  result.vert.reinitialize(result.total + 1);
  result.edge.reinitialize(result.total + 1);
  result.loop.reinitialize(result.total + 1);
  result.poly.reinitialize(result.total + 1);

  result.main_indices.reinitialize(result.total);
  result.profile_indices.reinitialize(result.total);

  const OffsetIndices<int> main_offsets = info.main.evaluated_points_by_curve();
  const OffsetIndices<int> profile_offsets = info.profile.evaluated_points_by_curve();

  int mesh_index = 0;
  int vert_offset = 0;
  int edge_offset = 0;
  int loop_offset = 0;
  int poly_offset = 0;
  for (const int i_main : info.main.curves_range()) {
    const bool main_cyclic = info.main_cyclic[i_main];
    const int main_point_num = main_offsets.size(i_main);
    const int main_segment_num = curves::segments_num(main_point_num, main_cyclic);
    for (const int i_profile : info.profile.curves_range()) {
      result.vert[mesh_index] = vert_offset;
      result.edge[mesh_index] = edge_offset;
      result.loop[mesh_index] = loop_offset;
      result.poly[mesh_index] = poly_offset;

      result.main_indices[mesh_index] = i_main;
      result.profile_indices[mesh_index] = i_profile;

      const bool profile_cyclic = info.profile_cyclic[i_profile];
      const int profile_point_num = profile_offsets.size(i_profile);
      const int profile_segment_num = curves::segments_num(profile_point_num, profile_cyclic);

      const bool has_caps = fill_caps && !main_cyclic && profile_cyclic && profile_point_num > 2;
      const int tube_face_num = main_segment_num * profile_segment_num;

      vert_offset += main_point_num * profile_point_num;

      /* Add the ring edges, with one ring for every curve vertex, and the edge loops
       * that run along the length of the curve, starting on the first profile. */
      edge_offset += main_point_num * profile_segment_num + main_segment_num * profile_point_num;

      /* Add two cap N-gons for every ending. */
      poly_offset += tube_face_num + (has_caps ? 2 : 0);

      /* All faces on the tube are quads, and all cap faces are N-gons with an edge for each
       * profile edge. */
      loop_offset += tube_face_num * 4 + (has_caps ? profile_segment_num * 2 : 0);

      mesh_index++;
    }
  }

  result.vert.last() = vert_offset;
  result.edge.last() = edge_offset;
  result.loop.last() = loop_offset;
  result.poly.last() = poly_offset;

  return result;
}

static eAttrDomain get_attribute_domain_for_mesh(const AttributeAccessor &mesh_attributes,
                                                 const AttributeIDRef &attribute_id)
{
  /* Only use a different domain if it is builtin and must only exist on one domain. */
  if (!mesh_attributes.is_builtin(attribute_id)) {
    return ATTR_DOMAIN_POINT;
  }

  std::optional<AttributeMetaData> meta_data = mesh_attributes.lookup_meta_data(attribute_id);
  if (!meta_data) {
    return ATTR_DOMAIN_POINT;
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

static GSpan evaluated_attribute_if_necessary(const GVArray &src,
                                              const CurvesGeometry &curves,
                                              const std::array<int, CURVE_TYPES_NUM> &type_counts,
                                              Vector<std::byte> &buffer)
{
  if (type_counts[CURVE_TYPE_POLY] == curves.curves_num() && src.is_span()) {
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
  IndexRange poly_range;
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
  const OffsetIndices<int> poly_offsets(offsets.poly);
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
                         poly_offsets[i],
                         loop_offsets[i]});
    }
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

static void copy_main_point_domain_attribute_to_mesh(const CurvesInfo &curves_info,
                                                     const ResultOffsets &offsets,
                                                     const eAttrDomain dst_domain,
                                                     const GSpan src_all,
                                                     GMutableSpan dst_all)
{
  attribute_math::convert_to_static_type(src_all.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const Span<T> src = src_all.typed<T>();
    MutableSpan<T> dst = dst_all.typed<T>();
    switch (dst_domain) {
      case ATTR_DOMAIN_POINT:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_main_point_data_to_mesh_verts(
              src.slice(info.main_points), info.profile_points.size(), dst.slice(info.vert_range));
        });
        break;
      case ATTR_DOMAIN_EDGE:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_main_point_data_to_mesh_edges(src.slice(info.main_points),
                                             info.profile_points.size(),
                                             info.main_segment_num,
                                             info.profile_segment_num,
                                             dst.slice(info.edge_range));
        });
        break;
      case ATTR_DOMAIN_FACE:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_main_point_data_to_mesh_faces(src.slice(info.main_points),
                                             info.main_segment_num,
                                             info.profile_segment_num,
                                             dst.slice(info.poly_range));
        });
        break;
      case ATTR_DOMAIN_CORNER:
        /* Unsupported for now, since there are no builtin attributes to convert into. */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });
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
                                                        const eAttrDomain dst_domain,
                                                        const GSpan src_all,
                                                        GMutableSpan dst_all)
{
  attribute_math::convert_to_static_type(src_all.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const Span<T> src = src_all.typed<T>();
    MutableSpan<T> dst = dst_all.typed<T>();
    switch (dst_domain) {
      case ATTR_DOMAIN_POINT:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_profile_point_data_to_mesh_verts(
              src.slice(info.profile_points), info.main_points.size(), dst.slice(info.vert_range));
        });
        break;
      case ATTR_DOMAIN_EDGE:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_profile_point_data_to_mesh_edges(
              src.slice(info.profile_points), info.main_segment_num, dst.slice(info.edge_range));
        });
        break;
      case ATTR_DOMAIN_FACE:
        foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
          copy_profile_point_data_to_mesh_faces(src.slice(info.profile_points),
                                                info.main_segment_num,
                                                info.profile_segment_num,
                                                dst.slice(info.poly_range));
        });
        break;
      case ATTR_DOMAIN_CORNER:
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
  devirtualize_varray(src, [&](const auto &src) {
    threading::parallel_for(curve_indices.index_range(), 512, [&](IndexRange range) {
      for (const int i : range) {
        dst.slice(mesh_offsets[i]).fill(src[curve_indices[i]]);
      }
    });
  });
}

static void copy_curve_domain_attribute_to_mesh(const ResultOffsets &mesh_offsets,
                                                const Span<int> curve_indices,
                                                const eAttrDomain dst_domain,
                                                const GVArray &src,
                                                GMutableSpan dst)
{
  Span<int> offsets;
  switch (dst_domain) {
    case ATTR_DOMAIN_POINT:
      offsets = mesh_offsets.vert;
      break;
    case ATTR_DOMAIN_EDGE:
      offsets = mesh_offsets.edge;
      break;
    case ATTR_DOMAIN_FACE:
      offsets = mesh_offsets.poly;
      break;
    case ATTR_DOMAIN_CORNER:
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
      !handle_types_right.contains(BEZIER_HANDLE_VECTOR)) {
    return;
  }

  sharp_edges = mesh_attributes.lookup_or_add_for_write_span<bool>("sharp_edge", ATTR_DOMAIN_EDGE);

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

  Mesh *mesh = BKE_mesh_new_nomain(
      offsets.vert.last(), offsets.edge.last(), 0, offsets.loop.last(), offsets.poly.last());
  mesh->flag |= ME_AUTOSMOOTH;
  mesh->smoothresh = DEG2RADF(180.0f);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<MEdge> edges = mesh->edges_for_write();
  MutableSpan<MPoly> polys = mesh->polys_for_write();
  MutableSpan<MLoop> loops = mesh->loops_for_write();

  foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
    fill_mesh_topology(info.vert_range.start(),
                       info.edge_range.start(),
                       info.poly_range.start(),
                       info.loop_range.start(),
                       info.main_points.size(),
                       info.profile_points.size(),
                       info.main_cyclic,
                       info.profile_cyclic,
                       fill_caps,
                       edges,
                       loops,
                       polys);
  });

  const Span<float3> main_positions = main.evaluated_positions();
  const Span<float3> tangents = main.evaluated_tangents();
  const Span<float3> normals = main.evaluated_normals();
  const Span<float3> profile_positions = profile.evaluated_positions();

  Vector<std::byte> eval_buffer;

  const AttributeAccessor main_attributes = main.attributes();
  const AttributeAccessor profile_attributes = profile.attributes();

  Span<float> radii = {};
  if (main_attributes.contains("radius")) {
    radii = evaluated_attribute_if_necessary(
                main_attributes.lookup_or_default<float>("radius", ATTR_DOMAIN_POINT, 1.0f),
                main,
                main.curve_type_counts(),
                eval_buffer)
                .typed<float>();
  }

  foreach_curve_combination(curves_info, offsets, [&](const CombinationInfo &info) {
    fill_mesh_positions(info.main_points.size(),
                        info.profile_points.size(),
                        main_positions.slice(info.main_points),
                        profile_positions.slice(info.profile_points),
                        tangents.slice(info.main_points),
                        normals.slice(info.main_points),
                        radii.is_empty() ? radii : radii.slice(info.main_points),
                        positions.slice(info.vert_range));
  });

  MutableAttributeAccessor mesh_attributes = mesh->attributes_for_write();

  SpanAttributeWriter<bool> sharp_edges;
  write_sharp_bezier_edges(curves_info, offsets, mesh_attributes, sharp_edges);
  if (fill_caps) {
    if (!sharp_edges) {
      sharp_edges = mesh_attributes.lookup_or_add_for_write_span<bool>("sharp_edge",
                                                                       ATTR_DOMAIN_EDGE);
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

  Set<AttributeIDRef> main_attributes_set;

  main_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (!should_add_attribute_to_mesh(
            main_attributes, mesh_attributes, id, meta_data, propagation_info)) {
      return true;
    }
    main_attributes_set.add_new(id);

    const eAttrDomain src_domain = meta_data.domain;
    const eCustomDataType type = meta_data.data_type;
    GVArray src = main_attributes.lookup(id, src_domain, type);

    const eAttrDomain dst_domain = get_attribute_domain_for_mesh(mesh_attributes, id);
    GSpanAttributeWriter dst = mesh_attributes.lookup_or_add_for_write_only_span(
        id, dst_domain, type);
    if (!dst) {
      return true;
    }

    if (src_domain == ATTR_DOMAIN_POINT) {
      copy_main_point_domain_attribute_to_mesh(
          curves_info,
          offsets,
          dst_domain,
          evaluated_attribute_if_necessary(src, main, main.curve_type_counts(), eval_buffer),
          dst.span);
    }
    else if (src_domain == ATTR_DOMAIN_CURVE) {
      copy_curve_domain_attribute_to_mesh(
          offsets, offsets.main_indices, dst_domain, src, dst.span);
    }

    dst.finish();
    return true;
  });

  profile_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData meta_data) {
    if (main_attributes.contains(id)) {
      return true;
    }
    if (!should_add_attribute_to_mesh(
            profile_attributes, mesh_attributes, id, meta_data, propagation_info)) {
      return true;
    }
    const eAttrDomain src_domain = meta_data.domain;
    const eCustomDataType type = meta_data.data_type;
    GVArray src = profile_attributes.lookup(id, src_domain, type);

    const eAttrDomain dst_domain = get_attribute_domain_for_mesh(mesh_attributes, id);
    GSpanAttributeWriter dst = mesh_attributes.lookup_or_add_for_write_only_span(
        id, dst_domain, type);
    if (!dst) {
      return true;
    }

    if (src_domain == ATTR_DOMAIN_POINT) {
      copy_profile_point_domain_attribute_to_mesh(
          curves_info,
          offsets,
          dst_domain,
          evaluated_attribute_if_necessary(src, profile, profile.curve_type_counts(), eval_buffer),
          dst.span);
    }
    else if (src_domain == ATTR_DOMAIN_CURVE) {
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
  curves.positions_for_write().fill(float3(0));
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
