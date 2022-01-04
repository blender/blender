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

namespace blender::nodes::node_geo_input_mesh_vertex_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Vertex Count"))
      .field_source()
      .description(N_("Vertex count and edge count are equal"));
  b.add_output<decl::Int>(N_("Face Count"))
      .field_source()
      .description(N_("Number of faces that contain the vertex"));
}

static VArray<int> construct_vertex_count_gvarray(const MeshComponent &component,
                                                  const AttributeDomain domain)
{
  const Mesh *mesh = component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  if (domain == ATTR_DOMAIN_POINT) {
    Array<int> vertices(mesh->totvert, 0);
    for (const int i : IndexRange(mesh->totedge)) {
      vertices[mesh->medge[i].v1]++;
      vertices[mesh->medge[i].v2]++;
    }
    return VArray<int>::ForContainer(std::move(vertices));
  }
  return {};
}

class VertexCountFieldInput final : public GeometryFieldInput {
 public:
  VertexCountFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Vertex Count Field")
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
    return 23574528465;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const VertexCountFieldInput *>(&other) != nullptr;
  }
};

static VArray<int> construct_face_count_gvarray(const MeshComponent &component,
                                                const AttributeDomain domain)
{
  const Mesh *mesh = component.get_for_read();
  if (mesh == nullptr) {
    return {};
  }

  if (domain == ATTR_DOMAIN_POINT) {
    Array<int> vertices(mesh->totvert, 0);
    for (const int i : IndexRange(mesh->totloop)) {
      int vertex = mesh->mloop[i].v;
      vertices[vertex]++;
    }
    return VArray<int>::ForContainer(std::move(vertices));
  }
  return {};
}

class VertexFaceCountFieldInput final : public GeometryFieldInput {
 public:
  VertexFaceCountFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Vertex Face Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_MESH) {
      const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
      return construct_face_count_gvarray(mesh_component, domain);
    }
    return {};
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
