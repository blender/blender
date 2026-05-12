/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Face Count"_ustr)
      .field_source()
      .description("The number of faces that use each edge as one of their sides");
}

class EdgeNeighborCountFieldInput final : public bke::MeshFieldInput {
 public:
  EdgeNeighborCountFieldInput()
      : bke::MeshFieldInput(CPPType::get<int>(), "Edge Neighbor Count Field")
  {
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    Array<int> counts(mesh.edges_num, 0);
    array_utils::count_indices(mesh.corner_edges(), counts);
    return mesh.attributes().adapt_domain<int>(
        VArray<int>::from_container(std::move(counts)), AttrDomain::Edge, domain);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Face Count"_ustr, Field<int>::from_input<EdgeNeighborCountFieldInput>());
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeInputMeshEdgeNeighbors"_ustr, GEO_NODE_INPUT_MESH_EDGE_NEIGHBORS);
  ntype.ui_name = "Edge Neighbors";
  ntype.ui_description = "Retrieve the number of faces that use each edge as one of their sides";
  ntype.enum_name_legacy = "MESH_EDGE_NEIGHBORS";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_edge_neighbors_cc
