/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "BKE_action.hh"
#include "BKE_armature.hh"

#include "DEG_depsgraph_query.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

#include <fmt/format.h>

namespace blender::nodes::node_geo_bone_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Object>("Armature"_ustr)
      .optional_label()
      .description("Armature object to retrieve the bone information from");
  b.add_input<decl::String>("Bone Name"_ustr)
      .optional_label()
      .description("Name of the bone to retrieve");

  b.add_output<decl::Matrix>("Pose"_ustr)
      .description("Evaluated final transform of the bone in armature space");
  b.add_output<decl::Matrix>("Local Pose"_ustr)
      .description("Difference between the pose and rest pose relative to the parent bone");
  b.add_output<decl::Matrix>("Transform Pose"_ustr)
      .description("Matrix representing the bone's location, rotation, and scale properties");
  b.add_output<decl::Matrix>("Rest Pose"_ustr)
      .description("Original transform of the bone in armature space, defined in edit mode");
  b.add_output<decl::Float>("Rest Length"_ustr).description("Original length of the bone");
  b.add_output<decl::Bool>("Exists"_ustr).description("Whether the bone exists in the armature");
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNodeTree &node_tree = *id_cast<const bNodeTree *>(ptr->owner_id);
  if (node_tree.type == NTREE_GEOMETRY) {
    layout.prop(ptr, "transform_space", ui::ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype other_type = params.other_socket().type;

  if (params.in_out() == SOCK_OUT) {
    if (ELEM(other_type, SOCK_MATRIX, SOCK_ROTATION)) {
      params.add_item(IFACE_("Pose"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Pose"_ustr);
      });
      params.add_item(IFACE_("Local Pose"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Local Pose"_ustr);
      });
      params.add_item(IFACE_("Transform Pose"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Transform Pose"_ustr);
      });
      params.add_item(IFACE_("Rest Pose"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Rest Pose"_ustr);
      });
    }
    if (params.node_tree().typeinfo->validate_link(other_type, SOCK_FLOAT)) {
      params.add_item(IFACE_("Rest Length"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Rest Length"_ustr);
      });
    }
  }
  else {
    if (other_type == SOCK_STRING) {
      params.add_item(IFACE_("Bone Name"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Bone Name"_ustr);
      });
    }
    if (other_type == SOCK_OBJECT) {
      params.add_item(IFACE_("Armature"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBoneInfo"_ustr);
        params.update_and_connect_available_socket(node, "Armature"_ustr);
      });
    }
  }
}

static void node_node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = GEO_NODE_TRANSFORM_SPACE_ORIGINAL;
}

struct BoneInfo {
  float4x4 pose;
  float4x4 local_pose;
  float4x4 transform_pose;
  float4x4 rest_pose;
  float rest_length;
};

/* Computes the bone info of the bone with the given pose channel in the given armature object.
 * If provided, the bone will be transformed into the given transformation space. */
static BoneInfo compute_bone_info(const Object &object,
                                  const bPoseChannel &pchan,
                                  const float4x4 &space_transformation = float4x4::identity())
{
  const Bone *bone = pchan.bone_get(object);
  const float4x4 pose = space_transformation * float4x4(pchan.pose_mat);
  const float4x4 rest_pose = space_transformation * float4x4(bone->arm_mat);

  const float4x4 parent_pose = pchan.parent ? float4x4(pchan.parent->pose_mat) :
                                              float4x4::identity();
  const float4x4 parent_rest_pose = bone->parent ? float4x4(bone->parent->arm_mat) :
                                                   float4x4::identity();
  const float4x4 local_pose = math::invert(rest_pose) * parent_rest_pose *
                              math::invert(parent_pose) * pose;

  float4x4 transform_pose;
  BKE_pchan_to_mat4({&pchan, bone}, transform_pose.ptr());

  return {pose, local_pose, transform_pose, rest_pose, bone->length};
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Object *object = params.extract_input<Object *>("Armature"_ustr);
  if (!object) {
    params.set_default_remaining_outputs();
    return;
  }
  if (object->type != OB_ARMATURE) {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, TIP_("Object is not an armature"));
    return;
  }
  const std::string bone_name = params.extract_input<std::string>("Bone Name"_ustr);
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

  const bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose, bone_name.c_str());
  if (!pchan) {
    params.set_default_remaining_outputs();
    if (!params.output_is_required("Exists"_ustr)) {
      params.error_message_add(
          NodeWarningType::Error,
          fmt::format(fmt::runtime(TIP_("Bone \"{}\" not found")), bone_name));
    }
    return;
  }

  const BoneInfo values = compute_bone_info(*object, *pchan, geometry_transform);
  params.set_output("Pose"_ustr, values.pose);
  params.set_output("Local Pose"_ustr, values.local_pose);
  params.set_output("Transform Pose"_ustr, values.transform_pose);
  params.set_output("Rest Pose"_ustr, values.rest_pose);
  params.set_output("Rest Length"_ustr, values.rest_length);
  params.set_output("Exists"_ustr, true);
}

