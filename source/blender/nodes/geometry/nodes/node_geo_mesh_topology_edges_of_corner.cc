/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_edges_of_corner_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Corner Index"_ustr)
      .implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("The corner to retrieve data from. Defaults to the corner from the context")
      .structure_type(StructureType::Field);
  b.add_output<decl::Int>("Next Edge Index"_ustr)
      .field_source_reference_all()
      .description(
          "The edge after the corner in the face, in the direction of increasing indices");
  b.add_output<decl::Int>("Previous Edge Index"_ustr)
      .field_source_reference_all()
      .description(
          "The edge before the corner in the face, in the direction of decreasing indices");
}

class CornerNextEdgeFieldInput final : public bke::MeshFieldInput {
 public:
  CornerNextEdgeFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Next Edge") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Corner) {
      return {};
    }
    return VArray<int>::from_span(mesh.corner_edges());
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Corner;
  }
};

class CornerPreviousEdgeFieldInput final : public bke::MeshFieldInput {
 public:
  CornerPreviousEdgeFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Previous Edge")
  {
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Corner) {
      return {};
    }
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_edges = mesh.corner_edges();
    const Span<int> corner_to_face = mesh.corner_to_face_map();
    return VArray<int>::from_func(
        corner_edges.size(), [faces, corner_edges, corner_to_face](const int corner) {
          return corner_edges[bke::mesh::face_corner_prev(faces[corner_to_face[corner]], corner)];
        });
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Corner;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> corner_index = params.extract_input<Field<int>>("Corner Index"_ustr);
  if (params.output_is_required("Next Edge Index"_ustr)) {
    params.set_output(
        "Next Edge Index"_ustr,
        Field<int>::from_input<bke::EvaluateAtIndexInput>(
            corner_index, Field<int>::from_input<CornerNextEdgeFieldInput>(), AttrDomain::Corner));
  }
  if (params.output_is_required("Previous Edge Index"_ustr)) {
    params.set_output("Previous Edge Index"_ustr,
                      Field<int>::from_input<bke::EvaluateAtIndexInput>(
                          corner_index,
                          Field<int>::from_input<CornerPreviousEdgeFieldInput>(),
                          AttrDomain::Corner));
  }
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeEdgesOfCorner"_ustr, GEO_NODE_MESH_TOPOLOGY_EDGES_OF_CORNER);
  ntype.ui_name = "Edges of Corner";
  ntype.ui_description = "Retrieve the edges on both sides of a face corner";
  ntype.enum_name_legacy = "EDGES_OF_CORNER";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_edges_of_corner_cc
