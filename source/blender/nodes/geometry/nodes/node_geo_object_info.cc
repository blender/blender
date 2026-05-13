/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_euler.hh"
#include "BLI_math_matrix.hh"

#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "DNA_object_types.h"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_transform.hh"

#include "BLT_translation.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_object_info_cc {

NODE_STORAGE_FUNCS(NodeGeometryObjectInfo)

static void node_declare(NodeDeclarationBuilder &b)
{
  const bool is_geometry = b.tree_or_null() ? b.tree_or_null()->type == NTREE_GEOMETRY : true;
  b.add_input<decl::Object>("Object"_ustr).optional_label();
  b.add_input<decl::Bool>("As Instance"_ustr)
      .description(
          "Output the entire object as single instance. "
          "This allows instancing non-geometry object types")
      .available(is_geometry);
  b.add_output<decl::Matrix>("Transform"_ustr)
      .description(
          "Transformation matrix containing the location, rotation and scale of the object");
  b.add_output<decl::Vector>("Location"_ustr);
  b.add_output<decl::Rotation>("Rotation"_ustr);
  b.add_output<decl::Vector>("Scale"_ustr);
  b.add_output<decl::Geometry>("Geometry"_ustr).available(is_geometry);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNodeTree &node_tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  if (node_tree.type == NTREE_GEOMETRY) {
    layout.prop(ptr, "transform_space", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryObjectInfo &storage = node_storage(params.node());
  const bool transform_space_relative = (storage.transform_space ==
                                         GEO_NODE_TRANSFORM_SPACE_RELATIVE);

  Object *object = params.extract_input<Object *>("Object"_ustr);

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

  params.set_output("Location"_ustr, location);
  params.set_output("Rotation"_ustr, rotation);
  params.set_output("Scale"_ustr, scale);
  params.set_output("Transform"_ustr, output_transform);

  if (!params.output_is_required("Geometry"_ustr)) {
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
  if (params.extract_input<bool>("As Instance"_ustr)) {
    auto instances = std::make_unique<bke::Instances>(1);
    instances->reference_handles_for_write().first() = instances->add_reference(*object);
    if (transform_space_relative) {
      instances->transforms_for_write().first() = *geometry_transform;
    }
    else {
      instances->transforms_for_write().first() = float4x4::identity();
    }
    geometry_set = GeometrySet::from_instances(std::move(instances));
  }
  else {
    geometry_set = bke::object_get_evaluated_geometry_set(*object);
    if (transform_space_relative) {
      geometry::transform_geometry(geometry_set, *geometry_transform);
    }
  }

  geometry_set.set_name(object->id.name + 2);
  params.set_output("Geometry"_ustr, geometry_set);
}

using namespace blender::compositor;

class ObjectInfoOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Object *object = this->get_input("Object").get_single_value<Object *>();
    if (!object) {
      this->allocate_default_remaining_outputs();
      return;
    }

    const float4x4 transform = object->object_to_world();

    float3 location, scale;
    math::Quaternion rotation;
    math::to_loc_rot_scale_safe<true>(transform, location, rotation, scale);

    Result &transform_result = this->get_result("Transform");
    if (transform_result.should_compute()) {
      transform_result.allocate_single_value();
      transform_result.set_single_value(transform);
    }

    Result &location_result = this->get_result("Location");
    if (location_result.should_compute()) {
      location_result.allocate_single_value();
      location_result.set_single_value(location);
    }

    Result &rotation_result = this->get_result("Rotation");
    if (rotation_result.should_compute()) {
      rotation_result.allocate_single_value();
      rotation_result.set_single_value(rotation);
    }

    Result &scale_result = this->get_result("Scale");
    if (scale_result.should_compute()) {
      scale_result.allocate_single_value();
      scale_result.set_single_value(scale);
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new ObjectInfoOperation(context, node);
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryObjectInfo *data = MEM_new<NodeGeometryObjectInfo>(__func__);
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

static void node_gather_link_searches(nodes::GatherLinkSearchOpParams &params)
{
  if (params.node_tree().type == NTREE_GEOMETRY) {
    search_link_ops_for_basic_node(params);
    return;
  }

  static Set<UString> skip_socket_identifiers = {"As Instance"_ustr, "Geometry"_ustr};
  nodes::search_filtered_link_ops_for_basic_node(params, skip_socket_identifiers);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "GeometryNodeObjectInfo"_ustr, GEO_NODE_OBJECT_INFO);
  ntype.ui_name = "Object Info";
  ntype.ui_description = "Retrieve information from an object";
  ntype.enum_name_legacy = "OBJECT_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.initfunc = node_node_init;
  bke::node_type_storage(
      ntype, "NodeGeometryObjectInfo", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_object_info_cc
