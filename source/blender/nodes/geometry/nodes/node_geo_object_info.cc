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

#include "BLI_math_matrix.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_object_info_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Object>(N_("Object")).hide_label();
  b.add_input<decl::Bool>(N_("As Instance"))
      .description(
          N_("Output the entire object as single instance. "
          "This allows instancing non-geometry object types"));
  b.add_output<decl::Vector>(N_("Location"));
  b.add_output<decl::Vector>(N_("Rotation"));
  b.add_output<decl::Vector>(N_("Scale"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_object_info_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "transform_space", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_object_info_exec(GeoNodeExecParams params)
{
  const bNode &bnode = params.node();
  NodeGeometryObjectInfo *node_storage = (NodeGeometryObjectInfo *)bnode.storage;
  const bool transform_space_relative = (node_storage->transform_space ==
                                         GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  Object *object = params.get_input<Object *>("Object");

  float3 location = {0, 0, 0};
  float3 rotation = {0, 0, 0};
  float3 scale = {0, 0, 0};
  GeometrySet geometry_set;

  const Object *self_object = params.self_object();

  if (object != nullptr) {
    const float4x4 transform = float4x4(self_object->imat) * float4x4(object->obmat);

    float quaternion[4];
    if (transform_space_relative) {
      mat4_decompose(location, quaternion, scale, transform.values);
    }
    else {
      mat4_decompose(location, quaternion, scale, object->obmat);
    }
    quat_to_eul(rotation, quaternion);

    if (object != self_object) {
      if (params.get_input<bool>("As Instance")) {
        InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
        const int handle = instances.add_reference(*object);
        if (transform_space_relative) {
          instances.add_instance(handle, transform);
        }
        else {
          float unit_transform[4][4];
          unit_m4(unit_transform);
          instances.add_instance(handle, unit_transform);
        }
      }
      else {
        geometry_set = bke::object_get_evaluated_geometry_set(*object);
        if (transform_space_relative) {
          transform_geometry_set(geometry_set, transform, *params.depsgraph());
        }
      }
    }
  }

  params.set_output("Location", location);
  params.set_output("Rotation", rotation);
  params.set_output("Scale", scale);
  params.set_output("Geometry", geometry_set);
}

static void geo_node_object_info_node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryObjectInfo *data = (NodeGeometryObjectInfo *)MEM_callocN(
      sizeof(NodeGeometryObjectInfo), __func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

}  // namespace blender::nodes

void register_node_type_geo_object_info()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT, 0);
  node_type_init(&ntype, blender::nodes::geo_node_object_info_node_init);
  node_type_storage(
      &ntype, "NodeGeometryObjectInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_object_info_exec;
  ntype.draw_buttons = blender::nodes::geo_node_object_info_layout;
  ntype.declare = blender::nodes::geo_node_object_info_declare;
  nodeRegisterType(&ntype);
}
