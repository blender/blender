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

#include "BLI_disjoint_set.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_island_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Island Index"))
      .field_source()
      .description(N_("The index of the each vertex's island. Indices are based on the "
                      "lowest vertex index contained in each island"));
  b.add_output<decl::Int>(N_("Island Count"))
      .field_source()
      .description(N_("The total number of mesh islands"));
}

class IslandFieldInput final : public GeometryFieldInput {
 public:
  IslandFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Island Index")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh == nullptr) {
      return {};
    }

    DisjointSet islands(mesh->totvert);
    for (const int i : IndexRange(mesh->totedge)) {
      islands.join(mesh->medge[i].v1, mesh->medge[i].v2);
    }

    Array<int> output(mesh->totvert);
    VectorSet<int> ordered_roots;
    for (const int i : IndexRange(mesh->totvert)) {
      const int64_t root = islands.find_root(i);
      output[i] = ordered_roots.index_of_or_add(root);
    }

    return mesh_component.attribute_try_adapt_domain<int>(
        VArray<int>::ForContainer(std::move(output)), ATTR_DOMAIN_POINT, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 635467354;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const IslandFieldInput *>(&other) != nullptr;
  }
};

class IslandCountFieldInput final : public GeometryFieldInput {
 public:
  IslandCountFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Island Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_MESH) {
      return {};
    }
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh == nullptr) {
      return {};
    }

    DisjointSet islands(mesh->totvert);
    for (const int i : IndexRange(mesh->totedge)) {
      islands.join(mesh->medge[i].v1, mesh->medge[i].v2);
    }

    Set<int> island_list;
    for (const int i_vert : IndexRange(mesh->totvert)) {
      const int64_t root = islands.find_root(i_vert);
      island_list.add(root);
    }

    return VArray<int>::ForSingle(island_list.size(),
                                  mesh_component.attribute_domain_size(domain));
  }

  uint64_t hash() const override
  {
    /* Some random hash. */
    return 45634572457;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const IslandCountFieldInput *>(&other) != nullptr;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  if (params.output_is_required("Island Index")) {
    Field<int> field{std::make_shared<IslandFieldInput>()};
    params.set_output("Island Index", std::move(field));
  }
  if (params.output_is_required("Island Count")) {
    Field<int> field{std::make_shared<IslandCountFieldInput>()};
    params.set_output("Island Count", std::move(field));
  }
}

}  // namespace blender::nodes::node_geo_input_mesh_island_cc

void register_node_type_geo_input_mesh_island()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_island_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_ISLAND, "Mesh Island", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
