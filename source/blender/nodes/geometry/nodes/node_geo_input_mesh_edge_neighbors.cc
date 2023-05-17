/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

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
                                 const IndexMask /*mask*/) const final
  {
    const Span<int> corner_edges = mesh.corner_edges();
    Array<int> face_count(mesh.totedge, 0);
    for (const int edge : corner_edges) {
      face_count[edge]++;
    }

    return mesh.attributes().adapt_domain<int>(
        VArray<int>::ForContainer(std::move(face_count)), ATTR_DOMAIN_EDGE, domain);
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

}  // namespace blender::nodes::node_geo_input_mesh_edge_neighbors_cc

void register_node_type_geo_input_mesh_edge_neighbors()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_edge_neighbors_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_EDGE_NEIGHBORS, "Edge Neighbors", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
