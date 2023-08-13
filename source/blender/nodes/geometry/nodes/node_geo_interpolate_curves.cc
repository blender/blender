/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_vector.hh"

#include "BLI_kdtree.h"
#include "BLI_length_parameterize.hh"
#include "BLI_math_rotation.h"
#include "BLI_task.hh"

#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"

#include "DNA_pointcloud_types.h"

namespace blender::nodes::node_geo_interpolate_curves_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Guide Curves")
      .description("Base curves that new curves are interpolated between");
  b.add_input<decl::Vector>("Guide Up")
      .field_on({0})
      .hide_value()
      .description("Optional up vector that is typically a surface normal");
  b.add_input<decl::Int>("Guide Group ID")
      .field_on({0})
      .hide_value()
      .description(
          "Splits guides into separate groups. New curves interpolate existing curves "
          "from a single group");
  b.add_input<decl::Geometry>("Points").description(
      "First control point positions for new interpolated curves");
  b.add_input<decl::Vector>("Point Up")
      .field_on({3})
      .hide_value()
      .description("Optional up vector that is typically a surface normal");
  b.add_input<decl::Int>("Point Group ID")
      .field_on({3})
      .hide_value()
      .description("The curve group to interpolate in");
  b.add_input<decl::Int>("Max Neighbors")
      .default_value(4)
      .min(1)
      .description(
          "Maximum amount of close guide curves that are taken into account for interpolation");
  b.add_output<decl::Geometry>("Curves").propagate_all();
  b.add_output<decl::Int>("Closest Index")
      .field_on_all()
      .description("Index of the closest guide curve for each generated curve");
  b.add_output<decl::Float>("Closest Weight")
      .field_on_all()
      .description("Weight of the closest guide curve for each generated curve");
}

/**
 * Guides are split into groups. Every point will only interpolate between guides within the group
 * with the same id.
 */
static MultiValueMap<int, int> separate_guides_by_group(const VArray<int> &guide_group_ids)
{
  MultiValueMap<int, int> guides_by_group;
  for (const int curve_i : guide_group_ids.index_range()) {
    const int group = guide_group_ids[curve_i];
    guides_by_group.add(group, curve_i);
  }
  return guides_by_group;
}

/**
 * Checks if all curves within a group have the same number of points. If yes, a better
 * interpolation algorithm can be used, that does not require resampling curves.
 */
static Map<int, int> compute_points_per_curve_by_group(
    const MultiValueMap<int, int> &guides_by_group, const bke::CurvesGeometry &guide_curves)
{
  const OffsetIndices points_by_curve = guide_curves.points_by_curve();
  Map<int, int> points_per_curve_by_group;
  for (const auto &[group, guide_curve_indices] : guides_by_group.items()) {
    int group_control_points = points_by_curve[guide_curve_indices[0]].size();
    for (const int guide_curve_i : guide_curve_indices.as_span().drop_front(1)) {
      const int control_points = points_by_curve[guide_curve_i].size();
      if (group_control_points != control_points) {
        group_control_points = -1;
        break;
      }
    }
    if (group_control_points != -1) {
      points_per_curve_by_group.add(group, group_control_points);
    }
  }
  return points_per_curve_by_group;
}

/**
 * Build a kdtree for every guide group.
 */
static Map<int, KDTree_3d *> build_kdtrees_for_root_positions(
    const MultiValueMap<int, int> &guides_by_group, const bke::CurvesGeometry &guide_curves)
{
  Map<int, KDTree_3d *> kdtrees;
  const Span<float3> positions = guide_curves.positions();
  const Span<int> offsets = guide_curves.offsets();

  for (const auto item : guides_by_group.items()) {
    const int group = item.key;
    const Span<int> guide_indices = item.value;

    KDTree_3d *kdtree = BLI_kdtree_3d_new(guide_indices.size());
    kdtrees.add_new(group, kdtree);

    for (const int curve_i : guide_indices) {
      const int first_point_i = offsets[curve_i];
      const float3 &root_pos = positions[first_point_i];
      BLI_kdtree_3d_insert(kdtree, curve_i, root_pos);
    }
  }
  threading::parallel_for_each(kdtrees.values(),
                               [](KDTree_3d *kdtree) { BLI_kdtree_3d_balance(kdtree); });
  return kdtrees;
}

/**
 * For every start point of newly generated curves, find the closest guide curves within the same
 * group and compute a weight for each of them.
 */
