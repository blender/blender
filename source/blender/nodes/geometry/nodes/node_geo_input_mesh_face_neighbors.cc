/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_face_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Vertex Count"))
      .field_source()
      .description(N_("Number of edges or points in the face"));
  b.add_output<decl::Int>(N_("Face Count"))
      .field_source()
      .description(N_("Number of faces which share an edge with the face"));
}

static VArray<int> construct_neighbor_count_varray(const Mesh &mesh, const eAttrDomain domain)
{
  const Span<MEdge> edges(mesh.medge, mesh.totedge);
  const Span<MPoly> polygons(mesh.mpoly, mesh.totpoly);
  const Span<MLoop> loops(mesh.mloop, mesh.totloop);

  Array<int> edge_count(edges.size(), 0);
  for (const int i : loops.index_range()) {
    edge_count[loops[i].e]++;
  }

  Array<int> poly_count(polygons.size(), 0);
  for (const int poly_i : polygons.index_range()) {
    const MPoly &poly = polygons[poly_i];
    for (const MLoop &loop : loops.slice(poly.loopstart, poly.totloop)) {
      poly_count[poly_i] += edge_count[loop.e] - 1;
    }
  }

  return bke::mesh_attributes(mesh).adapt_domain<int>(
      VArray<int>::ForContainer(std::move(poly_count)), ATTR_DOMAIN_FACE, domain);
}

class FaceNeighborCountFieldInput final : public bke::MeshFieldInput {
 public:
  FaceNeighborCountFieldInput()
      : bke::MeshFieldInput(CPPType::get<int>(), "Face Neighbor Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 IndexMask UNUSED(mask)) const final
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
};

static VArray<int> construct_vertex_count_varray(const Mesh &mesh, const eAttrDomain domain)
{
  const Span<MPoly> polygons(mesh.mpoly, mesh.totpoly);
  return bke::mesh_attributes(mesh).adapt_domain<int>(
      VArray<int>::ForFunc(polygons.size(),
                           [polygons](const int i) -> float { return polygons[i].totloop; }),
      ATTR_DOMAIN_FACE,
      domain);
}

class FaceVertexCountFieldInput final : public bke::MeshFieldInput {
 public:
  FaceVertexCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Vertex Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 IndexMask UNUSED(mask)) const final
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
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> vertex_count_field{std::make_shared<FaceVertexCountFieldInput>()};
  Field<int> neighbor_count_field{std::make_shared<FaceNeighborCountFieldInput>()};
  params.set_output("Vertex Count", std::move(vertex_count_field));
  params.set_output("Face Count", std::move(neighbor_count_field));
}

}  // namespace blender::nodes::node_geo_input_mesh_face_neighbors_cc

void register_node_type_geo_input_mesh_face_neighbors()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_face_neighbors_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_MESH_FACE_NEIGHBORS, "Face Neighbors", NODE_CLASS_INPUT);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
