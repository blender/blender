/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh_sample.hh"
#include "BKE_spline.hh"

#include "GEO_add_curves_on_mesh.hh"

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

static float3 compute_surface_point_normal(const MLoopTri &looptri,
                                           const float3 &bary_coord,
                                           const Span<float3> corner_normals)
{
  const int l0 = looptri.tri[0];
  const int l1 = looptri.tri[1];
  const int l2 = looptri.tri[2];

  const float3 &l0_normal = corner_normals[l0];
  const float3 &l1_normal = corner_normals[l1];
  const float3 &l2_normal = corner_normals[l2];

  const float3 normal = math::normalize(
      attribute_math::mix3(bary_coord, l0_normal, l1_normal, l2_normal));
  return normal;
}

static void initialize_straight_curve_positions(const float3 &p1,
                                                const float3 &p2,
                                                MutableSpan<float3> r_positions)
{
  const float step = 1.0f / (float)(r_positions.size() - 1);
  for (const int i : r_positions.index_range()) {
    r_positions[i] = math::interpolate(p1, p2, i * step);
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
void interpolate_from_neighbors(const Span<NeighborCurves> neighbors_per_curve,
                                const T &fallback,
                                const GetValueF &get_value_from_neighbor,
                                MutableSpan<T> r_interpolated_values)
{
  attribute_math::DefaultMixer<T> mixer{r_interpolated_values};
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
  });
  mixer.finalize();
}

static void interpolate_position_without_interpolation(
    CurvesGeometry &curves,
    const int old_curves_num,
    const Span<float3> root_positions_cu,
    const Span<float> new_lengths_cu,
    const Span<float3> new_normals_su,
    const float4x4 &surface_to_curves_normal_mat)
{
  const int added_curves_num = root_positions_cu.size();
  MutableSpan<float3> positions_cu = curves.positions_for_write();
  threading::parallel_for(IndexRange(added_curves_num), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const int curve_i = old_curves_num + i;
      const IndexRange points = curves.points_for_curve(curve_i);
      const float3 &root_cu = root_positions_cu[i];
      const float length = new_lengths_cu[i];
      const float3 &normal_su = new_normals_su[i];
      const float3 normal_cu = math::normalize(surface_to_curves_normal_mat * normal_su);
      const float3 tip_cu = root_cu + length * normal_cu;

      initialize_straight_curve_positions(root_cu, tip_cu, positions_cu.slice(points));
    }
  });
}

