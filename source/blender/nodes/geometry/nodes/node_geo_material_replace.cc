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

namespace blender::nodes {

static void geo_node_material_replace_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Material>(N_("Old"));
  b.add_input<decl::Material>(N_("New"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_material_replace_exec(GeoNodeExecParams params)
{
  Material *old_material = params.extract_input<Material *>("Old");
  Material *new_material = params.extract_input<Material *>("New");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    Mesh *mesh = geometry_set.get_mesh_for_write();
    if (mesh != nullptr) {
      for (const int i : IndexRange(mesh->totcol)) {
        if (mesh->mat[i] == old_material) {
          mesh->mat[i] = new_material;
        }
      }
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_material_replace()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_REPLACE_MATERIAL, "Replace Material", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_material_replace_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_material_replace_exec;
  nodeRegisterType(&ntype);
}
