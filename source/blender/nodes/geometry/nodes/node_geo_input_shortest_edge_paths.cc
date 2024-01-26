/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <queue>

#include "BLI_array_utils.hh"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_shortest_edge_paths_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("End Vertex").default_value(false).hide_value().supports_field();
  b.add_input<decl::Float>("Edge Cost").default_value(1.0f).hide_value().supports_field();
  b.add_output<decl::Int>("Next Vertex Index").field_source().reference_pass_all();
  b.add_output<decl::Float>("Total Cost").field_source().reference_pass_all();
}

using VertPriority = std::pair<float, int>;

static void shortest_paths(const Mesh &mesh,
                           const GroupedSpan<int> vert_to_edge,
                           const IndexMask end_selection,
                           const VArray<float> &input_cost,
                           MutableSpan<int> r_next_index,
                           MutableSpan<float> r_cost)
{
  const Span<int2> edges = mesh.edges();
  Array<bool> visited(mesh.verts_num, false);

  std::priority_queue<VertPriority, std::vector<VertPriority>, std::greater<VertPriority>> queue;

  end_selection.foreach_index([&](const int start_vert_i) {
    r_cost[start_vert_i] = 0.0f;
    queue.emplace(0.0f, start_vert_i);
  });

  /* Though it uses more memory, calculating the adjacent vertex
   * across each edge beforehand is noticeably faster. */
  Array<int> other_vertex(vert_to_edge.data.size());
  threading::parallel_for(vert_to_edge.index_range(), 2048, [&](const IndexRange range) {
    for (const int vert_i : range) {
      for (const int edge_i : vert_to_edge.offsets[vert_i]) {
        other_vertex[edge_i] = bke::mesh::edge_other_vert(edges[vert_to_edge.data[edge_i]],
                                                          vert_i);
      }
    }
  });

  while (!queue.empty()) {
    const float cost_i = queue.top().first;
    const int vert_i = queue.top().second;
    queue.pop();
    if (visited[vert_i]) {
      continue;
    }
    visited[vert_i] = true;
    for (const int index : vert_to_edge.offsets[vert_i]) {
      const int edge_i = vert_to_edge.data[index];
      const int neighbor_vert_i = other_vertex[index];
      if (visited[neighbor_vert_i]) {
        continue;
      }
      const float edge_cost = std::max(0.0f, input_cost[edge_i]);
      const float new_neighbor_cost = cost_i + edge_cost;
      if (new_neighbor_cost < r_cost[neighbor_vert_i]) {
        r_cost[neighbor_vert_i] = new_neighbor_cost;
        r_next_index[neighbor_vert_i] = vert_i;
        queue.emplace(new_neighbor_cost, neighbor_vert_i);
      }
    }
  }
}

class ShortestEdgePathsNextVertFieldInput final : public bke::MeshFieldInput {
 private:
  Field<bool> end_selection_;
  Field<float> cost_;

 public:
  ShortestEdgePathsNextVertFieldInput(Field<bool> end_selection, Field<float> cost)
      : bke::MeshFieldInput(CPPType::get<int>(), "Shortest Edge Paths Next Vertex Field"),
        end_selection_(end_selection),
        cost_(cost)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::MeshFieldContext edge_context{mesh, AttrDomain::Edge};
    fn::FieldEvaluator edge_evaluator{edge_context, mesh.edges_num};
    edge_evaluator.add(cost_);
    edge_evaluator.evaluate();
    const VArray<float> input_cost = edge_evaluator.get_evaluated<float>(0);

    const bke::MeshFieldContext point_context{mesh, AttrDomain::Point};
    fn::FieldEvaluator point_evaluator{point_context, mesh.verts_num};
    point_evaluator.add(end_selection_);
    point_evaluator.evaluate();
    const IndexMask end_selection = point_evaluator.get_evaluated_as_mask(0);

    Array<int> next_index(mesh.verts_num, -1);
    Array<float> cost(mesh.verts_num, FLT_MAX);

    if (end_selection.is_empty()) {
      array_utils::fill_index_range<int>(next_index);
      return mesh.attributes().adapt_domain<int>(
          VArray<int>::ForContainer(std::move(next_index)), AttrDomain::Point, domain);
    }

