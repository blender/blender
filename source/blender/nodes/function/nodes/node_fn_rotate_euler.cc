/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_function_util.hh"

namespace blender::nodes::node_fn_rotate_euler_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  auto enable_axis_angle = [](bNode &node) {
    node.custom1 = FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE;
  };

  b.is_function_node();
  b.add_input<decl::Vector>(N_("Rotation")).subtype(PROP_EULER).hide_value();
  b.add_input<decl::Vector>(N_("Rotate By")).subtype(PROP_EULER).make_available([](bNode &node) {
    node.custom1 = FN_NODE_ROTATE_EULER_TYPE_EULER;
  });
  b.add_input<decl::Vector>(N_("Axis"))
      .default_value({0.0, 0.0, 1.0})
      .subtype(PROP_XYZ)
      .make_available(enable_axis_angle);
  b.add_input<decl::Float>(N_("Angle")).subtype(PROP_ANGLE).make_available(enable_axis_angle);
  b.add_output<decl::Vector>(N_("Rotation"));
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *rotate_by_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1));
  bNodeSocket *axis_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 2));
  bNodeSocket *angle_socket = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 3));

  nodeSetSocketAvailability(
      ntree, rotate_by_socket, ELEM(node->custom1, FN_NODE_ROTATE_EULER_TYPE_EULER));
  nodeSetSocketAvailability(
      ntree, axis_socket, ELEM(node->custom1, FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE));
  nodeSetSocketAvailability(
      ntree, angle_socket, ELEM(node->custom1, FN_NODE_ROTATE_EULER_TYPE_AXIS_ANGLE));
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "type", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "space", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
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

}  // namespace blender::nodes::node_fn_rotate_euler_cc

void register_node_type_fn_rotate_euler()
{
  namespace file_ns = blender::nodes::node_fn_rotate_euler_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_ROTATE_EULER, "Rotate Euler", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.updatefunc = file_ns::node_update;
  ntype.build_multi_function = file_ns::node_build_multi_function;
  nodeRegisterType(&ntype);
}
