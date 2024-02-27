/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"

#include "BLI_kdtree.h"
#include "BLI_length_parameterize.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"

#include "GEO_add_curves_on_mesh.hh"
#include "GEO_reverse_uv_sampler.hh"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * cu: Local space of the curves object that is being edited.
 * su: Local space of the surface object.
 */

namespace blender::geometry {

using bke::CurvesGeometry;

struct NeighborCurve {
  /* Curve index of the neighbor. */
  int index;
  /* The weights of all neighbors of a new curve add up to 1. */
  float weight;
};

static constexpr int max_neighbors = 5;
using NeighborCurves = Vector<NeighborCurve, max_neighbors>;

float3 compute_surface_point_normal(const int3 &corner_tri,
                                    const float3 &bary_coord,
                                    const Span<float3> corner_normals)
{
  const float3 value = bke::mesh_surface_sample::sample_corner_attribute_with_bary_coords(
      bary_coord, corner_tri, corner_normals);
  return math::normalize(value);
}

template<typename T>
static inline void linear_interpolation(const T &a, const T &b, MutableSpan<T> dst)
{
  dst.first() = a;
  const float step = 1.0f / dst.size();
  for (const int i : dst.index_range().drop_front(1)) {
    dst[i] = bke::attribute_math::mix2(i * step, a, b);
  }
}

static Array<NeighborCurves> find_curve_neighbors(const Span<float3> root_positions,
                                                  const KDTree_3d &old_roots_kdtree)
{
  const int tot_added_curves = root_positions.size();
  Array<NeighborCurves> neighbors_per_curve(tot_added_curves);
  threading::parallel_for(IndexRange(tot_added_curves), 128, [&](const IndexRange range) {
    for (const int i : range) {
      const float3 root = root_positions[i];
      std::array<KDTreeNearest_3d, max_neighbors> nearest_n;
      const int found_neighbors = BLI_kdtree_3d_find_nearest_n(
          &old_roots_kdtree, root, nearest_n.data(), max_neighbors);
      float tot_weight = 0.0f;
      for (const int neighbor_i : IndexRange(found_neighbors)) {
        KDTreeNearest_3d &nearest = nearest_n[neighbor_i];
        const float weight = 1.0f / std::max(nearest.dist, 0.00001f);
        tot_weight += weight;
        neighbors_per_curve[i].append({nearest.index, weight});
      }
      /* Normalize weights. */
      for (NeighborCurve &neighbor : neighbors_per_curve[i]) {
        neighbor.weight /= tot_weight;
      }
    }
  });
  return neighbors_per_curve;
}

template<typename T, typename GetValueF>
void interpolate_from_neighbor_curves(const Span<NeighborCurves> neighbors_per_curve,
                                      const T &fallback,
                                      const GetValueF &get_value_from_neighbor,
                                      MutableSpan<T> r_interpolated_values)
{
  bke::attribute_math::DefaultMixer<T> mixer{r_interpolated_values};
  threading::parallel_for(r_interpolated_values.index_range(), 512, [&](const IndexRange range) {
    for (const int i : range) {
      const NeighborCurves &neighbors = neighbors_per_curve[i];
      if (neighbors.is_empty()) {
        mixer.mix_in(i, fallback, 1.0f);
      }
      else {
        for (const NeighborCurve &neighbor : neighbors) {
          const T neighbor_value = get_value_from_neighbor(neighbor.index);
          mixer.mix_in(i, neighbor_value, neighbor.weight);
        }
      }
    }
    mixer.finalize(range);
  });
}

static void calc_position_without_interpolation(CurvesGeometry &curves,
                                                const int old_curves_num,
                                                const Span<float3> root_positions_cu,
                                                const Span<float> new_lengths_cu,
                                                const Span<float3> new_normals_su,
                                                const float4x4 &surface_to_curves_normal_mat)
{
  const int added_curves_num = root_positions_cu.size();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions_cu = curves.positions_for_write();
  threading::parallel_for(IndexRange(added_curves_num), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const int curve_i = old_curves_num + i;
      const IndexRange points = points_by_curve[curve_i];
      const float3 &root_cu = root_positions_cu[i];
      const float length = new_lengths_cu[i];
      const float3 &normal_su = new_normals_su[i];
      const float3 normal_cu = math::normalize(
          math::transform_direction(surface_to_curves_normal_mat, normal_su));
      const float3 tip_cu = root_cu + length * normal_cu;

      linear_interpolation(root_cu, tip_cu, positions_cu.slice(points));
    }
  });
}

