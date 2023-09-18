/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_kdtree.h"
#include "BLI_map.hh"
#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_index_of_nearest_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Position").implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Int>("Group ID").supports_field().hide_value();

  b.add_output<decl::Int>("Index").field_source().description("Index of nearest element");
  b.add_output<decl::Bool>("Has Neighbor").field_source();
}

static KDTree_3d *build_kdtree(const Span<float3> positions, const IndexMask &mask)
{
  KDTree_3d *tree = BLI_kdtree_3d_new(mask.size());
  mask.foreach_index(
      [&](const int index) { BLI_kdtree_3d_insert(tree, index, positions[index]); });
  BLI_kdtree_3d_balance(tree);
  return tree;
}

static int find_nearest_non_self(const KDTree_3d &tree, const float3 &position, const int index)
{
  return BLI_kdtree_3d_find_nearest_cb_cpp(
      &tree,
      position,
      nullptr,
      [index](const int other, const float * /*co*/, const float /*dist_sq*/) {
        return index == other ? 0 : 1;
      });
}

static void find_neighbors(const KDTree_3d &tree,
                           const Span<float3> positions,
                           const IndexMask &mask,
                           MutableSpan<int> r_indices)
{
  mask.foreach_index(GrainSize(1024), [&](const int index) {
    r_indices[index] = find_nearest_non_self(tree, positions[index], index);
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

    if (group_ids.is_single()) {
      result.reinitialize(mask.min_array_size());
      KDTree_3d *tree = build_kdtree(positions, IndexRange(domain_size));
      find_neighbors(*tree, positions, mask, result);
      BLI_kdtree_3d_free(tree);
      return VArray<int>::ForContainer(std::move(result));
    }
    const VArraySpan<int> group_ids_span(group_ids);

    VectorSet<int> group_indexing;
    for (const int index : IndexRange(domain_size)) {
      const int group_id = group_ids_span[index];
      group_indexing.add(group_id);
    }
    const int groups_num = group_indexing.size();

    IndexMaskMemory mask_memory;
    Array<IndexMask> all_indices_by_group_id(groups_num);
    Array<IndexMask> lookup_indices_by_group_id(groups_num);

    const auto get_group_index = [&](const int i) {
      const int group_id = group_ids_span[i];
      return group_indexing.index_of(group_id);
    };

    IndexMask::from_groups<int>(
        IndexMask(domain_size), mask_memory, get_group_index, all_indices_by_group_id);

    if (mask.size() == domain_size) {
      lookup_indices_by_group_id = all_indices_by_group_id;
      result.reinitialize(domain_size);
    }
    else {
      IndexMask::from_groups<int>(mask, mask_memory, get_group_index, lookup_indices_by_group_id);
      result.reinitialize(mask.min_array_size());
    }

    /* The grain size should be larger as each tree gets smaller. */
    const int avg_tree_size = domain_size / group_indexing.size();
    const int grain_size = std::max(8192 / avg_tree_size, 1);
    threading::parallel_for(IndexRange(groups_num), grain_size, [&](const IndexRange range) {
      for (const int group_index : range) {
        const IndexMask &tree_mask = all_indices_by_group_id[group_index];
        const IndexMask &lookup_mask = lookup_indices_by_group_id[group_index];
        KDTree_3d *tree = build_kdtree(positions, tree_mask);
        find_neighbors(*tree, positions, lookup_mask, result);
        BLI_kdtree_3d_free(tree);
      }
    });

    return VArray<int>::ForContainer(std::move(result));
  }

 public:
  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const
  {
    positions_field_.node().for_each_field_input_recursive(fn);
    group_field_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const final
  {
    return get_default_hash_2(positions_field_, group_field_);
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (const auto *other_field = dynamic_cast<const IndexOfNearestFieldInput *>(&other)) {
      return positions_field_ == other_field->positions_field_ &&
             group_field_ == other_field->group_field_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const GeometryComponent &component) const final
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
      return VArray<bool>::ForSingle(false, mask.min_array_size());
    }

    fn::FieldEvaluator evaluator{context, domain_size};
    evaluator.add(group_field_);
    evaluator.evaluate();
    const VArray<int> group = evaluator.get_evaluated<int>(0);

    if (group.is_single()) {
      return VArray<bool>::ForSingle(true, mask.min_array_size());
    }

    Map<int, int> counts;
    const VArraySpan<int> group_span(group);
    mask.foreach_index([&](const int i) {
      counts.add_or_modify(
          group_span[i], [](int *count) { *count = 0; }, [](int *count) { (*count)++; });
    });
    Array<bool> result(mask.min_array_size());
    mask.foreach_index([&](const int i) { result[i] = counts.lookup(group_span[i]) > 1; });
    return VArray<bool>::ForContainer(std::move(result));
  }

 public:
  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const
  {
    group_field_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const final
  {
    return get_default_hash_2(39847876, group_field_);
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (const auto *other_field = dynamic_cast<const HasNeighborFieldInput *>(&other)) {
      return group_field_ == other_field->group_field_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const GeometryComponent &component) const final
  {
    return bke::try_detect_field_domain(component, group_field_);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");
  Field<int> group_field = params.extract_input<Field<int>>("Group ID");

  if (params.output_is_required("Index")) {
    params.set_output("Index",
                      Field<int>(std::make_shared<IndexOfNearestFieldInput>(
                          std::move(position_field), group_field)));
  }

  if (params.output_is_required("Has Neighbor")) {
    params.set_output(
        "Has Neighbor",
        Field<bool>(std::make_shared<HasNeighborFieldInput>(std::move(group_field))));
  }
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INDEX_OF_NEAREST, "Index of Nearest", NODE_CLASS_CONVERTER);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_index_of_nearest_cc
