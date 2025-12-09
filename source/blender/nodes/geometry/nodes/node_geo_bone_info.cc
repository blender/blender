/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"

#include "DEG_depsgraph_query.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_bone_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Object>("Armature")
      .optional_label()
      .description("Armature object to retrieve the bone information from");
  b.add_input<decl::String>("Bone Name")
      .optional_label()
      .description("Name of the bone to retrieve");

  b.add_output<decl::Matrix>("Pose").description(
      "Evaluated final transform of the bone in armature space");
  b.add_output<decl::Matrix>("Local Pose")
      .description("Difference between the pose and rest pose relative to the parent bone");
  b.add_output<decl::Matrix>("Transform Pose")
      .description("Matrix representing the bone's location, rotation, and scale properties");
  b.add_output<decl::Matrix>("Rest Pose")
      .description("Original transform of the bone in armature space, defined in edit mode");
  b.add_output<decl::Float>("Rest Length").description("Original length of the bone");
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "transform_space", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Object *object = params.extract_input<Object *>("Armature");
  if (!object) {
    params.set_default_remaining_outputs();
    return;
  }
  if (object->type != OB_ARMATURE) {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, TIP_("Object is not an armature"));
    return;
  }
  const std::string bone_name = params.extract_input<std::string>("Bone Name");
  if (bone_name.empty()) {
    params.set_default_remaining_outputs();
    return;
  }
  if (!object->pose) {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, TIP_("Object has no pose"));
    return;
  }

  const bool transform_space_relative = (params.node().custom1 ==
                                         GEO_NODE_TRANSFORM_SPACE_RELATIVE);
  float4x4 geometry_transform = float4x4::identity();
  if (transform_space_relative) {
    const Object *self_object = params.self_object();
    const bool self_transform_evaluated = DEG_object_transform_is_evaluated(*self_object);
    const bool object_transform_evaluated = DEG_object_transform_is_evaluated(*object);
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

  bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose, bone_name.c_str());
  if (!pchan) {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, TIP_("Bone not found"));
    return;
  }
  Bone *bone = pchan->bone;
  const float4x4 pose = geometry_transform * float4x4(pchan->pose_mat);
  const float4x4 rest_pose = geometry_transform * float4x4(bone->arm_mat);

  const float4x4 parent_pose = pchan->parent ? float4x4(pchan->parent->pose_mat) :
                                               float4x4::identity();
  const float4x4 parent_rest_pose = bone->parent ? float4x4(bone->parent->arm_mat) :
                                                   float4x4::identity();
  const float4x4 local_pose = math::invert(rest_pose) * parent_rest_pose *
                              math::invert(parent_pose) * pose;

  float4x4 transform_pose;
  BKE_pchan_to_mat4(pchan, transform_pose.ptr());

  params.set_output("Pose", pose);
  params.set_output("Local Pose", local_pose);
  params.set_output("Transform Pose", transform_pose);
  params.set_output("Rest Pose", rest_pose);
  params.set_output("Rest Length", bone->length);
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_object_info_transform_space_items[] = {
      {GEO_NODE_TRANSFORM_SPACE_ORIGINAL,
       "ORIGINAL",
       0,
       "Original",
       "Output the bone pose relative to the armature object transform"},
      {GEO_NODE_TRANSFORM_SPACE_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Bring the bone pose into the modified object"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop = RNA_def_node_enum(srna,
                                        "transform_space",
                                        "Transform Space",
                                        "The transformation of the vector and geometry outputs",
                                        rna_node_geometry_object_info_transform_space_items,
                                        NOD_inline_enum_accessors(custom1),
                                        GEO_NODE_TRANSFORM_SPACE_ORIGINAL);
  RNA_def_property_update_runtime(prop, rna_Node_update_relations);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeBoneInfo");
  ntype.ui_name = "Bone Info";
  ntype.ui_description = "Retrieve information of armature bones";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_node_init;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_bone_info_cc
