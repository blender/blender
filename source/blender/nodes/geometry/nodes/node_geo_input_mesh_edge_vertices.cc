/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_vertices_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Vertex Index 1"))
      .field_source()
      .description(N_("The index of the first vertex in the edge"));
  b.add_output<decl::Int>(N_("Vertex Index 2"))
      .field_source()
      .description(N_("The index of the second vertex in the edge"));
  b.add_output<decl::Vector>(N_("Position 1"))
      .field_source()
      .description(N_("The position of the first vertex in the edge"));
  b.add_output<decl::Vector>(N_("Position 2"))
      .field_source()
      .description(N_("The position of the second vertex in the edge"));
}

enum VertexNumber { VERTEX_ONE, VERTEX_TWO };

static VArray<int> construct_edge_verts_gvarray(const Mesh &mesh,
                                                const VertexNumber vertex,
                                                const eAttrDomain domain)
{
  const Span<MEdge> edges = mesh.edges();
  if (domain == ATTR_DOMAIN_EDGE) {
    if (vertex == VERTEX_ONE) {
      return VArray<int>::ForFunc(edges.size(),
                                  [edges](const int i) -> int { return edges[i].v1; });
    }
    return VArray<int>::ForFunc(edges.size(), [edges](const int i) -> int { return edges[i].v2; });
  }
  return {};
}

class EdgeVerticesFieldInput final : public bke::MeshFieldInput {
 private:
  VertexNumber vertex_;

 public:
  EdgeVerticesFieldInput(VertexNumber vertex)
      : bke::MeshFieldInput(CPPType::get<int>(), "Edge Vertices Field"), vertex_(vertex)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    return construct_edge_verts_gvarray(mesh, vertex_, domain);
  }

  uint64_t hash() const override
  {
    return vertex_ == VERTEX_ONE ? 23847562893465 : 92384598734567;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const EdgeVerticesFieldInput *other_field = dynamic_cast<const EdgeVerticesFieldInput *>(
            &other)) {
      return vertex_ == other_field->vertex_;
    }
    return false;
  }
};

static VArray<float3> construct_edge_positions_gvarray(const Mesh &mesh,
                                                       const VertexNumber vertex,
                                                       const eAttrDomain domain)
{
  const Span<MVert> verts = mesh.verts();
  const Span<MEdge> edges = mesh.edges();

  if (vertex == VERTEX_ONE) {
    return mesh.attributes().adapt_domain<float3>(
        VArray<float3>::ForFunc(edges.size(),
                                [verts, edges](const int i) { return verts[edges[i].v1].co; }),
        ATTR_DOMAIN_EDGE,
        domain);
  }
  return mesh.attributes().adapt_domain<float3>(
      VArray<float3>::ForFunc(edges.size(),
                              [verts, edges](const int i) { return verts[edges[i].v2].co; }),
      ATTR_DOMAIN_EDGE,
      domain);
}

class EdgePositionFieldInput final : public bke::MeshFieldInput {
 private:
  VertexNumber vertex_;

 public:
  EdgePositionFieldInput(VertexNumber vertex)
      : bke::MeshFieldInput(CPPType::get<float3>(), "Edge Position Field"), vertex_(vertex)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    return construct_edge_positions_gvarray(mesh, vertex_, domain);
  }

  uint64_t hash() const override
  {
    return vertex_ == VERTEX_ONE ? 987456978362 : 374587679866;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const EdgePositionFieldInput *other_field = dynamic_cast<const EdgePositionFieldInput *>(
            &other)) {
      return vertex_ == other_field->vertex_;
    }
    return false;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> vertex_field_1{std::make_shared<EdgeVerticesFieldInput>(VERTEX_ONE)};
  Field<int> vertex_field_2{std::make_shared<EdgeVerticesFieldInput>(VERTEX_TWO)};
  Field<float3> position_field_1{std::make_shared<EdgePositionFieldInput>(VERTEX_ONE)};
  Field<float3> position_field_2{std::make_shared<EdgePositionFieldInput>(VERTEX_TWO)};

  params.set_output("Vertex Index 1", std::move(vertex_field_1));
  params.set_output("Vertex Index 2", std::move(vertex_field_2));
  params.set_output("Position 1", std::move(position_field_1));
  params.set_output("Position 2", std::move(position_field_2));
}

}  // namespace blender::nodes::node_geo_input_mesh_edge_vertices_cc

void register_node_type_geo_input_mesh_edge_vertices()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_edge_vertices_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_EDGE_VERTICES, "Edge Vertices", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
