/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Vertex Count")
      .field_source()
      .description("Number of edges or points in the face");
  b.add_output<decl::Int>("Face Count")
      .field_source()
      .description("Number of faces which share an edge with the face");
}

static bool large_enough_total_size(const GroupedSpan<int> values,
                                    const Span<int> indices,
                                    const int max)
{
  int num = 0;
  for (const int i : indices) {
    num += values[i].size();
    if (max <= num) {
      return true;
    }
  }
  return false;
}

static int unique_num(const GroupedSpan<int> values, const Span<int> indices)
{
  if (large_enough_total_size(values, indices, 100)) {
    Set<int, 16> unique_values;
    for (const int i : indices) {
      unique_values.add_multiple(values[i]);
    }
    return unique_values.size();
  }
  Vector<int, 16> unique_values;
  for (const int i : indices) {
    unique_values.extend_non_duplicates(values[i]);
  }
  return unique_values.size();
}

static VArray<int> construct_neighbor_count_varray(const Mesh &mesh, const AttrDomain domain)
{
  const GroupedSpan<int> face_edges(mesh.faces(), mesh.corner_edges());

  Array<int> offsets;
  Array<int> indices;
  GroupedSpan<int> edge_to_faces_map = bke::mesh::build_edge_to_face_map(
      face_edges.offsets, face_edges.data, mesh.edges_num, offsets, indices);

  Array<int> face_count(face_edges.size());
  threading::parallel_for(face_edges.index_range(), 2048, [&](const IndexRange range) {
    for (const int64_t face_i : range) {
      face_count[face_i] = unique_num(edge_to_faces_map, face_edges[face_i]) - 1;
    }
  });
  return mesh.attributes().adapt_domain<int>(
      VArray<int>::ForContainer(std::move(face_count)), AttrDomain::Face, domain);
}

class FaceNeighborCountFieldInput final : public bke::MeshFieldInput {
 public:
  FaceNeighborCountFieldInput()
      : bke::MeshFieldInput(CPPType::get<int>(), "Face Neighbor Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_neighbor_count_varray(mesh, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 823543774;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const FaceNeighborCountFieldInput *>(&other) != nullptr;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Face;
  }
};

static VArray<int> construct_vertex_count_varray(const Mesh &mesh, const AttrDomain domain)
{
  const OffsetIndices faces = mesh.faces();
  return mesh.attributes().adapt_domain<int>(
      VArray<int>::ForFunc(faces.size(),
                           [faces](const int i) -> float { return faces[i].size(); }),
      AttrDomain::Face,
      domain);
}

class FaceVertexCountFieldInput final : public bke::MeshFieldInput {
 public:
  FaceVertexCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_vertex_count_varray(mesh, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 236235463634;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const FaceVertexCountFieldInput *>(&other) != nullptr;
  }

  std::optional<AttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return AttrDomain::Face;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> vertex_count_field{std::make_shared<FaceVertexCountFieldInput>()};
  Field<int> neighbor_count_field{std::make_shared<FaceNeighborCountFieldInput>()};
  params.set_output("Vertex Count", std::move(vertex_count_field));
  params.set_output("Face Count", std::move(neighbor_count_field));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_FACE_NEIGHBORS, "Face Neighbors", NODE_CLASS_INPUT);
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_mesh_face_neighbors_cc