static void calc_position_with_interpolation(CurvesGeometry &curves,
                                             const Span<float3> root_positions_cu,
                                             const Span<NeighborCurves> neighbors_per_curve,
                                             const int old_curves_num,
                                             const Span<float> new_lengths_cu,
                                             const Span<float3> new_normals_su,
                                             const bke::CurvesSurfaceTransforms &transforms,
                                             const Span<int3> corner_tris,
                                             const ReverseUVSampler &reverse_uv_sampler,
                                             const Span<float3> corner_normals_su)
{
  MutableSpan<float3> positions_cu = curves.positions_for_write();
  const int added_curves_num = root_positions_cu.size();

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Span<float2> uv_coords = curves.surface_uv_coords();

  threading::parallel_for(IndexRange(added_curves_num), 256, [&](const IndexRange range) {
    for (const int added_curve_i : range) {
      const NeighborCurves &neighbors = neighbors_per_curve[added_curve_i];
      const int curve_i = old_curves_num + added_curve_i;
      const IndexRange points = points_by_curve[curve_i];

      const float length_cu = new_lengths_cu[added_curve_i];
      const float3 &normal_su = new_normals_su[added_curve_i];
      const float3 normal_cu = math::normalize(
          math::transform_direction(transforms.surface_to_curves_normal, normal_su));

      const float3 &root_cu = root_positions_cu[added_curve_i];

      if (neighbors.is_empty()) {
        /* If there are no neighbors, just make a straight line. */
        const float3 tip_cu = root_cu + length_cu * normal_cu;
        linear_interpolation(root_cu, tip_cu, positions_cu.slice(points));
        continue;
      }

      positions_cu.slice(points).fill(root_cu);

      for (const NeighborCurve &neighbor : neighbors) {
        const int neighbor_curve_i = neighbor.index;
        const float2 neighbor_uv = uv_coords[neighbor_curve_i];
        const ReverseUVSampler::Result result = reverse_uv_sampler.sample(neighbor_uv);
        if (result.type != ReverseUVSampler::ResultType::Ok) {
          continue;
        }

        const float3 neighbor_normal_su = compute_surface_point_normal(
            corner_tris[result.tri_index], result.bary_weights, corner_normals_su);
        const float3 neighbor_normal_cu = math::normalize(
            math::transform_direction(transforms.surface_to_curves_normal, neighbor_normal_su));

        /* The rotation matrix used to transform relative coordinates of the neighbor curve
         * to the new curve. */
        float normal_rotation_cu[3][3];
        rotation_between_vecs_to_mat3(normal_rotation_cu, neighbor_normal_cu, normal_cu);

        const IndexRange neighbor_points = points_by_curve[neighbor_curve_i];
        const float3 &neighbor_root_cu = positions_cu[neighbor_points[0]];

        /* Sample the positions on neighbors and mix them into the final positions of the curve.
         * Resampling is necessary if the length of the new curve does not match the length of the
         * neighbors or the number of handle points is different.
         *
         * TODO: The lengths can be cached so they aren't recomputed if a curve is a neighbor for
         * multiple new curves. Also, allocations could be avoided by reusing some arrays. */

        const Span<float3> neighbor_positions_cu = positions_cu.slice(neighbor_points);
        if (neighbor_positions_cu.size() == 1) {
          /* Skip interpolating positions from neighbors with only one point. */
          continue;
        }
        Array<float, 32> lengths(length_parameterize::segments_num(neighbor_points.size(), false));
        length_parameterize::accumulate_lengths<float3>(neighbor_positions_cu, false, lengths);
        const float neighbor_length_cu = lengths.last();

        Array<float, 32> sample_lengths(points.size());
        const float length_factor = std::min(1.0f, length_cu / neighbor_length_cu);
        const float resample_factor = (1.0f / (points.size() - 1.0f)) * length_factor;
        for (const int i : sample_lengths.index_range()) {
          sample_lengths[i] = i * resample_factor * neighbor_length_cu;
        }

        Array<int, 32> indices(points.size());
        Array<float, 32> factors(points.size());
        length_parameterize::sample_at_lengths(lengths, sample_lengths, indices, factors);

        for (const int i : IndexRange(points.size())) {
          const float3 sample_cu = math::interpolate(neighbor_positions_cu[indices[i]],
                                                     neighbor_positions_cu[indices[i] + 1],
                                                     factors[i]);
          const float3 relative_to_root_cu = sample_cu - neighbor_root_cu;
          float3 rotated_relative_coord = relative_to_root_cu;
          mul_m3_v3(normal_rotation_cu, rotated_relative_coord);
          positions_cu[points[i]] += neighbor.weight * rotated_relative_coord;
        }
      }
    }
  });
}

static void calc_radius_without_interpolation(CurvesGeometry &curves,
                                              const IndexRange new_points_range,
                                              const float radius)
{
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter radius_attr = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  radius_attr.span.slice(new_points_range).fill(radius);
  radius_attr.finish();
}

