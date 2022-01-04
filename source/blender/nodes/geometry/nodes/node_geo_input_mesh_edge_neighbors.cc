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

namespace blender::nodes::node_geo_input_mesh_edge_neighbors_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Face Count"))
      .field_source()
      .description(N_("Number of faces that contain the edge"));
}

class EdgeNeighborCountFieldInput final : public GeometryFieldInput {
 public:
  EdgeNeighborCountFieldInput()
      : GeometryFieldInput(CPPType::get<int>(), "Edge Neighbor Count Field")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_MESH) {
      const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
      const Mesh *mesh = mesh_component.get_for_read();
      if (mesh == nullptr) {
        return {};
      }

      Array<int> face_count(mesh->totedge, 0);
      for (const int i : IndexRange(mesh->totloop)) {
        face_count[mesh->mloop[i].e]++;
      }

      return mesh_component.attribute_try_adapt_domain<int>(
          VArray<int>::ForContainer(std::move(face_count)), ATTR_DOMAIN_EDGE, domain);
    }
    return {};
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
