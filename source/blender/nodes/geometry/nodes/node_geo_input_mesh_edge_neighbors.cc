/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Face Count")
      .field_source()
      .description("The number of faces that use each edge as one of their sides");
}

class EdgeNeighborCountFieldInput final : public bke::MeshFieldInput {
 public:
  EdgeNeighborCountFieldInput()
      : bke::MeshFieldInput(CPPType::get<int>(), "Edge Neighbor Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    Array<int> counts(mesh.totedge, 0);
    array_utils::count_indices(mesh.corner_edges(), counts);
    return mesh.attributes().adapt_domain<int>(
        VArray<int>::ForContainer(std::move(counts)), ATTR_DOMAIN_EDGE, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 985671075;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const EdgeNeighborCountFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_EDGE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> neighbor_count_field{std::make_shared<EdgeNeighborCountFieldInput>()};
  params.set_output("Face Count", std::move(neighbor_count_field));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_EDGE_NEIGHBORS, "Edge Neighbors", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_edge_neighbors_cc
