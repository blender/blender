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

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"

static bNodeSocketTemplate geo_node_material_replace_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_MATERIAL, N_("Old")},
    {SOCK_MATERIAL, N_("New")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_material_replace_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void geo_node_material_replace_exec(GeoNodeExecParams params)
{
  Material *old_material = params.extract_input<Material *>("Old");
  Material *new_material = params.extract_input<Material *>("New");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh != nullptr) {
      for (const int i : IndexRange(mesh->totcol)) {
        if (mesh->mat[i] == old_material) {
          mesh->mat[i] = new_material;
        }
      }
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_material_replace()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MATERIAL_REPLACE, "Material Replace", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_material_replace_in, geo_node_material_replace_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_material_replace_exec;
  nodeRegisterType(&ntype);
}
