/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_index_of_nearest_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Position"_ustr)
      .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD)
      .structure_type(StructureType::Field);
  b.add_input<decl::Int>("Group ID"_ustr).structure_type(StructureType::Field).hide_value();

  b.add_output<decl::Int>("Index"_ustr)
      .structure_type(StructureType::Field)
      .propagate_references()
      .description("Index of nearest element");
  b.add_output<decl::Bool>("Has Neighbor"_ustr)
      .structure_type(StructureType::Field)
      .propagate_references();
}

static int find_nearest_non_self(const KDTreeNew<float3> &tree,
                                 const float3 &position,
                                 const int index)
{
  return tree.find_nearest_filtered(position, [index](const int other) { return other != index; });
}

static void find_neighbors(const KDTreeNew<float3> &tree,
                           const Span<float3> positions,
                           const IndexMask &mask,
                           MutableSpan<int> r_indices)
{
  mask.foreach_index(
      [&](const int index) {
        r_indices[index] = find_nearest_non_self(tree, positions[index], index);
      },
      exec_mode::grain_size(1024));
}

static void find_neighbors(const KDTreeNew<float3> &tree,
                           const Span<float3> positions,
                           const Span<int> indices,
                           MutableSpan<int> r_indices)
{
  threading::parallel_for(indices.index_range(), 1024, [&](const IndexRange range) {
    for (const int index : indices.slice(range)) {
      r_indices[index] = find_nearest_non_self(tree, positions[index], index);
    }
  });
}

class IndexOfNearestFieldInput final : public bke::GeometryFieldInput {
 private:
  const Field<float3> positions_field_;
  const Field<int> group_field_;

 public:
  IndexOfNearestFieldInput(Field<float3> positions_field, Field<int> group_field)
      : bke::GeometryFieldInput(CPPType::get<int>(), "Index of Nearest"),
        positions_field_(std::move(positions_field)),
        group_field_(std::move(group_field))
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
    evaluator.evaluate();
    const VArraySpan<float3> positions = evaluator.get_evaluated<float3>(0);
    const VArray<int> group_ids = evaluator.get_evaluated<int>(1);

    Array<int> result;

    /* With a full mask, we can run query's in the tree's internal index order, which can be much
     * faster because neighboring indices will be spatially contiguous and therefore make better
     * use of caches during traversal. Even with scattered writes from multiple threads, lookup
     * cost dominates and this is a significant performance improvement. */
    const bool query_all = mask.size() == domain_size;

    if (group_ids.is_single()) {
      result.reinitialize(mask.min_array_size());
      const KDTreeNew<float3> tree(positions);
      if (query_all) {
        find_neighbors(tree, positions, tree.tree_indices(), result);
      }
      else {
        find_neighbors(tree, positions, mask, result);
      }
      return VArray<int>::from_container(std::move(result));
    }
    const VArraySpan<int> group_ids_span(group_ids);

    Array<int> group_indices(domain_size);
    const int groups_num = array_utils::group_ids_to_indices(
        group_ids_span, IndexMask(domain_size), group_indices);

    Array<int> tree_offset_data;
    Array<int> tree_index_data;
    const GroupedSpan<int> tree_indices_by_group = offset_indices::build_groups_from_indices(
        group_indices, groups_num, tree_offset_data, tree_index_data);

    /* When only some elements are looked up, they are grouped separately. */
    GroupedSpan<int> lookup_indices_by_group;
    Array<int> lookup_offset_data;
    Array<int> lookup_index_data;
    if (query_all) {
      result.reinitialize(domain_size);
    }
    else {
      Array<int> lookup_group_indices(mask.size());
      array_utils::gather(group_indices.as_span(), mask, lookup_group_indices.as_mutable_span());
      lookup_indices_by_group = offset_indices::build_groups_from_indices(
          lookup_group_indices, groups_num, lookup_offset_data, lookup_index_data, mask);
      result.reinitialize(mask.min_array_size());
    }

    /* The grain size should be larger as each tree gets smaller. */
    const int avg_tree_size = domain_size / groups_num;
    const int grain_size = std::max(8192 / avg_tree_size, 1);
    threading::parallel_for(IndexRange(groups_num), grain_size, [&](const IndexRange range) {
      AlignedBuffer<4096, 8> tree_buffer;
      for (const int group_index : range) {
        LinearAllocator<> tree_memory;
        tree_memory.provide_buffer(tree_buffer);
        const KDTreeNew<float3> tree(positions, tree_indices_by_group[group_index], tree_memory);
        find_neighbors(tree,
                       positions,
                       query_all ? tree.tree_indices() : lookup_indices_by_group[group_index],
                       result);
      }
    });

    return VArray<int>::from_container(std::move(result));
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(positions_field_);
    fn(group_field_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(positions_field_));
    hash.add(deep_hash_cache.ensure(group_field_));
  }

  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const final
  {
    return bke::try_detect_field_domain(component, positions_field_);
  }
};

class HasNeighborFieldInput final : public bke::GeometryFieldInput {
 private:
  const Field<int> group_field_;

 public:
  HasNeighborFieldInput(Field<int> group_field)
      : bke::GeometryFieldInput(CPPType::get<bool>(), "Has Neighbor"),
        group_field_(std::move(group_field))
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    if (!context.attributes()) {
      return {};
    }
    const int domain_size = context.attributes()->domain_size(context.domain());
    if (domain_size == 1) {
      return VArray<bool>::from_single(false, mask.min_array_size());
    }

    fn::FieldEvaluator evaluator{context, domain_size};
    evaluator.add(group_field_);
    evaluator.evaluate();
    const VArray<int> group = evaluator.get_evaluated<int>(0);

    if (group.is_single()) {
      return VArray<bool>::from_single(true, mask.min_array_size());
    }

    Map<int, int> counts;
    const VArraySpan<int> group_span(group);
    mask.foreach_index([&](const int i) {
      counts.add_or_modify(
          group_span[i], [](int *count) { *count = 1; }, [](int *count) { (*count)++; });
    });
    Array<bool> result(mask.min_array_size());
    mask.foreach_index([&](const int i) { result[i] = counts.lookup(group_span[i]) > 1; });
    return VArray<bool>::from_container(std::move(result));
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(group_field_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const final
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(group_field_));
  }

  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const final
  {
    return bke::try_detect_field_domain(component, group_field_);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> position_field = params.extract_input<Field<float3>>("Position"_ustr);
  Field<int> group_field = params.extract_input<Field<int>>("Group ID"_ustr);

  if (params.output_is_required("Index"_ustr)) {
    params.set_output(
        "Index"_ustr,
        Field<int>::from_input<IndexOfNearestFieldInput>(std::move(position_field), group_field));
  }

  if (params.output_is_required("Has Neighbor"_ustr)) {
    params.set_output("Has Neighbor"_ustr,
                      Field<bool>::from_input<HasNeighborFieldInput>(std::move(group_field)));
  }
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeIndexOfNearest"_ustr, GEO_NODE_INDEX_OF_NEAREST);
  ntype.ui_name = "Index of Nearest";
  ntype.ui_description =
      "Find the nearest element in a group. Similar to the \"Sample Nearest\" node";
  ntype.enum_name_legacy = "INDEX_OF_NEAREST";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_index_of_nearest_cc
