/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_edge_paths_to_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Start Vertices"_ustr).default_value(true).hide_value().supports_field();
  b.add_input<decl::Int>("Next Vertex Index"_ustr).default_value(-1).hide_value().supports_field();
  b.add_output<decl::Bool>("Selection"_ustr).field_source_reference_all();
}

static void edge_paths_to_selection(const Mesh &src_mesh,
                                    const IndexMask &start_selection,
                                    const Span<int> next_indices,
                                    MutableSpan<bool> r_edge_selection)
{
  Array<bool> vert_selection(src_mesh.verts_num, false);

  const IndexRange vert_range(src_mesh.verts_num);
  start_selection.foreach_index(
      [&](const int start_vert) {
        /* If vertex is selected, all next is already selected too. */
        for (int current_vert = start_vert; !vert_selection[current_vert];
             current_vert = next_indices[current_vert])
        {
          if (UNLIKELY(!vert_range.contains(current_vert))) {
            break;
          }
          vert_selection[current_vert] = true;
        }
      },
      exec_mode::grain_size(2048));

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
        VArray<bool>::from_container(std::move(selection)), AttrDomain::Edge, domain);
  }

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const override
  {
    fn(start_vertices_);
    fn(next_vertex_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(start_vertices_));
    hash.add(deep_hash_cache.ensure(next_vertex_));
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<bool> start_verts = params.extract_input<Field<bool>>("Start Vertices"_ustr);
  Field<int> next_vertex = params.extract_input<Field<int>>("Next Vertex Index"_ustr);
  params.set_output(
      "Selection"_ustr,
      Field<bool>::from_input<PathToEdgeSelectionFieldInput>(start_verts, next_vertex));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, "GeometryNodeEdgePathsToSelection"_ustr, GEO_NODE_EDGE_PATHS_TO_SELECTION);
  ntype.ui_name = "Edge Paths to Selection";
  ntype.ui_description = "Output a selection of edges by following paths across mesh edges";
  ntype.enum_name_legacy = "EDGE_PATHS_TO_SELECTION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 150, 100, 300);
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_edge_paths_to_selection_cc