static void interpolate_position_with_interpolation(CurvesGeometry &curves,
                                                    const Span<float3> root_positions_cu,
                                                    const Span<NeighborCurves> neighbors_per_curve,
                                                    const int old_curves_num,
                                                    const Span<float> new_lengths_cu,
                                                    const Span<float3> new_normals_su,
                                                    const float4x4 &surface_to_curves_normal_mat,
                                                    const float4x4 &curves_to_surface_mat,
                                                    const BVHTreeFromMesh &surface_bvh,
                                                    const Span<MLoopTri> surface_looptris,
                                                    const Mesh &surface,
                                                    const Span<float3> corner_normals_su)
{
  MutableSpan<float3> positions_cu = curves.positions_for_write();
  const int added_curves_num = root_positions_cu.size();

  threading::parallel_for(IndexRange(added_curves_num), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const NeighborCurves &neighbors = neighbors_per_curve[i];
      const int curve_i = old_curves_num + i;
      const IndexRange points = curves.points_for_curve(curve_i);

      const float length_cu = new_lengths_cu[i];
      const float3 &normal_su = new_normals_su[i];
      const float3 normal_cu = math::normalize(surface_to_curves_normal_mat * normal_su);

      const float3 &root_cu = root_positions_cu[i];

      if (neighbors.is_empty()) {
        /* If there are no neighbors, just make a straight line. */
        const float3 tip_cu = root_cu + length_cu * normal_cu;
        initialize_straight_curve_positions(root_cu, tip_cu, positions_cu.slice(points));
        continue;
      }

      positions_cu.slice(points).fill(root_cu);

      for (const NeighborCurve &neighbor : neighbors) {
        const int neighbor_curve_i = neighbor.index;
        const float3 &neighbor_first_pos_cu = positions_cu[curves.offsets()[neighbor_curve_i]];
        const float3 neighbor_first_pos_su = curves_to_surface_mat * neighbor_first_pos_cu;

        BVHTreeNearest nearest;
        nearest.dist_sq = FLT_MAX;
        BLI_bvhtree_find_nearest(surface_bvh.tree,
                                 neighbor_first_pos_su,
                                 &nearest,
                                 surface_bvh.nearest_callback,
                                 const_cast<BVHTreeFromMesh *>(&surface_bvh));
        const int neighbor_looptri_index = nearest.index;
        const MLoopTri &neighbor_looptri = surface_looptris[neighbor_looptri_index];

        const float3 neighbor_bary_coord =
            bke::mesh_surface_sample::compute_bary_coord_in_triangle(
                surface, neighbor_looptri, nearest.co);

        const float3 neighbor_normal_su = compute_surface_point_normal(
            surface_looptris[neighbor_looptri_index], neighbor_bary_coord, corner_normals_su);
        const float3 neighbor_normal_cu = math::normalize(surface_to_curves_normal_mat *
                                                          neighbor_normal_su);

        /* The rotation matrix used to transform relative coordinates of the neighbor curve
         * to the new curve. */
        float normal_rotation_cu[3][3];
        rotation_between_vecs_to_mat3(normal_rotation_cu, neighbor_normal_cu, normal_cu);

        const IndexRange neighbor_points = curves.points_for_curve(neighbor_curve_i);
        const float3 &neighbor_root_cu = positions_cu[neighbor_points[0]];

        /* Use a temporary #PolySpline, because that's the easiest way to resample an
         * existing curve right now. Resampling is necessary if the length of the new curve
         * does not match the length of the neighbors or the number of handle points is
         * different. */
        PolySpline neighbor_spline;
        neighbor_spline.resize(neighbor_points.size());
        neighbor_spline.positions().copy_from(positions_cu.slice(neighbor_points));
        neighbor_spline.mark_cache_invalid();

        const float neighbor_length_cu = neighbor_spline.length();
        const float length_factor = std::min(1.0f, length_cu / neighbor_length_cu);

        const float resample_factor = (1.0f / (points.size() - 1.0f)) * length_factor;
        for (const int j : IndexRange(points.size())) {
          const Spline::LookupResult lookup = neighbor_spline.lookup_evaluated_factor(
              j * resample_factor);
          const float index_factor = lookup.evaluated_index + lookup.factor;
          float3 p;
          neighbor_spline.sample_with_index_factors<float3>(
              neighbor_spline.positions(), {&index_factor, 1}, {&p, 1});
          const float3 relative_coord = p - neighbor_root_cu;
          float3 rotated_relative_coord = relative_coord;
          mul_m3_v3(normal_rotation_cu, rotated_relative_coord);
          positions_cu[points[j]] += neighbor.weight * rotated_relative_coord;
        }
      }
    }
  });
}

