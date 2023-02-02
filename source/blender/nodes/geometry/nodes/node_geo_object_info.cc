/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_object_info_cc {

NODE_STORAGE_FUNCS(NodeGeometryObjectInfo)

static void node_declare(NodeDeclarationBuilder &b)
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

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "transform_space", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryObjectInfo &storage = node_storage(params.node());
  const bool transform_space_relative = (storage.transform_space ==
                                         GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  Object *object = params.get_input<Object *>("Object");

  const Object *self_object = params.self_object();
  if (object == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  const float4x4 object_matrix = float4x4(object->object_to_world);
  const float4x4 transform = float4x4(self_object->world_to_object) * object_matrix;

  float3 location, scale;
  math::EulerXYZ rotation;
  if (transform_space_relative) {
    math::to_loc_rot_scale(transform, location, rotation, scale);
  }
  else {
    math::to_loc_rot_scale(object_matrix, location, rotation, scale);
  }
  params.set_output("Location", location);
  params.set_output("Rotation", float3(rotation));
  params.set_output("Scale", scale);

  if (params.output_is_required("Geometry")) {
    if (object == self_object) {
      params.error_message_add(NodeWarningType::Error,
                               TIP_("Geometry cannot be retrieved from the modifier object"));
      params.set_default_remaining_outputs();
      return;
    }

    GeometrySet geometry_set;
    if (params.get_input<bool>("As Instance")) {
      std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
      const int handle = instances->add_reference(*object);
      if (transform_space_relative) {
        instances->add_instance(handle, transform);
      }
      else {
        instances->add_instance(handle, float4x4::identity());
      }
      geometry_set = GeometrySet::create_with_instances(instances.release());
    }
    else {
      geometry_set = bke::object_get_evaluated_geometry_set(*object);
      if (transform_space_relative) {
        transform_geometry_set(params, geometry_set, transform, *params.depsgraph());
      }
    }

    params.set_output("Geometry", geometry_set);
  }
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryObjectInfo *data = MEM_cnew<NodeGeometryObjectInfo>(__func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

}  // namespace blender::nodes::node_geo_object_info_cc

void register_node_type_geo_object_info()
{
  namespace file_ns = blender::nodes::node_geo_object_info_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_OBJECT_INFO, "Object Info", NODE_CLASS_INPUT);
  ntype.initfunc = file_ns::node_node_init;
  node_type_storage(
      &ntype, "NodeGeometryObjectInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