static void find_neighbor_guides(const Span<float3> positions,
                                 const VArray<int> point_group_ids,
                                 const Map<int, KDTree_3d *> kdtrees,
                                 const MultiValueMap<int, int> &guides_by_group,
                                 const int max_neighbor_count,
                                 MutableSpan<int> r_all_neighbor_indices,
                                 MutableSpan<float> r_all_neighbor_weights,
                                 MutableSpan<int> r_all_neighbor_counts)
{
  threading::parallel_for(positions.index_range(), 128, [&](const IndexRange range) {
    for (const int child_curve_i : range) {
      const float3 &position = positions[child_curve_i];
      const int group = point_group_ids[child_curve_i];
      const KDTree_3d *kdtree = kdtrees.lookup_default(group, nullptr);
      if (kdtree == nullptr) {
        r_all_neighbor_counts[child_curve_i] = 0;
        continue;
      }

      const int num_guides_in_group = guides_by_group.lookup(group).size();
      /* Finding an additional neighbor that currently has weight zero is necessary to ensure that
       * curves close by but with different guides still look similar. Otherwise there can be
       * visible artifacts. */
      const bool use_extra_neighbor = num_guides_in_group > max_neighbor_count;
      const int neighbors_to_find = max_neighbor_count + use_extra_neighbor;

      Vector<KDTreeNearest_3d, 16> nearest_n(neighbors_to_find);
      const int num_neighbors = BLI_kdtree_3d_find_nearest_n(
          kdtree, position, nearest_n.data(), neighbors_to_find);
      if (num_neighbors == 0) {
        r_all_neighbor_counts[child_curve_i] = 0;
        continue;
      }

      const IndexRange neighbors_range{child_curve_i * max_neighbor_count, max_neighbor_count};
      MutableSpan<int> neighbor_indices = r_all_neighbor_indices.slice(neighbors_range);
      MutableSpan<float> neighbor_weights = r_all_neighbor_weights.slice(neighbors_range);

      float tot_weight = 0.0f;
      /* A different weighting algorithm is necessary for smooth transitions when desired. */
      if (use_extra_neighbor) {
        /* Find the distance to the guide with the largest distance. At this distance, the weight
         * should become zero. */
        const float max_distance = std::max_element(
                                       nearest_n.begin(),
                                       nearest_n.begin() + num_neighbors,
                                       [](const KDTreeNearest_3d &a, const KDTreeNearest_3d &b) {
                                         return a.dist < b.dist;
                                       })
                                       ->dist;
        if (max_distance == 0.0f) {
          r_all_neighbor_counts[child_curve_i] = 1;
          neighbor_indices[0] = nearest_n[0].index;
          neighbor_weights[0] = 1.0f;
          continue;
        }

        int neighbor_counter = 0;
        for (const int neighbor_i : IndexRange(num_neighbors)) {
          const KDTreeNearest_3d &nearest = nearest_n[neighbor_i];
          /* Goal for this weight calculation:
           * - As distance gets closer to zero, it should become very large.
           * - At `max_distance` the weight should be zero. */
          const float weight = (max_distance - nearest.dist) / std::max(nearest.dist, 0.000001f);
          if (weight > 0.0f) {
            tot_weight += weight;
            neighbor_indices[neighbor_counter] = nearest.index;
            neighbor_weights[neighbor_counter] = weight;
            neighbor_counter++;
          }
        }
        r_all_neighbor_counts[child_curve_i] = neighbor_counter;
      }
      else {
        int neighbor_counter = 0;
        for (const int neighbor_i : IndexRange(num_neighbors)) {
          const KDTreeNearest_3d &nearest = nearest_n[neighbor_i];
          /* Goal for this weight calculation:
           * - As the distance gets closer to zero, it should become very large.
           * - As the distance gets larger, the weight should become zero. */
          const float weight = 1.0f / std::max(nearest.dist, 0.000001f);
          if (weight > 0.0f) {
            tot_weight += weight;
            neighbor_indices[neighbor_counter] = nearest.index;
            neighbor_weights[neighbor_counter] = weight;
            neighbor_counter++;
          }
        }
        r_all_neighbor_counts[child_curve_i] = neighbor_counter;
      }
      if (tot_weight > 0.0f) {
        /* Normalize weights so that their sum is 1. */
        const float weight_factor = 1.0f / tot_weight;
        for (float &weight : neighbor_weights.take_front(r_all_neighbor_counts[child_curve_i])) {
          weight *= weight_factor;
        }
      }
    }
  });
}

