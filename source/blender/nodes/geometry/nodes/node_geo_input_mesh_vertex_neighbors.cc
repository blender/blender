/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_vertex_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Vertex Count")
      .field_source()
      .description(
          "The number of vertices connected to this vertex with an edge, "
          "equal to the number of connected edges");
  b.add_output<decl::Int>("Face Count")
      .field_source()
      .description("Number of faces that contain the vertex");
}

static VArray<int> construct_vertex_count_gvarray(const Mesh &mesh, const eAttrDomain domain)
{
  const Span<int2> edges = mesh.edges();
  if (domain == ATTR_DOMAIN_POINT) {
    Array<int> counts(mesh.totvert, 0);
    for (const int i : edges.index_range()) {
      counts[edges[i][0]]++;
      counts[edges[i][1]]++;
    }
    return VArray<int>::ForContainer(std::move(counts));
  }
  return {};
}

class VertexCountFieldInput final : public bke::MeshFieldInput {
 public:
  VertexCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_vertex_count_gvarray(mesh, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 23574528465;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const VertexCountFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_POINT;
  }
};

static VArray<int> construct_face_count_gvarray(const Mesh &mesh, const eAttrDomain domain)
{
  const Span<int> corner_verts = mesh.corner_verts();
  if (domain == ATTR_DOMAIN_POINT) {
    Array<int> vertices(mesh.totvert, 0);
    for (const int vert : corner_verts) {
      vertices[vert]++;
    }
    return VArray<int>::ForContainer(std::move(vertices));
  }
  return {};
}

class VertexFaceCountFieldInput final : public bke::MeshFieldInput {
 public:
  VertexFaceCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Face Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_face_count_gvarray(mesh, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 3462374322;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const VertexFaceCountFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_POINT;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> vertex_field{std::make_shared<VertexCountFieldInput>()};
  Field<int> face_field{std::make_shared<VertexFaceCountFieldInput>()};

  params.set_output("Vertex Count", std::move(vertex_field));
  params.set_output("Face Count", std::move(face_field));
}

}  // namespace blender::nodes::node_geo_input_mesh_vertex_neighbors_cc

void register_node_type_geo_input_mesh_vertex_neighbors()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_vertex_neighbors_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_VERTEX_NEIGHBORS, "Vertex Neighbors", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
