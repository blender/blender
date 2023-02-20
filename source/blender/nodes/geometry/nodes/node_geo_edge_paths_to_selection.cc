/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "node_geometry_util.hh"

#include <set>

namespace blender::nodes::node_geo_edge_paths_to_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>(N_("Start Vertices")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Int>(N_("Next Vertex Index")).default_value(-1).hide_value().supports_field();
  b.add_output<decl::Bool>(N_("Selection")).field_source_reference_all();
}

static void edge_paths_to_selection(const Mesh &src_mesh,
                                    const IndexMask start_selection,
                                    const Span<int> next_indices,
                                    MutableSpan<bool> r_selection)
{
  const Span<MEdge> edges = src_mesh.edges();

  Array<bool> selection(src_mesh.totvert, false);

  for (const int start_vert : start_selection) {
    selection[start_vert] = true;
  }

  for (const int start_i : start_selection) {
    int iter = start_i;
    while (iter != next_indices[iter] && !selection[next_indices[iter]]) {
      if (next_indices[iter] < 0 || next_indices[iter] >= src_mesh.totvert) {
        break;
      }
      selection[next_indices[iter]] = true;
      iter = next_indices[iter];
    }
  }

  for (const int i : edges.index_range()) {
    const MEdge &edge = edges[i];
    if ((selection[edge.v1] && selection[edge.v2]) &&
        (edge.v1 == next_indices[edge.v2] || edge.v2 == next_indices[edge.v1])) {
      r_selection[i] = true;
    }
  }
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
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    bke::MeshFieldContext context{mesh, ATTR_DOMAIN_POINT};
    fn::FieldEvaluator evaluator{context, mesh.totvert};
    evaluator.add(next_vertex_);
    evaluator.add(start_vertices_);
    evaluator.evaluate();
    const VArraySpan<int> next_vert = evaluator.get_evaluated<int>(0);
    const IndexMask start_verts = evaluator.get_evaluated_as_mask(1);

    if (start_verts.is_empty()) {
      return {};
    }

    Array<bool> selection(mesh.totedge, false);
    MutableSpan<bool> selection_span = selection.as_mutable_span();

    edge_paths_to_selection(mesh, start_verts, next_vert, selection_span);

    return mesh.attributes().adapt_domain<bool>(
        VArray<bool>::ForContainer(std::move(selection)), ATTR_DOMAIN_EDGE, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    start_vertices_.node().for_each_field_input_recursive(fn);
    next_vertex_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return get_default_hash_2(start_vertices_, next_vertex_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const PathToEdgeSelectionFieldInput *other_field =
            dynamic_cast<const PathToEdgeSelectionFieldInput *>(&other)) {
      return other_field->start_vertices_ == start_vertices_ &&
             other_field->next_vertex_ == next_vertex_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_EDGE;
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

}  // namespace blender::nodes::node_geo_edge_paths_to_selection_cc

void register_node_type_geo_edge_paths_to_selection()
{
  namespace file_ns = blender::nodes::node_geo_edge_paths_to_selection_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_EDGE_PATHS_TO_SELECTION, "Edge Paths to Selection", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_size(&ntype, 150, 100, 300);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