/**
 * Compute how many points each generated curve will have. This is determined by looking at
 * neighboring points.
 */
static void compute_point_counts_per_child(const bke::CurvesGeometry &guide_curves,
                                           const VArray<int> &point_group_ids,
                                           const Map<int, int> &points_per_curve_by_group,
                                           const Span<int> all_neighbor_indices,
                                           const Span<float> all_neighbor_weights,
                                           const Span<int> all_neighbor_counts,
                                           const int max_neighbors,
                                           MutableSpan<int> r_points_per_child,
                                           MutableSpan<bool> r_use_direct_interpolation)
{
  const OffsetIndices guide_points_by_curve = guide_curves.points_by_curve();
  threading::parallel_for(r_points_per_child.index_range(), 512, [&](const IndexRange range) {
    for (const int child_curve_i : range) {
      const int neighbor_count = all_neighbor_counts[child_curve_i];
      if (neighbor_count == 0) {
        r_points_per_child[child_curve_i] = 1;
        r_use_direct_interpolation[child_curve_i] = false;
        continue;
      }
      const int group = point_group_ids[child_curve_i];
      const int points_per_curve_in_group = points_per_curve_by_group.lookup_default(group, -1);
      if (points_per_curve_in_group != -1) {
        r_points_per_child[child_curve_i] = points_per_curve_in_group;
        r_use_direct_interpolation[child_curve_i] = true;
        continue;
      }
      const IndexRange neighbors_range{child_curve_i * max_neighbors, neighbor_count};
      const Span<float> neighbor_weights = all_neighbor_weights.slice(neighbors_range);
      const Span<int> neighbor_indices = all_neighbor_indices.slice(neighbors_range);

      float neighbor_points_weighted_sum = 0.0f;
      for (const int neighbor_i : IndexRange(neighbor_count)) {
        const int neighbor_index = neighbor_indices[neighbor_i];
        const float neighbor_weight = neighbor_weights[neighbor_i];
        const int neighbor_points = guide_points_by_curve[neighbor_index].size();
        neighbor_points_weighted_sum += neighbor_weight * float(neighbor_points);
      }
      const int points_in_child = std::max<int>(1, roundf(neighbor_points_weighted_sum));
      r_points_per_child[child_curve_i] = points_in_child;
      r_use_direct_interpolation[child_curve_i] = false;
    }
  });
}

/**
 * Prepares parameterized guide curves so that they can be used efficiently during interpolation.
 */
static void parameterize_guide_curves(const bke::CurvesGeometry &guide_curves,
                                      Array<int> &r_parameterized_guide_offsets,
                                      Array<float> &r_parameterized_guide_lengths)
{
  r_parameterized_guide_offsets.reinitialize(guide_curves.curves_num() + 1);
  const OffsetIndices guide_points_by_curve = guide_curves.points_by_curve();
  threading::parallel_for(guide_curves.curves_range(), 1024, [&](const IndexRange range) {
    for (const int guide_curve_i : range) {
      r_parameterized_guide_offsets[guide_curve_i] = length_parameterize::segments_num(
          guide_points_by_curve[guide_curve_i].size(), false);
    }
  });
  offset_indices::accumulate_counts_to_offsets(r_parameterized_guide_offsets);
  const OffsetIndices<int> parameterize_offsets{r_parameterized_guide_offsets};

  r_parameterized_guide_lengths.reinitialize(r_parameterized_guide_offsets.last());
  const Span<float3> guide_positions = guide_curves.positions();
  threading::parallel_for(guide_curves.curves_range(), 256, [&](const IndexRange range) {
    for (const int guide_curve_i : range) {
      const IndexRange points = guide_points_by_curve[guide_curve_i];
      const IndexRange lengths_range = parameterize_offsets[guide_curve_i];
      length_parameterize::accumulate_lengths<float3>(
          guide_positions.slice(points),
          false,
          r_parameterized_guide_lengths.as_mutable_span().slice(lengths_range));
    }
  });
}

/**
 * Initialize child curve positions by interpolating between guide curves.
 */
