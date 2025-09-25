/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotate_euler_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  auto enable_axis_angle = [](bNode &node) {
    node.custom1 = FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE;
  };

  b.is_function_node();
  b.add_input<decl::Vector>("Rotation").subtype(PROP_EULER).hide_value();
  b.add_input<decl::Vector>("Rotate By").subtype(PROP_EULER).make_available([](bNode &node) {
    node.custom1 = FN_NODE_ROTATE_EULER_TYPE_EULER;
  });
  b.add_input<decl::Vector>("Axis")
      .default_value({0.0, 0.0, 1.0})
      .subtype(PROP_XYZ)
      .make_available(enable_axis_angle);
  b.add_input<decl::Float>("Angle").subtype(PROP_ANGLE).make_available(enable_axis_angle);
  b.add_output<decl::Vector>("Rotation");
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *rotate_by_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1));
  bNodeSocket *axis_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 2));
  bNodeSocket *angle_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 3));

  bke::node_set_socket_availability(
      *ntree, *rotate_by_socket, ELEM(node->custom1, FN_NODE_ROTATE_EULER_TYPE_EULER));
  bke::node_set_socket_availability(
      *ntree, *axis_socket, ELEM(node->custom1, FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE));
  bke::node_set_socket_availability(
      *ntree, *angle_socket, ELEM(node->custom1, FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE));
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "rotation_type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(ptr, "space", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static const mf::MultiFunction *get_multi_function(const bNode &bnode)
{
  static auto obj_euler_rot = mf::build::SI2_SO<float3, float3, float3>(
      "Rotate Euler by Euler/Object", [](const float3 &input, const float3 &rotation) {
        float input_mat[3][3];
        eul_to_mat3(input_mat, input);
        float rot_mat[3][3];
        eul_to_mat3(rot_mat, rotation);
        float mat_res[3][3];
        mul_m3_m3m3(mat_res, rot_mat, input_mat);
        float3 result;
        mat3_to_eul(result, mat_res);
        return result;
      });
  static auto obj_AA_rot = mf::build::SI3_SO<float3, float3, float, float3>(
      "Rotate Euler by AxisAngle/Object",
      [](const float3 &input, const float3 &axis, float angle) {
        float input_mat[3][3];
        eul_to_mat3(input_mat, input);
        float rot_mat[3][3];
        axis_angle_to_mat3(rot_mat, axis, angle);
        float mat_res[3][3];
        mul_m3_m3m3(mat_res, rot_mat, input_mat);
        float3 result;
        mat3_to_eul(result, mat_res);
        return result;
      });
  static auto local_euler_rot = mf::build::SI2_SO<float3, float3, float3>(
      "Rotate Euler by Euler/Local", [](const float3 &input, const float3 &rotation) {
        float input_mat[3][3];
        eul_to_mat3(input_mat, input);
        float rot_mat[3][3];
        eul_to_mat3(rot_mat, rotation);
        float mat_res[3][3];
        mul_m3_m3m3(mat_res, input_mat, rot_mat);
        float3 result;
        mat3_to_eul(result, mat_res);
        return result;
      });
  static auto local_AA_rot = mf::build::SI3_SO<float3, float3, float, float3>(
      "Rotate Euler by AxisAngle/Local", [](const float3 &input, const float3 &axis, float angle) {
        float input_mat[3][3];
        eul_to_mat3(input_mat, input);
        float rot_mat[3][3];
        axis_angle_to_mat3(rot_mat, axis, angle);
        float mat_res[3][3];
        mul_m3_m3m3(mat_res, input_mat, rot_mat);
        float3 result;
        mat3_to_eul(result, mat_res);
        return result;
      });
  short type = bnode.custom1;
  short space = bnode.custom2;
  if (type == FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE) {
    return space == FN_NODE_ROTATE_EULER_SPACE_OBJECT ?
               static_cast<const mf::MultiFunction *>(&obj_AA_rot) :
               &local_AA_rot;
  }
  if (type == FN_NODE_ROTATE_EULER_TYPE_EULER) {
    return space == FN_NODE_ROTATE_EULER_SPACE_OBJECT ?
               static_cast<const mf::MultiFunction *>(&obj_euler_rot) :
               &local_euler_rot;
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeRotateEuler", FN_NODE_ROTATE_EULER);
  ntype.ui_name = "Rotate Euler";
  ntype.ui_description = "Apply a secondary Euler rotation to a given Euler rotation";
  ntype.enum_name_legacy = "ROTATE_EULER";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.updatefunc = node_update;
  ntype.build_multi_function = node_build_multi_function;
  ntype.deprecation_notice = N_("Use the \"Rotate Rotation\" node instead");
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_rotate_euler_cc
