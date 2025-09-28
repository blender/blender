/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_euler.hh"
#include "BLI_math_matrix.hh"

#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "DNA_object_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_transform.hh"

#include "BLT_translation.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_object_info_cc {

NODE_STORAGE_FUNCS(NodeGeometryObjectInfo)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Object>("Object").optional_label();
  b.add_input<decl::Bool>("As Instance")
      .description(
          "Output the entire object as single instance. "
          "This allows instancing non-geometry object types");
  b.add_output<decl::Matrix>("Transform")
      .description(
          "Transformation matrix containing the location, rotation and scale of the object");
  b.add_output<decl::Vector>("Location");
  b.add_output<decl::Rotation>("Rotation");
  b.add_output<decl::Vector>("Scale");
  b.add_output<decl::Geometry>("Geometry");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "transform_space", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryObjectInfo &storage = node_storage(params.node());
  const bool transform_space_relative = (storage.transform_space ==
                                         GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  Object *object = params.extract_input<Object *>("Object");

  const Object *self_object = params.self_object();
  if (object == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  const bool self_transform_evaluated = DEG_object_transform_is_evaluated(*self_object);
  const bool object_transform_evaluated = DEG_object_transform_is_evaluated(*object);
  const bool object_geometry_evaluated = DEG_object_geometry_is_evaluated(*object);

  float4x4 output_transform = float4x4::identity();
  bool show_transform_error = false;
  if (transform_space_relative) {
    if (self_transform_evaluated && object_transform_evaluated) {
      output_transform = self_object->world_to_object() * object->object_to_world();
    }
    else {
      show_transform_error = true;
    }
  }
  else {
    if (object_transform_evaluated) {
      output_transform = object->object_to_world();
    }
    else {
      show_transform_error = true;
    }
  }
  if (show_transform_error) {
    params.error_message_add(
        NodeWarningType::Error,
        TIP_("Cannot access object's transforms because it's not evaluated yet. "
             "This can happen when there is a dependency cycle"));
  }
  float3 location, scale;
  math::Quaternion rotation;
  math::to_loc_rot_scale_safe<true>(output_transform, location, rotation, scale);

  params.set_output("Location", location);
  params.set_output("Rotation", rotation);
  params.set_output("Scale", scale);
  params.set_output("Transform", output_transform);

  if (!params.output_is_required("Geometry")) {
    return;
  }
  /* Compare by `orig_id` because objects may be copied into separate depsgraphs. */
  if (DEG_get_original(object) == DEG_get_original(self_object)) {
    params.error_message_add(
        NodeWarningType::Error,
        params.user_data()->call_data->operator_data ?
            TIP_("Geometry cannot be retrieved from the edited object itself") :
            TIP_("Geometry cannot be retrieved from the modifier object"));
    params.set_default_remaining_outputs();
    return;
  }
  BLI_assert(object != self_object);

  if (!object_geometry_evaluated) {
    params.error_message_add(
        NodeWarningType::Error,
        TIP_("Cannot access object's geometry because it's not evaluated yet. "
             "This can happen when there is a dependency cycle"));
    params.set_default_remaining_outputs();
    return;
  }

  std::optional<float4x4> geometry_transform;
  if (transform_space_relative) {
    if (!self_transform_evaluated || !object_transform_evaluated) {
      params.error_message_add(
          NodeWarningType::Error,
          TIP_("Cannot access object's transforms because it's not evaluated yet. "
               "This can happen when there is a dependency cycle"));
      params.set_default_remaining_outputs();
      return;
    }
    geometry_transform = self_object->world_to_object() * object->object_to_world();
  }

  GeometrySet geometry_set;
  if (params.extract_input<bool>("As Instance")) {
    std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
    const int handle = instances->add_reference(*object);
    if (transform_space_relative) {
      instances->add_instance(handle, *geometry_transform);
    }
    else {
      instances->add_instance(handle, float4x4::identity());
    }
    geometry_set = GeometrySet::from_instances(instances.release());
  }
  else {
    geometry_set = bke::object_get_evaluated_geometry_set(*object);
    if (transform_space_relative) {
      geometry::transform_geometry(geometry_set, *geometry_transform);
    }
  }

  geometry_set.name = object->id.name + 2;
  params.set_output("Geometry", geometry_set);
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryObjectInfo *data = MEM_callocN<NodeGeometryObjectInfo>(__func__);
  data->transform_space = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
  node->storage = data;
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_object_info_transform_space_items[] = {
      {GEO_NODE_TRANSFORM_SPACE_ORIGINAL,
       "ORIGINAL",
       0,
       "Original",
       "Output the geometry relative to the input object transform, and the location, rotation "
       "and "
       "scale relative to the world origin"},
      {GEO_NODE_TRANSFORM_SPACE_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Bring the input object geometry, location, rotation and scale into the modified object, "
       "maintaining the relative position between the two objects in the scene"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop = RNA_def_node_enum(srna,
                                        "transform_space",
                                        "Transform Space",
                                        "The transformation of the vector and geometry outputs",
                                        rna_node_geometry_object_info_transform_space_items,
                                        NOD_storage_enum_accessors(transform_space),
                                        GEO_NODE_TRANSFORM_SPACE_ORIGINAL);
  RNA_def_property_update_runtime(prop, rna_Node_update_relations);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeObjectInfo", GEO_NODE_OBJECT_INFO);
  ntype.ui_name = "Object Info";
  ntype.ui_description = "Retrieve information from an object";
  ntype.enum_name_legacy = "OBJECT_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.initfunc = node_node_init;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryObjectInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_object_info_cc