static void interpolate_curve_shapes(bke::CurvesGeometry &child_curves,
                                     const bke::CurvesGeometry &guide_curves,
                                     const int max_neighbors,
                                     const Span<int> all_neighbor_indices,
                                     const Span<float> all_neighbor_weights,
                                     const Span<int> all_neighbor_counts,
                                     const VArray<float3> &guides_up,
                                     const VArray<float3> &points_up,
                                     const Span<float3> point_positions,
                                     const OffsetIndices<int> parameterized_guide_offsets,
                                     const Span<float> parameterized_guide_lengths,
                                     const Span<bool> use_direct_interpolation_per_child)
{
  const OffsetIndices guide_points_by_curve = guide_curves.points_by_curve();
  const OffsetIndices child_points_by_curve = child_curves.points_by_curve();
  const MutableSpan<float3> children_positions = child_curves.positions_for_write();
  const Span<float3> guide_positions = guide_curves.positions();

  threading::parallel_for(child_curves.curves_range(), 128, [&](const IndexRange range) {
    Vector<float, 16> sample_lengths;
    Vector<int, 16> sample_segments;
    Vector<float, 16> sample_factors;

    for (const int child_curve_i : range) {
      const IndexRange points = child_points_by_curve[child_curve_i];
      const int neighbor_count = all_neighbor_counts[child_curve_i];
      const float3 child_up = points_up[child_curve_i];
      BLI_assert(math::is_unit_scale(child_up));
      const float3 &child_root_position = point_positions[child_curve_i];
      MutableSpan<float3> child_positions = children_positions.slice(points);

      child_positions.fill(child_root_position);
      if (neighbor_count == 0) {
        /* Creates a curve with a single point at the root position. */
        continue;
      }

      const IndexRange neighbors_range{child_curve_i * max_neighbors, neighbor_count};
      const Span<float> neighbor_weights = all_neighbor_weights.slice(neighbors_range);
      const Span<int> neighbor_indices = all_neighbor_indices.slice(neighbors_range);

      const bool use_direct_interpolation = use_direct_interpolation_per_child[child_curve_i];

      for (const int neighbor_i : IndexRange(neighbor_count)) {
        const int neighbor_index = neighbor_indices[neighbor_i];
        const float neighbor_weight = neighbor_weights[neighbor_i];
        const IndexRange guide_points = guide_points_by_curve[neighbor_index];
        const Span<float3> neighbor_positions = guide_positions.slice(guide_points);
        const float3 &neighbor_root = neighbor_positions[0];
        const float3 neighbor_up = guides_up[neighbor_index];
        BLI_assert(math::is_unit_scale(neighbor_up));

        const bool is_same_up_vector = neighbor_up == child_up;

        float3x3 normal_rotation;
        if (!is_same_up_vector) {
          rotation_between_vecs_to_mat3(normal_rotation.ptr(), neighbor_up, child_up);
        }

        if (use_direct_interpolation) {
          /* In this method, the control point positions are interpolated directly instead of
           * looking at evaluated points. This is much faster than the method below but only works
           * if all guides have the same number of points. */
          for (const int i : IndexRange(points.size())) {
            const float3 &neighbor_pos = neighbor_positions[i];
            const float3 relative_to_root = neighbor_pos - neighbor_root;
            float3 rotated_relative = relative_to_root;
            if (!is_same_up_vector) {
              rotated_relative = normal_rotation * rotated_relative;
            }
            child_positions[i] += neighbor_weight * rotated_relative;
          }
        }
        else {
          /* This method is used when guide curves have different amounts of control points. In
           * this case, some additional interpolation is necessary compared to the method above. */

          const Span<float> lengths = parameterized_guide_lengths.slice(
              parameterized_guide_offsets[neighbor_index]);
          const float neighbor_length = lengths.last();

          sample_lengths.reinitialize(points.size());
          const float sample_length_factor = safe_divide(neighbor_length, points.size() - 1);
          for (const int i : sample_lengths.index_range()) {
            sample_lengths[i] = i * sample_length_factor;
          }

          sample_segments.reinitialize(points.size());
          sample_factors.reinitialize(points.size());
          length_parameterize::sample_at_lengths(
              lengths, sample_lengths, sample_segments, sample_factors);

          for (const int i : IndexRange(points.size())) {
            const int segment = sample_segments[i];
            const float factor = sample_factors[i];
            const float3 sample_pos = math::interpolate(
                neighbor_positions[segment], neighbor_positions[segment + 1], factor);
            const float3 relative_to_root = sample_pos - neighbor_root;
            float3 rotated_relative = relative_to_root;
            if (!is_same_up_vector) {
              rotated_relative = normal_rotation * rotated_relative;
            }
            child_positions[i] += neighbor_weight * rotated_relative;
          }
        }
      }
    }
  });

  /* Can only create catmull rom curves for now. */
  child_curves.fill_curve_types(CURVE_TYPE_CATMULL_ROM);
}

