/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_mesh_mapping.hh"

#include "BLI_atomic_disjoint_set.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_edges_to_face_groups_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Boundary Edges")
      .default_value(true)
      .hide_value()
      .supports_field()
      .description("Edges used to split faces into separate groups");
  b.add_output<decl::Int>("Face Group ID")
      .field_source_reference_all()
      .description("Index of the face group inside each boundary edge region");
}

/** Join all unique unordered combinations of indices. */
static void join_indices(AtomicDisjointSet &set, const Span<int> indices)
{
  for (const int i : indices.index_range().drop_back(1)) {
    set.join(indices[i], indices[i + 1]);
  }
}

class FaceSetFromBoundariesInput final : public bke::MeshFieldInput {
 private:
  Field<bool> non_boundary_edge_field_;

 public:
  FaceSetFromBoundariesInput(Field<bool> selection)
      : bke::MeshFieldInput(CPPType::get<int>(), "Edges to Face Groups"),
        non_boundary_edge_field_(std::move(selection))
  {
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::MeshFieldContext context{mesh, AttrDomain::Edge};
    fn::FieldEvaluator evaluator{context, mesh.edges_num};
    evaluator.add(non_boundary_edge_field_);
    evaluator.evaluate();
    const IndexMask non_boundary_edges = evaluator.get_evaluated_as_mask(0);

    const OffsetIndices faces = mesh.faces();

    Array<int> edge_to_face_offsets;
    Array<int> edge_to_face_indices;
    const GroupedSpan<int> edge_to_face_map = bke::mesh::build_edge_to_face_map(
        faces, mesh.corner_edges(), mesh.edges_num, edge_to_face_offsets, edge_to_face_indices);

    AtomicDisjointSet islands(faces.size());
    non_boundary_edges.foreach_index(
        GrainSize(2048), [&](const int edge) { join_indices(islands, edge_to_face_map[edge]); });

    Array<int> output(faces.size());
    islands.calc_reduced_ids(output);

    return mesh.attributes().adapt_domain(
        VArray<int>::from_container(std::move(output)), AttrDomain::Face, domain);
  }

  uint64_t hash() const override
  {
    return non_boundary_edge_field_.hash();
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const auto *other_field = dynamic_cast<const FaceSetFromBoundariesInput *>(&other)) {
      return other_field->non_boundary_edge_field_ == non_boundary_edge_field_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Face;
  }
};

static void geo_node_exec(GeoNodeExecParams params)
{
  Field<bool> boundary_edges = params.extract_input<Field<bool>>("Boundary Edges");
  Field<bool> non_boundary_edges = fn::invert_boolean_field(std::move(boundary_edges));
  params.set_output(
      "Face Group ID",
      Field<int>(std::make_shared<FaceSetFromBoundariesInput>(std::move(non_boundary_edges))));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeEdgesToFaceGroups", GEO_NODE_EDGES_TO_FACE_GROUPS);
  ntype.ui_name = "Edges to Face Groups";
  ntype.ui_description = "Group faces into regions surrounded by the selected boundary edges";
  ntype.enum_name_legacy = "EDGES_TO_FACE_GROUPS";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = geo_node_exec;
  ntype.declare = node_declare;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_edges_to_face_groups_cc
