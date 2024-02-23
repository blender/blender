/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "node_geometry_util.hh"

#include <set>

namespace blender::nodes::node_geo_edge_paths_to_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Start Vertices").default_value(true).hide_value().supports_field();
  b.add_input<decl::Int>("Next Vertex Index").default_value(-1).hide_value().supports_field();
  b.add_output<decl::Bool>("Selection").field_source_reference_all();
}

static void edge_paths_to_selection(const Mesh &src_mesh,
                                    const IndexMask &start_selection,
                                    const Span<int> next_indices,
                                    MutableSpan<bool> r_edge_selection)
{
  Array<bool> vert_selection(src_mesh.verts_num, false);

  const IndexRange vert_range(src_mesh.verts_num);
  start_selection.foreach_index(GrainSize(2048), [&](const int start_vert) {
    /* If vertex is selected, all next is already selected too. */
    for (int current_vert = start_vert; !vert_selection[current_vert];
         current_vert = next_indices[current_vert])
    {
      if (UNLIKELY(!vert_range.contains(current_vert))) {
        break;
      }
      vert_selection[current_vert] = true;
    }
  });

  const Span<int2> edges = src_mesh.edges();
  threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const int2 edge = edges[i];
      if (!(vert_selection[edge[0]] && vert_selection[edge[1]])) {
        continue;
      }
      if (edge[0] == next_indices[edge[1]] || edge[1] == next_indices[edge[0]]) {
        r_edge_selection[i] = true;
      }
    }
  });
}

class PathToEdgeSelectionFieldInput final : public bke::MeshFieldInput {
 private:
  Field<bool> start_vertices_;
  Field<int> next_vertex_;

 public:
  PathToEdgeSelectionFieldInput(Field<bool> start_verts, Field<int> next_vertex)
      : bke::MeshFieldInput(CPPType::get<bool>(), "Edge Selection"),
        start_vertices_(start_verts),
        next_vertex_(next_vertex)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::MeshFieldContext context{mesh, AttrDomain::Point};
    fn::FieldEvaluator evaluator{context, mesh.verts_num};
    evaluator.add(next_vertex_);
    evaluator.add(start_vertices_);
    evaluator.evaluate();
    const VArraySpan<int> next_vert = evaluator.get_evaluated<int>(0);
    const IndexMask start_verts = evaluator.get_evaluated_as_mask(1);
    if (start_verts.is_empty()) {
      return {};
    }

    Array<bool> selection(mesh.edges_num, false);
    edge_paths_to_selection(mesh, start_verts, next_vert, selection);

    return mesh.attributes().adapt_domain<bool>(
        VArray<bool>::ForContainer(std::move(selection)), AttrDomain::Edge, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    start_vertices_.node().for_each_field_input_recursive(fn);
    next_vertex_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return get_default_hash(start_vertices_, next_vertex_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const PathToEdgeSelectionFieldInput *other_field =
            dynamic_cast<const PathToEdgeSelectionFieldInput *>(&other))
    {
      return other_field->start_vertices_ == start_vertices_ &&
             other_field->next_vertex_ == next_vertex_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> start_verts = params.extract_input<Field<bool>>("Start Vertices");
  Field<int> next_vertex = params.extract_input<Field<int>>("Next Vertex Index");
  Field<bool> selection_field{
      std::make_shared<PathToEdgeSelectionFieldInput>(start_verts, next_vertex)};
  params.set_output("Selection", std::move(selection_field));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_EDGE_PATHS_TO_SELECTION, "Edge Paths to Selection", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  blender::bke::node_type_size(&ntype, 150, 100, 300);
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_edge_paths_to_selection_cc
