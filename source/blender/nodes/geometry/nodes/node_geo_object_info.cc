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

#include "BKE_mesh.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"

#include "BLI_math_matrix.h"

static bNodeSocketTemplate geo_node_object_info_in[] = {
    {SOCK_OBJECT, N_("Object")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_object_info_out[] = {
    {SOCK_VECTOR, N_("Location")},
    {SOCK_VECTOR, N_("Rotation")},
    {SOCK_VECTOR, N_("Scale")},
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {
static void geo_node_object_info_exec(GeoNodeExecParams params)
{
  bke::PersistentObjectHandle object_handle = params.extract_input<bke::PersistentObjectHandle>(
      "Object");
  Object *object = params.handle_map().lookup(object_handle);

  float3 location = {0, 0, 0};
  float3 rotation = {0, 0, 0};
  float3 scale = {0, 0, 0};
  GeometrySet geometry_set;

  const Object *self_object = params.self_object();

  if (object != nullptr) {
    float transform[4][4];
    mul_m4_m4m4(transform, self_object->imat, object->obmat);

    float quaternion[4];
    mat4_decompose(location, quaternion, scale, transform);
    quat_to_eul(rotation, quaternion);

    if (object != self_object) {
      if (object->type == OB_MESH) {
        Mesh *mesh = BKE_modifier_get_evaluated_mesh_from_evaluated_object(object, false);
        if (mesh != nullptr) {
          BKE_mesh_wrapper_ensure_mdata(mesh);

          /* Make a copy because the life time of the other mesh might be shorter. */
          Mesh *copied_mesh = BKE_mesh_copy_for_eval(mesh, false);

          /* Transform into the local space of the object that is being modified. */
          BKE_mesh_transform(copied_mesh, transform, true);

          MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
          mesh_component.replace(copied_mesh);
          mesh_component.copy_vertex_group_names_from_object(*object);
        }
      }
    }
  }

  params.set_output("Location", location);
  params.set_output("Rotation", rotation);
  params.set_output("Scale", scale);
  params.set_output("Geometry", geometry_set);
}
}  // namespace blender::nodes

void register_node_type_geo_object_info()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, geo_node_object_info_in, geo_node_object_info_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_object_info_exec;
  nodeRegisterType(&ntype);
}
