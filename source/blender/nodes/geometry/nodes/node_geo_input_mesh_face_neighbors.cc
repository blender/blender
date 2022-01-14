/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

static VArray<int> construct_neighbor_count_gvarray(const MeshComponent &component,
                                                    const AttributeDomain domain)
{
  const Mesh *mesh = component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  Array<int> edge_count(mesh->totedge, 0);
  for (const int i : IndexRange(mesh->totloop)) {
    edge_count[mesh->mloop[i].e]++;
  }

  Array<int> poly_count(mesh->totpoly, 0);
  for (const int poly_num : IndexRange(mesh->totpoly)) {
    MPoly &poly = mesh->mpoly[poly_num];
    for (const int loop_num : IndexRange(poly.loopstart, poly.totloop)) {
      poly_count[poly_num] += edge_count[mesh->mloop[loop_num].e] - 1;
    }
  }

  return component.attribute_try_adapt_domain<int>(
      VArray<int>::ForContainer(std::move(poly_count)), ATTR_DOMAIN_FACE, domain);
}

class FaceNeighborCountFieldInput final : public GeometryFieldInput {
 public:
  FaceNeighborCountFieldInput()
      : GeometryFieldInput(CPPType::get<int>(), "Face Neighbor Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_MESH) {
      const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
      return construct_neighbor_count_gvarray(mesh_component, domain);
    }
    return {};
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

static VArray<int> construct_vertex_count_gvarray(const MeshComponent &component,
                                                  const AttributeDomain domain)
{
  const Mesh *mesh = component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  return component.attribute_try_adapt_domain<int>(
      VArray<int>::ForFunc(mesh->totpoly,
                           [mesh](const int i) -> float { return mesh->mpoly[i].totloop; }),
      ATTR_DOMAIN_FACE,
      domain);
}

class FaceVertexCountFieldInput final : public GeometryFieldInput {
 public:
  FaceVertexCountFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Vertex Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_MESH) {
      const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
      return construct_vertex_count_gvarray(mesh_component, domain);
    }
    return {};
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
