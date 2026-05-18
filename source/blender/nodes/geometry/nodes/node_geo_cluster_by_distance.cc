/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree.hh"

#include "BKE_geometry_fields.hh"

#include "atomic_ops.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_cluster_by_distance_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Selection"_ustr).default_value(true).supports_field().hide_value();
  b.add_input<decl::Int>("Group ID"_ustr).supports_field().hide_value();
  b.add_input<decl::Vector>("Position"_ustr)
      .implicit_field_on_all(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Float>("Distance"_ustr).default_value(0.001f).min(0.0f).subtype(PROP_DISTANCE);

  b.add_output<decl::Int>("Cluster ID"_ustr).field_source_reference_all();
}

constexpr int NO_CLUSTER_VALUE = -1;

static void set_no_cluster_value(MutableSpan<int> r_cluster_ids, const IndexMask &mask)
{
  mask.foreach_index_optimized<int>(
      [&](const int i) {
        if (r_cluster_ids[i] == NO_CLUSTER_VALUE) {
          r_cluster_ids[i] = i;
        }
      },
      exec_mode::grain_size(4096));
}

class ClusterByDistanceFieldInput final : public bke::GeometryFieldInput {
 private:
  Field<float3> positions_field_;
  Field<int> group_field_;
  Field<bool> selection_field_;
  float distance_;

 public:
  ClusterByDistanceFieldInput(Field<float3> positions_field,
                              Field<int> group_field,
                              Field<bool> selection_field,
                              const float distance)
      : bke::GeometryFieldInput(CPPType::get<int>(), "Cluster by Distance"),
        positions_field_(std::move(positions_field)),
        group_field_(std::move(group_field)),
        selection_field_(std::move(selection_field)),
        distance_(distance)
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    if (!context.attributes()) {
      return {};
    }

    const int domain_size = context.attributes()->domain_size(context.domain());
    fn::FieldEvaluator evaluator{context, domain_size};
    evaluator.add(positions_field_);
    evaluator.add(group_field_);
    evaluator.set_selection(selection_field_);
    evaluator.evaluate();
    const VArraySpan<float3> positions = evaluator.get_evaluated<float3>(0);
    const VArray<int> group_ids = evaluator.get_evaluated<int>(1);
    const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

    IndexMaskMemory memory;
    /* With context info about mask to compute we can skip processing of the rest of the values.
     * But in current case this will affect result values since some cluster might have lowest ID
     * of element outside of visible mask. */
    const IndexMask mask_to_cluster = IndexMask::from_intersection(mask, selection, memory);
    if (mask_to_cluster.is_empty()) {
      return fn::IndexFieldInput::get_index_varray(mask);
    }

    Array<int> cluster_ids(mask.min_array_size());

    const IndexMask mask_to_fallback = IndexMask::from_difference(mask, mask_to_cluster, memory);
    array_utils::fill_index_range<int>(mask_to_fallback, cluster_ids);

    std::optional<VArraySpan<int>> group_id_span;
    const auto group_indices = [&]() -> VectorSet<int> {
      if (group_ids.is_single()) {
        return {group_ids.get_internal_single()};
      }
      VectorSet<int> group_indices;
      group_id_span.emplace(group_ids);
      mask_to_cluster.foreach_index_optimized<int>(
          [&](const int i) { group_indices.add((*group_id_span)[i]); });
      return group_indices;
    }();

    const int groups_num = group_indices.size();
    if (groups_num == 1) {
      KDTree<float3> *tree = kdtree_new<float3>(mask_to_cluster.size());
      mask_to_cluster.foreach_index(
          [&](const int i) { kdtree_insert<float3>(tree, i, positions[i]); });
      kdtree_balance<float3>(tree);
      index_mask::masked_fill<int>(cluster_ids, NO_CLUSTER_VALUE, mask_to_cluster);
      kdtree_calc_duplicates_fast<float3>(tree, distance_, true, cluster_ids.data());
      kdtree_free<float3>(tree);
      set_no_cluster_value(cluster_ids, mask_to_cluster);
      return VArray<int>::from_container(std::move(cluster_ids));
    }

    Array<int> group_offset_data(groups_num + 1, 0);
    mask_to_cluster.foreach_index_optimized<int>(
        [&](const int i) {
          const int group_i = group_indices.index_of((*group_id_span)[i]);
          atomic_add_and_fetch_int32(&group_offset_data[group_i], 1);
        },
        exec_mode::grain_size(8192));
    const OffsetIndices<int> group_offsets = offset_indices::accumulate_counts_to_offsets(
        group_offset_data);

    Array<int> indices_by_group(group_offsets.total_size());
    Array<int> group_counts(groups_num, 0);
    mask_to_cluster.foreach_index_optimized<int>(
        [&](const int i) {
          const int group_i = group_indices.index_of((*group_id_span)[i]);
          const int index_in_group = atomic_fetch_and_add_int32(&group_counts[group_i], 1);
          indices_by_group[group_offsets[group_i][index_in_group]] = int(i);
        },
        exec_mode::grain_size(8192));
    offset_indices::sort_groups(group_offsets, indices_by_group);

    threading::parallel_for(
        IndexRange(groups_num),
        1024,
        [&](const IndexRange range) {
          Vector<int, 64> group_cluster_ids;
          for (const int group_i : range) {
            const Span group = indices_by_group.as_span().slice(group_offsets[group_i]);
            group_cluster_ids.resize(group.size());
            group_cluster_ids.fill(NO_CLUSTER_VALUE);
            KDTree<float3> *tree = kdtree_new<float3>(group.size());
            for (const int pos : group.index_range()) {
              kdtree_insert<float3>(tree, pos, positions[group[pos]]);
            }
            kdtree_balance<float3>(tree);
            kdtree_calc_duplicates_fast<float3>(tree, distance_, true, group_cluster_ids.data());
            kdtree_free<float3>(tree);
            set_no_cluster_value(group_cluster_ids, group_cluster_ids.index_range());
            threading::parallel_for(group.index_range(), 4096, [&](const IndexRange range) {
              for (const int pos : range) {
                const int i = group[pos];
                cluster_ids[i] = group[group_cluster_ids[pos]];
              }
            });
          }
        },
        threading::accumulated_task_sizes(
            [&](const IndexRange range) { return group_offsets[range].size(); }));

#ifndef NDEBUG
    mask.foreach_index([&](const int i) { BLI_assert(cluster_ids[i] != NO_CLUSTER_VALUE); });
#endif

    return VArray<int>::from_container(std::move(cluster_ids));
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(positions_field_);
    fn(group_field_);
    fn(selection_field_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(positions_field_));
    hash.add(deep_hash_cache.ensure(group_field_));
    hash.add(deep_hash_cache.ensure(selection_field_));
    hash.add(distance_);
  }

  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const final
  {
    return bke::try_detect_field_domain(component, positions_field_);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Cluster ID"_ustr,
                    Field<int>::from_input<ClusterByDistanceFieldInput>(
                        params.extract_input<Field<float3>>("Position"_ustr),
                        params.extract_input<Field<int>>("Group ID"_ustr),
                        params.extract_input<Field<bool>>("Selection"_ustr),
                        params.extract_input<float>("Distance"_ustr)));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeClusterByDistance"_ustr);
  ntype.ui_name = "Cluster by Distance";
  ntype.ui_description = "Group elements into integer IDs based on proximity of vector values";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_cluster_by_distance_cc