/**
 * Propagate attributes from the guides and source points to the child curves.
 */
static void interpolate_curve_attributes(bke::CurvesGeometry &child_curves,
                                         const bke::CurvesGeometry &guide_curves,
                                         const AttributeAccessor &point_attributes,
                                         const AnonymousAttributePropagationInfo &propagation_info,
                                         const int max_neighbors,
                                         const Span<int> all_neighbor_indices,
                                         const Span<float> all_neighbor_weights,
                                         const Span<int> all_neighbor_counts,
                                         const OffsetIndices<int> parameterized_guide_offsets,
                                         const Span<float> parameterized_guide_lengths,
                                         const Span<bool> use_direct_interpolation_per_child)
{
  const AttributeAccessor guide_curve_attributes = guide_curves.attributes();
  MutableAttributeAccessor children_attributes = child_curves.attributes_for_write();

  const OffsetIndices child_points_by_curve = child_curves.points_by_curve();
  const OffsetIndices guide_points_by_curve = guide_curves.points_by_curve();

  /* Interpolate attributes from guide curves to child curves. Attributes stay on the same domain
   * that they had on the guides. */
  guide_curve_attributes.for_all([&](const AttributeIDRef &id,
                                     const AttributeMetaData &meta_data) {
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    const eCustomDataType type = meta_data.data_type;
    if (type == CD_PROP_STRING) {
      return true;
    }
    if (guide_curve_attributes.is_builtin(id) && !ELEM(id.name(), "radius", "tilt", "resolution"))
    {
      return true;
    }

    if (meta_data.domain == ATTR_DOMAIN_CURVE) {
      const GVArraySpan src_generic = *guide_curve_attributes.lookup(id, ATTR_DOMAIN_CURVE, type);

      GSpanAttributeWriter dst_generic = children_attributes.lookup_or_add_for_write_only_span(
          id, ATTR_DOMAIN_CURVE, type);
      if (!dst_generic) {
        return true;
      }
      bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
        using T = decltype(dummy);
        const Span<T> src = src_generic.typed<T>();
        MutableSpan<T> dst = dst_generic.span.typed<T>();

        bke::attribute_math::DefaultMixer<T> mixer(dst);
        threading::parallel_for(child_curves.curves_range(), 256, [&](const IndexRange range) {
          for (const int child_curve_i : range) {
            const int neighbor_count = all_neighbor_counts[child_curve_i];
            const IndexRange neighbors_range{child_curve_i * max_neighbors, neighbor_count};
            const Span<float> neighbor_weights = all_neighbor_weights.slice(neighbors_range);
            const Span<int> neighbor_indices = all_neighbor_indices.slice(neighbors_range);

            for (const int neighbor_i : IndexRange(neighbor_count)) {
              const int neighbor_index = neighbor_indices[neighbor_i];
              const float neighbor_weight = neighbor_weights[neighbor_i];
              mixer.mix_in(child_curve_i, src[neighbor_index], neighbor_weight);
            }
          }
          mixer.finalize(range);
        });
      });

      dst_generic.finish();
    }
    else {
      BLI_assert(meta_data.domain == ATTR_DOMAIN_POINT);
      const GVArraySpan src_generic = *guide_curve_attributes.lookup(id, ATTR_DOMAIN_POINT, type);
      GSpanAttributeWriter dst_generic = children_attributes.lookup_or_add_for_write_only_span(
          id, ATTR_DOMAIN_POINT, type);
      if (!dst_generic) {
        return true;
      }

      bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
        using T = decltype(dummy);
        const Span<T> src = src_generic.typed<T>();
        MutableSpan<T> dst = dst_generic.span.typed<T>();

        bke::attribute_math::DefaultMixer<T> mixer(dst);
        threading::parallel_for(child_curves.curves_range(), 256, [&](const IndexRange range) {
          Vector<float, 16> sample_lengths;
          Vector<int, 16> sample_segments;
          Vector<float, 16> sample_factors;
          for (const int child_curve_i : range) {
            const IndexRange points = child_points_by_curve[child_curve_i];
            const int neighbor_count = all_neighbor_counts[child_curve_i];
            const IndexRange neighbors_range{child_curve_i * max_neighbors, neighbor_count};
            const Span<float> neighbor_weights = all_neighbor_weights.slice(neighbors_range);
            const Span<int> neighbor_indices = all_neighbor_indices.slice(neighbors_range);
            const bool use_direct_interpolation =
                use_direct_interpolation_per_child[child_curve_i];

            for (const int neighbor_i : IndexRange(neighbor_count)) {
              const int neighbor_index = neighbor_indices[neighbor_i];
              const float neighbor_weight = neighbor_weights[neighbor_i];
              const IndexRange guide_points = guide_points_by_curve[neighbor_index];

              if (use_direct_interpolation) {
                for (const int i : IndexRange(points.size())) {
                  mixer.mix_in(points[i], src[guide_points[i]], neighbor_weight);
                }
              }
              else {
                const Span<float> lengths = parameterized_guide_lengths.slice(
                    parameterized_guide_offsets[neighbor_index]);
                const float neighbor_length = lengths.last();

                sample_lengths.reinitialize(points.size());
                const float sample_length_factor = safe_divide(neighbor_length, points.size() - 1);
                for (const int i : sample_lengths.index_range()) {
                  sample_lengths[i] = i * sample_length_factor;
                }

                sample_segments.reinitialize(points.size());
                sample_factors.reinitialize(points.size());
                length_parameterize::sample_at_lengths(
                    lengths, sample_lengths, sample_segments, sample_factors);

                for (const int i : IndexRange(points.size())) {
                  const int segment = sample_segments[i];
                  const float factor = sample_factors[i];
                  const T value = math::interpolate(
                      src[guide_points[segment]], src[guide_points[segment + 1]], factor);
                  mixer.mix_in(points[i], value, neighbor_weight);
                }
              }
            }
          }
          mixer.finalize(child_points_by_curve[range]);
        });
      });

      dst_generic.finish();
    }

    return true;
  });

  /* Interpolate attributes from the points to child curves. All attributes become curve
   * attributes. */
  point_attributes.for_all([&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
    if (point_attributes.is_builtin(id) && !children_attributes.is_builtin(id)) {
      return true;
    }
    if (guide_curve_attributes.contains(id)) {
      return true;
    }
    if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
      return true;
    }
    if (meta_data.data_type == CD_PROP_STRING) {
      return true;
    }

    const GAttributeReader src = point_attributes.lookup(id);
    if (src.sharing_info && src.varray.is_span()) {
      const bke::AttributeInitShared init(src.varray.get_internal_span().data(),
                                          *src.sharing_info);
      children_attributes.add(id, ATTR_DOMAIN_CURVE, meta_data.data_type, init);
    }
    else {
      children_attributes.add(
          id, ATTR_DOMAIN_CURVE, meta_data.data_type, bke::AttributeInitVArray(src.varray));
    }
    return true;
  });
}