    const Span<int2> edges = mesh.edges();
    Array<int> vert_to_edge_offset_data;
    Array<int> vert_to_edge_indices;
    const GroupedSpan<int> vert_to_edge = bke::mesh::build_vert_to_edge_map(
        edges, mesh.verts_num, vert_to_edge_offset_data, vert_to_edge_indices);
    shortest_paths(mesh, vert_to_edge, end_selection, input_cost, next_index, cost);

    threading::parallel_for(next_index.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        if (next_index[i] == -1) {
          next_index[i] = i;
        }
      }
    });
    return mesh.attributes().adapt_domain<int>(
        VArray<int>::ForContainer(std::move(next_index)), AttrDomain::Point, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    end_selection_.node().for_each_field_input_recursive(fn);
    cost_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return get_default_hash(end_selection_, cost_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const ShortestEdgePathsNextVertFieldInput *other_field =
            dynamic_cast<const ShortestEdgePathsNextVertFieldInput *>(&other))
    {
      return other_field->end_selection_ == end_selection_ && other_field->cost_ == cost_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Point;
  }
};

class ShortestEdgePathsCostFieldInput final : public bke::MeshFieldInput {
 private:
  Field<bool> end_selection_;
  Field<float> cost_;

 public:
  ShortestEdgePathsCostFieldInput(Field<bool> end_selection, Field<float> cost)
      : bke::MeshFieldInput(CPPType::get<float>(), "Shortest Edge Paths Cost Field"),
        end_selection_(end_selection),
        cost_(cost)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::MeshFieldContext edge_context{mesh, AttrDomain::Edge};
    fn::FieldEvaluator edge_evaluator{edge_context, mesh.edges_num};
    edge_evaluator.add(cost_);
    edge_evaluator.evaluate();
    const VArray<float> input_cost = edge_evaluator.get_evaluated<float>(0);

    const bke::MeshFieldContext point_context{mesh, AttrDomain::Point};
    fn::FieldEvaluator point_evaluator{point_context, mesh.verts_num};
    point_evaluator.add(end_selection_);
    point_evaluator.evaluate();
    const IndexMask end_selection = point_evaluator.get_evaluated_as_mask(0);

    if (end_selection.is_empty()) {
      return mesh.attributes().adapt_domain<float>(
          VArray<float>::ForSingle(0.0f, mesh.verts_num), AttrDomain::Point, domain);
    }

    Array<int> next_index(mesh.verts_num, -1);
    Array<float> cost(mesh.verts_num, FLT_MAX);

    const Span<int2> edges = mesh.edges();
    Array<int> vert_to_edge_offset_data;
    Array<int> vert_to_edge_indices;
    const GroupedSpan<int> vert_to_edge = bke::mesh::build_vert_to_edge_map(
        edges, mesh.verts_num, vert_to_edge_offset_data, vert_to_edge_indices);
    shortest_paths(mesh, vert_to_edge, end_selection, input_cost, next_index, cost);

    threading::parallel_for(cost.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        if (cost[i] == FLT_MAX) {
          cost[i] = 0;
        }
      }
    });
    return mesh.attributes().adapt_domain<float>(
        VArray<float>::ForContainer(std::move(cost)), AttrDomain::Point, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    end_selection_.node().for_each_field_input_recursive(fn);
    cost_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return get_default_hash(end_selection_, cost_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const ShortestEdgePathsCostFieldInput *other_field =
            dynamic_cast<const ShortestEdgePathsCostFieldInput *>(&other))
    {
      return other_field->end_selection_ == end_selection_ && other_field->cost_ == cost_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Point;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> end_selection = params.extract_input<Field<bool>>("End Vertex");
  Field<float> cost = params.extract_input<Field<float>>("Edge Cost");

  Field<int> next_vert_field{
      std::make_shared<ShortestEdgePathsNextVertFieldInput>(end_selection, cost)};
  Field<float> cost_field{std::make_shared<ShortestEdgePathsCostFieldInput>(end_selection, cost)};
  params.set_output("Next Vertex Index", std::move(next_vert_field));
  params.set_output("Total Cost", std::move(cost_field));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_SHORTEST_EDGE_PATHS, "Shortest Edge Paths", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_shortest_edge_paths_cc