void add_curves_on_mesh(CurvesGeometry &curves, const AddCurvesOnMeshInputs &inputs)
{
  const bool use_interpolation = inputs.interpolate_length || inputs.interpolate_point_count ||
                                 inputs.interpolate_shape;

  Array<NeighborCurves> neighbors_per_curve;
  if (use_interpolation) {
    BLI_assert(inputs.old_roots_kdtree != nullptr);
    neighbors_per_curve = find_curve_neighbors(inputs.root_positions_cu, *inputs.old_roots_kdtree);
  }

  const int added_curves_num = inputs.root_positions_cu.size();
  const int old_points_num = curves.points_num();
  const int old_curves_num = curves.curves_num();
  const int new_curves_num = old_curves_num + added_curves_num;

  /* Grow number of curves first, so that the offsets array can be filled. */
  curves.resize(old_points_num, new_curves_num);

  /* Compute new curve offsets. */
  MutableSpan<int> curve_offsets = curves.offsets_for_write();
  MutableSpan<int> new_point_counts_per_curve = curve_offsets.take_back(added_curves_num);
  if (inputs.interpolate_point_count) {
    interpolate_from_neighbors<int>(
        neighbors_per_curve,
        inputs.fallback_point_count,
        [&](const int curve_i) { return curves.points_for_curve(curve_i).size(); },
        new_point_counts_per_curve);
  }
  else {
    new_point_counts_per_curve.fill(inputs.fallback_point_count);
  }
  for (const int i : IndexRange(added_curves_num)) {
    curve_offsets[old_curves_num + i + 1] += curve_offsets[old_curves_num + i];
  }

  const int new_points_num = curves.offsets().last();
  curves.resize(new_points_num, new_curves_num);
  MutableSpan<float3> positions_cu = curves.positions_for_write();

  /* Determine length of new curves. */
  Array<float> new_lengths_cu(added_curves_num);
  if (inputs.interpolate_length) {
    interpolate_from_neighbors<float>(
        neighbors_per_curve,
        inputs.fallback_curve_length,
        [&](const int curve_i) {
          const IndexRange points = curves.points_for_curve(curve_i);
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
  threading::parallel_for(IndexRange(added_curves_num), 256, [&](const IndexRange range) {
    for (const int i : range) {
      const int looptri_index = inputs.looptri_indices[i];
      const float3 &bary_coord = inputs.bary_coords[i];
      new_normals_su[i] = compute_surface_point_normal(
          inputs.surface_looptris[looptri_index], bary_coord, inputs.corner_normals_su);
    }
  });

  /* Propagate attachment information. */
  if (!inputs.surface_uv_map.is_empty()) {
    MutableSpan<float2> surface_uv_coords = curves.surface_uv_coords_for_write();
    bke::mesh_surface_sample::sample_corner_attribute(
        *inputs.surface,
        inputs.looptri_indices,
        inputs.bary_coords,
        GVArray::ForSpan(inputs.surface_uv_map),
        IndexRange(added_curves_num),
        surface_uv_coords.take_back(added_curves_num));
  }

  /* Update selection arrays when available. */
  const VArray<float> points_selection = curves.selection_point_float();
  if (points_selection.is_span()) {
    MutableSpan<float> points_selection_span = curves.selection_point_float_for_write();
    points_selection_span.drop_front(old_points_num).fill(1.0f);
  }
  const VArray<float> curves_selection = curves.selection_curve_float();
  if (curves_selection.is_span()) {
    MutableSpan<float> curves_selection_span = curves.selection_curve_float_for_write();
    curves_selection_span.drop_front(old_curves_num).fill(1.0f);
  }

  /* Initialize position attribute. */
  if (inputs.interpolate_shape) {
    interpolate_position_with_interpolation(curves,
                                            inputs.root_positions_cu,
                                            neighbors_per_curve,
                                            old_curves_num,
                                            new_lengths_cu,
                                            new_normals_su,
                                            inputs.surface_to_curves_normal_mat,
                                            inputs.curves_to_surface_mat,
                                            *inputs.surface_bvh,
                                            inputs.surface_looptris,
                                            *inputs.surface,
                                            inputs.corner_normals_su);
  }
  else {
    interpolate_position_without_interpolation(curves,
                                               old_curves_num,
                                               inputs.root_positions_cu,
                                               new_lengths_cu,
                                               new_normals_su,
                                               inputs.surface_to_curves_normal_mat);
  }

  curves.update_curve_types();
}

}  // namespace blender::geometry
