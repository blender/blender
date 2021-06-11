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

#include "BLI_task.hh"

#include "BKE_material.h"

static bNodeSocketTemplate geo_node_select_by_material_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_MATERIAL,
     N_("Material"),
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     0.0f,
     PROP_NONE,
     SOCK_HIDE_LABEL},
    {SOCK_STRING, N_("Selection")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_select_by_material_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void select_mesh_by_material(const Mesh &mesh,
                                    const Material *material,
                                    const MutableSpan<bool> r_selection)
{
  BLI_assert(mesh.totpoly == r_selection.size());
  Vector<int> material_indices;
  for (const int i : IndexRange(mesh.totcol)) {
    if (mesh.mat[i] == material) {
      material_indices.append(i);
    }
  }
  parallel_for(r_selection.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      r_selection[i] = material_indices.contains(mesh.mpoly[i].mat_nr);
    }
  });
}

static void geo_node_select_by_material_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material");
  const std::string selection_name = params.extract_input<std::string>("Selection");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
    const Mesh *mesh = mesh_component.get_for_read();
    if (mesh != nullptr) {
      OutputAttribute_Typed<bool> selection =
          mesh_component.attribute_try_get_for_output_only<bool>(selection_name, ATTR_DOMAIN_FACE);
      if (selection) {
        select_mesh_by_material(*mesh, material, selection.as_span());
        selection.save();
      }
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_select_by_material()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SELECT_BY_MATERIAL, "Select by Material", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(
      &ntype, geo_node_select_by_material_in, geo_node_select_by_material_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_select_by_material_exec;
  nodeRegisterType(&ntype);
}