class BoneInfoOperation : public compositor::NodeOperation {
 public:
  using compositor::NodeOperation::NodeOperation;

  void execute() override
  {
    const Object *object = this->get_input("Armature").get_single_value<Object *>();
    if (!object) {
      this->allocate_default_remaining_outputs();
      return;
    }

    if (object->type != OB_ARMATURE) {
      this->allocate_default_remaining_outputs();
      this->add_warning(NodeWarningType::Error, TIP_("Object is not an armature"));
      return;
    }

    const std::string bone_name = this->get_input("Bone Name").get_single_value<std::string>();
    if (bone_name.empty()) {
      this->allocate_default_remaining_outputs();
      return;
    }

    if (!object->pose) {
      this->allocate_default_remaining_outputs();
      this->add_warning(NodeWarningType::Error, TIP_("Object has no pose"));
      return;
    }

    const bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose, bone_name.c_str());
    if (!pchan) {
      this->allocate_default_remaining_outputs();
      if (!this->get_result("Exists").should_compute()) {
        this->add_warning(NodeWarningType::Error,
                          fmt::format(fmt::runtime(TIP_("Bone \"{}\" not found")), bone_name));
      }
      return;
    }

    const BoneInfo values = compute_bone_info(*object, *pchan);

    compositor::Result &pose_result = this->get_result("Pose");
    if (pose_result.should_compute()) {
      pose_result.allocate_single_value();
      pose_result.set_single_value(values.pose);
    }

    compositor::Result &local_pose_result = this->get_result("Local Pose");
    if (local_pose_result.should_compute()) {
      local_pose_result.allocate_single_value();
      local_pose_result.set_single_value(values.local_pose);
    }

    compositor::Result &transform_pose_result = this->get_result("Transform Pose");
    if (transform_pose_result.should_compute()) {
      transform_pose_result.allocate_single_value();
      transform_pose_result.set_single_value(values.transform_pose);
    }

    compositor::Result &rest_pose_result = this->get_result("Rest Pose");
    if (rest_pose_result.should_compute()) {
      rest_pose_result.allocate_single_value();
      rest_pose_result.set_single_value(values.rest_pose);
    }

    compositor::Result &rest_length_result = this->get_result("Rest Length");
    if (rest_length_result.should_compute()) {
      rest_length_result.allocate_single_value();
      rest_length_result.set_single_value(values.rest_length);
    }

    compositor::Result &exists_result = this->get_result("Exists");
    if (exists_result.should_compute()) {
      exists_result.allocate_single_value();
      exists_result.set_single_value(true);
    }
  }
};

static compositor::NodeOperation *get_compositor_operation(compositor::Context &context,
                                                           const bNode &node)
{
  return new BoneInfoOperation(context, node);
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
  static bke::bNodeType ntype;
  geo_cmp_node_type_base(&ntype, "GeometryNodeBoneInfo"_ustr);
  ntype.ui_name = "Bone Info";
  ntype.ui_description = "Retrieve information of armature bones";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_node_init;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.get_compositor_operation = get_compositor_operation;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_bone_info_cc
