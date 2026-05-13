/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "DNA_mesh_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_vertex_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Vertex Count"_ustr)
      .field_source()
      .description(
          "The number of vertices connected to this vertex with an edge, "
          "equal to the number of connected edges");
  b.add_output<decl::Int>("Face Count"_ustr)
      .field_source()
      .description("Number of faces that contain the vertex");
}

class VertexCountFieldInput final : public bke::MeshFieldInput {
 public:
  VertexCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Count Field") {}

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Point) {
      return {};
    }
    Array<int> counts(mesh.verts_num, 0);
    array_utils::count_indices(mesh.edges().cast<int>(), counts);
    return VArray<int>::from_container(std::move(counts));
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Point;
  }
};

class VertexFaceCountFieldInput final : public bke::MeshFieldInput {
 public:
  VertexFaceCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Face Count Field")
  {
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != AttrDomain::Point) {
      return {};
    }
    Array<int> counts(mesh.verts_num, 0);
    array_utils::count_indices(mesh.corner_verts(), counts);
    return VArray<int>::from_container(std::move(counts));
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Point;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Vertex Count"_ustr, Field<int>::from_input<VertexCountFieldInput>());
  params.set_output("Face Count"_ustr, Field<int>::from_input<VertexFaceCountFieldInput>());
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeInputMeshVertexNeighbors"_ustr, GEO_NODE_INPUT_MESH_VERTEX_NEIGHBORS);
  ntype.ui_name = "Vertex Neighbors";
  ntype.ui_description = "Retrieve topology information relating to each vertex of a mesh";
  ntype.enum_name_legacy = "MESH_VERTEX_NEIGHBORS";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_vertex_neighbors_cc
