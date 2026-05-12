/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_vertex_of_corner_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Corner Index"_ustr)
      .implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("The corner to retrieve data from. Defaults to the corner from the context")
      .structure_type(StructureType::Field);
  b.add_output<decl::Int>("Vertex Index"_ustr)
      .field_source_reference_all()
      .description("The vertex the corner is attached to");
}

class CornerVertFieldInput final : public bke::MeshFieldInput {
 public:
  CornerVertFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Vertex") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Corner) {
      return {};
    }
    return VArray<int>::from_span(mesh.corner_verts());
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
  params.set_output("Vertex Index"_ustr,
                    Field<int>::from_input<bke::EvaluateAtIndexInput>(
                        params.extract_input<Field<int>>("Corner Index"_ustr),
                        Field<int>::from_input<CornerVertFieldInput>(),
                        AttrDomain::Corner));
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeVertexOfCorner"_ustr, GEO_NODE_MESH_TOPOLOGY_VERTEX_OF_CORNER);
  ntype.ui_name = "Vertex of Corner";
  ntype.ui_description = "Retrieve the vertex each face corner is attached to";
  ntype.enum_name_legacy = "VERTEX_OF_CORNER";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_vertex_of_corner_cc
