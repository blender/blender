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

static void geo_node_legacy_material_assign_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Material>(N_("Material")).hide_label(true);
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void assign_material_to_faces(Mesh &mesh, const VArray<bool> &face_mask, Material *material)
{
  int new_material_index = -1;
  for (const int i : IndexRange(mesh.totcol)) {
    Material *other_material = mesh.mat[i];
    if (other_material == material) {
      new_material_index = i;
      break;
    }
  }
  if (new_material_index == -1) {
    /* Append a new material index. */
    new_material_index = mesh.totcol;
    BKE_id_material_eval_assign(&mesh.id, new_material_index + 1, material);
  }

  mesh.mpoly = (MPoly *)CustomData_duplicate_referenced_layer(&mesh.pdata, CD_MPOLY, mesh.totpoly);
  for (const int i : IndexRange(mesh.totpoly)) {
    if (face_mask[i]) {
      MPoly &poly = mesh.mpoly[i];
      poly.mat_nr = new_material_index;
    }
  }
}

static void geo_node_legacy_material_assign_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material");
  const std::string mask_name = params.extract_input<std::string>("Selection");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh != nullptr) {
      GVArray_Typed<bool> face_mask = mesh_component.attribute_get_for_read<bool>(
          mask_name, ATTR_DOMAIN_FACE, true);
      assign_material_to_faces(*mesh, face_mask, material);
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_legacy_material_assign()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_MATERIAL_ASSIGN, "Material Assign", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_legacy_material_assign_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_legacy_material_assign_exec;
  nodeRegisterType(&ntype);
}
