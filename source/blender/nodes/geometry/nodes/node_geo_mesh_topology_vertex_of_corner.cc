/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_vertex_of_corner_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Corner Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The corner to retrieve data from. Defaults to the corner from the context");
  b.add_output<decl::Int>("Vertex Index")
      .field_source_reference_all()
      .description("The vertex the corner is attached to");
}

class CornerVertFieldInput final : public bke::MeshFieldInput {
 public:
  CornerVertFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Vertex")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Corner) {
      return {};
    }
    return VArray<int>::ForSpan(mesh.corner_verts());
  }

  uint64_t hash() const final
  {
    return 30495867093876;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    return dynamic_cast<const CornerVertFieldInput *>(&other) != nullptr;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return AttrDomain::Corner;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Vertex Index",
                    Field<int>(std::make_shared<bke::EvaluateAtIndexInput>(
                        params.extract_input<Field<int>>("Corner Index"),
                        Field<int>(std::make_shared<CornerVertFieldInput>()),
                        AttrDomain::Corner)));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TOPOLOGY_VERTEX_OF_CORNER, "Vertex of Corner", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_topology_vertex_of_corner_cc
