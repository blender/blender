/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_vertices_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Vertex Index 1")
      .field_source()
      .description("The index of the first vertex in the edge");
  b.add_output<decl::Int>("Vertex Index 2")
      .field_source()
      .description("The index of the second vertex in the edge");
  b.add_output<decl::Vector>("Position 1")
      .field_source()
      .description("The position of the first vertex in the edge");
  b.add_output<decl::Vector>("Position 2")
      .field_source()
      .description("The position of the second vertex in the edge");
}

enum class VertNumber { V1, V2 };

static VArray<int> construct_edge_verts_gvarray(const Mesh &mesh,
                                                const VertNumber vertex,
                                                const AttrDomain domain)
{
  const Span<int2> edges = mesh.edges();
  if (domain == AttrDomain::Edge) {
    if (vertex == VertNumber::V1) {
      return VArray<int>::ForFunc(edges.size(), [edges](const int i) { return edges[i][0]; });
    }
    return VArray<int>::ForFunc(edges.size(), [edges](const int i) { return edges[i][1]; });
  }
  return {};
}

class EdgeVertsInput final : public bke::MeshFieldInput {
 private:
  VertNumber vertex_;

 public:
  EdgeVertsInput(VertNumber vertex)
      : bke::MeshFieldInput(CPPType::get<int>(), "Edge Vertices Field"), vertex_(vertex)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_edge_verts_gvarray(mesh, vertex_, domain);
  }

  uint64_t hash() const override
  {
    return vertex_ == VertNumber::V1 ? 23847562893465 : 92384598734567;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const EdgeVertsInput *other_field = dynamic_cast<const EdgeVertsInput *>(&other)) {
      return vertex_ == other_field->vertex_;
    }
    return false;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Edge;
  }
};

static VArray<float3> construct_edge_positions_gvarray(const Mesh &mesh,
                                                       const VertNumber vertex,
                                                       const AttrDomain domain)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int2> edges = mesh.edges();

  if (vertex == VertNumber::V1) {
    return mesh.attributes().adapt_domain<float3>(
        VArray<float3>::ForFunc(
            edges.size(), [positions, edges](const int i) { return positions[edges[i][0]]; }),
        AttrDomain::Edge,
        domain);
  }
  return mesh.attributes().adapt_domain<float3>(
      VArray<float3>::ForFunc(edges.size(),
                              [positions, edges](const int i) { return positions[edges[i][1]]; }),
      AttrDomain::Edge,
      domain);
}

class EdgePositionFieldInput final : public bke::MeshFieldInput {
 private:
  VertNumber vertex_;

 public:
  EdgePositionFieldInput(VertNumber vertex)
      : bke::MeshFieldInput(CPPType::get<float3>(), "Edge Position Field"), vertex_(vertex)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_edge_positions_gvarray(mesh, vertex_, domain);
  }

  uint64_t hash() const override
  {
    return vertex_ == VertNumber::V1 ? 987456978362 : 374587679866;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const EdgePositionFieldInput *other_field = dynamic_cast<const EdgePositionFieldInput *>(
            &other))
    {
      return vertex_ == other_field->vertex_;
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
  Field<int> vertex_field_1{std::make_shared<EdgeVertsInput>(VertNumber::V1)};
  Field<int> vertex_field_2{std::make_shared<EdgeVertsInput>(VertNumber::V2)};
  Field<float3> position_field_1{std::make_shared<EdgePositionFieldInput>(VertNumber::V1)};
  Field<float3> position_field_2{std::make_shared<EdgePositionFieldInput>(VertNumber::V2)};

  params.set_output("Vertex Index 1", std::move(vertex_field_1));
  params.set_output("Vertex Index 2", std::move(vertex_field_2));
  params.set_output("Position 1", std::move(position_field_1));
  params.set_output("Position 2", std::move(position_field_2));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_EDGE_VERTICES, "Edge Vertices", NODE_CLASS_INPUT);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_edge_vertices_cc