static void store_output_attributes(bke::CurvesGeometry &child_curves,
                                    const AnonymousAttributeIDPtr weight_attribute_id,
                                    const AnonymousAttributeIDPtr index_attribute_id,
                                    const int max_neighbors,
                                    const Span<int> all_neighbor_counts,
                                    const Span<int> all_neighbor_indices,
                                    const Span<float> all_neighbor_weights)
{
  if (!weight_attribute_id && !index_attribute_id) {
    return;
  }
  SpanAttributeWriter<float> weight_attribute;
  if (weight_attribute_id) {
    weight_attribute =
        child_curves.attributes_for_write().lookup_or_add_for_write_only_span<float>(
            *weight_attribute_id, ATTR_DOMAIN_CURVE);
  }
  SpanAttributeWriter<int> index_attribute;
  if (index_attribute_id) {
    index_attribute = child_curves.attributes_for_write().lookup_or_add_for_write_only_span<int>(
        *index_attribute_id, ATTR_DOMAIN_CURVE);
  }
  threading::parallel_for(child_curves.curves_range(), 512, [&](const IndexRange range) {
    for (const int child_curve_i : range) {
      const int neighbor_count = all_neighbor_counts[child_curve_i];

      int closest_index;
      float closest_weight;
      if (neighbor_count == 0) {
        closest_index = 0;
        closest_weight = 0.0f;
      }
      else {
        const IndexRange neighbors_range{child_curve_i * max_neighbors, neighbor_count};
        const Span<float> neighbor_weights = all_neighbor_weights.slice(neighbors_range);
        const Span<int> neighbor_indices = all_neighbor_indices.slice(neighbors_range);
        const int max_index = std::max_element(neighbor_weights.begin(), neighbor_weights.end()) -
                              neighbor_weights.begin();
        closest_index = neighbor_indices[max_index];
        closest_weight = neighbor_weights[max_index];
      }
      if (index_attribute) {
        index_attribute.span[child_curve_i] = closest_index;
      }
      if (weight_attribute) {
        weight_attribute.span[child_curve_i] = closest_weight;
      }
    }
  });
  if (index_attribute) {
    index_attribute.finish();
  }
  if (weight_attribute) {
    weight_attribute.finish();
  }
}

