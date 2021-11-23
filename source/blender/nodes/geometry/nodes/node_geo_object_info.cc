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

namespace blender::nodes::node_geo_object_info_cc {

static void geo_node_object_info_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Object>(N_("Object")).hide_label();
  b.add_input<decl::Bool>(N_("As Instance"))
      .description(N_("Output the entire object as single instance. "
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

  auto default_transform = [&]() {
    params.set_output("Location", float3(0));
    params.set_output("Rotation", float3(0));
    params.set_output("Scale", float3(0));
  };
  auto default_geometry = [&]() { params.set_output("Geometry", GeometrySet()); };

  Object *object = params.get_input<Object *>("Object");

  const Object *self_object = params.self_object();
  if (object == nullptr) {
    default_transform();
    default_geometry();
    return;
  }

  const float4x4 &object_matrix = object->obmat;
  const float4x4 transform = float4x4(self_object->imat) * object_matrix;

  if (transform_space_relative) {
    params.set_output("Location", transform.translation());
    params.set_output("Rotation", transform.to_euler());
    params.set_output("Scale", transform.scale());
  }
  else {
    params.set_output("Location", object_matrix.translation());
    params.set_output("Rotation", object_matrix.to_euler());
    params.set_output("Scale", object_matrix.scale());
  }

  if (params.output_is_required("Geometry")) {
    if (object == self_object) {
      params.error_message_add(NodeWarningType::Error,
                               TIP_("Geometry cannot be retrieved from the modifier object"));
      default_geometry();
      return;
    }

    GeometrySet geometry_set;
    if (params.get_input<bool>("As Instance")) {
      InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
      const int handle = instances.add_reference(*object);
      if (transform_space_relative) {
        instances.add_instance(handle, transform);
      }
      else {
        instances.add_instance(handle, float4x4::identity());
      }
    }
    else {
      geometry_set = bke::object_get_evaluated_geometry_set(*object);
      if (transform_space_relative) {
        transform_geometry_set(geometry_set, transform, *params.depsgraph());
      }
    }

    params.set_output("Geometry", geometry_set);
  }
}

static void geo_node_object_info_node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryObjectInfo *data = (NodeGeometryObjectInfo *)MEM_callocN(
      sizeof(NodeGeometryObjectInfo), __func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

}  // namespace blender::nodes::node_geo_object_info_cc

void register_node_type_geo_object_info()
{
  namespace file_ns = blender::nodes::node_geo_object_info_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT, 0);
  node_type_init(&ntype, file_ns::geo_node_object_info_node_init);
  node_type_storage(
      &ntype, "NodeGeometryObjectInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::geo_node_object_info_exec;
  ntype.draw_buttons = file_ns::geo_node_object_info_layout;
  ntype.declare = file_ns::geo_node_object_info_declare;
  nodeRegisterType(&ntype);
}