static void calc_radius_with_interpolation(CurvesGeometry &curves,
                                           const int old_curves_num,
                                           const float radius,
                                           const Span<float> new_lengths_cu,
                                           const Span<NeighborCurves> neighbors_per_curve)
{
  const int added_curves_num = new_lengths_cu.size();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter radius_attr = attributes.lookup_for_write_span<float>("radius");
  if (!radius_attr) {
    return;
  }

  MutableSpan<float3> positions_cu = curves.positions_for_write();
  MutableSpan<float> radii_cu = radius_attr.span;

  threading::parallel_for(IndexRange(added_curves_num), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const NeighborCurves &neighbors = neighbors_per_curve[i];
      const float length_cu = new_lengths_cu[i];
      const int curve_i = old_curves_num + i;
      const IndexRange points = points_by_curve[curve_i];

      if (neighbors.is_empty()) {
        /* If there are no neighbors, just using uniform radius. */
        radii_cu.slice(points).fill(radius);
        continue;
      }

      radii_cu.slice(points).fill(0.0f);

      for (const NeighborCurve &neighbor : neighbors) {
        const int neighbor_curve_i = neighbor.index;
        const IndexRange neighbor_points = points_by_curve[neighbor_curve_i];
        const Span<float3> neighbor_positions_cu = positions_cu.slice(neighbor_points);
        const Span<float> neighbor_radii_cu = radius_attr.span.slice(neighbor_points);

        Array<float, 32> lengths(length_parameterize::segments_num(neighbor_points.size(), false));
        length_parameterize::accumulate_lengths<float3>(neighbor_positions_cu, false, lengths);

        const float neighbor_length_cu = lengths.last();

        Array<float, 32> sample_lengths(points.size());
        const float length_factor = std::min(1.0f, length_cu / neighbor_length_cu);
        const float resample_factor = (1.0f / (points.size() - 1.0f)) * length_factor;
        for (const int i : sample_lengths.index_range()) {
          sample_lengths[i] = i * resample_factor * neighbor_length_cu;
        }

        Array<int, 32> indices(points.size());
        Array<float, 32> factors(points.size());
        length_parameterize::sample_at_lengths(lengths, sample_lengths, indices, factors);

        for (const int i : IndexRange(points.size())) {
          const float sample_cu = math::interpolate(
              neighbor_radii_cu[indices[i]], neighbor_radii_cu[indices[i] + 1], factors[i]);

          radii_cu[points[i]] += neighbor.weight * sample_cu;
        }
      }
    }
  });
  radius_attr.finish();
}