static GeometrySet generate_interpolated_curves(
    const Curves &guide_curves_id,
    const AttributeAccessor &point_attributes,
    const VArray<float3> &guides_up,
    const VArray<float3> &points_up,
    const VArray<int> &guide_group_ids,
    const VArray<int> &point_group_ids,
    const int max_neighbors,
    const AnonymousAttributePropagationInfo &propagation_info,
    const AnonymousAttributeIDPtr &index_attribute_id,
    const AnonymousAttributeIDPtr &weight_attribute_id)
{
  const bke::CurvesGeometry &guide_curves = guide_curves_id.geometry.wrap();

  const MultiValueMap<int, int> guides_by_group = separate_guides_by_group(guide_group_ids);
  const Map<int, int> points_per_curve_by_group = compute_points_per_curve_by_group(
      guides_by_group, guide_curves);

  Map<int, KDTree_3d *> kdtrees = build_kdtrees_for_root_positions(guides_by_group, guide_curves);
  BLI_SCOPED_DEFER([&]() {
    for (KDTree_3d *kdtree : kdtrees.values()) {
      BLI_kdtree_3d_free(kdtree);
    }
  });

  const VArraySpan point_positions = *point_attributes.lookup<float3>("position");
  const int num_child_curves = point_attributes.domain_size(ATTR_DOMAIN_POINT);

  /* The set of guides per child are stored in a flattened array to allow fast access, reduce
   * memory consumption and reduce number of allocations. */
  Array<int> all_neighbor_indices(num_child_curves * max_neighbors);
  Array<float> all_neighbor_weights(num_child_curves * max_neighbors);
  Array<int> all_neighbor_counts(num_child_curves);

  find_neighbor_guides(point_positions,
                       point_group_ids,
                       kdtrees,
                       guides_by_group,
                       max_neighbors,
                       all_neighbor_indices,
                       all_neighbor_weights,
                       all_neighbor_counts);

  Curves *child_curves_id = bke::curves_new_nomain(0, num_child_curves);
  bke::CurvesGeometry &child_curves = child_curves_id->geometry.wrap();
  MutableSpan<int> children_curve_offsets = child_curves.offsets_for_write();

  Array<bool> use_direct_interpolation_per_child(num_child_curves);
  compute_point_counts_per_child(guide_curves,
                                 point_group_ids,
                                 points_per_curve_by_group,
                                 all_neighbor_indices,
                                 all_neighbor_weights,
                                 all_neighbor_counts,
                                 max_neighbors,
                                 children_curve_offsets.drop_back(1),
                                 use_direct_interpolation_per_child);
  offset_indices::accumulate_counts_to_offsets(children_curve_offsets);
  const int num_child_points = children_curve_offsets.last();
  child_curves.resize(num_child_points, num_child_curves);

  /* Stores parameterization of all guide curves in flat arrays. */
  Array<int> parameterized_guide_offsets;
  Array<float> parameterized_guide_lengths;
  parameterize_guide_curves(
      guide_curves, parameterized_guide_offsets, parameterized_guide_lengths);

  interpolate_curve_shapes(child_curves,
                           guide_curves,
                           max_neighbors,
                           all_neighbor_indices,
                           all_neighbor_weights,
                           all_neighbor_counts,
                           guides_up,
                           points_up,
                           point_positions,
                           OffsetIndices<int>(parameterized_guide_offsets),
                           parameterized_guide_lengths,
                           use_direct_interpolation_per_child);
  interpolate_curve_attributes(child_curves,
                               guide_curves,
                               point_attributes,
                               propagation_info,
                               max_neighbors,
                               all_neighbor_indices,
                               all_neighbor_weights,
                               all_neighbor_counts,
                               OffsetIndices<int>(parameterized_guide_offsets),
                               parameterized_guide_lengths,
                               use_direct_interpolation_per_child);

  store_output_attributes(child_curves,
                          weight_attribute_id,
                          index_attribute_id,
                          max_neighbors,
                          all_neighbor_counts,
                          all_neighbor_indices,
                          all_neighbor_weights);

  if (guide_curves_id.mat != nullptr) {
    child_curves_id->mat = static_cast<Material **>(MEM_dupallocN(guide_curves_id.mat));
    child_curves_id->totcol = guide_curves_id.totcol;
  }

  return GeometrySet::from_curves(child_curves_id);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet guide_curves_geometry = params.extract_input<GeometrySet>("Guide Curves");
  const GeometrySet points_geometry = params.extract_input<GeometrySet>("Points");

  if (!guide_curves_geometry.has_curves()) {
    params.set_default_remaining_outputs();
    return;
  }
  const GeometryComponent *points_component = points_geometry.get_component<PointCloudComponent>();
  if (points_component == nullptr) {
    points_component = points_geometry.get_component<MeshComponent>();
  }
  if (points_component == nullptr || points_geometry.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  const int max_neighbors = std::max<int>(1, params.extract_input<int>("Max Neighbors"));

  static auto normalize_fn = mf::build::SI1_SO<float3, float3>(
      "Normalize",
      [](const float3 &v) { return math::normalize(v); },
      mf::build::exec_presets::AllSpanOrSingle());

  /* Normalize up fields so that is done as part of field evaluation. */
  Field<float3> guides_up_field(
      FieldOperation::Create(normalize_fn, {params.extract_input<Field<float3>>("Guide Up")}));
  Field<float3> points_up_field(
      FieldOperation::Create(normalize_fn, {params.extract_input<Field<float3>>("Point Up")}));

  Field<int> guide_group_field = params.extract_input<Field<int>>("Guide Group ID");
  Field<int> point_group_field = params.extract_input<Field<int>>("Point Group ID");

  const Curves &guide_curves_id = *guide_curves_geometry.get_curves();

  const bke::CurvesFieldContext curves_context{guide_curves_id.geometry.wrap(), ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator curves_evaluator{curves_context, guide_curves_id.geometry.curve_num};
  curves_evaluator.add(guides_up_field);
  curves_evaluator.add(guide_group_field);
  curves_evaluator.evaluate();
  const VArray<float3> guides_up = curves_evaluator.get_evaluated<float3>(0);
  const VArray<int> guide_group_ids = curves_evaluator.get_evaluated<int>(1);

  const bke::GeometryFieldContext points_context(*points_component, ATTR_DOMAIN_POINT);
  fn::FieldEvaluator points_evaluator{points_context,
                                      points_component->attribute_domain_size(ATTR_DOMAIN_POINT)};
  points_evaluator.add(points_up_field);
  points_evaluator.add(point_group_field);
  points_evaluator.evaluate();
  const VArray<float3> points_up = points_evaluator.get_evaluated<float3>(0);
  const VArray<int> point_group_ids = points_evaluator.get_evaluated<int>(1);

  const AnonymousAttributePropagationInfo propagation_info = params.get_output_propagation_info(
      "Curves");

  AnonymousAttributeIDPtr index_attribute_id = params.get_output_anonymous_attribute_id_if_needed(
      "Closest Index");
  AnonymousAttributeIDPtr weight_attribute_id = params.get_output_anonymous_attribute_id_if_needed(
      "Closest Weight");

  GeometrySet new_curves = generate_interpolated_curves(guide_curves_id,
                                                        *points_component->attributes(),
                                                        guides_up,
                                                        points_up,
                                                        guide_group_ids,
                                                        point_group_ids,
                                                        max_neighbors,
                                                        propagation_info,
                                                        index_attribute_id,
                                                        weight_attribute_id);

  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(guide_curves_geometry);
  if (const auto *curve_edit_data =
          guide_curves_geometry.get_component<GeometryComponentEditData>()) {
    new_curves.add(*curve_edit_data);
  }

  params.set_output("Curves", std::move(new_curves));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INTERPOLATE_CURVES, "Interpolate Curves", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_interpolate_curves_cc