AddCurvesOnMeshOutputs add_curves_on_mesh(CurvesGeometry &curves,
                                          const AddCurvesOnMeshInputs &inputs)
{
  AddCurvesOnMeshOutputs outputs;

  const bool use_interpolation = inputs.interpolate_length || inputs.interpolate_point_count ||
                                 inputs.interpolate_radius || inputs.interpolate_shape ||
                                 inputs.interpolate_resolution;

  Vector<float3> root_positions_cu;
  Vector<float3> bary_coords;
  Vector<int> tri_indices;
  Vector<float2> used_uvs;

  /* Find faces that the passed in uvs belong to. */
  const Span<float3> surface_positions = inputs.surface->vert_positions();
  const Span<int> surface_corner_verts = inputs.surface->corner_verts();
  for (const int i : inputs.uvs.index_range()) {
    const float2 &uv = inputs.uvs[i];
    const ReverseUVSampler::Result result = inputs.reverse_uv_sampler->sample(uv);
    if (result.type != ReverseUVSampler::ResultType::Ok) {
      outputs.uv_error = true;
      continue;
    }
    const int3 &tri = inputs.surface_corner_tris[result.tri_index];
    bary_coords.append(result.bary_weights);
    tri_indices.append(result.tri_index);
    const float3 root_position_su = bke::attribute_math::mix3<float3>(
        result.bary_weights,
        surface_positions[surface_corner_verts[tri[0]]],
        surface_positions[surface_corner_verts[tri[1]]],
        surface_positions[surface_corner_verts[tri[2]]]);
    root_positions_cu.append(
        math::transform_point(inputs.transforms->surface_to_curves, root_position_su));
    used_uvs.append(uv);
  }

  Array<NeighborCurves> neighbors_per_curve;
  if (use_interpolation) {
    BLI_assert(inputs.old_roots_kdtree != nullptr);
    neighbors_per_curve = find_curve_neighbors(root_positions_cu, *inputs.old_roots_kdtree);
  }

  const int added_curves_num = root_positions_cu.size();
  const int old_points_num = curves.points_num();
  const int old_curves_num = curves.curves_num();
  const int new_curves_num = old_curves_num + added_curves_num;

  /* Grow number of curves first, so that the offsets array can be filled. */
  curves.resize(old_points_num, new_curves_num);
  if (new_curves_num == 0) {
    return outputs;
  }

  /* Compute new curve offsets. */
  MutableSpan<int> curve_offsets = curves.offsets_for_write();
  Array<int> new_point_counts_per_curve(added_curves_num);
  if (inputs.interpolate_point_count && old_curves_num > 0) {
    const OffsetIndices<int> old_points_by_curve{curve_offsets.take_front(old_curves_num + 1)};
    interpolate_from_neighbor_curves<int>(
        neighbors_per_curve,
        inputs.fallback_point_count,
        [&](const int curve_i) { return old_points_by_curve[curve_i].size(); },
        new_point_counts_per_curve);
  }
  else {
    new_point_counts_per_curve.fill(inputs.fallback_point_count);
  }
  curve_offsets[old_curves_num] = old_points_num;
  int offset = old_points_num;
  for (const int i : new_point_counts_per_curve.index_range()) {
    const int point_count_in_curve = new_point_counts_per_curve[i];
    curve_offsets[old_curves_num + i + 1] = offset + point_count_in_curve;
    offset += point_count_in_curve;
  }

  const int new_points_num = curves.offsets().last();
  curves.resize(new_points_num, new_curves_num);
  const OffsetIndices points_by_curve = curves.points_by_curve();

  /* The new elements are added at the end of the arrays. */
  outputs.new_points_range = curves.points_range().drop_front(old_points_num);
  outputs.new_curves_range = curves.curves_range().drop_front(old_curves_num);

  /* Initialize attachment information. */
  MutableSpan<float2> surface_uv_coords = curves.surface_uv_coords_for_write();
  surface_uv_coords.take_back(added_curves_num).copy_from(used_uvs);

  /* Determine length of new curves. */
  Span<float3> positions_cu = curves.positions();
  Array<float> new_lengths_cu(added_curves_num);
  if (inputs.interpolate_length) {
    interpolate_from_neighbor_curves<float>(
        neighbors_per_curve,
        inputs.fallback_curve_length,
        [&](const int curve_i) {
          const IndexRange points = points_by_curve[curve_i];
          float length = 0.0f;
          for (const int segment_i : points.drop_back(1)) {
            const float3 &p1 = positions_cu[segment_i];
            const float3 &p2 = positions_cu[segment_i + 1];
            length += math::distance(p1, p2);
          }
          return length;
        },
        new_lengths_cu);
  }
  else {
    new_lengths_cu.fill(inputs.fallback_curve_length);
  }

  /* Find surface normal at root points. */
  Array<float3> new_normals_su(added_curves_num);
  bke::mesh_surface_sample::sample_corner_normals(inputs.surface_corner_tris,
                                                  tri_indices,
                                                  bary_coords,
                                                  inputs.corner_normals_su,
                                                  IndexMask(added_curves_num),
                                                  new_normals_su);

  /* Initialize position attribute. */
  if (inputs.interpolate_shape) {
    calc_position_with_interpolation(curves,
                                     root_positions_cu,
                                     neighbors_per_curve,
                                     old_curves_num,
                                     new_lengths_cu,
                                     new_normals_su,
                                     *inputs.transforms,
                                     inputs.surface_corner_tris,
                                     *inputs.reverse_uv_sampler,
                                     inputs.corner_normals_su);
  }
  else {
    calc_position_without_interpolation(curves,
                                        old_curves_num,
                                        root_positions_cu,
                                        new_lengths_cu,
                                        new_normals_su,
                                        inputs.transforms->surface_to_curves_normal);
  }

  /* Initialize radius attribute */
  if (inputs.interpolate_radius) {
    calc_radius_with_interpolation(
        curves, old_curves_num, inputs.fallback_curve_radius, new_lengths_cu, neighbors_per_curve);
  }
  else {
    calc_radius_without_interpolation(
        curves, outputs.new_points_range, inputs.fallback_curve_radius);
  }

  curves.fill_curve_types(outputs.new_curves_range, CURVE_TYPE_CATMULL_ROM);

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  if (bke::SpanAttributeWriter<int> resolution = attributes.lookup_for_write_span<int>(
          "resolution"))
  {
    if (inputs.interpolate_resolution) {
      interpolate_from_neighbor_curves(
          neighbors_per_curve,
          12,
          [&](const int curve_i) { return resolution.span[curve_i]; },
          resolution.span.take_back(added_curves_num));
      resolution.finish();
    }
    else {
      resolution.span.take_back(added_curves_num).fill(12);
    }
  }

  /* Explicitly set all other attributes besides those processed above to default values. */
  bke::fill_attribute_range_default(
      attributes, bke::AttrDomain::Point, {"position", "radius"}, outputs.new_points_range);
  bke::fill_attribute_range_default(attributes,
                                    bke::AttrDomain::Curve,
                                    {"curve_type", "surface_uv_coordinate", "resolution"},
                                    outputs.new_curves_range);

  return outputs;
}

}  // namespace blender::geometry
